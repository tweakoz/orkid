#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
################################################################

"""
HTTP Log Server with ZMQ subscription and SSE streaming.
Supports multiple orkid clients with split-panel UI.
Features: Status dashboard, PerfItem graphs, Log stream.

Usage: ork.logger.httpserver.py [http_port] [zmq_port]
Default ports: 12288 (HTTP), 12289 (ZMQ)
"""

import sys
import json
import threading
import queue
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
import zmq

# Default ports
DEFAULT_HTTP_PORT = 12288
DEFAULT_ZMQ_PORT = 12289

# Client timeout (seconds) - remove client if no heartbeat
CLIENT_TIMEOUT = 5.0

# Thread-safe data structures
clients_lock = threading.Lock()
clients = {}  # key: "app:pid", value: {"app": str, "pid": int, "last_seen": float, "entries": [], "channels": {}}

# Queue for SSE broadcast
sse_queues = []  # List of queues for connected browsers
sse_queues_lock = threading.Lock()

HTML_PAGE = r'''<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Orkid Log Server</title>
<style>
:root {
  --bg: #0d0d0d;
  --fg: #e0e0e0;
  --entry-bg: #1a1a1a;
  --border: #333;
  --controls-bg: #141414;
  --panel-bg: #111;
  --status-bg: #0a0a12;
  --graph-bg: #0a0a0a;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: var(--bg);
  color: var(--fg);
  font-family: 'SF Mono', 'Monaco', 'Menlo', 'Consolas', monospace;
  font-size: 12px;
  line-height: 1.4;
  height: 100vh;
  display: flex;
  flex-direction: column;
}
#global-controls {
  background: var(--controls-bg);
  padding: 10px 12px;
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}
#client-bar {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}
.section-title {
  font-weight: 600;
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  color: #666;
}
.client-toggle {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 4px 10px;
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 3px;
  cursor: pointer;
  font-size: 11px;
  transition: all 0.15s ease;
}
.client-toggle:hover {
  background: #252525;
  border-color: #444;
}
.client-toggle.active {
  background: #1a3a1a;
  border-color: #4a4;
}
.client-toggle.dead {
  opacity: 0.5;
  text-decoration: line-through;
}
.client-toggle input {
  margin: 0;
}
.client-name {
  color: #8cf;
}
.client-pid {
  color: #888;
  font-size: 10px;
}
.live-dot {
  width: 6px;
  height: 6px;
  background: #4a4;
  border-radius: 50%;
  opacity: 0.4;
  transition: opacity 0.1s ease-out;
}
.live-dot.beat {
  opacity: 1;
  box-shadow: 0 0 6px #4a4;
}
.live-dot.dead {
  background: #a44;
  opacity: 1;
}
.btn {
  padding: 4px 12px;
  background: #222;
  border: 1px solid #444;
  border-radius: 3px;
  color: #aaa;
  cursor: pointer;
  font-size: 10px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.3px;
  transition: all 0.15s ease;
}
.btn:hover { background: #333; color: #fff; border-color: #555; }
.btn:active { background: #444; }
#no-clients {
  color: #666;
  font-style: italic;
}
#panels-container {
  flex: 1;
  display: grid;
  gap: 0;
  background: var(--bg);
  overflow: hidden;
}
.panel {
  background: var(--panel-bg);
  display: flex;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid var(--border);
}
.grid-gutter-col {
  width: 4px;
  background: #000;
  cursor: col-resize;
  transition: background 0.1s;
}
.grid-gutter-col:hover {
  background: #bbb;
}
.grid-gutter-col.dragging {
  background: #ff0;
}
.grid-gutter-row {
  height: 4px;
  background: #000;
  cursor: row-resize;
  transition: background 0.1s;
  grid-column: 1 / -1;
}
.grid-gutter-row:hover {
  background: #bbb;
}
.grid-gutter-row.dragging {
  background: #ff0;
}
.panel-header {
  background: var(--controls-bg);
  padding: 8px 10px;
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}
.panel-title {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 6px;
}
.panel-title .app-name {
  font-weight: 600;
  color: #8cf;
}
.panel-title .app-pid {
  color: #666;
  font-size: 10px;
}
.panel-title .app-host {
  color: #686;
  font-size: 10px;
  margin-left: 4px;
}
.btn-remove {
  padding: 2px 6px;
  font-size: 10px;
  margin-left: 8px;
  background: #622;
  border-color: #944;
  color: #faa;
}
.btn-remove:hover {
  background: #833;
}
.btn-remove.hidden {
  display: none;
}
.panel-controls {
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.channel-toggles {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  flex: 1;
}
.controls-row {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  border-top: 1px solid #333;
  padding-top: 4px;
}
.channel-toggle {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 2px 6px;
  border-radius: 2px;
  cursor: pointer;
  font-size: 10px;
  font-weight: 500;
  border: 1px solid transparent;
}
.channel-toggle:hover { filter: brightness(1.2); }
.channel-toggle input {
  -webkit-appearance: none;
  appearance: none;
  width: 10px;
  height: 10px;
  border-radius: 2px;
  cursor: pointer;
  position: relative;
}
.channel-toggle input:checked::after {
  content: '';
  position: absolute;
  left: 2px;
  top: 0px;
  width: 4px;
  height: 6px;
  border: solid currentColor;
  border-width: 0 2px 2px 0;
  transform: rotate(45deg);
}
.filter-group {
  display: flex;
  align-items: center;
  gap: 4px;
}
.filter-label {
  font-size: 9px;
  color: #666;
  text-transform: uppercase;
}
.filter-input {
  padding: 2px 6px;
  background: #1a1a1a;
  border: 1px solid #333;
  border-radius: 2px;
  color: #e0e0e0;
  font-family: inherit;
  font-size: 10px;
  width: 100px;
}
.filter-input:focus {
  outline: none;
  border-color: #555;
}
.filter-input.error {
  border-color: #a44;
  background: #1a0a0a;
}

/* 3-section panel layout */
.panel-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* Status Dashboard */
.status-section {
  background: var(--status-bg);
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
}
.status-section.collapsed {
  max-height: 24px !important;
}
.status-section.collapsed .status-container {
  display: none;
}
.status-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 4px 8px;
  background: rgba(0,0,0,0.3);
  border-bottom: 1px solid var(--border);
}
.status-header-left {
  display: flex;
  align-items: center;
  gap: 8px;
}
.status-title {
  font-size: 10px;
  color: #666;
  text-transform: uppercase;
}
.status-toggle {
  font-size: 10px;
  cursor: pointer;
  color: #888;
  user-select: none;
}
.status-toggle:hover { color: #fff; }
.status-container {
  max-height: 258px;
  overflow-y: auto;
}
.status-channels {
  display: flex;
  flex-wrap: wrap;
  gap: 2px;
  padding: 4px;
}
.status-channel {
  background: rgba(0,0,0,0.3);
  border: 1px solid #222;
  border-radius: 3px;
  padding: 3px 6px;
  min-width: 120px;
  max-width: 280px;
}
.status-channel-name {
  font-size: 9px;
  color: #666;
  text-transform: uppercase;
  margin-bottom: 2px;
  border-bottom: 1px solid #222;
  padding-bottom: 2px;
}
.status-items {
  display: flex;
  flex-direction: column;
  gap: 1px;
}
.status-item {
  display: flex;
  align-items: baseline;
  gap: 4px;
  font-size: 10px;
  line-height: 1.3;
}
.status-key {
  color: #777;
  white-space: nowrap;
}
.status-value {
  font-weight: 500;
  word-break: break-word;
}

/* Graph Section */
.graph-section {
  background: var(--graph-bg);
  border-bottom: 1px solid var(--border);
  flex-shrink: 0;
  position: relative;
  max-height: 300px;
}
.graph-section:empty {
  display: none;
}
.graph-section.collapsed {
  height: 24px !important;
  min-height: 24px !important;
  max-height: 24px !important;
}
.graph-section.collapsed .graph-container {
  display: none;
}
.graph-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 4px 8px;
  background: rgba(0,0,0,0.3);
  border-bottom: 1px solid var(--border);
}
.graph-header-left {
  display: flex;
  align-items: center;
  gap: 8px;
}
.graph-title {
  font-size: 10px;
  color: #666;
  text-transform: uppercase;
}
.graph-toggle {
  font-size: 10px;
  cursor: pointer;
  color: #888;
  user-select: none;
}
.graph-toggle:hover { color: #fff; }
.graph-container {
  overflow-y: auto;
  max-height: 276px;
}
.graph-canvas-wrapper {
  position: relative;
  min-height: 80px;
}
.graph-canvas {
  display: block;
  width: 100%;
}

/* Log Section */
.log-section {
  flex: 1;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}
.panel-log {
  flex: 1;
  overflow-y: auto;
  padding: 6px;
}
.entry {
  padding: 2px 6px;
  border-left: 3px solid #888;
  margin: 1px 0;
  background: var(--entry-bg);
  border-radius: 0 2px 2px 0;
  white-space: pre-wrap;
  word-break: break-all;
  font-size: 11px;
}
.entry .ts { color: #555; margin-right: 8px; font-variant-numeric: tabular-nums; }
.entry .ch { font-weight: 600; margin-right: 8px; }
.entry .sub { color: #777; margin-right: 6px; }
.entry.warn { background: #1a1500; border-left-color: #b58900 !important; }
.entry.error { background: #1a0a0a; border-left-color: #dc322f !important; }
.hidden { display: none !important; }
.panel-footer {
  background: var(--controls-bg);
  padding: 4px 10px;
  border-top: 1px solid var(--border);
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 10px;
  color: #666;
}
.entry-count {
  font-variant-numeric: tabular-nums;
}
#empty-state {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: #444;
  font-size: 14px;
}
</style>
</head>
<body>
<div id="global-controls">
  <div id="client-bar">
    <span class="section-title">Clients:</span>
    <span id="client-list"></span>
    <span id="no-clients">No clients connected</span>
    <button class="btn" id="btn-select-all">Select All</button>
    <button class="btn" id="btn-select-none">Select None</button>
  </div>
</div>
<div id="panels-container">
  <div id="empty-state">Select clients to view their logs</div>
</div>

<script>
// State
const clients = new Map();  // clientId -> {app, pid, alive, channels, entries, status, perf, panel}
const selectedClients = new Set();

// DOM elements
const clientListEl = document.getElementById('client-list');
const noClientsEl = document.getElementById('no-clients');
const panelsContainer = document.getElementById('panels-container');

// Graph constants
const GRAPH_MAX_SAMPLES = 500;
const GRAPH_FPS = 30;

// Utility functions
function esc(s) {
  if (!s) return '';
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function hexToRgb(hex) {
  const r = parseInt(hex.slice(1,3), 16);
  const g = parseInt(hex.slice(3,5), 16);
  const b = parseInt(hex.slice(5,7), 16);
  return {r, g, b};
}

function getLuminance(r, g, b) {
  const [rs, gs, bs] = [r, g, b].map(c => {
    c = c / 255;
    return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
  });
  return 0.2126 * rs + 0.7152 * gs + 0.0722 * bs;
}

function getContrastColors(hex) {
  const {r, g, b} = hexToRgb(hex || '#888888');
  const lum = getLuminance(r, g, b);
  if (lum > 0.4) {
    return { text: hex, bg: '#0d0d0d', border: hex + '40', isDark: false };
  } else if (lum > 0.15) {
    const brighten = 1.3;
    const nr = Math.min(255, Math.round(r * brighten));
    const ng = Math.min(255, Math.round(g * brighten));
    const nb = Math.min(255, Math.round(b * brighten));
    const brightHex = '#' + [nr, ng, nb].map(c => c.toString(16).padStart(2,'0')).join('');
    return { text: brightHex, bg: '#0d0d0d', border: hex + '50', isDark: false };
  } else {
    const lighten = 0.85;
    const nr = Math.round(255 - (255 - r) * (1 - lighten));
    const ng = Math.round(255 - (255 - g) * (1 - lighten));
    const nb = Math.round(255 - (255 - b) * (1 - lighten));
    const lightBg = '#' + [nr, ng, nb].map(c => c.toString(16).padStart(2,'0')).join('');
    return { text: '#111', bg: lightBg, border: hex, isDark: true };
  }
}

function formatValue(v) {
  if (Math.abs(v) >= 1000) return v.toFixed(0);
  if (Math.abs(v) >= 100) return v.toFixed(1);
  if (Math.abs(v) >= 10) return v.toFixed(2);
  if (Math.abs(v) >= 1) return v.toFixed(3);
  return v.toPrecision(3);
}

// Client management - use PID as primary key, update app name dynamically
function getClientId(app, pid) {
  return `${pid}`;  // Use only PID as key
}

function registerClient(app, pid, host = 'localhost') {
  const clientId = getClientId(app, pid);
  if (!clients.has(clientId)) {
    clients.set(clientId, {
      app, pid, host,
      alive: true,
      channels: new Map(),
      entries: [],
      status: new Map(),   // key: "ch:sub", value: {ch, sub, msg, color, ts}
      perf: new Map(),     // key: "ch:sub", value: {ch, sub, color, samples: [], min, max, visible}
      panel: null,
      includeRegex: null,
      excludeRegex: null,
      autoScroll: true,
      statusCollapsed: true,
      graphCollapsed: true
    });
    updateClientList();
  } else {
    const client = clients.get(clientId);
    // Update app name if it changed (EzApp sets it after initial registration)
    if (client.app !== app && app !== 'orkid_app') {
      client.app = app;
      updateClientList();
      // Update panel header if visible
      if (client.panel) {
        const nameEl = client.panel.querySelector('.app-name');
        if (nameEl) nameEl.textContent = app;
      }
    }
    // Update host if provided
    if (host && host !== 'localhost' && client.host !== host) {
      client.host = host;
      if (client.panel) {
        const hostEl = client.panel.querySelector('.app-host');
        if (hostEl) hostEl.textContent = host;
      }
    }
    if (!client.alive) {
      client.alive = true;
      updateClientList();
    }
  }
}

function markClientDead(app, pid) {
  const clientId = getClientId(app, pid);
  const client = clients.get(clientId);
  if (client) {
    client.alive = false;
    updateClientList();
    // Update all dots for this client (including panel header)
    const dots = document.querySelectorAll(`.live-dot[data-client-id="${clientId}"]`);
    dots.forEach(dot => dot.classList.add('dead'));
    // Show remove button in panel
    if (client.panel) {
      const removeBtn = client.panel.querySelector('.btn-remove');
      if (removeBtn) removeBtn.classList.remove('hidden');
    }
  }
}

function removeClient(clientId) {
  const client = clients.get(clientId);
  if (client) {
    // Remove panel from DOM
    if (client.panel) {
      client.panel.remove();
    }
    // Stop graph loop
    if (graphLoops.has(clientId)) {
      cancelAnimationFrame(graphLoops.get(clientId));
      graphLoops.delete(clientId);
    }
    // Remove from selected clients
    selectedClients.delete(clientId);
    // Remove from clients map
    clients.delete(clientId);
    // Update UI
    updateClientList();
  }
}

function pulseHeartbeat(clientId) {
  const dots = document.querySelectorAll(`.live-dot[data-client-id="${clientId}"]`);
  dots.forEach(dot => {
    if (!dot.classList.contains('dead')) {
      dot.classList.add('beat');
      setTimeout(() => dot.classList.remove('beat'), 200);
    }
  });
}

function updateClientList() {
  clientListEl.innerHTML = '';
  const hasClients = clients.size > 0;
  noClientsEl.style.display = hasClients ? 'none' : 'inline';

  // Sort clients by app name (case insensitive alphanumeric)
  const sortedClients = [...clients.entries()].sort((a, b) => {
    return a[1].app.toLowerCase().localeCompare(b[1].app.toLowerCase(), undefined, { numeric: true });
  });

  sortedClients.forEach(([clientId, client]) => {
    const label = document.createElement('label');
    label.className = 'client-toggle' + (selectedClients.has(clientId) ? ' active' : '') + (client.alive ? '' : ' dead');

    const cb = document.createElement('input');
    cb.type = 'checkbox';
    cb.checked = selectedClients.has(clientId);
    cb.onchange = () => toggleClient(clientId, cb.checked);

    const dot = document.createElement('span');
    dot.className = 'live-dot' + (client.alive ? '' : ' dead');
    dot.dataset.clientId = clientId;

    const nameSpan = document.createElement('span');
    nameSpan.className = 'client-name';
    nameSpan.textContent = client.app;

    const pidSpan = document.createElement('span');
    pidSpan.className = 'client-pid';
    pidSpan.textContent = `[${client.pid}]`;

    label.appendChild(cb);
    label.appendChild(dot);
    label.appendChild(nameSpan);
    label.appendChild(pidSpan);
    clientListEl.appendChild(label);
  });
}

function toggleClient(clientId, selected) {
  if (selected) {
    selectedClients.add(clientId);
  } else {
    selectedClients.delete(clientId);
  }
  updateClientList();
  updatePanels();
}

function selectAllClients() {
  clients.forEach((_, clientId) => selectedClients.add(clientId));
  updateClientList();
  updatePanels();
}

function selectNoClients() {
  selectedClients.clear();
  updateClientList();
  updatePanels();
}

// Panel management
let gridColSizes = [];  // Track column sizes for resizing
let gridRowSizes = [];  // Track row sizes for resizing

function updatePanels() {
  const count = selectedClients.size;

  if (count === 0) {
    panelsContainer.innerHTML = '<div id="empty-state">Select clients to view their logs</div>';
    panelsContainer.style.gridTemplateColumns = '1fr';
    panelsContainer.style.gridTemplateRows = '1fr';
    gridColSizes = [];
    gridRowSizes = [];
    return;
  }

  // Calculate grid dimensions
  let cols;
  if (count === 1) cols = 1;
  else if (count === 2) cols = 2;
  else if (count <= 4) cols = 2;
  else if (count <= 6) cols = 3;
  else cols = Math.ceil(Math.sqrt(count));

  const rows = Math.ceil(count / cols);

  // Initialize sizes (1fr each, with 4px gutters)
  gridColSizes = [];
  for (let c = 0; c < cols; c++) {
    gridColSizes.push('1fr');
    if (c < cols - 1) gridColSizes.push('4px');  // gutter
  }
  gridRowSizes = [];
  for (let r = 0; r < rows; r++) {
    gridRowSizes.push('1fr');
    if (r < rows - 1) gridRowSizes.push('4px');  // gutter
  }

  panelsContainer.style.gridTemplateColumns = gridColSizes.join(' ');
  panelsContainer.style.gridTemplateRows = gridRowSizes.join(' ');
  panelsContainer.innerHTML = '';

  // Sort selected clients by app name (case insensitive alphanumeric)
  const sortedSelected = [...selectedClients].sort((a, b) => {
    const clientA = clients.get(a);
    const clientB = clients.get(b);
    if (!clientA || !clientB) return 0;
    return clientA.app.toLowerCase().localeCompare(clientB.app.toLowerCase(), undefined, { numeric: true });
  });

  // Create panels and gutters
  let idx = 0;
  for (let r = 0; r < rows; r++) {
    const gridRow = r * 2 + 1;  // 1-based, skip gutter rows

    for (let c = 0; c < cols; c++) {
      const gridCol = c * 2 + 1;  // 1-based, skip gutter cols

      if (idx < sortedSelected.length) {
        const clientId = sortedSelected[idx];
        const client = clients.get(clientId);
        if (client) {
          const panel = createPanel(clientId, client);
          panel.style.gridColumn = gridCol;
          panel.style.gridRow = gridRow;
          panelsContainer.appendChild(panel);
          client.panel = panel;
          updateChannelToggles(clientId);
          renderStatus(clientId);
          renderEntries(clientId);
          startGraphLoop(clientId);
        }
      }

      // Add column gutter (except after last column)
      if (c < cols - 1) {
        const gutter = document.createElement('div');
        gutter.className = 'grid-gutter-col';
        gutter.style.gridColumn = gridCol + 1;
        gutter.style.gridRow = gridRow;
        gutter.dataset.colIndex = c * 2;  // Index into gridColSizes
        panelsContainer.appendChild(gutter);
        setupColGutter(gutter);
      }

      idx++;
    }

    // Add row gutter (except after last row)
    if (r < rows - 1) {
      const gutter = document.createElement('div');
      gutter.className = 'grid-gutter-row';
      gutter.style.gridRow = gridRow + 1;
      gutter.dataset.rowIndex = r * 2;  // Index into gridRowSizes
      panelsContainer.appendChild(gutter);
      setupRowGutter(gutter);
    }
  }
}

function setupColGutter(gutter) {
  let startX, startWidths, colIdx;

  const onMouseMove = (e) => {
    const dx = e.clientX - startX;
    const containerWidth = panelsContainer.clientWidth;
    const totalGutterWidth = (gridColSizes.filter(s => s === '4px').length) * 4;
    const availableWidth = containerWidth - totalGutterWidth;

    // Convert fr to pixels, apply delta, convert back
    const leftPx = startWidths.left + dx;
    const rightPx = startWidths.right - dx;

    // Minimum 50px per panel
    if (leftPx < 50 || rightPx < 50) return;

    const leftFr = leftPx / availableWidth;
    const rightFr = rightPx / availableWidth;

    gridColSizes[colIdx] = leftFr + 'fr';
    gridColSizes[colIdx + 2] = rightFr + 'fr';
    panelsContainer.style.gridTemplateColumns = gridColSizes.join(' ');
  };

  const onMouseUp = () => {
    gutter.classList.remove('dragging');
    document.removeEventListener('mousemove', onMouseMove);
    document.removeEventListener('mouseup', onMouseUp);
  };

  gutter.addEventListener('mousedown', (e) => {
    e.preventDefault();
    gutter.classList.add('dragging');
    startX = e.clientX;
    colIdx = parseInt(gutter.dataset.colIndex);

    // Get current pixel widths of adjacent columns
    const panels = panelsContainer.querySelectorAll('.panel');
    const containerWidth = panelsContainer.clientWidth;
    const totalGutterWidth = (gridColSizes.filter(s => s === '4px').length) * 4;
    const availableWidth = containerWidth - totalGutterWidth;

    // Parse current fr values
    const leftFr = parseFloat(gridColSizes[colIdx]) || 1;
    const rightFr = parseFloat(gridColSizes[colIdx + 2]) || 1;
    const totalFr = gridColSizes.filter(s => s.endsWith('fr')).reduce((sum, s) => sum + parseFloat(s), 0);

    startWidths = {
      left: (leftFr / totalFr) * availableWidth,
      right: (rightFr / totalFr) * availableWidth
    };

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
  });
}

function setupRowGutter(gutter) {
  let startY, startHeights, rowIdx;

  const onMouseMove = (e) => {
    const dy = e.clientY - startY;
    const containerHeight = panelsContainer.clientHeight;
    const totalGutterHeight = (gridRowSizes.filter(s => s === '4px').length) * 4;
    const availableHeight = containerHeight - totalGutterHeight;

    const topPx = startHeights.top + dy;
    const bottomPx = startHeights.bottom - dy;

    // Minimum 50px per panel
    if (topPx < 50 || bottomPx < 50) return;

    const topFr = topPx / availableHeight;
    const bottomFr = bottomPx / availableHeight;

    gridRowSizes[rowIdx] = topFr + 'fr';
    gridRowSizes[rowIdx + 2] = bottomFr + 'fr';
    panelsContainer.style.gridTemplateRows = gridRowSizes.join(' ');
  };

  const onMouseUp = () => {
    gutter.classList.remove('dragging');
    document.removeEventListener('mousemove', onMouseMove);
    document.removeEventListener('mouseup', onMouseUp);
  };

  gutter.addEventListener('mousedown', (e) => {
    e.preventDefault();
    gutter.classList.add('dragging');
    startY = e.clientY;
    rowIdx = parseInt(gutter.dataset.rowIndex);

    const containerHeight = panelsContainer.clientHeight;
    const totalGutterHeight = (gridRowSizes.filter(s => s === '4px').length) * 4;
    const availableHeight = containerHeight - totalGutterHeight;

    const topFr = parseFloat(gridRowSizes[rowIdx]) || 1;
    const bottomFr = parseFloat(gridRowSizes[rowIdx + 2]) || 1;
    const totalFr = gridRowSizes.filter(s => s.endsWith('fr')).reduce((sum, s) => sum + parseFloat(s), 0);

    startHeights = {
      top: (topFr / totalFr) * availableHeight,
      bottom: (bottomFr / totalFr) * availableHeight
    };

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
  });
}

function createPanel(clientId, client) {
  const panel = document.createElement('div');
  panel.className = 'panel';
  panel.dataset.clientId = clientId;

  panel.innerHTML = `
    <div class="panel-header">
      <div class="panel-title">
        <span class="app-name">${esc(client.app)}</span>
        <span class="app-pid">[${client.pid}]</span>
        <span class="app-host">@${esc(client.host || 'localhost')}</span>
        <span class="live-dot ${client.alive ? '' : 'dead'}" data-client-id="${clientId}"></span>
        <button class="btn btn-remove ${client.alive ? 'hidden' : ''}" title="Remove client">✕</button>
      </div>
      <div class="panel-controls">
        <div class="channel-toggles" data-client="${clientId}"></div>
        <div class="controls-row">
          <button class="btn btn-ch-all">All</button>
          <button class="btn btn-ch-none">None</button>
          <div class="filter-group">
            <span class="filter-label">Inc:</span>
            <input type="text" class="filter-input filter-include" placeholder="regex">
          </div>
          <div class="filter-group">
            <span class="filter-label">Exc:</span>
            <input type="text" class="filter-input filter-exclude" placeholder="regex">
          </div>
          <button class="btn btn-clear">Clear</button>
        </div>
      </div>
    </div>
    <div class="panel-content">
      <div class="status-section ${client.statusCollapsed ? 'collapsed' : ''}">
        <div class="status-header">
          <div class="status-header-left">
            <span class="status-title">Status</span>
          </div>
          <span class="status-toggle">${client.statusCollapsed ? '[+]' : '[-]'}</span>
        </div>
        <div class="status-container">
          <div class="status-channels"></div>
        </div>
      </div>
      <div class="graph-section ${client.graphCollapsed ? 'collapsed' : ''}">
        <div class="graph-header">
          <div class="graph-header-left">
            <span class="graph-title">Performance</span>
          </div>
          <span class="graph-toggle">${client.graphCollapsed ? '[+]' : '[-]'}</span>
        </div>
        <div class="graph-container">
          <div class="graph-canvas-wrapper">
            <canvas class="graph-canvas"></canvas>
          </div>
        </div>
      </div>
      <div class="log-section">
        <div class="panel-log"></div>
      </div>
    </div>
    <div class="panel-footer">
      <span class="entry-count">0 / 0</span>
      <label style="cursor:pointer"><input type="checkbox" class="auto-scroll" checked> Auto-scroll</label>
    </div>
  `;

  // Setup filter inputs
  const includeInput = panel.querySelector('.filter-include');
  const excludeInput = panel.querySelector('.filter-exclude');

  let filterTimeout;
  const onFilterChange = () => {
    clearTimeout(filterTimeout);
    filterTimeout = setTimeout(() => {
      client.includeRegex = compileRegex(includeInput.value, includeInput);
      client.excludeRegex = compileRegex(excludeInput.value, excludeInput);
      applyFilters(clientId);
    }, 150);
  };

  includeInput.addEventListener('input', onFilterChange);
  excludeInput.addEventListener('input', onFilterChange);

  // Setup clear button
  panel.querySelector('.btn-clear').onclick = () => {
    client.entries = [];
    renderEntries(clientId);
  };

  // Setup remove button (removes dead client)
  panel.querySelector('.btn-remove').onclick = () => {
    removeClient(clientId);
  };

  // Setup channel all/none buttons
  panel.querySelector('.btn-ch-all').onclick = () => {
    client.channels.forEach(info => { info.visible = true; });
    updateChannelToggles(clientId);
    applyFilters(clientId);
  };
  panel.querySelector('.btn-ch-none').onclick = () => {
    client.channels.forEach(info => { info.visible = false; });
    updateChannelToggles(clientId);
    applyFilters(clientId);
  };

  // Setup auto-scroll checkbox
  panel.querySelector('.auto-scroll').onchange = (e) => {
    client.autoScroll = e.target.checked;
  };

  // Setup status toggle
  panel.querySelector('.status-toggle').onclick = () => {
    client.statusCollapsed = !client.statusCollapsed;
    const statusSection = panel.querySelector('.status-section');
    statusSection.classList.toggle('collapsed', client.statusCollapsed);
    panel.querySelector('.status-toggle').textContent = client.statusCollapsed ? '[+]' : '[-]';
  };

  // Setup graph toggle
  panel.querySelector('.graph-toggle').onclick = () => {
    client.graphCollapsed = !client.graphCollapsed;
    const graphSection = panel.querySelector('.graph-section');
    graphSection.classList.toggle('collapsed', client.graphCollapsed);
    panel.querySelector('.graph-toggle').textContent = client.graphCollapsed ? '[+]' : '[-]';
  };

  // Initialize canvas size
  requestAnimationFrame(() => {
    const canvas = panel.querySelector('.graph-canvas');
    const wrapper = panel.querySelector('.graph-canvas-wrapper');
    if (canvas && wrapper) {
      canvas.width = wrapper.clientWidth;
      canvas.height = wrapper.clientHeight;
    }
  });

  return panel;
}

function compileRegex(pattern, inputEl) {
  if (!pattern || pattern.trim() === '') {
    inputEl.classList.remove('error');
    return null;
  }
  try {
    const regex = new RegExp(pattern);
    inputEl.classList.remove('error');
    return regex;
  } catch (e) {
    inputEl.classList.add('error');
    return null;
  }
}

function rebuildChannelsFromData(clientId) {
  const client = clients.get(clientId);
  if (!client) return;

  // Scan all entries for channels
  client.entries.forEach(entry => {
    if (!client.channels.has(entry.ch)) {
      client.channels.set(entry.ch, { visible: true, color: entry.color || '#888' });
    }
  });

  // Scan status for channels
  client.status.forEach((item, key) => {
    if (!client.channels.has(item.ch)) {
      client.channels.set(item.ch, { visible: true, color: item.color || '#888' });
    }
  });

  // Scan perf for channels
  client.perf.forEach((series, key) => {
    if (!client.channels.has(series.ch)) {
      client.channels.set(series.ch, { visible: true, color: series.color || '#888' });
    }
  });
}

function updateChannelToggles(clientId) {
  const client = clients.get(clientId);
  if (!client || !client.panel) return;

  // Rebuild channels from all data sources
  rebuildChannelsFromData(clientId);

  const container = client.panel.querySelector('.channel-toggles');
  container.innerHTML = '';

  // Sort channels alphabetically
  const sortedChannels = Array.from(client.channels.entries()).sort((a, b) => a[0].localeCompare(b[0]));

  sortedChannels.forEach(([ch, info]) => {
    const colors = getContrastColors(info.color);

    const label = document.createElement('label');
    label.className = 'channel-toggle';
    label.style.color = colors.text;
    label.style.background = colors.bg;
    label.style.borderColor = colors.border;

    const cb = document.createElement('input');
    cb.type = 'checkbox';
    cb.checked = info.visible;
    const chanColor = info.color || '#888';
    cb.style.background = chanColor;
    cb.style.border = '1px solid ' + colors.border;
    // Determine checkmark color based on channel color luminance
    const {r, g, b} = hexToRgb(chanColor);
    const chanLum = getLuminance(r, g, b);
    cb.style.color = chanLum > 0.4 ? '#000' : '#fff';
    cb.onchange = () => {
      info.visible = cb.checked;
      applyFilters(clientId);
    };

    label.appendChild(cb);
    label.appendChild(document.createTextNode(ch));
    container.appendChild(label);
  });
}

// Status dashboard
function renderStatus(clientId) {
  const client = clients.get(clientId);
  if (!client || !client.panel) return;

  const container = client.panel.querySelector('.status-channels');
  container.innerHTML = '';

  // Group items by channel
  const channels = new Map();
  client.status.forEach((item, key) => {
    if (!channels.has(item.ch)) {
      channels.set(item.ch, []);
    }
    channels.get(item.ch).push(item);
  });

  // Render each channel as a box
  channels.forEach((items, chName) => {
    const channelDiv = document.createElement('div');
    channelDiv.className = 'status-channel';

    // Get color from first item
    const chColor = items[0]?.color || '#888';

    const header = document.createElement('div');
    header.className = 'status-channel-name';
    header.style.color = chColor;
    header.textContent = chName;
    channelDiv.appendChild(header);

    const itemsDiv = document.createElement('div');
    itemsDiv.className = 'status-items';

    items.forEach(item => {
      const div = document.createElement('div');
      div.className = 'status-item';
      div.innerHTML = `<span class="status-key">${esc(item.sub)}:</span><span class="status-value" style="color:${item.color}">${esc(item.msg)}</span>`;
      itemsDiv.appendChild(div);
    });

    channelDiv.appendChild(itemsDiv);
    container.appendChild(channelDiv);
  });
}

function addStatus(clientId, data) {
  const client = clients.get(clientId);
  if (!client) return;

  const key = `${data.ch}:${data.sub}`;
  client.status.set(key, {
    ch: data.ch,
    sub: data.sub,
    msg: data.msg,
    color: data.color || '#888',
    ts: data.ts
  });

  if (client.panel) {
    renderStatus(clientId);
  }
}

// Performance graphs
function addPerfSample(clientId, data) {
  const client = clients.get(clientId);
  if (!client) return;

  const key = `${data.ch}:${data.sub}`;
  let series = client.perf.get(key);

  if (!series) {
    series = {
      ch: data.ch,
      sub: data.sub,
      color: data.color || '#888',
      samples: [],
      min: data.value,
      max: data.value,
      smoothMin: data.value,
      smoothMax: data.value,
      visible: true
    };
    client.perf.set(key, series);
  }

  series.samples.push(data.value);
  if (series.samples.length > GRAPH_MAX_SAMPLES) {
    series.samples.shift();
  }

  // Update min/max
  series.min = Math.min(series.min, data.value);
  series.max = Math.max(series.max, data.value);

  // Smooth min/max (like graphview.cpp)
  const blend = 0.03;
  series.smoothMin = series.smoothMin * (1 - blend) + series.min * blend;
  series.smoothMax = series.smoothMax * (1 - blend) + series.max * blend;
}

function drawGraph(clientId) {
  const client = clients.get(clientId);
  if (!client || !client.panel || client.graphCollapsed) return;

  const canvas = client.panel.querySelector('.graph-canvas');
  const wrapper = client.panel.querySelector('.graph-canvas-wrapper');
  if (!canvas || !wrapper) return;

  // Count visible series for lane assignment
  const visibleSeries = [];
  client.perf.forEach((series, key) => {
    if (series.visible && series.samples.length > 0) {
      visibleSeries.push({ key, series });
    }
  });

  // Set canvas height based on number of series (80px per lane)
  const laneHeight = 80;
  const totalHeight = Math.max(80, visibleSeries.length * laneHeight);
  const w = wrapper.clientWidth;

  if (canvas.width !== w || canvas.height !== totalHeight) {
    canvas.width = w;
    canvas.height = totalHeight;
    wrapper.style.height = totalHeight + 'px';
  }

  const ctx = canvas.getContext('2d');
  const h = canvas.height;

  // Clear
  ctx.fillStyle = '#0a0a0a';
  ctx.fillRect(0, 0, w, h);

  // No data check
  if (client.perf.size === 0) {
    ctx.fillStyle = '#333';
    ctx.font = '11px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('No performance data', w / 2, h / 2);
    return;
  }

  if (visibleSeries.length === 0) {
    ctx.fillStyle = '#333';
    ctx.font = '11px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('All series hidden', w / 2, h / 2);
    return;
  }

  // Label area on the right
  const labelWidth = 140;
  const graphWidth = w - labelWidth - 10;

  visibleSeries.forEach(({ key, series }, laneIndex) => {
    const samples = series.samples;
    const count = samples.length;

    // Calculate lane bounds
    const laneTop = laneIndex * laneHeight;
    const laneCenter = laneTop + laneHeight / 2;
    const laneAmplitude = laneHeight * 0.38;

    // Draw lane separator
    if (laneIndex > 0) {
      ctx.strokeStyle = '#333';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, laneTop + 0.5);
      ctx.lineTo(w, laneTop + 0.5);
      ctx.stroke();
    }

    // Calculate value range with padding (like graphview.cpp)
    let minVal = series.smoothMin;
    let maxVal = series.smoothMax;
    const range = maxVal - minVal;

    // Ensure minimum range
    if (range < 0.001) {
      const center = (minVal + maxVal) / 2;
      minVal = center - 0.5;
      maxVal = center + 0.5;
    }

    const valueRange = maxVal - minVal;
    const currentVal = count > 0 ? samples[count - 1] : 0;

    // Draw center line for this lane
    ctx.strokeStyle = '#262626';
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 3]);
    ctx.beginPath();
    ctx.moveTo(0, laneCenter + 0.5);
    ctx.lineTo(graphWidth, laneCenter + 0.5);
    ctx.stroke();
    ctx.setLineDash([]);

    // Draw min/max reference lines
    ctx.strokeStyle = '#1a1a1a';
    ctx.lineWidth = 1;
    const topLine = laneTop + laneHeight * 0.1;
    const botLine = laneTop + laneHeight * 0.9;
    ctx.beginPath();
    ctx.moveTo(0, topLine + 0.5);
    ctx.lineTo(graphWidth, topLine + 0.5);
    ctx.moveTo(0, botLine + 0.5);
    ctx.lineTo(graphWidth, botLine + 0.5);
    ctx.stroke();

    // Draw label area background (right side)
    ctx.fillStyle = 'rgba(10,10,10,0.9)';
    ctx.fillRect(graphWidth + 5, laneTop + 2, labelWidth, laneHeight - 4);

    // Draw series name and current value on same line (right side, left-justified)
    const labelX = graphWidth + 10;
    ctx.font = '11px monospace';
    ctx.textAlign = 'left';
    ctx.fillStyle = series.color;
    ctx.fillText(series.sub, labelX, laneTop + 18);

    ctx.font = 'bold 14px monospace';
    ctx.fillStyle = '#fff';
    ctx.fillText(formatValue(currentVal), labelX, laneTop + 36);

    // Draw min/max on second row
    ctx.font = '10px monospace';
    ctx.fillStyle = '#6a6';
    ctx.fillText('min: ' + formatValue(minVal), labelX, laneTop + 52);
    ctx.fillStyle = '#a66';
    ctx.fillText('max: ' + formatValue(maxVal), labelX, laneTop + 66);

    if (count < 2) return;

    // Draw line - RIGHT JUSTIFIED (latest on right, grows to left)
    ctx.strokeStyle = series.color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();

    const valueCenter = (minVal + maxVal) / 2;

    for (let i = 0; i < count; i++) {
      // Right-justify: offset so rightmost sample is at x = graphWidth
      const xOffset = GRAPH_MAX_SAMPLES - count;
      const x = ((i + xOffset) / (GRAPH_MAX_SAMPLES - 1)) * graphWidth;

      const normalized = (samples[i] - valueCenter) / valueRange;  // -0.5 to 0.5
      const y = laneCenter - normalized * laneAmplitude * 2;

      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
  });
}

let graphLoops = new Map();  // clientId -> requestAnimationFrame ID

function startGraphLoop(clientId) {
  // Cancel any existing loop
  if (graphLoops.has(clientId)) {
    cancelAnimationFrame(graphLoops.get(clientId));
  }

  let lastTime = 0;
  const interval = 1000 / GRAPH_FPS;

  function loop(timestamp) {
    if (!selectedClients.has(clientId)) {
      graphLoops.delete(clientId);
      return;
    }

    if (timestamp - lastTime >= interval) {
      drawGraph(clientId);
      lastTime = timestamp;
    }

    graphLoops.set(clientId, requestAnimationFrame(loop));
  }

  graphLoops.set(clientId, requestAnimationFrame(loop));
}

// Log entries
function matchesFilter(client, entry) {
  const text = entry.ch + ' ' + (entry.sub || '') + ' ' + entry.msg;
  if (client.includeRegex && !client.includeRegex.test(text)) return false;
  if (client.excludeRegex && client.excludeRegex.test(text)) return false;
  return true;
}

function applyFilters(clientId) {
  const client = clients.get(clientId);
  if (!client || !client.panel) return;

  const logDiv = client.panel.querySelector('.panel-log');
  const entries = logDiv.querySelectorAll('.entry');
  let visible = 0;

  entries.forEach((el, idx) => {
    const entry = client.entries[idx];
    const channelInfo = client.channels.get(entry.ch);
    const channelVisible = channelInfo ? channelInfo.visible : true;
    const filterMatch = matchesFilter(client, entry);
    const hidden = !channelVisible || !filterMatch;
    el.classList.toggle('hidden', hidden);
    if (!hidden) visible++;
  });

  updateEntryCount(clientId, visible, client.entries.length);
}

function updateEntryCount(clientId, visible, total) {
  const client = clients.get(clientId);
  if (!client || !client.panel) return;
  client.panel.querySelector('.entry-count').textContent = `${visible} / ${total}`;
}

function renderEntries(clientId) {
  const client = clients.get(clientId);
  if (!client || !client.panel) return;

  const logDiv = client.panel.querySelector('.panel-log');
  logDiv.innerHTML = '';

  let visible = 0;
  client.entries.forEach(entry => {
    const div = createEntryElement(client, entry);
    logDiv.appendChild(div);
    if (!div.classList.contains('hidden')) visible++;
  });

  updateEntryCount(clientId, visible, client.entries.length);
}

function createEntryElement(client, entry) {
  const div = document.createElement('div');
  let cls = 'entry';
  if (entry.level === 'warn') cls += ' warn';
  if (entry.level === 'error') cls += ' error';
  div.className = cls;
  div.style.borderLeftColor = entry.color || '#888';

  let html = `<span class="ts">${esc(entry.ts)}</span>`;
  html += `<span class="ch" style="color:${entry.color || '#888'}">[${esc(entry.ch)}]</span>`;
  if (entry.sub) html += `<span class="sub">${esc(entry.sub)}:</span>`;
  html += esc(entry.msg);
  div.innerHTML = html;

  const channelInfo = client.channels.get(entry.ch);
  const channelVisible = channelInfo ? channelInfo.visible : true;
  const filterMatch = matchesFilter(client, entry);
  if (!channelVisible || !filterMatch) div.classList.add('hidden');

  return div;
}

function addEntry(clientId, entry) {
  const client = clients.get(clientId);
  if (!client) return;

  // Register channel if new
  if (!client.channels.has(entry.ch)) {
    client.channels.set(entry.ch, { visible: true, color: entry.color || '#888' });
    if (client.panel) {
      updateChannelToggles(clientId);
    }
  }

  client.entries.push(entry);

  // Limit entries
  if (client.entries.length > 10000) {
    client.entries.shift();
  }

  // If panel is visible, add to DOM
  if (client.panel) {
    const logDiv = client.panel.querySelector('.panel-log');
    const div = createEntryElement(client, entry);
    logDiv.appendChild(div);

    // Remove oldest if too many
    if (logDiv.children.length > 10000) {
      logDiv.removeChild(logDiv.firstChild);
    }

    const isVisible = !div.classList.contains('hidden');
    const countSpan = client.panel.querySelector('.entry-count');
    const [vis, tot] = countSpan.textContent.split(' / ').map(s => parseInt(s));
    updateEntryCount(clientId, isVisible ? vis + 1 : vis, tot + 1);

    if (client.autoScroll && isVisible) {
      logDiv.scrollTop = logDiv.scrollHeight;
    }
  }
}

// Event handlers
document.getElementById('btn-select-all').onclick = selectAllClients;
document.getElementById('btn-select-none').onclick = selectNoClients;

// SSE connection
const evtSource = new EventSource('/events');
evtSource.onmessage = (event) => {
  try {
    const data = JSON.parse(event.data);
    const clientId = getClientId(data.app, data.pid);

    switch (data.type) {
      case 'register':
        registerClient(data.app, data.pid, data.host);
        break;
      case 'heartbeat':
        registerClient(data.app, data.pid, data.host);
        pulseHeartbeat(clientId);
        break;
      case 'disconnect':
        markClientDead(data.app, data.pid);
        break;
      case 'log':
        registerClient(data.app, data.pid, data.host);
        addEntry(clientId, data);
        break;
      case 'status':
        registerClient(data.app, data.pid, data.host);
        addStatus(clientId, data);
        break;
      case 'perf':
        registerClient(data.app, data.pid, data.host);
        addPerfSample(clientId, data);
        break;
    }
  } catch (e) {
    console.error('Failed to parse message:', e);
  }
};
evtSource.onerror = () => {
  console.log('SSE connection error, will retry...');
};
</script>
</body>
</html>
'''


