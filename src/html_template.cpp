#include "html_template.h"
#include "markdown.h"
#include <map>
#include <sstream>
#include "mermaid_js.h"
#include "hljs_js.h"
#include "hljs_css_light.h"
#include "hljs_css_dark.h"
#include <cstdio>
#include <string>

static const std::string& GetMermaidJS() {
    static std::string cached;
    if (cached.empty()) {
        cached.assign(reinterpret_cast<const char*>(mermaid_js_data), mermaid_js_data_len);
        const std::string bad  = "</script>";
        const std::string safe = "<\\/script>";
        size_t pos = 0;
        while ((pos = cached.find(bad, pos)) != std::string::npos) {
            cached.replace(pos, bad.size(), safe);
            pos += safe.size();
        }
    }
    return cached;
}

static const std::string& GetHighlightJS() {
    static std::string cached;
    if (cached.empty()) {
        cached.assign(reinterpret_cast<const char*>(hljs_js_data), hljs_js_data_len);
        const std::string bad  = "</script>";
        const std::string safe = "<\\/script>";
        size_t pos = 0;
        while ((pos = cached.find(bad, pos)) != std::string::npos) {
            cached.replace(pos, bad.size(), safe);
            pos += safe.size();
        }
    }
    return cached;
}

static const std::string& GetHighlightCSSLight() {
    static std::string cached;
    if (cached.empty())
        cached.assign(reinterpret_cast<const char*>(hljs_css_light_data), hljs_css_light_data_len);
    return cached;
}

static const std::string& GetHighlightCSSDark() {
    static std::string cached;
    if (cached.empty())
        cached.assign(reinterpret_cast<const char*>(hljs_css_dark_data), hljs_css_dark_data_len);
    return cached;
}

