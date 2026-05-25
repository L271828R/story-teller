#include "monitor_panel_html.h"
#include "html_subst.h"
#include "monitor_panel_html_data.h"
#include <string>

std::string BuildMonitorPanelHTML(bool darkMode) {
    std::string html(reinterpret_cast<const char*>(monitor_panel_html_data),
                     monitor_panel_html_data_len);
    htmlSubst(html, "{{BODY_CLASS}}", darkMode ? "dark" : "");
    return html;
}
