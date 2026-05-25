#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <string>
#include <functional>

class QuizTab : public wxPanel {
public:
    QuizTab(wxWindow* parent, bool darkMode);

    // Call when the active project/document changes.
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
    void        writeFile(const std::string& path, const std::string& content);
};