std::string BuildHTML(const std::string& body,
                      const std::string& title,
                      bool darkMode,
                      int fontSizePercent,
                      const std::map<std::string, std::string>& personaImages) {
    const std::string htmlClass    = darkMode ? " class=\"dark\"" : "";
    const std::string mermaidTheme = darkMode ? "dark" : "default";
    char fsBuf[32];
    snprintf(fsBuf, sizeof(fsBuf), "%.4g", 16.0 * fontSizePercent / 100.0);
    const std::string fsPx       = std::string(fsBuf) + "px";
    const std::string fontPctStr = std::to_string(fontSizePercent);

    // Build window._personaImages JSON object literal.
    std::string personaJS = "window._personaImages={";
    bool first = true;
    for (const auto& kv : personaImages) {
        if (!first) personaJS += ",";
        first = false;
        // Escape the URL (file paths on macOS shouldn't need much escaping).
        personaJS += "\"" + kv.first + "\":\"" + kv.second + "\"";
    }
    personaJS += "};";

    return R"HTML(<!DOCTYPE html>
<html lang="en")HTML" + htmlClass + R"HTML(>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)HTML" + EscapeHTML(title) + R"HTML(</title>
<script>)HTML" + GetMermaidJS() + R"HTML(</script>
<script>)HTML" + GetHighlightJS() + R"HTML(</script>
<style>)HTML" + (darkMode ? GetHighlightCSSDark() : GetHighlightCSSLight()) + R"HTML(</style>
<style>
/* ── Theme tokens ───────────────────────────────────────────────────────── */
:root {
  --bg:            #ffffff;
  --surface:       #f6f8fa;
  --surface2:      #fafbfc;
  --text:          #24292f;
  --text-muted:    #57606a;
  --border:        #d0d7de;
  --link:          #0969da;
  --link-hover:    #0550ae;
  --code-inline:   rgba(175,184,193,0.2);
  --del:           #656d76;
  --zm-svg-bg:     #ffffff;
  --mermaid-hover: rgba(9,105,218,0.16);
  --mermaid-ring:  rgba(9,105,218,0.53);
}
.dark {
  --bg:            #0d1117;
  --surface:       #161b22;
  --surface2:      #1c2128;
  --text:          #e6edf3;
  --text-muted:    #8b949e;
  --border:        #30363d;
  --link:          #58a6ff;
  --link-hover:    #79c0ff;
  --code-inline:   rgba(110,118,129,0.4);
  --del:           #8b949e;
  --zm-svg-bg:     #1e2430;
  --mermaid-hover: rgba(88,166,255,0.12);
  --mermaid-ring:  rgba(88,166,255,0.45);
}

/* ── Reset & base ───────────────────────────────────────────────────────── */
*{box-sizing:border-box;margin:0;padding:0}
body{
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;
  font-size:)HTML" + fsPx + R"HTML(;line-height:1.65;
  color:var(--text);background:var(--bg);
  padding:32px 24px;max-width:960px;margin:0 auto;
  overflow-wrap:anywhere;word-break:break-word;
}

/* ── Typography ─────────────────────────────────────────────────────────── */
h1,h2,h3,h4,h5,h6{margin:24px 0 16px;font-weight:600;line-height:1.25;color:var(--text)}
h1{font-size:2em;  border-bottom:1px solid var(--border);padding-bottom:.3em}
h2{font-size:1.5em;border-bottom:1px solid var(--border);padding-bottom:.3em}
h3{font-size:1.25em}
h4{font-size:1em}
p{margin-bottom:16px;overflow-wrap:anywhere;word-break:break-word}
a{color:var(--link);text-decoration:none}
a:hover{color:var(--link-hover);text-decoration:underline}
strong{font-weight:600}
del{color:var(--del)}

/* ── Code ───────────────────────────────────────────────────────────────── */
code{
  font-family:'SFMono-Regular',Consolas,'Liberation Mono',Menlo,monospace;
  font-size:85%;background:var(--code-inline);
  padding:.2em .4em;border-radius:3px;color:var(--text);
}
pre{
  background:var(--surface);border:1px solid var(--border);
  border-radius:6px;padding:16px;overflow-x:auto;margin-bottom:16px;
}
pre code{background:none;padding:0;font-size:87.5%;border:none}
.hljs{background:transparent}
.code-wrapper{position:relative;display:block;margin-bottom:16px}
.code-wrapper pre{margin-bottom:0}
.copy-btn{
  position:absolute;top:8px;right:8px;
  padding:3px 9px;font-size:11px;line-height:1.5;
  background:var(--surface2);border:1px solid var(--border);
  border-radius:4px;cursor:pointer;color:var(--text-muted);
  opacity:0;transition:opacity .15s,background .15s;
}
.code-wrapper:hover .copy-btn{opacity:1}
.copy-btn:hover{background:var(--border)}
.copy-btn.copied{color:var(--link)}

/* ── Blockquote ─────────────────────────────────────────────────────────── */
blockquote{
  padding:0 1em;color:var(--text-muted);
  border-left:.25em solid var(--border);margin-bottom:16px;
}

/* ── Tidbit ─────────────────────────────────────────────────────────────── */
details.tidbit{
  border:1px solid var(--border);border-radius:6px;
  margin-bottom:16px;background:var(--surface);
}
details.tidbit summary{
  padding:10px 14px;cursor:pointer;font-style:italic;
  color:var(--text-muted);user-select:none;list-style:none;
}
details.tidbit summary::before{content:"💬 "}
details.tidbit summary::-webkit-details-marker{display:none}
details.tidbit[open] summary{border-bottom:1px solid var(--border)}
.tidbit-body{padding:12px 16px}
.tidbit-body p:last-child{margin-bottom:0}

/* ── Tidbit carousel ─────────────────────────────────────────────────────── */
.tidbit-carousel{border:1px solid var(--border);border-radius:6px;margin-bottom:16px;background:var(--surface)}
.tidbit-carousel-header{display:flex;justify-content:space-between;align-items:center;padding:10px 14px;font-style:italic;color:var(--text-muted);user-select:none}
.tidbit-carousel-nav{display:flex;align-items:center;gap:6px}
.tidbit-carousel-counter{font-size:.8em;opacity:.75}
.tidbit-carousel-arrow{background:none;border:1px solid var(--border);border-radius:4px;color:var(--text-muted);cursor:pointer;font-size:1.1em;padding:0 7px;line-height:1.5}
.tidbit-carousel-arrow:hover{background:var(--border)}
.tidbit-carousel-speaker{cursor:pointer}
.tidbit-carousel-speaker:hover{color:var(--text)}
.tidbit-carousel-body{padding:12px 16px;border-top:1px solid var(--border)}
.tidbit-carousel-body p:last-child{margin-bottom:0}

.persona-img{
  float:right;max-width:110px;max-height:110px;
  border-radius:50%;margin:0 0 10px 14px;
  object-fit:cover;border:2px solid var(--border);
  box-shadow:0 2px 8px rgba(0,0,0,.18);
}

/* ── Conversation ───────────────────────────────────────────────────────── */
details.conversation{
  border:1px solid var(--border);border-radius:6px;
  margin-bottom:16px;background:var(--surface);
}
details.conversation summary{
  padding:10px 14px;cursor:pointer;font-style:italic;
  color:var(--text-muted);user-select:none;list-style:none;
}
details.conversation summary::-webkit-details-marker{display:none}
details.conversation[open] summary{border-bottom:1px solid var(--border)}
.conversation-body{padding:12px 16px}
.qa-turn{margin-bottom:16px}
.qa-turn:last-child{margin-bottom:0}
.qa-q{background:var(--surface2);border-radius:6px 6px 6px 2px;
  padding:8px 12px;margin-bottom:6px;font-weight:500;color:var(--text)}
.qa-a{background:var(--bg);border:1px solid var(--border);
  border-radius:2px 6px 6px 6px;padding:8px 12px;color:var(--text)}
.chat-btn{
  margin-left:10px;background:none;border:none;cursor:pointer;
  font-size:0.85em;opacity:0.4;transition:opacity .15s;vertical-align:middle;
  padding:0 4px;color:inherit;
}
h2:hover .chat-btn{opacity:1}
#doc-chat-btn{
  background:none;border:1px solid var(--border);border-radius:20px;
  padding:3px 10px;cursor:pointer;font-size:0.85em;
  color:var(--text-muted);transition:background .15s,color .15s;
}
#doc-chat-btn:hover{background:var(--border);color:var(--text)}

