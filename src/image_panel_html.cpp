#include "image_panel_html.h"
#include "html_subst.h"
#include "image_panel_html_data.h"
#include <cstdio>
#include <string>

static std::string jpstr(const std::string& s) {
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

    std::string html(reinterpret_cast<const char*>(image_panel_html_data),
                     image_panel_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    htmlSubst(html, "{{IMGS_LIST_JS}}", imgsJS);
    htmlSubst(html, "{{HEADS_JS}}", headsJS);
    return html;
}
