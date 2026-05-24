#pragma once
#include <functional>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/webview.h>

// Edit tab: file browser + tidbit/section viewer + rewrite/translate/git.
// Implemented as a wxWebView panel; all business logic runs in C++.
class EditPanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string& filepath)>;
    EditPanel(wxWindow* parent, OpenCallback onFileChanged, bool darkMode = false);

    void RefreshChapters();
    void SetDarkMode(bool dark);

private:
    OpenCallback m_openCallback;
    wxWebView*   m_webView    = nullptr;
    bool         m_darkMode   = false;
    bool         m_ready      = false;
    bool         m_pendingRefresh = false;
    bool         m_busy       = false;

    std::vector<std::string> m_files;
    int                      m_selFile = -1;
    bool                     m_showSections  = false;
    std::string              m_rewriteTarget = "tidbit"; // "tidbit"|"chapter"|"document"

    struct TidbitEntry  { int id; std::string preview; };
    struct SectionEntry { int id; std::string preview; };
    std::vector<TidbitEntry>  m_tidbits;
    std::vector<SectionEntry> m_sections;

    struct CommitEntry { std::string hash, shortHash, date, subject; };
    std::vector<CommitEntry> m_commits;

    // ── WebView helpers ────────────────────────────────────────────────────
    void Run(const std::string& js);
    void HandleMessage(const std::string& json);

    // ── State pushers ──────────────────────────────────────────────────────
    void PushFiles();
    void PushItems();
    void PushHistory();
    void PushStatus(const std::string& msg);
    void PushBusy(bool on);

    // ── Business logic ─────────────────────────────────────────────────────
    std::string CurrentProjectPath() const;
    std::string ChapterPath(const std::string& filename) const;
    void        DoRefresh();
    void        LoadItemsForFile(const std::string& filename);
    void        LoadHistoryForProject();
    void        SaveCurrentFileOrder() const;
    void        OpenInVim(const std::string& path);

    // ── Action handlers ────────────────────────────────────────────────────
    void OnSelectFile(const std::string& filename);
    void OnOpenFile(const std::string& filename, const std::string& mode);
    void OnMoveFile(const std::string& filename, const std::string& direction);
    void OnRenameFile();
    void OnDeleteFile();
    void OnRewrite(const std::string& json);
    void OnTranslate(const std::string& json);
    void OnCommit(const std::string& filename, const std::string& message);
    void OnViewVersion(const std::string& commitHash);
    void OnDiff(const std::string& filename, const std::string& hash1, const std::string& hash2);
    void OnRestore(const std::string& filename, const std::string& commitHash,
                   const std::string& shortHash, const std::string& subject);
    void OnCheckout(const std::string& commitHash, const std::string& shortHash,
                    const std::string& subject);
    void OnStash();
    void OnUnstash();
};