/* ── Notes ─────────────────────────────────────────────────────────────── */
.note-marker{
  text-decoration:underline dotted var(--link);
  text-decoration-skip-ink:none;
  cursor:pointer;
}
.note-tooltip{
  position:fixed;z-index:5000;
  background:var(--surface);border:1px solid var(--border);
  border-radius:6px;padding:8px 12px;max-width:300px;
  box-shadow:0 4px 16px rgba(0,0,0,.18);font-size:0.85em;
  color:var(--text);line-height:1.5;pointer-events:none;
  white-space:pre-wrap;
}
/* Selection toolbar */
#note-toolbar{
  position:fixed;z-index:4999;display:none;
  background:var(--surface);border:1px solid var(--border);
  border-radius:6px;padding:4px 8px;
  box-shadow:0 2px 8px rgba(0,0,0,.15);
  cursor:pointer;font-size:0.85em;color:var(--text);
  white-space:nowrap;user-select:none;
}
#note-toolbar:hover{background:var(--border)}

/* ── Lists ──────────────────────────────────────────────────────────────── */
ul,ol{padding-left:2em;margin-bottom:16px}
li{margin:4px 0;color:var(--text)}
li,summary,.tidbit-body{overflow-wrap:anywhere;word-break:break-word}

/* ── Media ──────────────────────────────────────────────────────────────── */
img{max-width:100%;height:auto;border-radius:4px}
/* Project image size/align classes (set by SubstituteLocalImages) */
.img-small {width:160px}
.img-medium{width:320px}
.img-large {width:560px}
.img-full  {width:100%}
.img-left  {float:left;margin:0 16px 12px 0;clear:left}
.img-right {float:right;margin:0 0 12px 16px;clear:right}
.img-center{display:block;margin-left:auto;margin-right:auto}

/* ── Rule ───────────────────────────────────────────────────────────────── */
hr{height:.25em;padding:0;margin:24px 0;background:var(--border);border:0}

/* ── Focus / reading mode ───────────────────────────────────────────────── */
.focus-line{
  background:rgba(255,215,0,.38);border-radius:2px;
  outline:2px solid #f5c800;outline-offset:1px;
}

/* ── Table ──────────────────────────────────────────────────────────────── */
table{border-collapse:collapse;margin-bottom:16px;width:100%}
th,td{border:1px solid var(--border);padding:6px 13px;text-align:left;color:var(--text)}
th{background:var(--surface);font-weight:600}
tr:nth-child(even) td{background:var(--surface)}

/* ── Mermaid wrapper ────────────────────────────────────────────────────── */
.mermaid-wrapper{
  display:inline-block;cursor:zoom-in;
  border:1px solid var(--border);border-radius:6px;
  padding:20px;margin-bottom:16px;
  background:var(--surface2);
  transition:box-shadow .18s,border-color .18s;
  width:100%;text-align:center;
}
.mermaid-wrapper:hover{
  box-shadow:0 0 0 3px var(--mermaid-hover);
  border-color:var(--mermaid-ring);
}

/* ── Zoom modal ─────────────────────────────────────────────────────────── */
#zm-overlay{
  display:none;position:fixed;inset:0;background:rgba(0,0,0,.88);
  z-index:9999;align-items:center;justify-content:center;
}
#zm-overlay.open{display:flex}
#zm-stage{
  position:relative;width:90vw;height:90vh;
  overflow:hidden;cursor:grab;user-select:none;
}
#zm-stage.dragging{cursor:grabbing}
#zm-inner{
  display:flex;align-items:center;justify-content:center;
  width:100%;height:100%;transform-origin:center center;
}
#zm-inner svg{
  max-width:85vw;max-height:85vh;
  background:var(--zm-svg-bg);border-radius:8px;padding:24px;
  box-shadow:0 8px 32px rgba(0,0,0,.5);
}
#zm-close{
  position:fixed;top:14px;right:18px;background:rgba(255,255,255,.12);
  border:none;color:#fff;font-size:20px;width:36px;height:36px;
  border-radius:50%;cursor:pointer;z-index:10001;
  display:flex;align-items:center;justify-content:center;
  transition:background .15s;
}
#zm-close:hover{background:rgba(255,255,255,.28)}
#zm-hint{
  position:fixed;bottom:14px;left:50%;transform:translateX(-50%);
  color:rgba(255,255,255,.55);font-size:12px;pointer-events:none;white-space:nowrap;
}
#zm-scale{
  position:fixed;top:16px;left:50%;transform:translateX(-50%);
  color:rgba(255,255,255,.7);font-size:12px;background:rgba(0,0,0,.4);
  padding:3px 10px;border-radius:20px;pointer-events:none;
}
</style>
</head>
<body>
)HTML" + body + R"HTML(

<!-- ── Zoom modal ───────────────────────────────────────────────────────── -->
<div id="zm-overlay">
  <button id="zm-close" title="Close (Esc)">&#x2715;</button>
  <div id="zm-stage">
    <div id="zm-inner"></div>
  </div>
  <div id="zm-hint">Scroll to zoom &nbsp;·&nbsp; Drag to pan &nbsp;·&nbsp; ESC to close</div>
  <div id="zm-scale">100%</div>
</div>

<script>
// ── Highlight.js ─────────────────────────────────────────────────────────
hljs.highlightAll();

// ── Copy buttons on fenced code blocks ───────────────────────────────────
document.querySelectorAll('pre code').forEach(function(block) {
  var pre = block.parentElement;
  var wrapper = document.createElement('div');
  wrapper.className = 'code-wrapper';
  pre.parentNode.insertBefore(wrapper, pre);
  wrapper.appendChild(pre);
  var btn = document.createElement('button');
  btn.className = 'copy-btn';
  btn.textContent = 'Copy';
  btn.addEventListener('click', function() {
    var text = block.innerText;
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.clipboardCopy) {
      window.webkit.messageHandlers.clipboardCopy.postMessage(text);
    }
    btn.textContent = 'Copied!';
    btn.classList.add('copied');
    setTimeout(function() { btn.textContent = 'Copy'; btn.classList.remove('copied'); }, 1500);
  });
  wrapper.appendChild(btn);
});

