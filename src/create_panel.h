#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/webview.h>
#include "config.h"
#include "creator.h"

// Create tab: a wxWebView running create_panel_html.h, communicating with
// C++ business logic via window.webkit.messageHandlers.create / RunScript.
class CreatePanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string& filepath)>;
    CreatePanel(wxWindow* parent, OpenCallback onFileGenerated, bool darkMode = false);
    void SyncProject(const std::string& filePath = "");
    void SetDarkMode(bool dark);

private:
    OpenCallback m_openCallback;
    wxWebView*   m_webView    = nullptr;
    bool         m_darkMode   = false;
    bool         m_ready      = false;   // true after JS fires 'ready'
    bool         m_generating = false;
    bool         m_translating = false;
    std::string  m_pendingSync;          // buffered SyncProject path pre-ready

    // ── Business state ────────────────────────────────────────────────────
    std::string  m_currentProject;       // relative path, e.g. "Literature/agatha"
    std::string  m_selectedCat;
    std::map<std::string, std::vector<std::string>> m_charsByCategory;
    std::set<std::string> m_checkedChars;

    // ── Webview helpers ───────────────────────────────────────────────────
    void Run(const std::string& js);     // RunScript wrapper, no-op if not ready
    void HandleMessage(const std::string& json);

    // ── State push (C++ → JS) ─────────────────────────────────────────────
    void PushInitialState();
    void PushProjectList();
    void PushChapters();
    void PushContext();
    void PushCharLibrary();
    void PushStatus(const std::string& msg);
    void PushGenerating(bool on);
    void PushTranslating(bool on);

    // ── Business logic helpers ────────────────────────────────────────────
    void LoadCharLibrary();
    void SaveCharLibrary() const;
    std::string CurrentProjectPath() const;
    void SelectProject(const std::string& nameOrRel);

    // ── Action handlers (dispatched by HandleMessage) ─────────────────────
    void DoNewProject(const std::string& name);
    void DoSelectProject(const std::string& name);
    void DoGenerate(const std::string& topic, const std::string& style,
                    const std::string& context, const std::string& backend,
                    const std::string& apiKey, const std::string& ollamaModel);
    void DoCopyPrompt(const std::string& topic, const std::string& style,
                      const std::string& context, const std::string& backend,
                      const std::string& apiKey, const std::string& ollamaModel);
    void DoSaveContext(const std::string& text);
    void DoSaveState(const std::string& topic, const std::string& style,
                     const std::string& backend, const std::string& apiKey,
                     const std::string& ollamaModel);
    void DoBackendChanged(const std::string& backend);
    void DoRefreshOllama();
    void DoSelectCategory(const std::string& cat);
    void DoAddCategory(const std::string& name);
    void DoDeleteCategory(const std::string& name);
    void DoAddCharacter(const std::string& cat, const std::string& name);
    void DoDeleteCharacter(const std::string& cat, const std::string& name);
    void DoToggleCharacter(const std::string& name, bool checked);
    void DoOpenFile(const std::string& filename);
    void DoTranslateFile(const std::string& filename, const std::string& language,
                         const std::string& backend, const std::string& apiKey,
                         const std::string& ollamaModel);
    void DoDeleteFile(const std::string& filename);
};
