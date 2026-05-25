#include "create_panel_html.h"

std::string BuildCreatePanelHTML(bool darkMode) {
    const char* bodyClass = darkMode ? "dark" : "";

    return R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<style>
*{box-sizing:border-box;margin:0;padding:0}

:root{
  --bg:#ffffff;--surface:#f6f8fa;--border:#d0d7de;
  --text:#1f2328;--muted:#656d76;--accent:#0969da;
  --accent-fg:#ffffff;--btn:#f6f8fa;--btn-border:#d0d7de;
  --btn-hover:#e9ecef;--danger:#d1242f;
  --in-bg:#ffffff;--in-border:#d0d7de;
  --sel-bg:#dbeafe;--sel-text:#1d4ed8;
}
.dark{
  --bg:#0d1117;--surface:#161b22;--border:#30363d;
  --text:#e6edf3;--muted:#8b949e;--accent:#58a6ff;
  --accent-fg:#0d1117;--btn:#21262d;--btn-border:#30363d;
  --btn-hover:#30363d;--danger:#f85149;
  --in-bg:#0d1117;--in-border:#30363d;
  --sel-bg:#1d3a6e;--sel-text:#93c5fd;
}

body{background:var(--bg);color:var(--text);
     font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
     font-size:13px;line-height:1.5;overflow-y:scroll}

#panel{padding:16px;max-width:960px}

hr{border:none;border-top:1px solid var(--border);margin:12px 0}

.row{display:flex;align-items:center;gap:6px;flex-wrap:wrap}
.col{display:flex;flex-direction:column}

.label{color:var(--muted);font-size:12px;font-weight:500;
       white-space:nowrap;min-width:110px}
.big-label{font-size:18px;font-weight:700;margin-bottom:8px}
.sec-label{font-size:12px;font-weight:600;margin-bottom:4px}
.path-text{font-size:11px;color:var(--muted);margin-top:3px;
           min-height:14px;word-break:break-all}

.hidden{display:none!important}
.mt4{margin-top:4px}
.mt6{margin-top:6px}
.mb4{margin-bottom:4px}

input,select,textarea{
  background:var(--in-bg);color:var(--text);
  border:1px solid var(--in-border);border-radius:6px;
  padding:4px 8px;font-size:13px;font-family:inherit;outline:none}
input:focus,select:focus,textarea:focus{
  border-color:var(--accent);
  box-shadow:0 0 0 3px rgba(9,105,218,.15)}
.dark input:focus,.dark select:focus,.dark textarea:focus{
  box-shadow:0 0 0 3px rgba(88,166,255,.15)}
textarea{resize:vertical;width:100%}

.inp-full{width:100%}
.inp-200{width:200px}
.inp-220{width:220px}
.inp-280{width:280px}
.inp-sm{width:130px;font-size:12px}

.textarea-lg{min-height:80px;margin-top:4px}
.textarea-md{min-height:56px;margin-top:4px}

button{border-radius:6px;font-size:12px;font-family:inherit;
       font-weight:500;cursor:pointer;white-space:nowrap;
       border:1px solid var(--btn-border);background:var(--btn);color:var(--text)}
button:hover{background:var(--btn-hover)}
button:disabled{opacity:.5;cursor:default}

.btn{padding:5px 14px;font-size:13px}
.btn-sm{padding:3px 10px}
.btn-xs{padding:2px 7px;font-size:11px}

.btn-primary{background:var(--accent);color:var(--accent-fg);border-color:var(--accent)}
.btn-primary:hover{filter:brightness(1.08)}

.btn-danger{color:var(--danger);border-color:var(--danger);background:transparent}
.btn-danger:hover{background:rgba(209,36,47,.08)}

/* Character panel */
.char-panel{display:flex;gap:10px;align-items:flex-start}
.cat-col{display:flex;flex-direction:column;flex:0 0 148px}
.char-col{display:flex;flex-direction:column;flex:1;min-width:0}

.item-list{border:1px solid var(--border);border-radius:6px;
           overflow-y:auto;min-height:110px;max-height:130px;
           background:var(--surface)}

.cat-item{padding:4px 10px;cursor:pointer;user-select:none;
          font-size:13px;border-bottom:1px solid var(--border)}
.cat-item:last-child{border-bottom:none}
.cat-item:hover{background:var(--btn-hover)}
.cat-item.active{background:var(--sel-bg);color:var(--sel-text);font-weight:500}

.char-item{display:flex;align-items:center;gap:7px;padding:3px 10px;
           cursor:pointer;font-size:13px;border-bottom:1px solid var(--border)}
.char-item:last-child{border-bottom:none}
.char-item:hover{background:var(--btn-hover)}
.char-item input[type=checkbox]{cursor:pointer;margin:0}

/* Character description textareas */
.char-desc-list{margin-top:8px;display:flex;flex-direction:column;gap:6px}
.char-desc-row{display:flex;flex-direction:column;gap:2px}
.char-desc-label{font-size:12px;font-weight:500;color:var(--muted)}
.char-desc-ta{min-height:40px;font-size:12px;resize:vertical}

/* File table */
.files-layout{display:flex;gap:8px;align-items:flex-start}
.table-wrap{flex:1;border:1px solid var(--border);border-radius:6px;
            max-height:130px;overflow:auto}

#fileTable{width:100%;border-collapse:collapse;font-size:12px}
#fileTable th{background:var(--surface);padding:5px 10px;text-align:left;
              font-weight:500;color:var(--muted);border-bottom:1px solid var(--border);
              position:sticky;top:0;z-index:1}
