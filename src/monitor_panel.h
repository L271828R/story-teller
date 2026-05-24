#pragma once
#include <wx/wx.h>
#include <wx/webview.h>

class MonitorPanel : public wxPanel {
public:
    MonitorPanel(wxWindow* parent, bool darkMode = false);
    void SetDarkMode(bool dark);
    void Refresh();   // push fresh process list immediately

private:
    wxWebView* m_webView  = nullptr;
    bool       m_darkMode = false;
    bool       m_ready    = false;

    void PushProcessList();
    void HandleMessage(const std::string& json);
    void Run(const std::string& js);
};
