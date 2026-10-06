import asyncio
import base64
import ipaddress
import json
import os
import time
from contextlib import asynccontextmanager

import boto3
import httpx
from fastapi import FastAPI, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse, JSONResponse, Response, StreamingResponse
from websockets import connect as ws_connect
from websockets.exceptions import ConnectionClosed

AWS_REGION = os.environ["AWS_REGION"]
EC2_INSTANCE_ID = os.environ["EC2_INSTANCE_ID"]
EC2_APP_PORT = int(os.environ.get("EC2_APP_PORT", "80"))
IDLE_STOP_MINUTES = float(os.environ.get("IDLE_STOP_MINUTES", "0"))
WARM_CHECK_TIMEOUT_S = float(os.environ.get("WARM_CHECK_TIMEOUT_S", "4"))
IDLE_CHECK_INTERVAL_S = 60
START_DEBOUNCE_S = 20
READY_STATUS_TTL_S = 5
WAKE_LOG_PATH = os.environ.get("WAKE_LOG_PATH", "/app/logs/wake-events.jsonl")
MAX_TRACKED_VISITORS = 200
MAX_TRACKED_PATHS = 100
S3_BUCKET_NAME = os.environ.get("S3_BUCKET_NAME")
S3_LOG_PREFIX = "logs/dronechallenger-wake-service"
PRIVATE_PATH_PREFIXES = ("/api", "/api-definition")
DRONE_PNG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "drone.png")

ec2 = boto3.client("ec2", region_name=AWS_REGION)
s3 = boto3.client("s3", region_name=AWS_REGION)

_state = {
    "last_activity": time.time(), "cached_ip": None, "warm": False, "last_start_call": 0.0,
    "ready_status": None, "ready_status_at": 0.0, "client": None,
}
_describe_lock = asyncio.Lock()
_status_lock = asyncio.Lock()
_session: dict | None = None


def _iso(ts: float | None) -> str | None:
    if ts is None:
        return None
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(ts))


def _log_event(event: str, **fields):
    record = {"event": event, "at": _iso(time.time()), "instance_id": EC2_INSTANCE_ID, **fields}
    line = json.dumps(record, default=str)
    print(line, flush=True)
    if _session is not None:
        _session["events"].append(line)
    try:
        os.makedirs(os.path.dirname(WAKE_LOG_PATH), exist_ok=True)
        with open(WAKE_LOG_PATH, "a") as f:
            f.write(line + "\n")
    except OSError as e:
        print(f"wake log write failed: {e}", flush=True)


def client_info(headers, client_host: str | None, path: str, method: str) -> dict:
    forwarded = headers.get("x-forwarded-for", "").split(",")[0].strip()
    ip = headers.get("cf-connecting-ip") or forwarded or headers.get("x-real-ip") or client_host
    try:
        ip_version = ipaddress.ip_address(ip).version
    except (ValueError, TypeError):
        ip_version = None
    return {
        "ip": ip, "ip_version": ip_version, "country": headers.get("cf-ipcountry"),
        "user_agent": headers.get("user-agent"), "referer": headers.get("referer"),
        "method": method, "path": "/" + path.lstrip("/"),
    }


def _path_bucket(path: str) -> str:
    return "/" + "/".join(path.strip("/").split("/")[:2])


def _open_session(kind: str, trigger: dict | None):
    global _session
    now = time.time()
    _session = {
        "id": time.strftime("%Y%m%dT%H%M%SZ", time.gmtime(now)), "kind": kind, "opened_at": now,
        "trigger": trigger, "ready_at": None, "last_request_at": None,
        "requests": 0, "websockets": 0, "visitors": {}, "paths": {}, "events": [],
    }
    _log_event("wake" if kind == "wake" else "session_adopted", session_id=_session["id"], trigger=trigger)