#fileTable td{padding:4px 10px;border-bottom:1px solid var(--border)}
#fileTable tr:last-child td{border-bottom:none}
#fileBody tr{cursor:pointer}
#fileBody tr:hover{background:var(--surface)}
#fileBody tr.selected{background:var(--sel-bg);color:var(--sel-text)}

.file-btns{display:flex;flex-direction:column;gap:4px;flex-shrink:0}

/* Status */
.status-bar{font-size:12px;color:var(--muted);padding-top:8px;
            border-top:1px solid var(--border);margin-top:10px;white-space:pre-wrap}
</style>
</head>
<body class=")HTML"
    + std::string(bodyClass)
    + R"HTML(">
<div id="panel">

<!-- ── Project ─────────────────────────────────── -->
<div class="row mb4">
  <span class="label">Project</span>
  <select id="projectChoice" class="inp-200" onchange="onProjectChange()"></select>
  <button class="btn-sm" onclick="toggleNewProject()">New…</button>
</div>
<div id="projectPath" class="path-text"></div>
<div id="newProjectRow" class="row mt4 hidden">
  <input id="newProjectInput" class="inp-sm" placeholder="Project name…"
         onkeydown="if(event.key==='Enter')submitNewProject()">
  <button class="btn-sm btn-primary" onclick="submitNewProject()">Create</button>
  <button class="btn-sm" onclick="toggleNewProject()">Cancel</button>
</div>

<hr>

<!-- ── Topic ────────────────────────────────────── -->
<div class="big-label">What do you want to learn?</div>
<textarea id="topic" class="textarea-lg"
  placeholder="Describe your topic — be as specific or broad as you like…"></textarea>

<!-- ── Project context ──────────────────────────── -->
<div class="row mt6 mb4">
  <span class="label">Project context</span>
  <button class="btn-sm" onclick="saveContext()">Save</button>
</div>
<textarea id="context" class="textarea-md"
  placeholder="Describe this project — characters, setting, tone. Prepended to every generation."></textarea>

<hr>

<!-- ── Style ────────────────────────────────────── -->
<div class="row">
  <span class="label">Style</span>
  <input id="style" list="styleList" class="inp-200" value="Academic essay">
  <datalist id="styleList">
    <option>Academic essay</option>
    <option>Children's book</option>
    <option>Crime noir</option>
    <option>Fairy tale</option>
    <option>Horror</option>
    <option>Long-form essay</option>
    <option>Podcast transcript</option>
    <option>Popular science</option>
    <option>Socratic dialogue</option>
    <option>Tech blog post</option>
  </datalist>
</div>
<div class="row mt4">
  <span class="label">Tidbits / chapter</span>
  <input type="number" id="tidbitsPerChapter" min="0" max="10" value="1"
         style="width:60px;text-align:center">
  <span style="font-size:12px;color:var(--muted)">(0 = none)</span>
