'use strict';

const http   = require('http');
const crypto = require('crypto');
const fs     = require('fs');
const path   = require('path');

const BT_DIR      = path.join(__dirname, 'arborist', 'viewer');
const MINIMAP_DIR = path.join(__dirname, 'minimap');

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js':   'application/javascript',
  '.css':  'text/css',
  '.json': 'application/json',
};

function serveFile(res, filePath) {
  const ext = path.extname(filePath).toLowerCase();
  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('404 – ' + filePath);
      return;
    }
    res.writeHead(200, {
      'Content-Type': MIME[ext] || 'application/octet-stream',
      'Access-Control-Allow-Origin': '*',
    });
    res.end(data);
  });
}

const MOCK_TREE = {
  name: 'root',
  type: 'Selector',
  children: [
    { name: 'stop', type: 'Action' },
    {
      name: 'capture_sequence',
      type: 'Sequence',
      children: [
        { name: 'manage_patrol_path',      type: 'Action' },
        { name: 'stop',                    type: 'Action' },
        { name: 'pulse_beacon_fast',       type: 'Action' },
        { name: 'increment_capture_timer', type: 'Action' },
      ],
    },
    {
      name: 'spotted_sequence',
      type: 'Sequence',
      children: [
        { name: 'manage_patrol_path', type: 'Action' },
        { name: 'deactivate_beacon',  type: 'Action' },
        { name: 'set_speed_fast',     type: 'Action' },
        { name: 'advance_along_path', type: 'Action' },
      ],
    },
    {
      name: 'patrol_sequence',
      type: 'Sequence',
      children: [
        { name: 'manage_patrol_path', type: 'Action' },
        { name: 'set_speed_normal',   type: 'Action' },
        { name: 'pulse_beacon_slow',  type: 'Action' },
        { name: 'advance_along_path', type: 'Action' },
      ],
    },
  ],
};

const ADVANCE_TICKS = () => 20 + Math.floor(Math.random() * 12);

let patrolState   = 0;
let patrolElapsed = 0;
let advanceTicks  = ADVANCE_TICKS();

const PATROL_STATES = [
  {
    ticks: 2,
    path: () => [
      { name: 'root',               status: 'RUNNING' },
      { name: 'patrol_sequence',    status: 'RUNNING' },
      { name: 'manage_patrol_path', status: 'RUNNING' },
    ],
  },
  {
    ticks: 1,
    path: () => [
      { name: 'root',               status: 'RUNNING' },
      { name: 'patrol_sequence',    status: 'RUNNING' },
      { name: 'manage_patrol_path', status: 'SUCCESS' },
      { name: 'set_speed_normal',   status: 'RUNNING' },
    ],
  },
  {
    ticks: 1,
    path: () => [
      { name: 'root',               status: 'RUNNING' },
      { name: 'patrol_sequence',    status: 'RUNNING' },
      { name: 'manage_patrol_path', status: 'SUCCESS' },
      { name: 'set_speed_normal',   status: 'SUCCESS' },
      { name: 'pulse_beacon_slow',  status: 'RUNNING' },
    ],
  },
  {
    ticks: null,
    path: () => [
      { name: 'root',               status: 'RUNNING' },
      { name: 'patrol_sequence',    status: 'RUNNING' },
      { name: 'manage_patrol_path', status: 'SUCCESS' },
      { name: 'set_speed_normal',   status: 'SUCCESS' },
      { name: 'pulse_beacon_slow',  status: 'SUCCESS' },
      { name: 'advance_along_path', status: 'RUNNING' },
    ],
  },
  {
    ticks: 3,
    path: () => [
      { name: 'root',               status: 'SUCCESS' },
      { name: 'patrol_sequence',    status: 'SUCCESS' },
      { name: 'manage_patrol_path', status: 'SUCCESS' },
      { name: 'set_speed_normal',   status: 'SUCCESS' },
      { name: 'pulse_beacon_slow',  status: 'SUCCESS' },
      { name: 'advance_along_path', status: 'SUCCESS' },
    ],
  },
];

function nextPatrolPath() {
  const state      = PATROL_STATES[patrolState];
  const activePath = state.path();
  patrolElapsed++;
  const limit = patrolState === 3 ? advanceTicks : state.ticks;
  if (patrolElapsed >= limit) {
    patrolElapsed = 0;
    patrolState   = (patrolState + 1) % PATROL_STATES.length;
    if (patrolState === 0) advanceTicks = ADVANCE_TICKS();
  }
  return activePath;
}

let spottedCountdown  = 0;
let ticksSinceSpotted = 80;

function maybeInterrupt() {
  if (spottedCountdown > 0) return 'spotted';
  ticksSinceSpotted++;
  if (ticksSinceSpotted >= 80 && Math.random() < 0.05) {
    spottedCountdown  = 12 + Math.floor(Math.random() * 8);
    ticksSinceSpotted = 0;
  }
  return spottedCountdown > 0 ? 'spotted' : null;
}

