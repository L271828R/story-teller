#include "character_tab_html.h"
#include "html_subst.h"
#include "character_tab_html_data.h"
#include <string>

// These arrays are defined once in html_template.cpp (via xxd-generated headers).
extern "C" {
extern unsigned char mermaid_js_data[];
extern unsigned int  mermaid_js_data_len;
extern unsigned char hljs_js_data[];
extern unsigned int  hljs_js_data_len;
extern unsigned char hljs_css_light_data[];
extern unsigned int  hljs_css_light_data_len;
extern unsigned char hljs_css_dark_data[];
extern unsigned int  hljs_css_dark_data_len;
}

static std::string hljsCSS(bool darkMode) {
    if (darkMode) {
        std::string s(reinterpret_cast<const char*>(hljs_css_dark_data), hljs_css_dark_data_len);
        return "<style>" + s + "\n.hljs{background:transparent}</style>";
    }
    std::string s(reinterpret_cast<const char*>(hljs_css_light_data), hljs_css_light_data_len);
    return "<style>" + s + "\n.hljs{background:transparent}</style>";
}

// Escape </script> inside library payloads so WebKit doesn't close the tag early.
static void escapeScriptClose(std::string& s) {
    const std::string bad  = "</script>";
    const std::string safe = "<\\/script>";
    size_t pos = 0;
    while ((pos = s.find(bad, pos)) != std::string::npos) {
        s.replace(pos, bad.size(), safe);
        pos += safe.size();
    }
}

static std::string hljsJS() {
    std::string s(reinterpret_cast<const char*>(hljs_js_data), hljs_js_data_len);
    escapeScriptClose(s);
    return "<script>" + s + "</script>";
}

static std::string mermaidJS(bool darkMode) {
    std::string s(reinterpret_cast<const char*>(mermaid_js_data), mermaid_js_data_len);
    escapeScriptClose(s);
    std::string theme = darkMode ? "dark" : "default";
    return "<script>" + s + "</script>"
           "<script>mermaid.initialize({startOnLoad:false,theme:'" + theme + "',securityLevel:'loose'});</script>";
}

std::string BuildCharacterTabHTML(bool darkMode) {
    std::string html(reinterpret_cast<const char*>(character_tab_html_data),
                     character_tab_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    htmlSubst(html, "{{HLJS_CSS}}", hljsCSS(darkMode));
    htmlSubst(html, "{{HLJS_JS}}",  hljsJS());
    htmlSubst(html, "{{MERMAID_JS}}", mermaidJS(darkMode));
    return html;
}