</div>

<hr>

<!-- ── Characters ───────────────────────────────── -->
<div class="sec-label">Tidbit characters</div>
<div class="char-panel">
  <div class="cat-col">
    <div id="catList" class="item-list"></div>
    <div class="row mt4">
      <input id="newCatInput" class="inp-sm" placeholder="Category…"
             onkeydown="if(event.key==='Enter')addCategory()">
      <button class="btn-xs" onclick="addCategory()" title="Add">+</button>
      <button class="btn-xs btn-danger" onclick="deleteCategory()" title="Delete">✕</button>
    </div>
  </div>
  <div class="char-col">
    <div id="charList" class="item-list"></div>
    <div class="row mt4">
      <input id="newCharInput" class="inp-sm" placeholder="Character…"
             onkeydown="if(event.key==='Enter')addCharacter()">
      <button class="btn-xs" onclick="addCharacter()" title="Add">+</button>
      <button class="btn-xs btn-danger" onclick="deleteCharacter()" title="Delete">✕</button>
    </div>
  </div>
</div>
<div id="charDescList" class="char-desc-list"></div>

<hr>

<!-- ── Backend ──────────────────────────────────── -->
<div class="row">
  <span class="label">LLM backend</span>
  <select id="backend" onchange="onBackendChange()">
    <option>Clipboard (manual)</option>
    <option>claude -p</option>
    <option>Codex CLI</option>
    <option>Gemini CLI</option>
    <option>Ollama (local)</option>
    <option>Anthropic API</option>
  </select>
</div>
<div id="apiKeyRow" class="row mt4 hidden">
  <span class="label">API key</span>
  <input type="password" id="apiKey" class="inp-280">
</div>
<div id="ollamaRow" class="row mt4 hidden">
  <span class="label">Ollama model</span>
  <input id="ollamaModel" list="ollamaModelList" class="inp-220" value="llama3">
  <datalist id="ollamaModelList"></datalist>
  <button class="btn-sm" onclick="send({action:'refreshOllama'})">Refresh</button>
</div>

<hr>

<!-- ── Actions ──────────────────────────────────── -->
<div class="row">
  <button id="generateBtn" class="btn btn-primary" onclick="generate()">Generate</button>
  <button class="btn" onclick="copyPrompt()">Copy Prompt</button>
  <span style="flex:1"></span>
  <button class="btn" onclick="saveState()">Save</button>
</div>

<hr>

<!-- ── Files in project ─────────────────────────── -->
<div class="sec-label mb4">Files in project</div>
<div class="files-layout">
  <div class="table-wrap">
    <table id="fileTable">
      <thead><tr><th>File</th><th>Language</th><th>Created</th><th>Updated</th></tr></thead>
      <tbody id="fileBody"></tbody>
    </table>
  </div>
  <div class="file-btns">
    <button class="btn-sm" onclick="openFile()">Open in View</button>
    <button id="translateBtn" class="btn-sm" onclick="showTranslate()">Translate…</button>
    <button class="btn-sm btn-danger" onclick="deleteFile()">Delete</button>
  </div>
</div>
<div id="translateRow" class="row mt6 hidden">
  <select id="translateLang">
    <option>English</option>
    <option>Spanish</option>
    <option>French</option>
    <option>German</option>
    <option>Italian</option>
    <option>Portuguese</option>
    <option>Japanese</option>
    <option>Korean</option>
    <option>Chinese (Mandarin)</option>
    <option>Arabic</option>
    <option>Russian</option>
  </select>
  <button class="btn-sm btn-primary" onclick="translateFile()">Translate</button>
  <button class="btn-sm" onclick="hideTranslate()">Cancel</button>
</div>

<hr>
<div id="status" class="status-bar">Ready.</div>

</div><!-- #panel -->
<script>
'use strict';

function send(obj) {
    try { window.webkit.messageHandlers.create.postMessage(JSON.stringify(obj)); }
    catch(err) { console.error('create send:', err); }
}

