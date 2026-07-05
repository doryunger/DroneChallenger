const STATUS_COLOR = {
  SUCCESS: { background: '#14532d', border: '#4ade80', font: '#dcfce7' },
  FAILURE: { background: '#3d1a2b', border: '#f38ba8', font: '#f38ba8' },
  RUNNING: { background: '#0f3d1e', border: '#22c55e', font: '#86efac' },
};
const NODE_TYPE = {
  Selector:  { shape: 'ellipse',   bg: '#2e1f4a', border: '#cba6f7' },
  Sequence:  { shape: 'box',       bg: '#1a2d5a', border: '#89b4fa' },
  Action:    { shape: 'box',       bg: '#1a3a28', border: '#a6e3a1' },
  Condition: { shape: 'diamond',   bg: '#3a1a28', border: '#f38ba8' },
};
const FALLBACK_TYPE = { shape: 'box', bg: '#313244', border: '#7f849c' };

let network      = null;
let nodesDS      = null;
let edgesDS      = null;
let nodeDefaults = {};
let nodeTypeMap  = {};
let allEdgeIds   = [];
let nameToUids   = {};
let uidToName    = {};
let nodeCounter  = 0;
let treeLoaded   = false;
let rootUid      = null;
let edgeNodesMap = {};
let parentOf     = {};
let childrenOf   = {};
let nodeShapeMap  = {};
let runningNodes  = new Set();
let animFrame     = null;
let overlayCanvas = null;

let allHistory   = [];
let currentIndex = -1;
let currentTab   = 'active';
let isLive       = true;
let isPlaying    = false;
let playTimer    = null;

let pollIntervalMs = 3000;
let pollTimer      = null;

function restartPollTimer() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = setInterval(pollHistory, pollIntervalMs);
}

window.setPollRate = function(ms) {
  if (ms === pollIntervalMs) return;
  pollIntervalMs = ms;
  restartPollTimer();
};

function makeTooltip(name, type, status) {
  const sc  = STATUS_COLOR[status];
  const col = sc ? sc.font : '#6c7086';
  return '<div style="background:#181825;border:1px solid #313244;padding:7px 11px;' +
    'border-radius:6px;font-family:monospace;font-size:11px;line-height:1.7">' +
    '<b style="color:#cdd6f4">' + (name || '?') + '</b><br>' +
    '<span style="color:#6c7086">type: </span><span style="color:#89b4fa">' + (type || '?') + '</span><br>' +
    '<span style="color:#6c7086">status: </span><span style="color:' + col + '">' + (status || '—') + '</span>' +
    '</div>';
}

function buildGraph(node, nodes, edges, parentUid) {
  const uid = 'n' + (nodeCounter++);
  const t   = NODE_TYPE[node.type] || FALLBACK_TYPE;
  nodeDefaults[uid]  = { background: t.bg, border: t.border };
  nodeTypeMap[uid]   = node.type || 'Unknown';
  nodeShapeMap[uid]  = t.shape;
  const displayName = node.name || (parentUid ? '?' : 'BT');
  uidToName[uid]    = displayName;
  if (!nameToUids[node.name]) nameToUids[node.name] = [];
  nameToUids[node.name].push(uid);

  nodes.push({
    id:          uid,
    label:       displayName,
    shape:       t.shape,
    color:       { background: t.bg, border: t.border },
    font:        { color: '#cdd6f4', size: 14, face: "'Fira Code','SF Mono',monospace" },
    margin:      { top: 8, bottom: 8, left: 12, right: 12 },
    borderWidth: 2,
    title:       makeTooltip(node.name, node.type, null),
  });

  if (parentUid) {
    const eid = parentUid + '→' + uid;
    allEdgeIds.push(eid);
    edgeNodesMap[eid] = { from: parentUid, to: uid };
    parentOf[uid]     = parentUid;
    if (!childrenOf[parentUid]) childrenOf[parentUid] = [];
    childrenOf[parentUid].push(uid);
    edges.push({ id: eid, from: parentUid, to: uid, color: { color: '#6272a4', opacity: 1 }, width: 1.5 });
  } else {
    rootUid = uid;
  }

  (node.children || []).forEach(c => buildGraph(c, nodes, edges, uid));
}

