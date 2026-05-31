#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <string>
#include <functional>

class SocialTab : public wxPanel {
public:
    SocialTab(wxWindow* parent, bool darkMode);

    void SetProject(const std::string& projDir,
                    const std::string& selectedDoc,
                    std::function<void()> onChanged);

    void SetDarkMode(bool dark);
    void Reload();

private:
    wxWebView* m_webView  = nullptr;
    bool       m_darkMode = false;
    std::string m_projectDir;
    std::string m_selectedDoc;
    std::function<void()> m_onChanged;

    void OnScriptMessage(wxWebViewEvent& evt);
    std::string readFile(const std::string& path) const;
};