class LogHandler(BaseHTTPRequestHandler):
    """HTTP request handler with SSE support."""

    def log_message(self, format, *args):
        pass  # Suppress HTTP access logs

    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
            self.send_header('Pragma', 'no-cache')
            self.send_header('Expires', '0')
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode('utf-8'))

        elif self.path == '/events':
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

            # Create queue for this connection
            q = queue.Queue()
            with sse_queues_lock:
                sse_queues.append(q)

            try:
                # Send current client states
                with clients_lock:
                    for client_id, client in clients.items():
                        msg = json.dumps({
                            "type": "register",
                            "app": client["app"],
                            "pid": client["pid"]
                        })
                        self.wfile.write(f"data: {msg}\n\n".encode('utf-8'))

                        # Send recent entries
                        for entry in client["entries"][-1000:]:
                            self.wfile.write(f"data: {json.dumps(entry)}\n\n".encode('utf-8'))
                    self.wfile.flush()

                # Stream new messages
                while True:
                    try:
                        msg = q.get(timeout=1.0)
                        self.wfile.write(f"data: {msg}\n\n".encode('utf-8'))
                        self.wfile.flush()
                    except queue.Empty:
                        # Send keepalive
                        try:
                            self.wfile.write(b": keepalive\n\n")
                            self.wfile.flush()
                        except BrokenPipeError:
                            break
                    except BrokenPipeError:
                        break
            finally:
                with sse_queues_lock:
                    sse_queues.remove(q)
        else:
            self.send_response(404)
            self.end_headers()