const SPOTTED_STEPS = [
  [
    { name: 'root',               status: 'RUNNING' },
    { name: 'spotted_sequence',   status: 'RUNNING' },
    { name: 'manage_patrol_path', status: 'RUNNING' },
  ],
  [
    { name: 'root',               status: 'RUNNING' },
    { name: 'spotted_sequence',   status: 'RUNNING' },
    { name: 'manage_patrol_path', status: 'SUCCESS' },
    { name: 'deactivate_beacon',  status: 'RUNNING' },
  ],
  [
    { name: 'root',               status: 'RUNNING' },
    { name: 'spotted_sequence',   status: 'RUNNING' },
    { name: 'manage_patrol_path', status: 'SUCCESS' },
    { name: 'deactivate_beacon',  status: 'SUCCESS' },
    { name: 'set_speed_fast',     status: 'SUCCESS' },
    { name: 'advance_along_path', status: 'RUNNING' },
  ],
];

function spottedPath(elapsed) {
  if (elapsed < 1) return SPOTTED_STEPS[0];
  if (elapsed < 2) return SPOTTED_STEPS[1];
  return SPOTTED_STEPS[2];
}

let tickCounter = 0;
const history   = [];

function appendTick() {
  const interrupt = maybeInterrupt();
  let behavior, activePath;

  if (interrupt === 'spotted') {
    const elapsed    = 12 - spottedCountdown;
    spottedCountdown = Math.max(0, spottedCountdown - 1);
    behavior         = 'spotted';
    activePath       = spottedPath(elapsed);
  } else {
    behavior   = 'patrol';
    activePath = nextPatrolPath();
  }

  const rootNode   = activePath.find(n => n.name === 'root');
  const treeStatus = rootNode ? rootNode.status : 'RUNNING';

  history.push({ tick: tickCounter, behavior, status: treeStatus, activePath });
  if (history.length > 32) history.shift();
  tickCounter++;
}

setInterval(appendTick, 100);

const btServer = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  res.setHeader('Access-Control-Allow-Origin', '*');

  if (url === '/tree') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(MOCK_TREE));
    return;
  }
  if (url === '/history') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(history));
    return;
  }
  if (url === '/log') {
    let body = '';
    req.on('data', d => { body += d; });
    req.on('end', () => {
      if (body.trim()) process.stdout.write('[bt] ' + body.trim() + '\n');
      res.writeHead(204);
      res.end();
    });
    return;
  }

  serveFile(res, path.join(BT_DIR, url === '/' ? 'viewer.html' : url));
});

btServer.listen(8080, '127.0.0.1', () => {
  console.log('BT viewer  →  http://localhost:8080/viewer.html');
});

let droneAngle = 0;

function makeDroneState() {
  droneAngle += 0.008;
  return JSON.stringify({
    dx: Math.cos(droneAngle) * 5000,
    dy: Math.sin(droneAngle) * 5000,
    dz: 5000,
    yaw: droneAngle * (180 / Math.PI),
    altM: 50 + Math.sin(droneAngle * 0.3) * 10,
    tx: 0, ty: 0, tz: 0,
    inFov: Math.sin(droneAngle) > 0.4,
  });
}

function wsHandshake(socket, key) {
  const accept = crypto
    .createHash('sha1')
    .update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
    .digest('base64');
  socket.write(
    'HTTP/1.1 101 Switching Protocols\r\n' +
    'Upgrade: websocket\r\nConnection: Upgrade\r\n' +
    'Sec-WebSocket-Accept: ' + accept + '\r\n\r\n'
  );
}

function wsFrame(text) {
  const payload = Buffer.from(text, 'utf8');
  const len     = payload.length;
  const header  = len < 126
    ? Buffer.from([0x81, len])
    : Buffer.from([0x81, 126, (len >> 8) & 0xff, len & 0xff]);
  return Buffer.concat([header, payload]);
}

const wsSockets = new Set();

const mmServer = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  res.setHeader('Access-Control-Allow-Origin', '*');

  if (url === '/state') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(makeDroneState());
    return;
  }
  if (url === '/log') {
    let body = '';
    req.on('data', d => { body += d; });
    req.on('end', () => {
      if (body.trim()) process.stdout.write('[minimap] ' + body.trim() + '\n');
      res.writeHead(204);
      res.end();
    });
    return;
  }

  serveFile(res, path.join(MINIMAP_DIR, url === '/' ? 'minimap.html' : url));
});

mmServer.on('upgrade', (req, socket) => {
  const key = req.headers['sec-websocket-key'];
  if (!key) { socket.destroy(); return; }
  wsHandshake(socket, key);
  wsSockets.add(socket);
  socket.on('close', () => wsSockets.delete(socket));
  socket.on('error', () => { wsSockets.delete(socket); socket.destroy(); });
});

mmServer.listen(8081, '127.0.0.1', () => {
  console.log('Minimap    →  http://localhost:8081/minimap.html');
  console.log('\nPress Ctrl-C to stop.\n');
});

setInterval(() => {
  if (!wsSockets.size) return;
  const frame = wsFrame(makeDroneState());
  wsSockets.forEach(s => { try { s.write(frame); } catch (_) {} });
}, 50);
