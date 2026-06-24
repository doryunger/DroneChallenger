'use strict';
(function () {
    const _orig = { log: console.log, warn: console.warn, error: console.error };
    function post(msg) {
        try {
            const x = new XMLHttpRequest();
            x.open('POST', 'http://127.0.0.1:8081/log', true);
            x.setRequestHeader('Content-Type', 'text/plain; charset=utf-8');
            x.send(msg);
        } catch (_) {}
    }
    ['log', 'warn', 'error'].forEach(function (k) {
        console[k] = function () {
            const msg = '[' + k.toUpperCase() + '] ' + Array.from(arguments).join(' ');
            _orig[k].apply(console, arguments);
            post(msg);
        };
    });
})();
window.onerror = (msg, src, line) => console.error('JS error: ' + msg + ' @ ' + src + ':' + line);
console.log('minimap.js: start');

/* ── colours — exact match to studio ──────────────────────────────────────── */
const COL = {
    bg:       'rgba(5,7,12,0.92)',
    ring:     'rgba(55,62,90,0.92)',
    roadBack: 'rgba(0,0,0,0.88)',
    road:     'rgba(165,170,178,0.88)',
    fovFill:  'rgba(100,200,255,0.13)',
    fovEdge:  'rgba(120,220,255,0.86)',
    tgt:      'rgba(215,30,30,1)',
    tgtLit:   'rgba(255,70,70,1)',
    compass:  'rgba(140,148,170,0.75)',
};

const ROAD_BACK_MAX = 9,   ROAD_BACK_MIN = 2.5;
const ROAD_MAX      = 5,   ROAD_MIN      = 1.2;

const ROADS = (function () {
    if (typeof GRAPH_NODES === 'undefined') return { nodes: {}, adj: {}, refElev: 0 };
    const nodes = {}, adj = {};
    let minZ = Infinity, maxZ = -Infinity;
    for (const [id, pos] of Object.entries(GRAPH_NODES)) {
        const nid = +id;
        nodes[nid] = { x: pos[0], y: pos[1], z: pos[2] || 0 };
        if (pos[2] < minZ) minZ = pos[2];
        if (pos[2] > maxZ) maxZ = pos[2];
        adj[nid] = [];
    }
    for (const [a, b] of GRAPH_EDGES) {
        if (adj[a] && adj[b]) { adj[a].push(b); adj[b].push(a); }
    }
    return { nodes, adj, refElev: (minZ + maxZ) / 2 };
})();

/* ── helpers ──────────────────────────────────────────────────────────────── */
const lerp  = (a, b, t) => a + (b - a) * t;
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
const deg   = d => d * Math.PI / 180;

function clipSeg(an, ae, bn, be, clipR) {
    const R2 = clipR * clipR;
    const aIn = an*an + ae*ae <= R2, bIn = bn*bn + be*be <= R2;
    if (aIn && bIn) return [an, ae, bn, be];
    const dn = bn-an, de = be-ae;
    const a = dn*dn + de*de;
    if (a < 1e-8) return null;
    const b = 2*(an*dn + ae*de);
    const c = an*an + ae*ae - R2;
    const disc = b*b - 4*a*c;
    if (disc < 0) return null;
    const sq = Math.sqrt(disc);
    const t0 = (-b-sq)/(2*a), t1 = (-b+sq)/(2*a);
    const pn = t => an+t*dn, pe = t => ae+t*de;
    if (aIn) { const t=clamp(t1,0,1); return [an,ae,pn(t),pe(t)]; }
    if (bIn) { const t=clamp(t0,0,1); return [pn(t),pe(t),bn,be]; }
    if (t1<0||t0>1||t0>=t1) return null;
    return [pn(clamp(t0,0,1)),pe(clamp(t0,0,1)),pn(clamp(t1,0,1)),pe(clamp(t1,0,1))];
}

/* ── canvas ───────────────────────────────────────────────────────────────── */
const cv  = document.getElementById('cv');
const ctx = cv.getContext('2d');

function resize() { cv.width = window.innerWidth; cv.height = window.innerHeight; }
window.addEventListener('resize', resize);
resize();

/* ── map constants — must match DroneHUDServer/widget properties ─────────── */
const MAP_RAD_CM   = 50000;   // default map radius (cm)
const DET_RANG_CM  = 50000;   // FOV frustum length (cm)
const FOV_DEG      = 90;      // horizontal FOV (degrees)
const REF_ALT_M    = 50;      // reference altitude for scale (m above terrain)
const MIN_RAD_CM   = 20000;   // min effective radius (cm)
const MAX_RAD_CM   = 200000;  // max effective radius (cm)