const TEMP_DEBUG_GRAPH_DRAW_DELAY_MS = 3000;

async function loadTree() {
  try {
    const res  = await fetch('http://127.0.0.1:8081/tree');
    const tree = await res.json();

    if (!tree || !tree.type) {
      console.warn('[BT] /tree not ready yet');
      return;
    }

    if (TEMP_DEBUG_GRAPH_DRAW_DELAY_MS > 0)
      await new Promise(r => setTimeout(r, TEMP_DEBUG_GRAPH_DRAW_DELAY_MS));

    nodeCounter  = 0;
    rootUid      = null; edgeNodesMap = {}; parentOf = {}; childrenOf = {};
    nodeShapeMap = {}; runningNodes = new Set();
    if (animFrame) { cancelAnimationFrame(animFrame); animFrame = null; }
    nodeDefaults = {}; nodeTypeMap  = {}; allEdgeIds = [];
    nameToUids   = {}; uidToName   = {};

    const rawNodes = [], rawEdges = [];
    buildGraph(tree, rawNodes, rawEdges, null);
    console.log('[BT] tree: ' + rawNodes.length + ' nodes, ' + rawEdges.length + ' edges');

    nodesDS = new vis.DataSet(rawNodes);
    edgesDS = new vis.DataSet(rawEdges);

    const opts = {
      layout: {
        hierarchical: {
          direction: 'LR', sortMethod: 'directed',
          levelSeparation: 200, nodeSpacing: 80, treeSpacing: 80,
        },
      },
      physics: false,
      edges: {
        arrows: { to: { enabled: true, scaleFactor: 0.7 } },
        smooth: { type: 'cubicBezier', forceDirection: 'horizontal', roundness: 0.25 },
        color:  { color: '#6272a4', opacity: 1 },
        width:  1.5,
      },
      interaction: {
        hover: true, zoomView: true, dragView: true, dragNodes: false, tooltipDelay: 150,
      },
      nodes: {
        margin:          { top: 8, bottom: 8, left: 12, right: 12 },
        borderWidth: 2, borderWidthSelected: 3,
        font:   { color: '#cdd6f4', size: 14, face: "'Fira Code','SF Mono',monospace" },
        shadow: { enabled: true, color: 'rgba(0,0,0,0.55)', size: 8, x: 2, y: 3 },
      },
    };

    const container = document.getElementById('graph');
    console.log('[BT] container:', container.offsetWidth, 'x', container.offsetHeight);

    if (network) network.destroy();
    network = new vis.Network(container, { nodes: nodesDS, edges: edgesDS }, opts);
    treeLoaded = true;

    network.once('afterDrawing', () => {
      const pos = network.getPositions();
      const ids = Object.keys(pos);
      console.log('[BT] afterDrawing — nodes:', ids.length,
        ids.length ? 'first pos: ' + JSON.stringify(pos[ids[0]]) : '');
      network.fit({ animation: false });
      const loadingEl = document.getElementById('graph-loading');
      if (loadingEl) loadingEl.style.display = 'none';
      console.log('BT_VIEWER_READY');
    });

    container.style.position = 'relative';
    if (overlayCanvas) overlayCanvas.remove();
    overlayCanvas = document.createElement('canvas');
    overlayCanvas.style.cssText = 'position:absolute;top:0;left:0;pointer-events:none;';
    overlayCanvas.width  = container.offsetWidth;
    overlayCanvas.height = container.offsetHeight;
    container.appendChild(overlayCanvas);

    if (currentIndex >= 0) applyTickColors(allHistory[currentIndex]);
  } catch (e) {
    console.error('[BT] loadTree failed:', e);
  }
}