def _close_session(reason: str):
    global _session
    if _session is None:
        return
    s, now = _session, time.time()
    visitors = sorted(s["visitors"].items(), key=lambda kv: -kv[1]["requests"])
    _log_event(
        "stop", session_id=s["id"], reason=reason, session_kind=s["kind"],
        opened_at=_iso(s["opened_at"]), ready_at=_iso(s["ready_at"]),
        last_request_at=_iso(s["last_request_at"]),
        awake_seconds=round(now - s["opened_at"]),
        boot_seconds=round(s["ready_at"] - s["opened_at"]) if s["ready_at"] else None,
        idle_tail_seconds=round(now - s["last_request_at"]) if s["last_request_at"] else None,
        trigger=s["trigger"], requests=s["requests"], websockets=s["websockets"],
        unique_visitors=len(s["visitors"]),
        visitors=[{"ip": ip, **v, "first_seen": _iso(v["first_seen"]), "last_seen": _iso(v["last_seen"])}
                  for ip, v in visitors],
        paths=dict(sorted(s["paths"].items(), key=lambda kv: -kv[1])),
    )
    _session = None
    if S3_BUCKET_NAME:
        asyncio.get_running_loop().run_in_executor(
            None, _upload_session, s["id"], ("\n".join(s["events"]) + "\n").encode(),
        )


def _upload_session(session_id: str, data: bytes):
    key = f"{S3_LOG_PREFIX}/{session_id}.jsonl"
    try:
        s3.put_object(Bucket=S3_BUCKET_NAME, Key=key, Body=data, ContentType="application/x-ndjson")
        print(f"session log uploaded to s3://{S3_BUCKET_NAME}/{key}", flush=True)
    except Exception as e:
        print(f"session log upload failed: {e}", flush=True)


def _mark_ready():
    if _session is not None and _session["ready_at"] is None:
        _session["ready_at"] = time.time()
        _log_event("ready", session_id=_session["id"],
                   boot_seconds=round(_session["ready_at"] - _session["opened_at"]))


def track(info: dict, ec2_state: str, websocket: bool = False):
    if _session is None:
        if ec2_state != "running":
            return
        _open_session("adopted", info)
    s, now = _session, time.time()
    s["last_request_at"] = now
    s["requests"] += 1
    if websocket:
        s["websockets"] += 1
    ip = info["ip"] or "unknown"
    v = s["visitors"].get(ip)
    if v is None and len(s["visitors"]) < MAX_TRACKED_VISITORS:
        v = s["visitors"][ip] = {
            "ip_version": info["ip_version"], "country": info["country"],
            "user_agents": [], "first_seen": now, "last_seen": now, "requests": 0,
        }
    if v is not None:
        v["last_seen"] = now
        v["requests"] += 1
        if info["user_agent"] and info["user_agent"] not in v["user_agents"] and len(v["user_agents"]) < 5:
            v["user_agents"].append(info["user_agent"])
    bucket = _path_bucket(info["path"])
    if bucket in s["paths"] or len(s["paths"]) < MAX_TRACKED_PATHS:
        s["paths"][bucket] = s["paths"].get(bucket, 0) + 1


def _describe_sync():
    resp = ec2.describe_instances(InstanceIds=[EC2_INSTANCE_ID])
    inst = resp["Reservations"][0]["Instances"][0]
    return inst["State"]["Name"], inst.get("PublicIpAddress")


async def describe():
    async with _describe_lock:
        return await asyncio.to_thread(_describe_sync)


async def maybe_start(trigger: dict | None = None):
    now = time.time()
    if now - _state["last_start_call"] < START_DEBOUNCE_S:
        return
    _state["last_start_call"] = now
    await asyncio.to_thread(ec2.start_instances, InstanceIds=[EC2_INSTANCE_ID])
    if _session is not None:
        _close_session("instance_stopped")
    _open_session("wake", trigger)


async def _api_list(ip: str, resource: str) -> list | None:
    try:
        async with httpx.AsyncClient(timeout=WARM_CHECK_TIMEOUT_S) as client:
            r = await client.get(f"http://{ip}:{EC2_APP_PORT}/api/{resource}")
            if r.status_code != 200:
                return None
            data = r.json()
            return data if isinstance(data, list) else None
    except (httpx.HTTPError, ValueError):
        return None


async def check_warm(ip: str) -> bool:
    streamers = await _api_list(ip, "streamers")
    return bool(streamers)


async def connected_players(ip: str) -> int:
    players = await _api_list(ip, "players")
    return len(players) if players is not None else 0


