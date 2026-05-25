#include "monitor_panel_html.h"
#include <string>

std::string BuildMonitorPanelHTML(bool darkMode) {
    const char* bodyClass = darkMode ? "dark" : "";
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<style>
:root {
  --bg: #f5f5f5; --surface: #fff; --border: #ddd;
  --text: #222; --muted: #666; --accent: #0066cc;
  --danger: #cc0000; --danger-hover: #990000;
  --orphan-bg: #fff0f0; --orphan-text: #cc0000;
  --running-text: #007700;
  --row-hover: #f0f4ff;
}
.dark {
  --bg: #1e1e1e; --surface: #2a2a2a; --border: #444;
  --text: #e0e0e0; --muted: #999; --accent: #4d9fff;
  --danger: #ff5555; --danger-hover: #ff2222;
  --orphan-bg: #2a1a1a; --orphan-text: #ff6666;
  --running-text: #55cc55;
  --row-hover: #2a2a3a;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  font-family: -apple-system, BlinkMacSystemFont, sans-serif;
  font-size: 13px; background: var(--bg); color: var(--text);
  padding: 16px;
}
h2 { font-size: 15px; font-weight: 600; margin-bottom: 12px; }
.toolbar { display: flex; align-items: center; gap: 10px; margin-bottom: 14px; }
button {
  padding: 5px 12px; border-radius: 4px; border: 1px solid var(--border);
  background: var(--surface); color: var(--text); cursor: pointer; font-size: 12px;
}
button:hover { background: var(--row-hover); }
.kill-btn {
  background: var(--danger); color: #fff; border-color: var(--danger);
  padding: 3px 10px;
}
.kill-btn:hover { background: var(--danger-hover); border-color: var(--danger-hover); }
.muted { color: var(--muted); font-size: 12px; }
table { width: 100%; border-collapse: collapse; background: var(--surface);
        border: 1px solid var(--border); border-radius: 6px; overflow: hidden; }
th { background: var(--bg); text-align: left; padding: 8px 10px;
     font-weight: 600; border-bottom: 1px solid var(--border); }
td { padding: 8px 10px; border-bottom: 1px solid var(--border); vertical-align: middle; }
tr:last-child td { border-bottom: none; }
tr:hover td { background: var(--row-hover); }
tr.orphaned td { background: var(--orphan-bg); }
.status-orphaned { color: var(--orphan-text); font-weight: 600; }
.status-running  { color: var(--running-text); }
.empty { padding: 24px; text-align: center; color: var(--muted);
         background: var(--surface); border: 1px solid var(--border); border-radius: 6px; }
#lastChecked { color: var(--muted); font-size: 11px; }
.timing-log { margin-top: 24px; }
.timing-log h2 { margin-bottom: 10px; }
.badge {
  display: inline-block; padding: 1px 7px; border-radius: 10px;
  font-size: 11px; font-weight: 600; background: var(--accent);
  color: #fff; white-space: nowrap;
}
.secs { font-variant-numeric: tabular-nums; color: var(--text); }
.archived-row { opacity: 0.45; }
.btn-archive { font-size: 11px; padding: 1px 7px; cursor: pointer;
               border:1px solid var(--border); border-radius: 4px;
               background: transparent; color: var(--muted); }
.btn-archive:hover { background: var(--border); }
.show-archived-row { margin-bottom: 6px; font-size: 12px; color: var(--muted); }
</style>
</head>
<body class=")HTML" + std::string(bodyClass) + R"HTML(">
<h2>LLM Process Monitor</h2>
<div class="toolbar">
  <button onclick="refresh()">Refresh</button>
  <span id="lastChecked"></span>
</div>
<div id="content"><div class="empty">Loading…</div></div>

<div class="timing-log">
  <h2>Timing Log</h2>
  <div class="show-archived-row">
    <label><input type="checkbox" id="showArchived" onchange="renderTimingLog()"> Show archived</label>
  </div>
  <div id="timing-log"><div class="empty">No timing data yet.</div></div>
</div>

<script>
function send(obj) {
  window.webkit.messageHandlers.monitor.postMessage(JSON.stringify(obj));
}

function refresh() {
  send({action: 'refresh'});
}

function kill(pid) {
  if (!confirm('Kill process ' + pid + '?')) return;
  send({action: 'kill', pid: pid});
}

function setProcessList(procs) {
  var now = new Date();
  document.getElementById('lastChecked').textContent =
    'Last checked: ' + now.toLocaleTimeString();

  var el = document.getElementById('content');
  if (!procs || procs.length === 0) {
    el.innerHTML = '<div class="empty">No LLM processes running.</div>';
    return;
  }

  var rows = procs.map(function(p) {
    var statusClass = p.orphaned ? 'status-orphaned' : 'status-running';
    var statusLabel = p.orphaned ? '⚠ Orphaned' : '● Running';
    var rowClass    = p.orphaned ? 'orphaned' : '';
    return '<tr class="' + rowClass + '">' +
      '<td>' + p.pid + '</td>' +
      '<td>' + escHtml(p.project || '—') + '</td>' +
      '<td>' + escHtml(p.backend || '—') + '</td>' +
      '<td>' + escHtml(p.action) + '</td>' +
      '<td>' + p.elapsed + '</td>' +
      '<td><span class="' + statusClass + '">' + statusLabel + '</span></td>' +
      '<td><button class="kill-btn" onclick="kill(' + p.pid + ')">Kill</button></td>' +
      '</tr>';
  }).join('');

  el.innerHTML =
    '<table>' +
    '<thead><tr><th>PID</th><th>Project</th><th>Backend</th><th>Action</th><th>Running for</th><th>Status</th><th></th></tr></thead>' +
    '<tbody>' + rows + '</tbody>' +
    '</table>';
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

var _timingEntries = [];

function setTimingLog(entries) {
  _timingEntries = entries || [];
  renderTimingLog();
}

function renderTimingLog() {
  var el = document.getElementById('timing-log');
  var showArchived = document.getElementById('showArchived').checked;
  var visible = _timingEntries.filter(function(e) {
    return showArchived || !e.archived;
  });
  if (visible.length === 0) {
    el.innerHTML = '<div class="empty">No completed LLM runs recorded yet.</div>';
    return;
  }
  var rows = visible.map(function(e) {
    var mins = Math.floor(e.secs / 60);
    var secs = e.secs % 60;
    var dur  = mins > 0 ? mins + 'm ' + secs + 's' : secs + 's';
    var archLabel = e.archived ? 'Unarchive' : 'Archive';
    var newVal    = e.archived ? 'false' : 'true';
    var rowClass  = e.archived ? 'archived-row' : '';
    return '<tr class="' + rowClass + '">' +
      '<td class="muted">' + escHtml(e.ts.replace('T',' ')) + '</td>' +
      '<td>' + escHtml(e.project) + '</td>' +
      '<td><span class="badge">' + escHtml(e.backend || '—') + '</span></td>' +
      '<td>' + escHtml(e.op) + '</td>' +
      '<td class="muted">' + escHtml(e.topic) + '</td>' +
      '<td class="secs">' + dur + '</td>' +
      '<td><button class="btn-archive" onclick="archiveEntry(\'' +
        escAttr(e.project) + '\',\'' + escAttr(e.ts) + '\',' + newVal + ')">' +
        archLabel + '</button></td>' +
      '</tr>';
  }).join('');
  el.innerHTML =
    '<table>' +
    '<thead><tr><th>When</th><th>Project</th><th>Backend</th><th>Operation</th><th>Topic</th><th>Duration</th><th></th></tr></thead>' +
    '<tbody>' + rows + '</tbody>' +
    '</table>';
}

function escAttr(s) {
  return String(s).replace(/\\/g,'\\\\').replace(/'/g,"\\'");
}

function archiveEntry(project, ts, archived) {
  send({action:'archiveTiming', project:project, ts:ts, archived:archived});
}

function setDarkMode(dark) {
  document.body.classList.toggle('dark', dark);
}

document.addEventListener('DOMContentLoaded', function() {
  send({action: 'ready'});
  setInterval(refresh, 5000);
});
</script>
</body>
</html>
)HTML";
}