function applyTickColors(record) {
  if (!nodesDS || !edgesDS || !network) return;

  const statusMap = {};
  (record.activePath || []).forEach(e => {
    (nameToUids[e.name] || []).forEach(uid => { statusMap[uid] = e.status; });
  });

  const apLen      = (record.activePath || []).length;
  const matchCount = Object.keys(statusMap).length;
  if (apLen > 0 && matchCount === 0) {
    const apNames   = (record.activePath || []).map(n => '"' + n.name + '"').slice(0, 5).join(', ');
    const treeNames = Object.keys(nameToUids).slice(0, 5).join(', ');
    console.warn('[BT] 0 name matches — activePath names:', apNames, '| tree nameToUids keys:', treeNames);
  } else if (apLen > 0) {
    console.log('[BT] ' + matchCount + '/' + apLen + ' nodes matched, tab=' + currentTab);
  }

  if (rootUid && matchCount > 0 && !statusMap[rootUid]) {
    const anyRunning = Object.values(statusMap).some(s => s === 'RUNNING');
    statusMap[rootUid] = anyRunning ? 'RUNNING' : 'SUCCESS';
  }

  if (currentTab !== 'active') {
    nodesDS.update(Object.keys(nodeDefaults).map(id => {
      const sc = STATUS_COLOR[statusMap[id]];
      return {
        id, hidden: false,
        color: sc ? { background: sc.background, border: sc.border } : nodeDefaults[id],
        font:  { color: sc ? sc.font : '#cdd6f4', size: 14, face: "'Fira Code','SF Mono',monospace" },
        title: makeTooltip(uidToName[id], nodeTypeMap[id], statusMap[id] || null),
      };
    }));
    runningNodes = new Set(Object.keys(statusMap).filter(id => statusMap[id] === 'RUNNING'));
    if (runningNodes.size > 0 && !animFrame) animFrame = requestAnimationFrame(driveAnim);
    edgesDS.update(allEdgeIds.map(id => ({ id, hidden: false, color: { color: '#6272a4', opacity: 0.8 }, width: 1.5 })));
    network.once('afterDrawing', () => network.fit({ animation: false }));
    return;
  }

  const onPath = new Set();
  function addActive(uid) {
    if (!uid || onPath.has(uid)) return;
    const s = statusMap[uid];
    if (s !== 'RUNNING' && s !== 'SUCCESS') return;
    onPath.add(uid);
    (childrenOf[uid] || []).forEach(addActive);
  }
  addActive(rootUid);

  runningNodes = new Set([...onPath].filter(id => statusMap[id] === 'RUNNING'));
  if (runningNodes.size > 0 && !animFrame) animFrame = requestAnimationFrame(driveAnim);

  nodesDS.update(Object.keys(nodeDefaults).map(id => {
    if (!onPath.has(id)) return { id, hidden: true };
    const sc = STATUS_COLOR[statusMap[id]];
    return {
      id, hidden: false,
      color: sc ? { background: sc.background, border: sc.border } : nodeDefaults[id],
      font:  { color: sc ? sc.font : '#cdd6f4', size: 14, face: "'Fira Code','SF Mono',monospace" },
      title: makeTooltip(uidToName[id], nodeTypeMap[id], statusMap[id] || null),
    };
  }));

  edgesDS.update(allEdgeIds.map(id => {
    const { from, to } = edgeNodesMap[id] || {};
    if (!onPath.has(from) || !onPath.has(to)) return { id, hidden: true };
    const active = statusMap[to] === 'RUNNING' || statusMap[to] === 'SUCCESS';
    return {
      id, hidden: false,
      color: active ? { color: '#89b4fa', opacity: 1 } : { color: '#6272a4', opacity: 0.8 },
      width: active ? 2.5 : 1.5,
    };
  }));

  network.once('afterDrawing', () => network.fit({ animation: false }));
}

function renderTick(index) {
  if (index < 0 || index >= allHistory.length) return;
  currentIndex = index;
  const record = allHistory[index];

  if (treeLoaded) applyTickColors(record);

  const tickBadge     = document.getElementById('tick-badge');
  const behaviorBadge = document.getElementById('behavior-badge');
  const statusBadge   = document.getElementById('status-badge');
  tickBadge.textContent = 'tick #' + record.tick;
  if (record.behavior) { behaviorBadge.textContent = record.behavior; behaviorBadge.style.display = 'inline-block'; }
  if (record.status) {
    const sc = STATUS_COLOR[record.status];
    statusBadge.textContent       = record.status;
    statusBadge.style.display     = 'inline-block';
    statusBadge.style.background  = sc ? sc.background : '#313244';
    statusBadge.style.color       = sc ? sc.font       : '#cdd6f4';
    statusBadge.style.borderColor = sc ? sc.border     : 'transparent';
  }
  syncControls();
}

