#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

class PersonaPanel : public wxFrame {
public:
    PersonaPanel(wxWindow* parent, bool darkMode,
                 const std::map<std::string, std::vector<std::string>>& categories,
                 const std::map<std::string, std::string>& images,
                 std::function<void()> onImageAdded = {});

private:
    wxWebView*           m_webView      = nullptr;
    std::string          m_html;
    std::function<void()> m_onImageAdded;

    void OnScriptMessage(wxWebViewEvent& evt);
};
