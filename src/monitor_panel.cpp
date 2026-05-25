#include "monitor_panel.h"
#include "monitor_panel_html.h"
#include "process_monitor.h"
#include "config.h"
#include "meta.h"
#include <sstream>
#include <wx/sizer.h>
#include <wx/webview.h>

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string js_str(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if      (c == '\'') out += "\\'";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') { /* skip */ }
        else out += c;
    }
    return out + "'";
}

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
             << "\"project\":"  << js_str(p.project) << ","
             << "\"action\":"   << js_str(p.action)  << ","
             << "\"backend\":"  << js_str(p.backend) << ","
             << "\"elapsed\":"  << js_str(p.elapsed) << ","
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
    } else if (action == "archiveTiming") {
        AppConfig cfg = LoadConfig();
        std::string proj = act("project");
        std::string ts   = act("ts");
        bool doArchive = json.find("\"archived\":true") != std::string::npos
                      || json.find("\"archived\": true") != std::string::npos;
        if (!proj.empty() && !ts.empty() && !cfg.defaultFolder.empty())
            ArchiveTimingEntry(cfg.defaultFolder, proj, ts, doArchive);
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

void MonitorPanel::PushTimingLog() {
    AppConfig cfg = LoadConfig();
    if (cfg.defaultFolder.empty()) {
        Run("setTimingLog([])");
        return;
    }
    auto entries = ScanAllTimings(cfg.defaultFolder);
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) json << ",";
        const auto& t = entries[i];
        json << "{"
             << "\"ts\":"       << js_str(t.timestamp)       << ","
             << "\"project\":"  << js_str(t.project)         << ","
             << "\"backend\":"  << js_str(t.backend)         << ","
             << "\"op\":"       << js_str(t.operation)       << ","
             << "\"topic\":"    << js_str(t.topic)           << ","
             << "\"secs\":"     << t.durationSeconds         << ","
             << "\"archived\":" << (t.archived ? "true" : "false")
             << "}";
    }
    json << "]";
    Run("setTimingLog(" + json.str() + ")");
}

void MonitorPanel::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Run(std::string("setDarkMode(") + (dark ? "true" : "false") + ")");
}

void MonitorPanel::Refresh() {
    PushProcessList();
}