async def current_status(trigger: dict | None = None) -> dict:
    ec2_state, ip = await describe()
    if ec2_state == "running" and ip:
        _state["cached_ip"] = ip
        _state["warm"] = await check_warm(ip)
        if _state["warm"]:
            _mark_ready()
    else:
        _state["warm"] = False
        if ec2_state != "running":
            _state["cached_ip"] = None

    if ec2_state == "running" and _state["warm"]:
        stage, detail = "ready", "Ready."
    elif ec2_state == "running":
        stage, detail = "warming", "Instance is up, starting the game..."
    elif ec2_state in ("pending",):
        stage, detail = "booting", "Instance is booting..."
    elif ec2_state == "stopped":
        await maybe_start(trigger)
        stage, detail = "starting", "Starting the game server..."
    elif ec2_state == "stopping":
        stage, detail = "booting", "Finishing the previous shutdown, then restarting..."
    else:
        stage, detail = "error", f"Unexpected instance state: {ec2_state}"

    return {
        "stage": stage, "detail": detail, "ec2_state": ec2_state,
        "ip": _state["cached_ip"], "warm": _state["warm"],
    }


def _fresh_ready_status() -> dict | None:
    status = _state["ready_status"]
    if status and time.monotonic() - _state["ready_status_at"] < READY_STATUS_TTL_S:
        return status
    return None


async def proxy_status(trigger: dict | None = None) -> dict:
    status = _fresh_ready_status()
    if status:
        return status
    async with _status_lock:
        status = _fresh_ready_status()
        if status:
            return status
        status = await current_status(trigger)
        if status["stage"] == "ready":
            _state["ready_status"], _state["ready_status_at"] = status, time.monotonic()
        else:
            _state["ready_status"] = None
        return status


async def idle_stop_loop():
    if IDLE_STOP_MINUTES <= 0:
        return
    while True:
        await asyncio.sleep(IDLE_CHECK_INTERVAL_S)
        idle_for = time.time() - _state["last_activity"]
        if idle_for < IDLE_STOP_MINUTES * 60:
            continue
        try:
            ec2_state, _ = await describe()
        except Exception:
            continue
        if ec2_state == "running":
            await asyncio.to_thread(ec2.stop_instances, InstanceIds=[EC2_INSTANCE_ID])
            _close_session("idle")
            _state["cached_ip"] = None
            _state["warm"] = False
            _state["ready_status"] = None
            _state["last_activity"] = time.time()


@asynccontextmanager
async def lifespan(app: FastAPI):
    _state["client"] = httpx.AsyncClient(
        timeout=60, limits=httpx.Limits(max_connections=100, max_keepalive_connections=32),
    )
    task = asyncio.create_task(idle_stop_loop())
    yield
    task.cancel()
    await _state["client"].aclose()


app = FastAPI(lifespan=lifespan)

with open(DRONE_PNG_PATH, "rb") as _f:
    DRONE_DATA_URI = "data:image/png;base64," + base64.b64encode(_f.read()).decode()

