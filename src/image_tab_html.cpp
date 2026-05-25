#include "image_tab_html.h"
#include "html_subst.h"
#include "image_tab_html_data.h"
#include <cstdio>
#include <string>

static std::string itq(const std::string& s) {
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

std::string BuildImageTabHTML(
    const std::vector<std::pair<std::string,std::string>>& images,
    const std::vector<ImageTabDoc>& docs,
    const std::string& selectedDoc,
    bool darkMode)
{
    std::string imgsJS = "[";
    for (size_t i = 0; i < images.size(); ++i) {
        if (i) imgsJS += ",";
        imgsJS += "{\"name\":" + itq(images[i].first)
               + ",\"src\":"  + itq(images[i].second) + "}";
    }
    imgsJS += "]";

    std::string docsJS = "[";
    for (size_t i = 0; i < docs.size(); ++i) {
        if (i) docsJS += ",";
        docsJS += "{\"name\":" + itq(docs[i].name) + ",\"headings\":[";
        for (size_t j = 0; j < docs[i].headings.size(); ++j) {
            if (j) docsJS += ",";
            docsJS += itq(docs[i].headings[j]);
        }
        docsJS += "]}";
    }
    docsJS += "]";

    std::string html(reinterpret_cast<const char*>(image_tab_html_data),
                     image_tab_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}",    darkMode ? "dark" : "");
    htmlSubst(html, "{{IMGS_LIST_JS}}", imgsJS);
    htmlSubst(html, "{{DOCS_JS}}",      docsJS);
    htmlSubst(html, "{{SEL_DOC_JS}}",   itq(selectedDoc));
    return html;
}
