#include "monitor_panel.h"
#include "monitor_panel_html.h"
#include "process_monitor.h"
#include "meta.h"
#include "js_util.h"
#include <sstream>
#include <wx/sizer.h>
#include <wx/webview.h>

// ── MonitorPanel ──────────────────────────────────────────────────────────────

MonitorPanel::MonitorPanel(wxWindow* parent, bool darkMode)
    : wxPanel(parent, wxID_ANY), m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY);
    m_webView->AddScriptMessageHandler("monitor");

    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) {
            HandleMessage(evt.GetString().ToStdString());
        }, wxID_ANY);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    m_webView->SetPage(wxString::FromUTF8(BuildMonitorPanelHTML(darkMode)), "");
}

void MonitorPanel::Run(const std::string& js) {
    if (!m_ready) return;
    m_webView->RunScript(wxString::FromUTF8(js));
}

void MonitorPanel::PushProcessList() {
    auto procs = ScanSpawnedProcesses();
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < procs.size(); ++i) {
        if (i) json << ",";
        const auto& p = procs[i];
        json << "{"
             << "\"pid\":"      << p.pid << ","
             << "\"project\":"  << JsStr(p.project) << ","
             << "\"action\":"   << JsStr(p.action)  << ","
             << "\"backend\":"  << JsStr(p.backend) << ","
             << "\"elapsed\":"  << JsStr(p.elapsed) << ","
             << "\"orphaned\":" << (p.orphaned ? "true" : "false")
             << "}";
    }
    json << "]";
    Run("setProcessList(" + json.str() + ")");
}

void MonitorPanel::HandleMessage(const std::string& json) {
    auto act = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        auto end = json.find('"', pos);
        return end == std::string::npos ? "" : json.substr(pos, end - pos);
    };

    std::string action = act("action");

    if (action == "ready") {
        m_ready = true;
        PushProcessList();
        PushTimingLog();
    } else if (action == "refresh") {
        PushProcessList();
        PushTimingLog();
    } else if (action == "kill") {
        // pid is a number, not a string — extract directly.
        auto pos = json.find("\"pid\":");
        if (pos != std::string::npos) {
            int pid = std::stoi(json.substr(pos + 6));
            KillSpawnedProcess(pid);
            PushProcessList();
        }
    }
}

void MonitorPanel::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Run(std::string("setDarkMode(") + (dark ? "true" : "false") + ")");
}

void MonitorPanel::SetProject(const std::string& projectDir) {
    m_projectDir = projectDir;
    if (m_ready) PushTimingLog();
}

void MonitorPanel::PushTimingLog() {
    if (m_projectDir.empty()) return;
    auto meta = LoadProjectMeta(m_projectDir);
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < meta.timings.size(); ++i) {
        if (i) json << ",";
        const auto& t = meta.timings[i];
        json << "{"
             << "\"ts\":"      << JsStr(t.timestamp)  << ","
             << "\"op\":"      << JsStr(t.operation)  << ","
             << "\"topic\":"   << JsStr(t.topic)      << ","
             << "\"secs\":"    << t.durationSeconds     << ","
             << "\"backend\":" << JsStr(t.backend)
             << "}";
    }
    json << "]";
    Run("setTimingLog(" + json.str() + ")");
}

void MonitorPanel::Refresh() {
    PushProcessList();
    PushTimingLog();
}