// ── Mermaid init (theme set by C++ based on current mode) ────────────────
mermaid.initialize({startOnLoad:true, theme:')HTML" + mermaidTheme + R"HTML(', securityLevel:'loose'});

// ── Chapter chat buttons ─────────────────────────────────────────────────
document.querySelectorAll('h2[data-ch-id]').forEach(function(h2) {
  var btn = document.createElement('button');
  btn.className = 'chat-btn';
  btn.textContent = '💬';
  btn.title = 'Ask about this chapter';
  btn.addEventListener('click', function(e) {
    e.stopPropagation();
    var id = h2.getAttribute('data-ch-id');
    var title = h2.textContent.replace('💬','').trim();
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.chat)
      window.webkit.messageHandlers.chat.postMessage(id + '|' + title);
  });
  h2.appendChild(btn);
});

// ── Document-level chat button ───────────────────────────────────────────
(function() {
  var bar = document.createElement('div');
  bar.style.cssText = 'text-align:right;margin-bottom:12px;';
  var btn = document.createElement('button');
  btn.id = 'doc-chat-btn';
  btn.textContent = '💬';
  btn.title = 'Chat about this document';
  btn.addEventListener('click', function(e) {
    e.stopPropagation();
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.chat)
      window.webkit.messageHandlers.chat.postMessage('-1|');
  });
  bar.appendChild(btn);
  document.body.prepend(bar);
})();

// ── Zoom state ───────────────────────────────────────────────────────────
let zmScale = 1, zmTX = 0, zmTY = 0;
let zmDrag = false, zmDX = 0, zmDY = 0;
const zmInner = document.getElementById('zm-inner');
const zmStage = document.getElementById('zm-stage');
const zmScaleLabel = document.getElementById('zm-scale');

function zmApply() {
  zmInner.style.transform = `translate(${zmTX}px,${zmTY}px) scale(${zmScale})`;
  zmScaleLabel.textContent = Math.round(zmScale * 100) + '%';
}

function zmOpen(svgHTML) {
  zmScale=1; zmTX=0; zmTY=0;
  zmInner.innerHTML = svgHTML;
  const svg = zmInner.querySelector('svg');
  if (svg) {
    svg.removeAttribute('width');
    svg.removeAttribute('height');
    svg.style.maxWidth  = '85vw';
    svg.style.maxHeight = '85vh';
  }
  zmApply();
  document.getElementById('zm-overlay').classList.add('open');
  document.body.style.overflow = 'hidden';
}

function zmClose() {
  document.getElementById('zm-overlay').classList.remove('open');
  document.body.style.overflow = '';
}

// Click on any mermaid wrapper → zoom (event delegation, works after async render)
document.addEventListener('click', e => {
  if (e.target.closest('#zm-overlay')) return;
  const w = e.target.closest('.mermaid-wrapper');
  if (w) { const svg = w.querySelector('svg'); if (svg) zmOpen(svg.outerHTML); }
});

document.getElementById('zm-close').addEventListener('click', zmClose);
document.getElementById('zm-overlay').addEventListener('click', e => {
  if (e.target === document.getElementById('zm-overlay')) zmClose();
});

document.addEventListener('keydown', e => { if (e.key === 'Escape') zmClose(); });

zmStage.addEventListener('wheel', e => {
  e.preventDefault();
  zmScale = Math.min(Math.max(zmScale * (e.deltaY < 0 ? 1.12 : 0.9), 0.08), 20);
  zmApply();
}, {passive:false});

