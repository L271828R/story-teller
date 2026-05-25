#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <string>

class MonitorPanel : public wxPanel {
public:
    MonitorPanel(wxWindow* parent, bool darkMode = false);
    void SetDarkMode(bool dark);
    void SetProject(const std::string& projectDir);
    void Refresh();   // push fresh process list immediately

private:
    wxWebView*  m_webView    = nullptr;
    bool        m_darkMode   = false;
    bool        m_ready      = false;
    std::string m_projectDir;

    void PushProcessList();
    void PushTimingLog();
    void HandleMessage(const std::string& json);
    void Run(const std::string& js);
};
