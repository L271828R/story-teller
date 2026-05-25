#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <functional>

class PromptsTab : public wxPanel {
public:
    PromptsTab(wxWindow* parent, bool darkMode);

    void SetDarkMode(bool dark);
    void Reload();

    // Called by the create panel to insert a prompt into the topic field.
    // onChanged is called whenever prompts change so the create panel can refresh.
    void SetOnChanged(std::function<void()> cb);

private:
    wxWebView* m_webView  = nullptr;
    bool       m_darkMode = false;
    std::function<void()> m_onChanged;

    void OnScriptMessage(wxWebViewEvent& evt);
};