zmStage.addEventListener('mousedown', e => {
  if (e.button !== 0) return;
  zmDrag = true; zmDX = e.clientX - zmTX; zmDY = e.clientY - zmTY;
  zmStage.classList.add('dragging'); e.preventDefault();
});
document.addEventListener('mousemove', e => {
  if (!zmDrag) return;
  zmTX = e.clientX - zmDX; zmTY = e.clientY - zmDY; zmApply();
});
document.addEventListener('mouseup', () => {
  zmDrag = false; zmStage.classList.remove('dragging');
});

let lastDist = 0;
zmStage.addEventListener('touchstart', e => {
  if (e.touches.length === 1) {
    zmDrag = true; zmDX = e.touches[0].clientX - zmTX; zmDY = e.touches[0].clientY - zmTY;
  } else if (e.touches.length === 2) {
    zmDrag = false;
    lastDist = Math.hypot(e.touches[0].clientX - e.touches[1].clientX,
                          e.touches[0].clientY - e.touches[1].clientY);
  }
  e.preventDefault();
}, {passive:false});
zmStage.addEventListener('touchmove', e => {
  if (e.touches.length === 1 && zmDrag) {
    zmTX = e.touches[0].clientX - zmDX; zmTY = e.touches[0].clientY - zmDY; zmApply();
  } else if (e.touches.length === 2) {
    const d = Math.hypot(e.touches[0].clientX - e.touches[1].clientX,
                         e.touches[0].clientY - e.touches[1].clientY);
    if (lastDist > 0) {
      zmScale = Math.min(Math.max(zmScale * (d / lastDist), 0.08), 20); zmApply();
    }
    lastDist = d;
  }
  e.preventDefault();
}, {passive:false});
zmStage.addEventListener('touchend', () => { zmDrag = false; lastDist = 0; });

// Ctrl/Meta+wheel — adjust document font size
(function(){
  var pct = )HTML" + fontPctStr + R"HTML(;
  document.addEventListener('wheel', function(e){
    if (!e.ctrlKey && !e.metaKey) return;
    e.preventDefault();
    pct = Math.max(50, Math.min(200, pct + (e.deltaY < 0 ? 10 : -10)));
    document.body.style.fontSize = (16 * pct / 100) + 'px';
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.fontSizeChange)
      window.webkit.messageHandlers.fontSizeChange.postMessage(String(pct));
  }, {passive:false});
})();

// ── Notes ────────────────────────────────────────────────────────────────
(function() {
  // Selection toolbar — only shown when the selection does not overlap a note.
  var toolbar = document.createElement('div');
  toolbar.id = 'note-toolbar';
  toolbar.textContent = 'Add note';
  document.body.appendChild(toolbar);

  var selTimeout = null;
  document.addEventListener('selectionchange', function() {
    clearTimeout(selTimeout);
    selTimeout = setTimeout(function() {
      var sel = window.getSelection();
      var text = sel ? sel.toString().trim() : '';
      if (!text || text.length < 2) { toolbar.style.display = 'none'; return; }
      var range = sel.getRangeAt(0);
      // Hide toolbar if the selection is inside or overlaps an existing note marker.
      var ancestor = range.commonAncestorContainer;
      var el = ancestor.nodeType === 3 ? ancestor.parentElement : ancestor;
      if (el && el.closest('.note-marker')) { toolbar.style.display = 'none'; return; }
      var frag = range.cloneContents();
      if (frag.querySelector('.note-marker')) { toolbar.style.display = 'none'; return; }
      var rects = range.getClientRects();
      var rect  = rects.length ? rects[rects.length - 1] : range.getBoundingClientRect();
      if (!rect || (rect.width === 0 && rect.height === 0)) { toolbar.style.display = 'none'; return; }
      toolbar.style.display = 'block';
      toolbar.style.left = Math.min(rect.left, window.innerWidth - 200) + 'px';
      toolbar.style.top  = (rect.top - 36) + 'px';
    }, 200);
  });

  toolbar.addEventListener('mousedown', function(e) {
    e.preventDefault();
    var sel = window.getSelection();
    if (!sel || !sel.toString().trim()) return;
    var selectedText = sel.toString().trim();
    var range = sel.getRangeAt(0);
    var container = range.startContainer;
    var fullText = container.textContent || '';
    var start = Math.max(0, range.startOffset - 60);
    var end   = Math.min(fullText.length, range.endOffset + 60);
    var context = fullText.substring(start, end);
    toolbar.style.display = 'none';
    sel.removeAllRanges();
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.note)
      window.webkit.messageHandlers.note.postMessage(
        JSON.stringify({action:'add', selectedText:selectedText, context:context}));
  });

  document.addEventListener('mousedown', function(e) {
    if (!e.target.classList.contains('note-marker'))
      toolbar.style.display = 'none';
  });

  // Hover tooltip
  var tooltip = document.createElement('div');
  tooltip.className = 'note-tooltip';
  tooltip.style.display = 'none';
  document.body.appendChild(tooltip);
  var tooltipTimer = null;

  document.addEventListener('mouseover', function(e) {
    var marker = e.target.closest('.note-marker');
    if (!marker) return;
    clearTimeout(tooltipTimer);
    tooltip.textContent = marker.getAttribute('data-note-text') || '';
    tooltip.style.display = 'block';
    var rect = marker.getBoundingClientRect();
    tooltip.style.left = Math.min(rect.left, window.innerWidth - 320) + 'px';
    tooltip.style.top  = (rect.bottom + 6) + 'px';
  });

  document.addEventListener('mouseout', function(e) {
    var marker = e.target.closest('.note-marker');
    if (!marker) return;
    tooltipTimer = setTimeout(function() { tooltip.style.display = 'none'; }, 100);
  });

  // Click on annotated text → edit the note directly (no popover needed).
  document.addEventListener('click', function(e) {
    var marker = e.target.closest('.note-marker');
    if (!marker) return;
    e.stopPropagation();
    tooltip.style.display = 'none';
    var noteId = marker.getAttribute('data-note-id');
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.note)
      window.webkit.messageHandlers.note.postMessage(
        JSON.stringify({action:'edit', id: parseInt(noteId)}));
  });
  // Close side chat when the user clicks anywhere in the main content.
  document.addEventListener('click', function(e) {
    if (e.target.closest('.chat-btn') || e.target.closest('.note-marker') ||
        e.target.closest('#note-toolbar') || e.target.closest('#zm-overlay')) return;
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.closeChat)
      window.webkit.messageHandlers.closeChat.postMessage('');
  });
})();

