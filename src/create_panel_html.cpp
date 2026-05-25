#include "create_panel_html.h"
#include "html_subst.h"
#include "create_panel_html_data.h"
#include <string>

std::string BuildCreatePanelHTML(bool darkMode) {
    std::string html(reinterpret_cast<const char*>(create_panel_html_data),
                     create_panel_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    return html;
}