BUSY_PAGE = """<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Drone Challenger</title>
<link rel="preconnect" href="https://fonts.googleapis.com" />
<link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&display=swap" rel="stylesheet" />
<style>
  html, body { height: 100%; margin: 0; }
  body {
    display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 36px;
    background: #000; color: #e8e8e8; overflow: hidden;
    font-family: "Press Start 2P", ui-monospace, Menlo, monospace;
  }
  .stage { position: relative; width: min(1040px, 96vw); height: 340px; }
  .title {
    position: absolute; left: 0; right: 0; top: 50%; transform: translateY(-50%);
    display: flex; justify-content: center; gap: 0.08em;
    font-size: clamp(28px, 7vw, 64px);
  }
  .title span {
    display: inline-block;
    background: linear-gradient(#fff6b0 0%, #ffd23a 35%, #f0a400 65%, #b86b00 100%);
    -webkit-background-clip: text; background-clip: text; color: transparent;
    animation: wave 1.8s ease-in-out infinite;
  }
  .drone-path { position: absolute; inset: 0; pointer-events: none; }
  .drone {
    position: absolute; width: clamp(72px, 13vw, 124px);
    filter: drop-shadow(0 0 8px rgba(255, 210, 58, 0.35));
    animation: loiter 11s linear infinite;
  }
  .drone img { display: block; width: 100%; animation: bob 1.3s ease-in-out infinite; }
  .status { font-size: clamp(10px, 2.2vw, 14px); color: #c9c9c9; letter-spacing: 0.04em; text-align: center; line-height: 1.8; }
  .sub { color: #7d7d7d; }
  @keyframes wave { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-0.18em); } }
  @keyframes loiter {
    0.00% { left: 50.00%; top: 9.00%; transform: translate(-50%, -50%) rotate(10.0deg); }
    4.17% { left: 62.16%; top: 10.40%; transform: translate(-50%, -50%) rotate(9.7deg); }
    8.33% { left: 73.50%; top: 14.49%; transform: translate(-50%, -50%) rotate(8.7deg); }
    12.50% { left: 83.23%; top: 21.01%; transform: translate(-50%, -50%) rotate(7.1deg); }
    16.67% { left: 90.70%; top: 29.50%; transform: translate(-50%, -50%) rotate(5.0deg); }
    20.83% { left: 95.40%; top: 39.39%; transform: translate(-50%, -50%) rotate(2.6deg); }
    25.00% { left: 97.00%; top: 50.00%; transform: translate(-50%, -50%) rotate(0.0deg); }
    29.17% { left: 95.40%; top: 60.61%; transform: translate(-50%, -50%) rotate(-2.6deg); }
    33.33% { left: 90.70%; top: 70.50%; transform: translate(-50%, -50%) rotate(-5.0deg); }
    37.50% { left: 83.23%; top: 78.99%; transform: translate(-50%, -50%) rotate(-7.1deg); }
    41.67% { left: 73.50%; top: 85.51%; transform: translate(-50%, -50%) rotate(-8.7deg); }
    45.83% { left: 62.16%; top: 89.60%; transform: translate(-50%, -50%) rotate(-9.7deg); }
    50.00% { left: 50.00%; top: 91.00%; transform: translate(-50%, -50%) rotate(-10.0deg); }
    54.17% { left: 37.84%; top: 89.60%; transform: translate(-50%, -50%) rotate(-9.7deg); }
    58.33% { left: 26.50%; top: 85.51%; transform: translate(-50%, -50%) rotate(-8.7deg); }
    62.50% { left: 16.77%; top: 78.99%; transform: translate(-50%, -50%) rotate(-7.1deg); }
    66.67% { left: 9.30%; top: 70.50%; transform: translate(-50%, -50%) rotate(-5.0deg); }
    70.83% { left: 4.60%; top: 60.61%; transform: translate(-50%, -50%) rotate(-2.6deg); }
    75.00% { left: 3.00%; top: 50.00%; transform: translate(-50%, -50%) rotate(-0.0deg); }
    79.17% { left: 4.60%; top: 39.39%; transform: translate(-50%, -50%) rotate(2.6deg); }
    83.33% { left: 9.30%; top: 29.50%; transform: translate(-50%, -50%) rotate(5.0deg); }
    87.50% { left: 16.77%; top: 21.01%; transform: translate(-50%, -50%) rotate(7.1deg); }
    91.67% { left: 26.50%; top: 14.49%; transform: translate(-50%, -50%) rotate(8.7deg); }
    95.83% { left: 37.84%; top: 10.40%; transform: translate(-50%, -50%) rotate(9.7deg); }
    100.00% { left: 50.00%; top: 9.00%; transform: translate(-50%, -50%) rotate(10.0deg); }
  }
  @keyframes bob { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-6px); } }
</style>
</head>
<body>
  <div class="stage">
    <div class="title" id="title"></div>
    <div class="drone-path"><div class="drone"><img src="__DRONE__" alt="" /></div></div>
  </div>
  <div class="status"><div>Someone is flying right now.</div><div class="sub">One pilot at a time. This page retries automatically.</div></div>
<script>
const title = document.getElementById("title");
[..."AIRSPACE BUSY"].forEach((ch, i) => {
  const s = document.createElement("span");
  s.textContent = ch === " " ? " " : ch;
  s.style.animationDelay = (i * 0.12) + "s";
  title.appendChild(s);
});
setTimeout(() => window.location.reload(), 15000);
</script>
</body>
</html>"""