// ── Persona images (set before carousel so lookupPersona has data) ───────
)HTML" + personaJS + R"HTML(

// ── Tidbit carousel ──────────────────────────────────────────────────────
(function() {
  function normName(s) {
    return s.trim().toLowerCase().replace(/\s+/g,'_').replace(/[^a-z0-9_-￿]/g,'');
  }
  function lookupPersona(name) {
    var imgs = window._personaImages;
    if (!imgs) return null;
    var src = imgs[normName(name)];
    if (src) return src;
    var m = name.match(/\(([^)]+)\)/);
    if (m) { src = imgs[normName(m[1])]; if (src) return src; }
    var nonAscii = name.replace(/[\x00-\x7F]+/g,'').trim();
    if (nonAscii) { src = imgs[normName(nonAscii)]; if (src) return src; }
    var ascii = name.replace(/[^\x00-\x7F]+/g,' ').replace(/[()]/g,' ').trim();
    if (ascii) { src = imgs[normName(ascii)]; if (src) return src; }
    return null;
  }

  var all = Array.from(document.querySelectorAll('details.tidbit'));
  var visited = new Set();
  all.forEach(function(el) {
    if (visited.has(el)) return;
    var group = [el];
    var sib = el.nextElementSibling;
    while (sib && sib.classList && sib.classList.contains('tidbit')) {
      group.push(sib); sib = sib.nextElementSibling;
    }
    group.forEach(function(e) { visited.add(e); });
    if (group.length < 2) return;

    var speakers = group.map(function(d) {
      var s = d.querySelector('summary');
      return s ? s.textContent.trim() : '';
    });
    var imgSrcs = speakers.map(function(name) { return lookupPersona(name); });
    var bodies = group.map(function(d) {
      var b = d.querySelector('.tidbit-body');
      return b ? b.innerHTML : '';
    });

    var cur = 0;
    var isOpen = false;

    var carousel = document.createElement('div');
    carousel.className = 'tidbit-carousel';

    // Header: name (clickable to expand) + counter + arrow
    var header = document.createElement('div');
    header.className = 'tidbit-carousel-header';
    var spk = document.createElement('span');
    spk.className = 'tidbit-carousel-speaker';
    spk.textContent = '💬 ' + speakers[0];
    spk.title = 'Click to expand';
    var nav = document.createElement('div');
    nav.className = 'tidbit-carousel-nav';
    var prev = document.createElement('button');
    prev.className = 'tidbit-carousel-arrow';
    prev.textContent = '‹';
    prev.title = 'Previous';
    var ctr = document.createElement('span');
    ctr.className = 'tidbit-carousel-counter';
    ctr.textContent = '1 / ' + group.length;
    var arr = document.createElement('button');
    arr.className = 'tidbit-carousel-arrow';
    arr.textContent = '›';
    arr.title = 'Next';
    nav.appendChild(prev);
    nav.appendChild(ctr);
    nav.appendChild(arr);
    header.appendChild(spk);
    header.appendChild(nav);
    carousel.appendChild(header);

    // Single body div — populated on expand, hidden by default
    var bodyDiv = document.createElement('div');
    bodyDiv.className = 'tidbit-carousel-body';
    bodyDiv.style.display = 'none';
    carousel.appendChild(bodyDiv);

    function renderBody() {
      bodyDiv.innerHTML = '';
      if (imgSrcs[cur]) {
        var img = document.createElement('img');
        img.src = imgSrcs[cur];
        img.className = 'persona-img';
        img.alt = speakers[cur];
        bodyDiv.appendChild(img);
      }
      var textDiv = document.createElement('div');
      textDiv.innerHTML = bodies[cur];
      bodyDiv.appendChild(textDiv);
    }

    spk.addEventListener('click', function() {
      isOpen = !isOpen;
      bodyDiv.style.display = isOpen ? '' : 'none';
      if (isOpen) renderBody();
    });

    prev.addEventListener('click', function(e) {
      e.stopPropagation();
      cur = (cur - 1 + group.length) % group.length;
      spk.textContent = '💬 ' + speakers[cur];
      ctr.textContent = (cur + 1) + ' / ' + group.length;
      if (isOpen) renderBody();
    });

    arr.addEventListener('click', function(e) {
      e.stopPropagation();
      cur = (cur + 1) % group.length;
      spk.textContent = '💬 ' + speakers[cur];
      ctr.textContent = (cur + 1) + ' / ' + group.length;
      if (isOpen) renderBody();
    });

    el.parentNode.insertBefore(carousel, el);
    group.forEach(function(e) { e.parentNode.removeChild(e); });
  });
})();

