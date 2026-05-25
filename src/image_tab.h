#pragma once
#include <functional>
#include <string>
#include <wx/wx.h>
#include <wx/webview.h>

// Permanent notebook tab for per-project image management.
// Upload images to the project folder, select one, pick any .md doc in the
// project, set size/align/heading, then place, resize, or remove an anchor.
class ImageTab : public wxPanel {
public:
    ImageTab(wxWindow* parent, bool darkMode);

    // Called when the active project/file changes.
    // projDir:     folder containing the project's .md files and images.
    // selectedDoc: filename (basename) of the .md file to pre-select.
    // onChanged:   called after an insert, resize, or remove so the caller
    //              can re-render the currently-open document.
    void SetProject(const std::string& projDir,
                    const std::string& selectedDoc,
                    std::function<void()> onChanged = {});

    void SetDarkMode(bool dark);
    void Reload();

private:
    std::string           m_projectDir;
    std::string           m_selectedDoc; // basename only, e.g. "chapter1.md"
    std::function<void()> m_onChanged;
    wxWebView*            m_webView  = nullptr;
    bool                  m_darkMode = false;

    std::string readFile(const std::string& path) const;
    void        writeFile(const std::string& path, const std::string& content);
    void        OnScriptMessage(wxWebViewEvent& evt);
};
