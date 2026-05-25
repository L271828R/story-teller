#include "persona_panel_html.h"
#include "html_subst.h"
#include "persona_panel_html_data.h"
#include <cstdio>
#include <string>

static std::string jstr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c < 0x20)  { char b[8]; snprintf(b, 8, "\\u%04x", c); o += b; }
        else                 o += (char)c;
    }
    return o + "\"";
}

std::string BuildPersonaPanelHTML(
    const std::map<std::string, std::vector<std::string>>& categories,
    const std::map<std::string, std::string>& images,
    bool darkMode)
{
    std::string catJS = "{";
    bool fc = true;
    for (const auto& kv : categories) {
        if (!fc) catJS += ",";
        fc = false;
        catJS += jstr(kv.first) + ":[";
        bool fn = true;
        for (const auto& n : kv.second) {
            if (!fn) catJS += ",";
            fn = false;
            catJS += jstr(n);
        }
        catJS += "]";
    }
    catJS += "}";

    std::string imgJS = "{";
    bool fi = true;
    for (const auto& kv : images) {
        if (!fi) imgJS += ",";
        fi = false;
        imgJS += jstr(kv.first) + ":" + jstr(kv.second);
    }
    imgJS += "}";

    std::string html(reinterpret_cast<const char*>(persona_panel_html_data),
                     persona_panel_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    htmlSubst(html, "{{CATS_JS}}", catJS);
    htmlSubst(html, "{{IMGS_JS}}", imgJS);
    return html;
}