// ── Persona images for individual (non-carousel) tidbits ─────────────────
(function() {
  function normName(s) {
    return s.trim().toLowerCase().replace(/\s+/g,'_').replace(/[^a-z0-9_-￿]/g,'');
  }
  function lookupPersona(name) {
    var imgs = window._personaImages;
    if (!imgs) return null;
    var src = imgs[normName(name)];
    if (src) return src;
    var m = name.match(/\(([^)]+)\)/);
    if (m) { src = imgs[normName(m[1])]; if (src) return src; }
    var nonAscii = name.replace(/[\x00-\x7F]+/g,'').trim();
    if (nonAscii) { src = imgs[normName(nonAscii)]; if (src) return src; }
    var ascii = name.replace(/[^\x00-\x7F]+/g,' ').replace(/[()]/g,' ').trim();
    if (ascii) { src = imgs[normName(ascii)]; if (src) return src; }
    return null;
  }
  document.querySelectorAll('details.tidbit').forEach(function(details) {
    details.addEventListener('toggle', function() {
      if (!details.open || details.querySelector('.persona-img')) return;
      var summary = details.querySelector('summary');
      if (!summary) return;
      var src = lookupPersona(summary.textContent || '');
      if (!src) return;
      var body = details.querySelector('.tidbit-body');
      if (!body) return;
      var img = document.createElement('img');
      img.src = src;
      img.className = 'persona-img';
      img.alt = summary.textContent.trim();
      body.prepend(img);
    });
  });
})();

