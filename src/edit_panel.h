#pragma once
#include <functional>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/checklst.h>

// Panel for browsing chapters and tidbits and rewriting them via an LLM.
class EditPanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string& filepath)>;
    EditPanel(wxWindow* parent, OpenCallback onFileChanged);

    // Call after a new chapter is generated so the chapter list refreshes.
    void RefreshChapters();

private:
    OpenCallback      m_openCallback;

    // ── Content navigation ────────────────────────────────────────────────
    wxListBox*        m_chapterList;
    wxStaticText*     m_rightLabel;
    wxListBox*        m_tidbitList;
    wxRadioButton*    m_radioTidbit;
    wxRadioButton*    m_radioChapter;

    // ── Rewrite ───────────────────────────────────────────────────────────
    wxTextCtrl*       m_instructCtrl;
    wxButton*         m_rewriteBtn;

    // ── Git version history ───────────────────────────────────────────────
    wxListBox*        m_historyList;   // git log of selected file
    wxTextCtrl*       m_commitMsgCtrl;
    wxButton*         m_commitBtn;
    wxButton*         m_viewVerBtn;
    wxButton*         m_diffBtn;
    wxButton*         m_restoreBtn;

    // ── Status ────────────────────────────────────────────────────────────
    wxTextCtrl*       m_statusCtrl;

    struct TidbitEntry  { int id; std::string preview; };
    struct SectionEntry { int id; std::string preview; };
    std::vector<TidbitEntry>  m_tidbits;
    std::vector<SectionEntry> m_sections;

    // git log parallel to m_historyList rows
    struct CommitEntry { std::string hash; std::string shortHash;
                         std::string date;  std::string subject; };
    std::vector<CommitEntry> m_commits;

    std::string CurrentProjectPath() const;
    std::string CurrentChapterPath() const;
    void        LoadTidbits();
    void        LoadSections();
    void        ReloadRightList();
    void        LoadHistory();

    void OnRefresh(wxCommandEvent&);
    void OnChapterSelected(wxCommandEvent&);
    void OnTargetChanged(wxCommandEvent&);
    void OnRewrite(wxCommandEvent&);
    void OnCommit(wxCommandEvent&);
    void OnViewVersion(wxCommandEvent&);
    void OnDiff(wxCommandEvent&);
    void OnRestore(wxCommandEvent&);
    void SetStatus(const wxString& msg);
    void SetBusy(bool on);

    wxDECLARE_EVENT_TABLE();
};
