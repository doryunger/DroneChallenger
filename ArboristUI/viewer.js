const STATUS_COLOR = {
  SUCCESS: { background: '#1a3d2b', border: '#a6e3a1', font: '#a6e3a1' },
  FAILURE: { background: '#3d1a2b', border: '#f38ba8', font: '#f38ba8' },
  RUNNING: { background: '#3d2a1a', border: '#fab387', font: '#fab387' },
};

const NODE_TYPE = {
  Selector:  { shape: 'ellipse',   bg: '#2a1e40', border: '#cba6f7' },
  Sequence:  { shape: 'box',       bg: '#1e2540', border: '#89b4fa' },
  Action:    { shape: 'roundRect', bg: '#1e3028', border: '#a6e3a1' },
  Condition: { shape: 'diamond',   bg: '#301e28', border: '#f38ba8' },
};
const FALLBACK_TYPE = { shape: 'box', bg: '#313244', border: '#7f849c' };

let network      = null;
let allHistory   = [];
let currentIndex = -1;
let isLive       = true;
let isPlaying    = false;
let playTimer    = null;
let nodeDefaults = {};
let nodeTypeMap  = {};
let allEdgeIds   = [];

function makeTooltip(name, type, status) {
  const sc = STATUS_COLOR[status];
  const sc2 = sc ? sc.font : '#6c7086';
  return '<div style="background:#181825;border:1px solid #313244;padding:7px 11px;' +
    'border-radius:6px;font-family:monospace;font-size:11px;line-height:1.7">' +
    '<b style="color:#cdd6f4">' + name + '</b><br>' +
    '<span style="color:#6c7086">type: </span><span style="color:#89b4fa">' + (type || '?') + '</span><br>' +
    '<span style="color:#6c7086">status: </span><span style="color:' + sc2 + '">' + (status || '—') + '</span>' +
    '</div>';
}

function buildGraph(node, nodes, edges, parentId) {
  const t = NODE_TYPE[node.type] || FALLBACK_TYPE;
  nodeDefaults[node.name] = { background: t.bg, border: t.border };
  nodeTypeMap[node.name]  = node.type || 'Unknown';
  nodes.push({
    id:    node.name,
    label: node.name,
    shape: t.shape,
    color: { background: t.bg, border: t.border },
    font:  { color: '#cdd6f4', size: 13, face: 'monospace' },
    widthConstraint: { minimum: 80, maximum: 160 },
    margin: 10,
    title: makeTooltip(node.name, node.type, null),
  });
  if (parentId) {
    const eid = parentId + '→' + node.name;
    allEdgeIds.push(eid);
    edges.push({ id: eid, from: parentId, to: node.name, color: { color: '#45475a', opacity: 0.8 }, width: 1 });
  }
  (node.children || []).forEach(c => buildGraph(c, nodes, edges, node.name));
}

async function loadTree() {
  try {
    const res = await fetch('/tree');
    const tree = await res.json();
    const nodes = [], edges = [];
    buildGraph(tree, nodes, edges, null);

    const container = document.getElementById('graph');
    const data = { nodes: new vis.DataSet(nodes), edges: new vis.DataSet(edges) };
    const options = {
      layout: {
        hierarchical: {
          direction:       'LR',
          sortMethod:      'directed',
          levelSeparation: 180,
          nodeSpacing:     70,
          treeSpacing:     80,
        },
      },
      physics: false,
      edges: {
        arrows: { to: { enabled: true, scaleFactor: 0.6 } },
        smooth: { type: 'cubicBezier', forceDirection: 'horizontal', roundness: 0.3 },
      },
      interaction: {
        hover:        true,
        zoomView:     true,
        dragView:     true,
        tooltipDelay: 150,
      },
      nodes: {
        borderWidth:         2,
        borderWidthSelected: 3,
        shadow: { enabled: true, color: 'rgba(0,0,0,0.4)', size: 6, x: 2, y: 2 },
      },
    };

    if (network) network.destroy();
    network = new vis.Network(container, data, options);
    network._nodeData = data.nodes;
    network._edgeData = data.edges;
    network.fit({ animation: false });
  } catch (e) { console.error('tree load failed', e); }
}

function renderTick(index) {
  if (index < 0 || index >= allHistory.length) return;
  currentIndex = index;
  const record = allHistory[index];

  const statusMap = {};
  (record.activePath || []).forEach(e => { statusMap[e.name] = e.status; });

  const activeEdgeSet = new Set();
  const path = record.activePath || [];
  for (let i = 0; i < path.length - 1; i++) {
    activeEdgeSet.add(path[i].name + '→' + path[i + 1].name);
  }

  if (network && network._nodeData) {
    network._nodeData.update(Object.keys(nodeDefaults).map(id => {
      const status = statusMap[id];
      const sc = status ? STATUS_COLOR[status] : null;
      return {
        id,
        color: sc ? { background: sc.background, border: sc.border } : nodeDefaults[id],
        font:  { color: sc ? sc.font : '#cdd6f4', size: 13, face: 'monospace' },
        title: makeTooltip(id, nodeTypeMap[id], status),
      };
    }));
  }

  if (network && network._edgeData) {
    network._edgeData.update(allEdgeIds.map(id => ({
      id,
      color: activeEdgeSet.has(id) ? { color: '#89b4fa', opacity: 1 } : { color: '#313244', opacity: 0.35 },
      width: activeEdgeSet.has(id) ? 3 : 1,
    })));
  }

  const tickBadge     = document.getElementById('tick-badge');
  const behaviorBadge = document.getElementById('behavior-badge');
  const statusBadge   = document.getElementById('status-badge');
  tickBadge.textContent = 'tick #' + record.tick;
  if (record.behavior) { behaviorBadge.textContent = record.behavior; behaviorBadge.style.display = 'inline-block'; }
  if (record.status) {
    const sc = STATUS_COLOR[record.status];
    statusBadge.textContent      = record.status;
    statusBadge.style.display    = 'inline-block';
    statusBadge.style.background = sc ? sc.background  : '#313244';
    statusBadge.style.color      = sc ? sc.font        : '#cdd6f4';
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

function seek(index) {
  const c = Math.max(0, Math.min(index, allHistory.length - 1));
  isLive = c === allHistory.length - 1;
  renderTick(c);
}

function stepPrev() { isLive = false; seek(currentIndex - 1); }
function stepNext() {
  const next = currentIndex + 1;
  if (next >= allHistory.length) { isLive = true; return; }
  isLive = next === allHistory.length - 1;
  seek(next);
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
    clearInterval(playTimer); playTimer = null;
  }
}

async function pollHistory() {
  try {
    const res = await fetch('/history');
    const history = await res.json();
    if (!history.length) return;
    const firstLoad = allHistory.length === 0;
    allHistory = history;
    if (firstLoad) { renderTick(allHistory.length - 1); return; }
    document.getElementById('scrubber').max = allHistory.length - 1;
    document.getElementById('tick-counter').textContent = (currentIndex + 1) + ' / ' + allHistory.length;
    if (isLive && !isPlaying) renderTick(allHistory.length - 1);
  } catch (e) { console.error('history poll failed', e); }
}

document.getElementById('btn-prev').addEventListener('click', stepPrev);
document.getElementById('btn-next').addEventListener('click', stepNext);
document.getElementById('btn-play').addEventListener('click', togglePlay);
document.getElementById('scrubber').addEventListener('input', e => {
  if (isPlaying) togglePlay();
  seek(parseInt(e.target.value, 10));
});

loadTree();
setInterval(pollHistory, 500);
