#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <functional>
#include <string>

class ImagePanel : public wxFrame {
public:
    // projectDir: folder where image files are stored alongside the .md file.
    // mdPath:     full path to the markdown file being edited.
    // onChanged:  called after an insert or remove so the caller can re-render.
    ImagePanel(wxWindow* parent, bool darkMode,
               const std::string& projectDir,
               const std::string& mdPath,
               std::function<void()> onChanged = {});

private:
    std::string            m_projectDir;
    std::string            m_mdPath;
    std::function<void()>  m_onChanged;
    wxWebView*             m_webView = nullptr;

    std::string readFile(const std::string& path);
    void        OnScriptMessage(wxWebViewEvent& evt);
};