/* ── live state ───────────────────────────────────────────────────────────── */
// droneX/Y/Z and targetX/Y/Z are ABSOLUTE UE5 world coordinates (cm)
// UE5 X = North axis, UE5 Y = East axis (for this Cesium Munich setup)
let S = { droneX:0, droneY:0, droneZ:0, yaw:0, altM:50,
          targetX:0, targetY:0, targetZ:0, inFov:false,
          fovDeg: FOV_DEG, detRangeCm: DET_RANG_CM };
let animT = 0;
let wsQueue = [];
let smoothAltM = -1;
let prevFrameMs = 0;
let hasData = false;

/* ── main draw loop ───────────────────────────────────────────────────────── */
function frame(now) {
    requestAnimationFrame(frame);
    if (wsQueue.length > 0) {
        const d = JSON.parse(wsQueue[wsQueue.length - 1]);
        wsQueue.length = 0;
        S.droneX  = d.dx;  S.droneY  = d.dy;  S.droneZ  = d.dz;
        S.yaw     = d.yaw;  S.altM = d.altM;
        S.targetX = d.tx;  S.targetY = d.ty;  S.targetZ = d.tz;
        S.inFov   = d.inFov;
        if (d.fovDeg     !== undefined) S.fovDeg     = d.fovDeg;
        if (d.detRangeCm !== undefined) S.detRangeCm = d.detRangeCm;
        if (!hasData) { hasData = true; cv.style.visibility = 'visible'; }
    }
    const dt = prevFrameMs > 0 ? Math.min((now - prevFrameMs) / 1000, 0.1) : 0;
    prevFrameMs = now;
    const rawAlt = Math.max(S.altM, 10);
    if (smoothAltM < 0) smoothAltM = rawAlt;
    else smoothAltM += (rawAlt - smoothAltM) * Math.min(dt * 4.0, 1.0);
    const wW = window.innerWidth, wH = window.innerHeight;
    if (cv.width !== wW || cv.height !== wH) { cv.width = wW; cv.height = wH; }
    animT = now * 0.001;
    const W = cv.width, H = cv.height;
    ctx.clearRect(0, 0, W, H);
    if (hasData && Math.min(W, H) > 4) drawMinimap(ctx, W, H);
}
console.log('minimap.js: starting RAF');
requestAnimationFrame(frame);

