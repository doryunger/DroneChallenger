# wake-service

## Why this exists

The streaming host is a `g4dn.xlarge` that costs money while it runs. This always-on service on the cheap Lightsail box (the same box and pattern as `geo-dataset-generator/wake-service`, which serves `refinery.stamsite.cc`) starts the EC2 instance when a player opens `dronechallenger.stamsite.cc`, shows a waking page until the game is streamable, then proxies the player page and the signalling websocket to the instance.

## Shape

```
browser --HTTPS--> Cloudflare --> Lightsail nginx --> wake-service (127.0.0.1:8091) --HTTP/WS--> EC2:80 (signalling server)
browser <==== WebRTC media (UDP, direct or via TURN on EC2) ====> EC2 game
```

Only the player page and the signalling websocket go through this service. WebRTC media goes straight between the browser and the instance (its public IP from ICE candidates, or the TURN relay on port 19303), so the EC2 security group must allow the media and TURN ports from anywhere, while port 80 only needs to accept the Lightsail box.

## Differences from the geo wake-service

- **Readiness** is "the signalling server has a connected streamer": `GET /api/streamers` on the instance (enabled with `--rest_api` in `start_streaming.ps1`) returns a non-empty list. Unlike the geo service, readiness is re-checked on every status computation rather than latched, because the game process can restart under the watchdog while the instance stays up.
- **One player at a time.** The signalling server runs with `--max_players 1`. In addition, when the instance is ready and `GET /api/players` reports a connected player, a request for the player page gets a "someone is flying right now" page that retries every 15 s, instead of a player page that would fail to connect.
- **The signalling REST API is not exposed.** Requests under `/api` and `/api-definition` are answered with 404 (HTTP) or closed (websocket); only this service talks to them, directly on the instance.
- **Idle-stop is off by default** (`IDLE_STOP_MINUTES=0`). Session limits live on the instance (`deploy/streaming/session_guard.ps1`: 10 minutes without a player, 20 minutes after boot). This service only sees HTTP requests and websocket connects, not the traffic inside a long-lived websocket, so an HTTP-based idle timer would stop the instance in the middle of a game. When the instance stops itself, the next wake closes the open session with reason `instance_stopped`.
- **No Elastic IP**, as in the geo service: the instance's current public IP is read from `DescribeInstances`, and the instance reads its own IP from instance metadata for TURN.

Everything else (debounced `StartInstances`, the 2-second status polling page, the cached `ready` status, pooled upstream client, session log and optional S3 upload, `CF-Connecting-IP` handling, the IPv6 `listen` line in the nginx site) is carried over unchanged, including the fixes documented in the geo service's notes.