WAKING_PAGE = """<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Drone Challenger</title>
<link rel="preconnect" href="https://fonts.googleapis.com" />
<link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&display=swap" rel="stylesheet" />
<style>
  html, body { height: 100%; margin: 0; }
  body {
    display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 36px;
    background: #000; color: #e8e8e8; overflow: hidden;
    font-family: "Press Start 2P", ui-monospace, Menlo, monospace;
  }
  .stage { position: relative; width: min(1040px, 96vw); height: 340px; }
  .title {
    position: absolute; left: 0; right: 0; top: 50%; transform: translateY(-50%);
    display: flex; justify-content: center; gap: 0.08em;
    font-size: clamp(28px, 7vw, 64px);
  }
  .title span {
    display: inline-block;
    background: linear-gradient(#fff6b0 0%, #ffd23a 35%, #f0a400 65%, #b86b00 100%);
    -webkit-background-clip: text; background-clip: text; color: transparent;
    animation: wave 1.8s ease-in-out infinite;
  }
  .drone-path { position: absolute; inset: 0; pointer-events: none; }
  .drone {
    position: absolute; width: clamp(72px, 13vw, 124px);
    filter: drop-shadow(0 0 8px rgba(255, 210, 58, 0.35));
    animation: loiter 11s linear infinite;
  }
  .drone img { display: block; width: 100%; animation: bob 1.3s ease-in-out infinite; }
  .status { font-size: clamp(10px, 2.2vw, 14px); color: #c9c9c9; letter-spacing: 0.04em; text-align: center; line-height: 1.8; }
  .sub { color: #7d7d7d; }
  @keyframes wave { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-0.18em); } }
  @keyframes loiter {
    0.00% { left: 50.00%; top: 9.00%; transform: translate(-50%, -50%) rotate(10.0deg); }
    4.17% { left: 62.16%; top: 10.40%; transform: translate(-50%, -50%) rotate(9.7deg); }
    8.33% { left: 73.50%; top: 14.49%; transform: translate(-50%, -50%) rotate(8.7deg); }
    12.50% { left: 83.23%; top: 21.01%; transform: translate(-50%, -50%) rotate(7.1deg); }
    16.67% { left: 90.70%; top: 29.50%; transform: translate(-50%, -50%) rotate(5.0deg); }
    20.83% { left: 95.40%; top: 39.39%; transform: translate(-50%, -50%) rotate(2.6deg); }
    25.00% { left: 97.00%; top: 50.00%; transform: translate(-50%, -50%) rotate(0.0deg); }
    29.17% { left: 95.40%; top: 60.61%; transform: translate(-50%, -50%) rotate(-2.6deg); }
    33.33% { left: 90.70%; top: 70.50%; transform: translate(-50%, -50%) rotate(-5.0deg); }
    37.50% { left: 83.23%; top: 78.99%; transform: translate(-50%, -50%) rotate(-7.1deg); }
    41.67% { left: 73.50%; top: 85.51%; transform: translate(-50%, -50%) rotate(-8.7deg); }
    45.83% { left: 62.16%; top: 89.60%; transform: translate(-50%, -50%) rotate(-9.7deg); }
    50.00% { left: 50.00%; top: 91.00%; transform: translate(-50%, -50%) rotate(-10.0deg); }
    54.17% { left: 37.84%; top: 89.60%; transform: translate(-50%, -50%) rotate(-9.7deg); }
    58.33% { left: 26.50%; top: 85.51%; transform: translate(-50%, -50%) rotate(-8.7deg); }
    62.50% { left: 16.77%; top: 78.99%; transform: translate(-50%, -50%) rotate(-7.1deg); }
    66.67% { left: 9.30%; top: 70.50%; transform: translate(-50%, -50%) rotate(-5.0deg); }
    70.83% { left: 4.60%; top: 60.61%; transform: translate(-50%, -50%) rotate(-2.6deg); }
    75.00% { left: 3.00%; top: 50.00%; transform: translate(-50%, -50%) rotate(-0.0deg); }
    79.17% { left: 4.60%; top: 39.39%; transform: translate(-50%, -50%) rotate(2.6deg); }
    83.33% { left: 9.30%; top: 29.50%; transform: translate(-50%, -50%) rotate(5.0deg); }
    87.50% { left: 16.77%; top: 21.01%; transform: translate(-50%, -50%) rotate(7.1deg); }
    91.67% { left: 26.50%; top: 14.49%; transform: translate(-50%, -50%) rotate(8.7deg); }
    95.83% { left: 37.84%; top: 10.40%; transform: translate(-50%, -50%) rotate(9.7deg); }
    100.00% { left: 50.00%; top: 9.00%; transform: translate(-50%, -50%) rotate(10.0deg); }
  }
  @keyframes bob { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-6px); } }
</style>
</head>
<body>
  <div class="stage">
    <div class="title" id="title"></div>
    <div class="drone-path"><div class="drone"><img src="__DRONE__" alt="" /></div></div>
  </div>
  <div class="status"><div id="status">Waking the server...</div><div class="sub" id="sub">This takes about 2 minutes</div></div>
<script>
const title = document.getElementById("title");
[..."STARTING..."].forEach((ch, i) => {
  const s = document.createElement("span");
  s.textContent = ch;
  s.style.animationDelay = (i * 0.12) + "s";
  title.appendChild(s);
});
const status = document.getElementById("status");
const sub = document.getElementById("sub");

async function poll() {
  try {
    const res = await fetch("/_wake/status", { cache: "no-store" });
    const data = await res.json();
    if (data.stage === "ready") {
      window.location.reload();
      return;
    }
    if (data.stage === "error") {
      status.textContent = "Something went wrong";
      sub.textContent = "Retrying...";
    } else if (data.ec2_state === "stopping") {
      status.textContent = "Closing the previous session...";
      sub.textContent = "Then the server starts again";
    } else if (data.stage === "warming") {
      status.textContent = "Launching the game...";
      sub.textContent = "Almost there";
    } else {
      status.textContent = "Waking the server...";
      sub.textContent = "This takes about 2 minutes";
    }
  } catch (e) {
    status.textContent = "Reconnecting...";
  }
  setTimeout(poll, 2000);
}
poll();
</script>
</body>
</html>"""