// ── Focus / reading mode — 10-token chunks ───────────────────────────────
(function() {
  var CHUNK_SIZE  = 10;
  var focusActive = false;
  var focusBlocks = [];
  var focusIdx    = -1;
  var focusEl     = null;

  // Split text into chunks of CHUNK_SIZE content tokens.
  // Tokens: each CJK character counts as one; each Latin/other word counts as one.
  // Whitespace is preserved by attaching it to the preceding token's chunk.
  function makeChunks(text, size) {
    var out  = [];
    var re   = /[一-鿿぀-ヿ가-힯]|[^\s一-鿿぀-ヿ가-힯]+|\s+/g;
    var toks = text.match(re) || [];
    var cur  = '', n = 0;
    for (var i = 0; i < toks.length; i++) {
      var t = toks[i];
      cur += t;
      if (/\S/.test(t)) {
        n++;
        if (n >= size) { if (cur.trim()) out.push(cur); cur = ''; n = 0; }
      }
    }
    if (cur.trim()) out.push(cur);
    return out;
  }

  // Walk all content text nodes, split into chunk spans, return span array.
  function wrapAll() {
    var spans = [];
    var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT,
      function(node) {
        if (!node.textContent.trim()) return NodeFilter.FILTER_SKIP;
        var p = node.parentNode;
        while (p && p !== document.body) {
          var tag = (p.tagName || '').toUpperCase();
          if (tag === 'SCRIPT' || tag === 'STYLE' || tag === 'BUTTON' || tag === 'SUMMARY')
            return NodeFilter.FILTER_SKIP;
          if (p.id === 'zm-overlay' || p.id === 'note-toolbar')
            return NodeFilter.FILTER_SKIP;
          if (p.dataset && p.dataset.focusChunk) return NodeFilter.FILTER_SKIP;
          p = p.parentNode;
        }
        return NodeFilter.FILTER_ACCEPT;
      }
    );
    var nodes = [], n;
    while ((n = walker.nextNode())) nodes.push(n);

    nodes.forEach(function(textNode) {
      var chunks = makeChunks(textNode.textContent, CHUNK_SIZE);
      if (!chunks.length) return;
      var frag = document.createDocumentFragment();
      chunks.forEach(function(chunk) {
        var sp = document.createElement('span');
        sp.dataset.focusChunk = '1';
        sp.textContent = chunk;
        frag.appendChild(sp);
        spans.push(sp);
      });
      textNode.parentNode.replaceChild(frag, textNode);
    });
    return spans;
  }

  // Remove all chunk spans and restore plain text nodes.
  function unwrapAll() {
    document.querySelectorAll('[data-focus-chunk]').forEach(function(sp) {
      var parent = sp.parentNode;
      while (sp.firstChild) parent.insertBefore(sp.firstChild, sp);
      parent.removeChild(sp);
    });
    document.body.normalize();
  }

  function clearFocus() {
    if (focusEl) { focusEl.classList.remove('focus-line'); focusEl = null; }
  }

  function moveFocus(idx) {
    clearFocus();
    if (!focusBlocks.length) return;
    focusIdx = Math.max(0, Math.min(idx, focusBlocks.length - 1));
    focusEl  = focusBlocks[focusIdx];
    focusEl.classList.add('focus-line');
    focusEl.scrollIntoView({block:'nearest', behavior:'smooth'});
  }

  window.toggleFocusMode = function() {
    focusActive = !focusActive;
    if (focusActive) {
      focusBlocks = wrapAll();
      moveFocus(0);
    } else {
      clearFocus();
      unwrapAll();
      focusBlocks = [];
      focusIdx = -1;
    }
    return focusActive;
  };

  document.addEventListener('keydown', function(e) {
    if (!focusActive) return;
    if (e.key === 'ArrowDown' || e.key === 'ArrowRight') {
      e.preventDefault(); moveFocus(focusIdx + 1);
    } else if (e.key === 'ArrowUp' || e.key === 'ArrowLeft') {
      e.preventDefault(); moveFocus(focusIdx - 1);
    } else if (e.key === 'Escape') {
      var overlay = document.getElementById('zm-overlay');
      if (!overlay || !overlay.classList.contains('open')) toggleFocusMode();
    }
  });

  document.addEventListener('click', function(e) {
    if (!focusActive) return;
    if (e.target.closest('#zm-overlay') || e.target.closest('.chat-btn') ||
        e.target.closest('#note-toolbar') || e.target.closest('.note-marker')) return;
    var chunk = e.target.closest('[data-focus-chunk]');
    if (chunk) {
      var i = focusBlocks.indexOf(chunk);
      if (i !== -1) moveFocus(i);
    }
  });
})();
</script>
</body>
</html>
)HTML";
}

std::string BuildLogsHTML(const std::string& rawLog,
                           const std::string& logPath,
                           bool darkMode) {
    const std::string bg      = darkMode ? "#0d1117" : "#ffffff";
    const std::string surface = darkMode ? "#161b22" : "#f6f8fa";
    const std::string border  = darkMode ? "#30363d" : "#d0d7de";
    const std::string text    = darkMode ? "#e6edf3" : "#24292f";
    const std::string muted   = darkMode ? "#8b949e" : "#57606a";
    const std::string green   = darkMode ? "#3fb950" : "#1a7f37";
    const std::string red     = darkMode ? "#f85149" : "#cf222e";

    std::string rows;
    std::istringstream ss(rawLog);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::string ts, msg;
        if (line.size() > 21 && line[10] == ' ' && line[19] == ' ') {
            ts  = line.substr(0, 19);
            msg = line.substr(21);
        } else {
            msg = line;
        }
        std::string msgColor = text;
        if (msg.find("FAILED") != std::string::npos || msg.find("error") != std::string::npos)
            msgColor = red;
        else if (msg.find("=== startup") != std::string::npos)
            msgColor = green;

        rows += "<tr>"
                "<td class='ts'>" + EscapeHTML(ts) + "</td>"
                "<td style='color:" + msgColor + "'>" + EscapeHTML(msg) + "</td>"
                "</tr>\n";
    }

    return R"HTML(<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<title>StoryTeller — Logs</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'SFMono-Regular',Consolas,monospace;font-size:13px;
     background:)HTML" + bg + R"HTML(;color:)HTML" + text + R"HTML(;padding:24px}
h2{font-size:15px;font-weight:600;margin-bottom:16px;color:)HTML" + text + R"HTML(}
table{width:100%;border-collapse:collapse}
tr{border-bottom:1px solid )HTML" + border + R"HTML(}
tr:last-child{border-bottom:none}
td{padding:5px 10px;vertical-align:top;white-space:pre-wrap;word-break:break-all}
.ts{color:)HTML" + muted + R"HTML(;white-space:nowrap;padding-right:20px;user-select:none}
tr:hover{background:)HTML" + surface + R"HTML(}
</style></head><body>
<h2>StoryTeller — Application Log</h2>
<p style="font-size:12px;color:)HTML" + muted + R"HTML(;margin:8px 0 16px">)HTML"
+ EscapeHTML(logPath) + R"HTML(</p>
<table>)HTML" + rows + R"HTML(</table>
</body></html>)HTML";
}