function syncControls() {
  const slider  = document.getElementById('scrubber');
  const counter = document.getElementById('tick-counter');
  const liveDot = document.getElementById('live-dot');
  slider.max   = Math.max(0, allHistory.length - 1);
  slider.value = currentIndex;
  counter.textContent = (currentIndex + 1) + ' / ' + allHistory.length;
  const atEnd = currentIndex === allHistory.length - 1;
  liveDot.textContent = atEnd ? '● LIVE' : '○ REPLAY';
  liveDot.className   = atEnd ? '' : 'paused';
}

function switchTab(tab) {
  currentTab = tab;
  document.getElementById('tab-active').classList.toggle('active', tab === 'active');
  document.getElementById('tab-tree').classList.toggle('active',   tab === 'tree');
  if (currentIndex >= 0) renderTick(currentIndex);
}

function seek(index) {
  const c = Math.max(0, Math.min(index, allHistory.length - 1));
  isLive = c === allHistory.length - 1;
  renderTick(c);
}

function stepPrev() { isLive = false; seek(currentIndex - 1); }
function stepNext() {
  if (currentIndex + 1 >= allHistory.length) { isLive = true; return; }
  isLive = currentIndex + 1 === allHistory.length - 1;
  seek(currentIndex + 1);
}

function togglePlay() {
  isPlaying = !isPlaying;
  document.getElementById('btn-play').textContent = isPlaying ? '⏸ Pause' : '⏵ Play';
  if (isPlaying) {
    isLive = false;
    playTimer = setInterval(() => {
      if (currentIndex >= allHistory.length - 1) { togglePlay(); isLive = true; return; }
      seek(currentIndex + 1);
    }, 250);
  } else {
    clearInterval(playTimer);
    playTimer = null;
  }
}

function driveAnim() {
  if (!overlayCanvas || !network || runningNodes.size === 0) {
    if (overlayCanvas) overlayCanvas.getContext('2d').clearRect(0, 0, overlayCanvas.width, overlayCanvas.height);
    animFrame = null;
    return;
  }
  const ctx = overlayCanvas.getContext('2d');
  ctx.clearRect(0, 0, overlayCanvas.width, overlayCanvas.height);
  const now = Date.now();
  runningNodes.forEach(id => {
    const bb = network.getBoundingBox(id);
    if (!bb) return;
    const tl = network.canvasToDOM({ x: bb.left,  y: bb.top    });
    const br = network.canvasToDOM({ x: bb.right, y: bb.bottom });
    const x = tl.x, y = tl.y, w = br.x - tl.x, h = br.y - tl.y;
    if (w < 1) return;
    ctx.save();
    ctx.beginPath();
    if (nodeShapeMap[id] === 'ellipse') {
      ctx.ellipse(x + w / 2, y + h / 2, w / 2, h / 2, 0, 0, Math.PI * 2);
    } else {
      const r = Math.min(4, w / 2, h / 2);
      ctx.moveTo(x + r, y);           ctx.lineTo(x + w - r, y);
      ctx.arcTo(x + w, y,     x + w, y + r,     r);  ctx.lineTo(x + w, y + h - r);
      ctx.arcTo(x + w, y + h, x + w - r, y + h, r);  ctx.lineTo(x + r, y + h);
      ctx.arcTo(x,     y + h, x,     y + h - r, r);  ctx.lineTo(x, y + r);
      ctx.arcTo(x,     y,     x + r, y,          r);
    }
    ctx.clip();
    const stripe = 10;
    const offset = ((now / 1000) * 50) % (stripe * 2);
    ctx.globalAlpha = 0.35;
    ctx.fillStyle   = '#bbf7d0';
    for (let sx = x - h + offset - stripe * 2; sx < x + w + h; sx += stripe * 2) {
      ctx.beginPath();
      ctx.moveTo(sx,              y);
      ctx.lineTo(sx + stripe,     y);
      ctx.lineTo(sx + stripe + h, y + h);
      ctx.lineTo(sx + h,          y + h);
      ctx.closePath();
      ctx.fill();
    }
    ctx.restore();
  });
  animFrame = requestAnimationFrame(driveAnim);
}

