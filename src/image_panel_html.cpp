#include "image_panel_html.h"
#include <cstdio>
#include <string>

static std::string jpstr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c < 0x20)  { char b[8]; snprintf(b,8,"\\u%04x",c); o += b; }
        else                 o += (char)c;
    }
    return o + "\"";
}

std::string BuildImagePanelHTML(
    const std::vector<std::pair<std::string,std::string>>& images,
    const std::vector<std::string>& headings,
    bool darkMode)
{
    std::string imgsJS = "[";
    for (size_t i = 0; i < images.size(); ++i) {
        if (i) imgsJS += ",";
        imgsJS += "{\"name\":" + jpstr(images[i].first)
               + ",\"src\":"  + jpstr(images[i].second) + "}";
    }
    imgsJS += "]";

    std::string headsJS = "[";
    for (size_t i = 0; i < headings.size(); ++i) {
        if (i) headsJS += ",";
        headsJS += jpstr(headings[i]);
    }
    headsJS += "]";

    const char* cls = darkMode ? "dark" : "";

    return R"HTML(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#ffffff;--surface:#f6f8fa;--border:#d0d7de;
  --text:#1f2328;--muted:#656d76;--accent:#0969da;
  --btn:#f6f8fa;--btn-border:#d0d7de;--btn-hover:#e9ecef;
  --danger:#d1242f;
}
.dark{
  --bg:#0d1117;--surface:#161b22;--border:#30363d;
  --text:#e6edf3;--muted:#8b949e;--accent:#58a6ff;
  --btn:#21262d;--btn-border:#30363d;--btn-hover:#30363d;
  --danger:#f85149;
}
body{background:var(--bg);color:var(--text);
     font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
     font-size:13px;line-height:1.5;padding:16px}
.sec-title{font-size:11px;font-weight:600;color:var(--muted);
           text-transform:uppercase;letter-spacing:.04em;margin-bottom:8px}
.two-col{display:flex;gap:20px;align-items:flex-start}
.left-pane{flex:1;min-width:0}
.right-pane{flex:0 0 220px}
hr{border:none;border-top:1px solid var(--border);margin:14px 0}
/* Image grid */
.img-grid{display:flex;flex-wrap:wrap;gap:10px}
.img-card{
  width:110px;background:var(--surface);border:1px solid var(--border);
  border-radius:8px;overflow:hidden;position:relative}
.img-card img{width:110px;height:80px;object-fit:cover;display:block}
.img-card .img-name{
  font-size:11px;color:var(--text);padding:4px 6px;
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.img-card .img-remove{
  position:absolute;top:4px;right:4px;
  background:rgba(0,0,0,.55);color:#fff;border:none;
  border-radius:4px;font-size:12px;line-height:1;padding:2px 5px;
  cursor:pointer;opacity:0;transition:opacity .15s}
.img-card:hover .img-remove{opacity:1}
.empty-msg{color:var(--muted);font-style:italic;font-size:12px}
/* Form */
.field{margin-bottom:12px}
.field-label{font-size:11px;color:var(--muted);margin-bottom:5px}
.pills{display:flex;gap:4px;flex-wrap:wrap}
.pill{
  background:var(--btn);color:var(--text);border:1px solid var(--btn-border);
  border-radius:20px;padding:3px 10px;font-size:12px;cursor:pointer}
.pill:hover{background:var(--btn-hover)}
.pill.active{background:var(--accent);color:#fff;border-color:var(--accent)}
select{
  width:100%;background:var(--btn);color:var(--text);
  border:1px solid var(--btn-border);border-radius:6px;
  padding:5px 8px;font-size:12px;font-family:inherit;outline:none}
select:focus{border-color:var(--accent)}
button.primary{
  width:100%;background:var(--accent);color:#fff;
  border:1px solid var(--accent);border-radius:6px;
  padding:7px 12px;font-size:13px;cursor:pointer;font-family:inherit;
  margin-top:4px}
button.primary:hover{opacity:.88}
</style>
</head>
<body class=")HTML" + std::string(cls) + R"HTML(">
<div class="sec-title">Project Images</div>
<div class="two-col">
  <div class="left-pane">
    <div class="img-grid" id="img-grid"></div>
  </div>
  <div class="right-pane">
    <div class="sec-title">Insert Image</div>

    <div class="field">
      <div class="field-label">Size</div>
      <div class="pills" id="size-pills">
        <button class="pill" data-val="small">Small</button>
        <button class="pill active" data-val="medium">Medium</button>
        <button class="pill" data-val="large">Large</button>
        <button class="pill" data-val="full">Full width</button>
      </div>
    </div>

    <div class="field">
      <div class="field-label">Align</div>
      <div class="pills" id="align-pills">
        <button class="pill" data-val="left">Left</button>
        <button class="pill active" data-val="center">Center</button>
        <button class="pill" data-val="right">Right</button>
      </div>
    </div>

    <div class="field">
      <div class="field-label">Insert after</div>
      <select id="heading-sel"></select>
    </div>

    <button class="primary" id="add-btn">Choose Image &amp; Insert</button>
  </div>
</div>

<script>
var _images   = )HTML" + imgsJS + R"HTML(;
var _headings = )HTML" + headsJS + R"HTML(;

// Populate heading dropdown
(function() {
  var sel = document.getElementById('heading-sel');
  var opt = document.createElement('option');
  opt.value = ''; opt.textContent = 'Top of document';
  sel.appendChild(opt);
  _headings.forEach(function(h) {
    var o = document.createElement('option');
    o.value = h; o.textContent = h;
    sel.appendChild(o);
  });
})();

// Render image grid
function renderGrid() {
  var grid = document.getElementById('img-grid');
  grid.innerHTML = '';
  if (_images.length === 0) {
    grid.innerHTML = '<span class="empty-msg">No images yet</span>';
    return;
  }
  _images.forEach(function(img) {
    var card = document.createElement('div');
    card.className = 'img-card';
    var el = document.createElement('img');
    el.src = img.src;
    var name = document.createElement('div');
    name.className = 'img-name';
    name.textContent = img.name;
    name.title = img.name;
    var rm = document.createElement('button');
    rm.className = 'img-remove';
    rm.textContent = '✕';
    rm.title = 'Remove';
    rm.addEventListener('click', function(e) {
      e.stopPropagation();
      if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.image)
        window.webkit.messageHandlers.image.postMessage(
          JSON.stringify({action:'remove', filename: img.name}));
    });
    card.appendChild(el);
    card.appendChild(name);
    card.appendChild(rm);
    grid.appendChild(card);
  });
}
renderGrid();

// Pill selection
function pillGroup(id) {
  var pills = document.querySelectorAll('#' + id + ' .pill');
  pills.forEach(function(p) {
    p.addEventListener('click', function() {
      pills.forEach(function(x) { x.classList.remove('active'); });
      p.classList.add('active');
    });
  });
}
pillGroup('size-pills');
pillGroup('align-pills');

function activeVal(id) {
  var a = document.querySelector('#' + id + ' .pill.active');
  return a ? a.getAttribute('data-val') : '';
}

// Insert button
document.getElementById('add-btn').addEventListener('click', function() {
  var size    = activeVal('size-pills');
  var align   = activeVal('align-pills');
  var heading = document.getElementById('heading-sel').value;
  if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.image)
    window.webkit.messageHandlers.image.postMessage(
      JSON.stringify({action:'insert', size:size, align:align, heading:heading}));
});
</script>
</body></html>
)HTML";
}