// HTML-escape a value for use in attribute or text content
function e(s) {
    return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;')
                    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
// JS-string-escape a value for use inside a single-quoted JS literal
function ej(s) {
    return String(s).replace(/\\/g,'\\\\').replace(/'/g,"\\'")
                    .replace(/\n/g,'\\n').replace(/\r/g,'');
}

// ── Init ──────────────────────────────────────────
document.addEventListener('DOMContentLoaded', function() {
    send({action:'ready'});
});

// ── Project ───────────────────────────────────────
function onProjectChange() {
    send({action:'selectProject', name: document.getElementById('projectChoice').value});
}
function toggleNewProject() {
    const row = document.getElementById('newProjectRow');
    row.classList.toggle('hidden');
    if (!row.classList.contains('hidden'))
        document.getElementById('newProjectInput').focus();
}
function submitNewProject() {
    const name = document.getElementById('newProjectInput').value.trim();
    if (!name) return;
    document.getElementById('newProjectInput').value = '';
    toggleNewProject();
    send({action:'newProject', name});
}

// ── Context ───────────────────────────────────────
function saveContext() {
    send({action:'saveContext', text: document.getElementById('context').value});
}

// ── Backend ───────────────────────────────────────
function onBackendChange() {
    const b = document.getElementById('backend').value;
    document.getElementById('apiKeyRow').classList.toggle('hidden', b !== 'Anthropic API');
    document.getElementById('ollamaRow').classList.toggle('hidden', b !== 'Ollama (local)');
    send({action:'backendChanged', backend: b});
}

// ── Generate / Prompt ─────────────────────────────
function formData() {
    return {
        topic:              document.getElementById('topic').value,
        style:              document.getElementById('style').value,
        context:            document.getElementById('context').value,
        backend:            document.getElementById('backend').value,
        apiKey:             document.getElementById('apiKey').value,
        ollamaModel:        document.getElementById('ollamaModel').value,
        tidbitsPerChapter:  parseInt(document.getElementById('tidbitsPerChapter').value) || 0
    };
}
function generate()   { send(Object.assign({action:'generate'},   formData())); }
function copyPrompt() { send(Object.assign({action:'copyPrompt'}, formData())); }
function saveState() {
    const d = formData();
    send({action:'saveState', topic:d.topic, style:d.style,
          backend:d.backend, apiKey:d.apiKey, ollamaModel:d.ollamaModel});
}

// ── Characters ────────────────────────────────────
let _cat = '';

function selectCat(name) {
    _cat = name;
    document.querySelectorAll('.cat-item').forEach(function(el) {
        el.classList.toggle('active', el.dataset.name === name);
    });
    send({action:'selectCategory', name});
}
function addCategory() {
    const inp = document.getElementById('newCatInput');
    const name = inp.value.trim();
    if (!name) return;
    inp.value = '';
    send({action:'addCategory', name});
}
function deleteCategory() {
    if (_cat) send({action:'deleteCategory', name:_cat});
}
function addCharacter() {
    const inp = document.getElementById('newCharInput');
    const name = inp.value.trim();
    if (!name) return;
    inp.value = '';
    send({action:'addCharacter', category:_cat, name});
}
function deleteCharacter() {
    const chk = document.querySelector('#charList input[type=checkbox]:checked');
    if (chk) send({action:'deleteCharacter', category:_cat, name:chk.dataset.name});
}
function toggleChar(name, checked) {
    send({action:'toggleCharacter', name:name, checked:checked});
}

// ── Files ─────────────────────────────────────────
let _file = '';

function selectFile(name) {
    _file = name;
    document.querySelectorAll('#fileBody tr').forEach(function(tr) {
        tr.classList.toggle('selected', tr.dataset.name === name);
    });
}
function openFile() {
    if (!_file) { setStatus('Select a file first.'); return; }
    send({action:'openFile', name:_file});
}
function showTranslate() {
    if (!_file) { setStatus('Select a file to translate first.'); return; }
    document.getElementById('translateRow').classList.remove('hidden');
}
function hideTranslate() {
    document.getElementById('translateRow').classList.add('hidden');
}
function translateFile() {
    const lang = document.getElementById('translateLang').value;
    const d = formData();
    send({action:'translateFile', name:_file, language:lang,
          backend:d.backend, apiKey:d.apiKey, ollamaModel:d.ollamaModel});
    hideTranslate();
}
function deleteFile() {
    if (!_file) { setStatus('Select a file to delete first.'); return; }
    send({action:'deleteFile', name:_file});
}

// ── C++ → JS state updates ────────────────────────
function setProjectList(projects, selected, path) {
    var sel = document.getElementById('projectChoice');
    sel.innerHTML = projects.map(function(p) {
        return '<option value="' + e(p) + '"' + (p===selected?' selected':'') + '>' + e(p) + '</option>';
    }).join('');
    document.getElementById('projectPath').textContent = path || '';
}

function setChapterList(files) {
    _file = '';
    document.getElementById('fileBody').innerHTML = files.map(function(f) {
        return '<tr data-name="' + e(f.name) + '"'
             + ' onclick="selectFile(\'' + ej(f.name) + '\')"'
             + ' ondblclick="send({action:\'openFile\',name:\'' + ej(f.name) + '\'})">'
             + '<td>' + e(f.name) + '</td>'
             + '<td>' + e(f.language) + '</td>'
             + '<td>' + e(f.created) + '</td>'
             + '<td>' + e(f.updated) + '</td>'
             + '</tr>';
    }).join('');
}

function setContext(text) {
    document.getElementById('context').value = text;
}

function setCharDescription(name, desc) {
    send({action:'setCharDescription', name:name, description:desc});
}

function setCharLibrary(data) {
    _cat = data.selected || '';
    document.getElementById('catList').innerHTML = data.categories.map(function(c) {
        return '<div class="cat-item' + (c===_cat?' active':'') + '" data-name="' + e(c)
             + '" onclick="selectCat(\'' + ej(c) + '\')">' + e(c) + '</div>';
    }).join('');
    document.getElementById('charList').innerHTML = data.chars.map(function(c) {
        return '<label class="char-item">'
             + '<input type="checkbox" data-name="' + e(c.name) + '"'
             + (c.checked?' checked':'')
             + ' onchange="toggleChar(\'' + ej(c.name) + '\',this.checked)">'
             + '<span>' + e(c.name) + '</span></label>';
    }).join('');

    // Description textareas for checked characters
    var checked = data.chars.filter(function(c) { return c.checked; });
    var descEl = document.getElementById('charDescList');
    if (!descEl) return;
    if (checked.length === 0) {
        descEl.innerHTML = '';
        return;
    }
    descEl.innerHTML = checked.map(function(c) {
        return '<div class="char-desc-row">'
             + '<div class="char-desc-label">' + e(c.name) + '</div>'
             + '<textarea class="char-desc-ta" data-char="' + e(c.name) + '"'
             + ' placeholder="Who is ' + e(c.name) + '? (optional — e.g. role, era, expertise)"'
             + ' onblur="setCharDescription(\'' + ej(c.name) + '\',this.value)">'
             + e(c.description || '')
             + '</textarea>'
             + '</div>';
    }).join('');
}

function setStatus(msg) {
    document.getElementById('status').textContent = msg;
}

function setGenerating(on) {
    var btn = document.getElementById('generateBtn');
    btn.disabled = on;
    btn.textContent = on ? 'Generating…' : 'Generate';
}

function setTranslating(on) {
    var btn = document.getElementById('translateBtn');
    btn.disabled = on;
    btn.textContent = on ? 'Translating…' : 'Translate…';
}

function setOllamaModels(models) {
    document.getElementById('ollamaModelList').innerHTML =
        models.map(function(m) { return '<option value="' + e(m) + '">'; }).join('');
}

function setDarkMode(on) {
    document.body.classList.toggle('dark', on);
}

function restoreFormState(s) {
    if (s.topic)       document.getElementById('topic').value = s.topic;
    if (s.style)       document.getElementById('style').value = s.style;
    if (s.backend) {
        document.getElementById('backend').value = s.backend;
        onBackendChange();
    }
    if (s.apiKey)      document.getElementById('apiKey').value = s.apiKey;
    if (s.ollamaModel) document.getElementById('ollamaModel').value = s.ollamaModel;
}
</script>
</body>
</html>
)HTML";
}
