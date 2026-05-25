#include "character_tab_html.h"
#include "html_subst.h"
#include "character_tab_html_data.h"
#include <string>

std::string BuildCharacterTabHTML(bool darkMode) {
    std::string html(reinterpret_cast<const char*>(character_tab_html_data),
                     character_tab_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    return html;
}