function drawMinimap(ctx, W, H) {
    const cx = W/2, cy = H/2;
    const R  = Math.min(cx, cy) * 0.78 - 2;
    if (R <= 0) return;

    const effRadCm = clamp(smoothAltM / REF_ALT_M * MAP_RAD_CM, MIN_RAD_CM, MAX_RAD_CM);
    const scale    = R / effRadCm;
    const clipR    = effRadCm * 0.80;
    const clipPx   = clipR * scale;

    const altT     = clamp((smoothAltM - 20) / (150 - 20), 0, 1);
    const roadBackW = lerp(ROAD_BACK_MAX, ROAD_BACK_MIN, altT);
    const roadW     = lerp(ROAD_MAX, ROAD_MIN, altT);

    function ts(relN, relE) {
        return [cx + relE * scale, cy - relN * scale];
    }

    const yr = deg(S.yaw);
    const cosY = Math.cos(yr), sinY = Math.sin(yr);
    function ws(worldX, worldY) {
        // UE5 X = North axis, UE5 Y = East axis
        const relN = worldX - S.droneX;
        const relE = worldY - S.droneY;
        const rotN =  relN*cosY + relE*sinY;
        const rotE = -relN*sinY + relE*cosY;
        return ts(rotN, rotE);
    }

    const frustLen = Math.min(S.detRangeCm, clipR * 0.95);
    const halfFov  = deg(S.fovDeg / 2);
    const ARC_N    = 22;

    function fovPt(a) { return ts(Math.cos(a)*frustLen, Math.sin(a)*frustLen); }

    const [tSx, tSy] = ws(S.targetX, S.targetY);
    const tRelN = S.targetX - S.droneX, tRelE = S.targetY - S.droneY;
    const tDist = Math.sqrt(tRelN*tRelN + tRelE*tRelE);

    /* ── background + clip ── */
    ctx.save();
    ctx.beginPath(); ctx.arc(cx, cy, R, 0, 2*Math.PI);
    ctx.fillStyle = COL.bg; ctx.fill(); ctx.clip();

    /* ── roads ── */
    const bpR2 = (clipR*2)**2;
    const segs = [];
    for (const [id, nbrs] of Object.entries(ROADS.adj)) {
        const na = ROADS.nodes[+id];
        for (const nb_id of nbrs) {
            if (nb_id <= +id) continue;
            const nb = ROADS.nodes[nb_id];
            // drone-relative position for culling
            const an = na.x - S.droneX, ae = na.y - S.droneY;
            const bn = nb.x - S.droneX, be = nb.y - S.droneY;
            if (an*an+ae*ae > bpR2 && bn*bn+be*be > bpR2) continue;
            const s = clipSeg(an, ae, bn, be, clipR);
            if (!s) continue;
            // yaw-rotate clipped endpoints then project
            const rotAN =  s[0]*cosY + s[1]*sinY, rotAE = -s[0]*sinY + s[1]*cosY;
            const rotBN =  s[2]*cosY + s[3]*sinY, rotBE = -s[2]*sinY + s[3]*cosY;
            segs.push([ts(rotAN, rotAE), ts(rotBN, rotBE)]);
        }
    }

    ctx.lineCap = 'round'; ctx.lineJoin = 'round';
    ctx.shadowColor = 'rgba(160,165,185,0.28)';
    ctx.shadowBlur  = roadBackW * 2.5;
    ctx.strokeStyle = COL.roadBack; ctx.lineWidth = roadBackW;
    ctx.beginPath();
    for (const [[ax,ay],[bx,by]] of segs) { ctx.moveTo(ax,ay); ctx.lineTo(bx,by); }
    ctx.stroke();
    ctx.shadowBlur  = 0;
    ctx.strokeStyle = COL.road; ctx.lineWidth = roadW;
    ctx.beginPath();
    for (const [[ax,ay],[bx,by]] of segs) { ctx.moveTo(ax,ay); ctx.lineTo(bx,by); }
    ctx.stroke();

    /* ── FOV cone ── */
    ctx.beginPath(); ctx.moveTo(cx, cy);
    for (let i = 0; i <= ARC_N; i++) ctx.lineTo(...fovPt(lerp(-halfFov, halfFov, i/ARC_N)));
    ctx.closePath(); ctx.fillStyle = COL.fovFill; ctx.fill();
    ctx.beginPath(); ctx.moveTo(...fovPt(-halfFov));
    for (let i = 1; i <= ARC_N; i++) ctx.lineTo(...fovPt(lerp(-halfFov, halfFov, i/ARC_N)));
    ctx.strokeStyle = COL.fovEdge; ctx.lineWidth = 1.5; ctx.stroke();

    /* ── target dot with pulse ── */
    if (tDist <= clipR) {
        const dotR = R * 0.042;
        const tCol = S.inFov ? COL.tgtLit : COL.tgt;
        const PERIOD = 1.4;
        for (const off of [0, PERIOD*0.5]) {
            const t  = ((animT+off) % PERIOD) / PERIOD;
            const pr = dotR * (1.5 + t*3.2);
            ctx.beginPath(); ctx.arc(tSx, tSy, pr, 0, 2*Math.PI);
            ctx.strokeStyle = tCol; ctx.lineWidth = 2.5;
            ctx.globalAlpha = (1-t)*0.65; ctx.stroke(); ctx.globalAlpha = 1;
        }
        ctx.beginPath(); ctx.arc(tSx, tSy, dotR*2, 0, 2*Math.PI);
        ctx.strokeStyle = tCol; ctx.lineWidth = 2.5; ctx.globalAlpha = 0.75; ctx.stroke(); ctx.globalAlpha = 1;
        ctx.beginPath(); ctx.arc(tSx, tSy, dotR, 0, 2*Math.PI);
        ctx.fillStyle = tCol; ctx.fill();
    }

    ctx.restore();  // end circular clip

    /* ── compass layer background (annular) ── */
    const annG = ctx.createRadialGradient(cx, cy, clipPx, cx, cy, R);
    annG.addColorStop(0,   'rgba(5,7,14,0.0)');
    annG.addColorStop(0.15,'rgba(5,7,14,0.55)');
    annG.addColorStop(1,   'rgba(5,7,14,0.72)');
    ctx.beginPath();
    ctx.arc(cx, cy, R,      0, 2*Math.PI, false);
    ctx.arc(cx, cy, clipPx, 0, 2*Math.PI, true);
    ctx.fillStyle = annG; ctx.fill();

    /* ── inner separator ring ── */
    ctx.beginPath(); ctx.arc(cx, cy, clipPx, 0, 2*Math.PI);
    ctx.strokeStyle = 'rgba(70,78,110,0.90)';
    ctx.lineWidth   = Math.max(1, R * 0.009);
    ctx.stroke();

    /* ── drone arrow (always above clip region) ── */
    const aFwd  =  R*0.135/scale, aBack = -R*0.047/scale, aHalf = R*0.068/scale;
    const [tipX,tipY] = ts( aFwd,  0);
    const [blX, blY ] = ts( aBack, -aHalf);
    const [brX, brY ] = ts( aBack,  aHalf);

    const glowG = ctx.createRadialGradient(cx,cy,0, cx,cy, R*0.22);
    glowG.addColorStop(0,   'rgba(60,140,255,0.30)');
    glowG.addColorStop(0.5, 'rgba(60,140,255,0.12)');
    glowG.addColorStop(1,   'rgba(60,140,255,0)');
    ctx.beginPath(); ctx.arc(cx, cy, R*0.22, 0, 2*Math.PI);
    ctx.fillStyle = glowG; ctx.fill();

    ctx.beginPath(); ctx.moveTo(tipX+1,tipY+2.5); ctx.lineTo(blX+1,blY+2.5); ctx.lineTo(brX+1,brY+2.5);
    ctx.closePath(); ctx.fillStyle = 'rgba(0,0,0,0.5)'; ctx.fill();

    const arrowG = ctx.createLinearGradient(cx, tipY, cx, blY);
    arrowG.addColorStop(0,    'rgba(200,230,255,0.97)');
    arrowG.addColorStop(0.38, '#55a5ff');
    arrowG.addColorStop(1,    'rgba(22,58,140,0.92)');
    ctx.beginPath(); ctx.moveTo(tipX,tipY); ctx.lineTo(blX,blY); ctx.lineTo(brX,brY);
    ctx.closePath(); ctx.fillStyle = arrowG; ctx.fill();
    ctx.beginPath(); ctx.moveTo(blX,blY); ctx.lineTo(tipX,tipY); ctx.lineTo(brX,brY);
    ctx.strokeStyle = 'rgba(180,220,255,0.55)'; ctx.lineWidth = 1; ctx.stroke();

    /* ── ring border ── */
    ctx.beginPath(); ctx.arc(cx, cy, R, 0, 2*Math.PI);
    ctx.strokeStyle = COL.ring; ctx.lineWidth = Math.max(1.5, R*0.017); ctx.stroke();

    /* ── compass: ticks + labels inside ring, heading triangle at top ── */
    const touter = R*0.98, tLg = R*0.07, tSm = R*0.04;
    const fSz    = Math.max(9, R*0.088);

    for (const [lbl, bear] of [['N',0],['E',90],['S',180],['W',270]]) {
        const a = deg(bear-90) - yr, dx = Math.cos(a), dy = Math.sin(a);
        ctx.beginPath();
        ctx.moveTo(cx + dx*touter,       cy + dy*touter);
        ctx.lineTo(cx + dx*(touter-tLg), cy + dy*(touter-tLg));
        ctx.strokeStyle = COL.compass; ctx.lineWidth = 1.5; ctx.stroke();

        const lblR = touter - tLg - fSz*0.65;
        ctx.fillStyle    = 'rgba(255,255,255,0.90)';
        ctx.font         = `bold ${fSz}px 'Segoe UI', sans-serif`;
        ctx.textAlign    = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(lbl, Math.round(cx + dx*lblR), Math.round(cy + dy*lblR));
    }

    for (let b = 0; b < 360; b += 10) {
        if (b % 90 === 0) continue;
        const maj = b % 30 === 0;
        const a = deg(b-90) - yr, dx = Math.cos(a), dy = Math.sin(a);
        const tL = maj ? tLg*0.6 : tSm;
        ctx.beginPath();
        ctx.moveTo(cx + dx*touter,      cy + dy*touter);
        ctx.lineTo(cx + dx*(touter-tL), cy + dy*(touter-tL));
        ctx.strokeStyle = maj ? 'rgba(200,208,230,0.90)' : 'rgba(170,178,200,0.65)';
        ctx.lineWidth = 1; ctx.stroke();
    }

    // Heading indicator — fixed white triangle at top of ring
    const hTip = cy - touter + 1, hBase = cy - touter + tLg*0.95, hHalf = tSm*0.65;
    ctx.beginPath();
    ctx.moveTo(cx, hTip); ctx.lineTo(cx - hHalf, hBase); ctx.lineTo(cx + hHalf, hBase);
    ctx.closePath(); ctx.fillStyle = 'rgba(255,255,255,0.92)'; ctx.fill();
}

function connectWS() {
    const ws = new WebSocket('ws://127.0.0.1:8081/ws');
    ws.onmessage = (e) => {
        wsQueue.push(e.data);
        if (wsQueue.length > 5) wsQueue.splice(0, wsQueue.length - 5);
    };
    ws.onclose = () => { wsQueue.length = 0; setTimeout(connectWS, 1000); };
    ws.onerror = () => ws.close();
}
connectWS();