async function pollHistory() {
  try {
    const res     = await fetch('http://127.0.0.1:8081/history');
    const history = await res.json();
    if (!history.length) return;

    if (!treeLoaded) await loadTree();

    const firstLoad = allHistory.length === 0;
    allHistory = history;

    if (firstLoad) { renderTick(allHistory.length - 1); return; }

    document.getElementById('scrubber').max             = allHistory.length - 1;
    document.getElementById('tick-counter').textContent = (currentIndex + 1) + ' / ' + allHistory.length;
    if (isLive && !isPlaying) renderTick(allHistory.length - 1);
  } catch (e) {
    console.error('[BT] pollHistory failed:', e);
  }
}

document.getElementById('btn-prev').addEventListener('click', stepPrev);
document.getElementById('btn-next').addEventListener('click', stepNext);
document.getElementById('btn-play').addEventListener('click', togglePlay);
document.getElementById('scrubber').addEventListener('input', e => {
  if (isPlaying) togglePlay();
  seek(parseInt(e.target.value, 10));
});

console.log('VIEWER_PAGE_LOADED');
loadTree();
restartPollTimer();

function dbgLine(text, cls) {
  const d = document.createElement('div');
  d.className = 'dbg' + (cls ? ' ' + cls : '');
  d.innerHTML = text;
  return d;
}

async function fetchDebug() {
  const el = document.getElementById('debug-lines');
  el.innerHTML = '';

  el.appendChild(dbgLine('<b>treeLoaded:</b> ' + treeLoaded + '  nodes: ' + Object.keys(nodeDefaults).length + '  edges: ' + allEdgeIds.length, treeLoaded ? 'ok' : 'warn'));
  el.appendChild(dbgLine('<b>history entries:</b> ' + allHistory.length + '  currentIndex: ' + currentIndex));

  try {
    const tr = await fetch('http://127.0.0.1:8081/tree');
    const tText = await tr.text();
    const preview = tText.slice(0, 180).replace(/</g, '&lt;');
    el.appendChild(dbgLine('<b>/tree (' + tText.length + ' bytes):</b> ' + preview, tText.length > 5 ? 'ok' : 'err'));
  } catch(e) {
    el.appendChild(dbgLine('<b>/tree failed:</b> ' + e.message, 'err'));
  }

  try {
    const hr = await fetch('http://127.0.0.1:8081/history');
    const hText = await hr.text();
    const preview = hText.slice(0, 180).replace(/</g, '&lt;');
    el.appendChild(dbgLine('<b>/history (' + hText.length + ' bytes):</b> ' + preview, hText.length > 2 ? 'ok' : 'err'));
  } catch(e) {
    el.appendChild(dbgLine('<b>/history failed:</b> ' + e.message, 'err'));
  }

  if (allHistory.length > 0) {
    const last    = allHistory[allHistory.length - 1];
    const apNames = (last.activePath || []).map(n => n.name);
    const matched = apNames.filter(n => nameToUids[n] && nameToUids[n].length > 0).length;
    el.appendChild(dbgLine('<b>nameToUids matches:</b> ' + matched + ' / ' + apNames.length,
      matched > 0 ? 'ok' : 'err'));
    el.appendChild(dbgLine('<b>activePath names:</b> ' + (apNames.join(', ') || '(empty)'),
      apNames.length > 0 ? '' : 'warn'));
    el.appendChild(dbgLine('<b>tree node names:</b> ' + Object.keys(nameToUids).join(', ')));
  }
}

function toggleDebug() {
  const body = document.getElementById('debug-body');
  const open = body.style.display === 'none';
  body.style.display = open ? 'block' : 'none';
  if (open) fetchDebug();
}