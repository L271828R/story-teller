#include "edit_panel_html.h"
#include "html_subst.h"
#include "edit_panel_html_data.h"
#include <string>

std::string BuildEditPanelHTML(bool darkMode) {
    std::string html(reinterpret_cast<const char*>(edit_panel_html_data),
                     edit_panel_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    return html;
}