@app.get("/_wake/status")
async def wake_status(request: Request):
    _state["last_activity"] = time.time()
    info = client_info(request.headers, request.client.host if request.client else None,
                       request.url.path, request.method)
    status = await current_status(info)
    track(info, status["ec2_state"])
    status["idle_seconds"] = round(time.time() - _state["last_activity"])
    status["idle_stop_minutes"] = IDLE_STOP_MINUTES
    return JSONResponse(status)


async def _proxy_http(request: Request, path: str) -> Response:
    ip = _state["cached_ip"]
    url = f"http://{ip}:{EC2_APP_PORT}/{path}"
    body = await request.body()
    headers = {k: v for k, v in request.headers.items() if k.lower() not in ("host", "content-length")}

    client = _state["client"]
    req = client.build_request(
        request.method, url, headers=headers, params=request.query_params, content=body,
    )
    upstream = await client.send(req, stream=True)

    async def body_stream():
        async for chunk in upstream.aiter_raw():
            yield chunk
        await upstream.aclose()

    return StreamingResponse(
        body_stream(), status_code=upstream.status_code,
        headers={k: v for k, v in upstream.headers.items() if k.lower() != "transfer-encoding"},
    )


@app.api_route("/{path:path}", methods=["GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS", "HEAD"])
async def proxy_http(request: Request, path: str):
    _state["last_activity"] = time.time()
    info = client_info(request.headers, request.client.host if request.client else None, path, request.method)
    status = await proxy_status(info)
    track(info, status["ec2_state"])

    if ("/" + path).startswith(PRIVATE_PATH_PREFIXES):
        return JSONResponse({"detail": "Not found"}, status_code=404)

    accepts_html = "text/html" in request.headers.get("accept", "")
    if status["stage"] != "ready":
        if accepts_html and request.method == "GET":
            return HTMLResponse(WAKING_PAGE.replace("__DRONE__", DRONE_DATA_URI))
        return JSONResponse(status, status_code=503, headers={"Retry-After": "3"})

    if accepts_html and request.method == "GET" and path in ("", "player.html"):
        if await connected_players(status["ip"]) > 0:
            return HTMLResponse(BUSY_PAGE.replace("__DRONE__", DRONE_DATA_URI), status_code=503, headers={"Retry-After": "15"})

    return await _proxy_http(request, path)


@app.websocket("/{path:path}")
async def proxy_ws(websocket: WebSocket, path: str):
    _state["last_activity"] = time.time()
    info = client_info(websocket.headers, websocket.client.host if websocket.client else None, path, "WS")
    status = await proxy_status(info)
    track(info, status["ec2_state"], websocket=True)
    if ("/" + path).startswith(PRIVATE_PATH_PREFIXES):
        await websocket.close(code=1008, reason="Not allowed")
        return
    if status["stage"] != "ready":
        await websocket.close(code=1013, reason="Instance not ready yet")
        return

    await websocket.accept()
    upstream_url = f"ws://{status['ip']}:{EC2_APP_PORT}/{path}"

    async with ws_connect(upstream_url) as upstream:
        async def client_to_upstream():
            try:
                while True:
                    msg = await websocket.receive_text()
                    await upstream.send(msg)
            except (WebSocketDisconnect, ConnectionClosed):
                pass

        async def upstream_to_client():
            try:
                async for msg in upstream:
                    await websocket.send_text(msg)
            except (WebSocketDisconnect, ConnectionClosed):
                pass

        await asyncio.gather(client_to_upstream(), upstream_to_client())