def broadcast_message(msg):
    """Send message to all connected browsers."""
    with sse_queues_lock:
        for q in sse_queues:
            try:
                q.put_nowait(msg)
            except queue.Full:
                pass


def zmq_subscriber(zmq_port):
    """Subscribe to ZMQ and process messages."""
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.bind(f"tcp://0.0.0.0:{zmq_port}")  # BIND - server waits for clients
    socket.setsockopt_string(zmq.SUBSCRIBE, "")  # Subscribe to all

    print(f"ZMQ listening on port {zmq_port}")

    while True:
        try:
            message = socket.recv_string()
            data = json.loads(message)

            app = data.get("app", "unknown")
            pid = data.get("pid", 0)
            msg_type = data.get("type", "log")
            client_id = f"{app}:{pid}"

            if msg_type in ("register", "disconnect"):
                print(f"[ZMQ] {msg_type}: {app} [{pid}]")

            with clients_lock:
                if msg_type in ("register", "heartbeat", "log", "status", "perf"):
                    if client_id not in clients:
                        clients[client_id] = {
                            "app": app,
                            "pid": pid,
                            "last_seen": time.time(),
                            "entries": [],
                            "channels": {}
                        }
                    clients[client_id]["last_seen"] = time.time()

                    if msg_type == "log":
                        # Store entry
                        clients[client_id]["entries"].append(data)
                        # Limit stored entries
                        if len(clients[client_id]["entries"]) > 10000:
                            clients[client_id]["entries"].pop(0)

                elif msg_type == "disconnect":
                    # Mark as dead but keep data
                    if client_id in clients:
                        clients[client_id]["last_seen"] = 0

            # Broadcast to browsers
            broadcast_message(message)

        except json.JSONDecodeError as e:
            print(f"Invalid JSON: {e}")
        except zmq.ZMQError:
            break


def cleanup_thread():
    """Periodically remove dead clients."""
    while True:
        time.sleep(CLIENT_TIMEOUT)
        now = time.time()
        with clients_lock:
            for client_id, client in list(clients.items()):
                if client["last_seen"] > 0 and (now - client["last_seen"]) > CLIENT_TIMEOUT:
                    # Mark as dead
                    client["last_seen"] = 0
                    msg = json.dumps({
                        "type": "disconnect",
                        "app": client["app"],
                        "pid": client["pid"]
                    })
                    broadcast_message(msg)


def main():
    http_port = DEFAULT_HTTP_PORT
    zmq_port = DEFAULT_ZMQ_PORT

    if len(sys.argv) >= 2:
        http_port = int(sys.argv[1])
    if len(sys.argv) >= 3:
        zmq_port = int(sys.argv[2])

    print(f"Orkid Log Server")
    # Get all network interface addresses
    import socket
    ips = ['127.0.0.1']
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ip = info[4][0]
            if ip != '127.0.0.1' and ip not in ips:
                ips.append(ip)
    except:
        pass
    print(f"  ZMQ endpoints:")
    for ip in ips:
        print(f"    tcp://{ip}:{zmq_port}")
    print(f"  HTTP endpoints:")
    for ip in ips:
        print(f"    http://{ip}:{http_port}")

    # Start ZMQ subscriber thread
    zmq_thread = threading.Thread(target=zmq_subscriber, args=(zmq_port,), daemon=True)
    zmq_thread.start()

    # Start cleanup thread
    cleanup = threading.Thread(target=cleanup_thread, daemon=True)
    cleanup.start()

    # Start HTTP server
    server = HTTPServer(('0.0.0.0', http_port), LogHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
    server.server_close()


if __name__ == '__main__':
    main()
