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

    wxListBox*        m_chapterList;
    wxStaticText*     m_rightLabel;
    wxListBox*        m_tidbitList;
    wxRadioButton*    m_radioTidbit;
    wxRadioButton*    m_radioChapter;
    wxTextCtrl*       m_instructCtrl;
    wxButton*         m_rewriteBtn;
    wxTextCtrl*       m_statusCtrl;

    struct TidbitEntry  { int id; std::string preview; };
    struct SectionEntry { int id; std::string preview; };
    std::vector<TidbitEntry>  m_tidbits;
    std::vector<SectionEntry> m_sections;

    std::string CurrentProjectPath() const;
    std::string CurrentChapterPath() const;
    void        LoadTidbits();
    void        LoadSections();
    void        ReloadRightList();

    void OnRefresh(wxCommandEvent&);
    void OnChapterSelected(wxCommandEvent&);
    void OnTargetChanged(wxCommandEvent&);
    void OnRewrite(wxCommandEvent&);
    void SetStatus(const wxString& msg);
    void SetBusy(bool on);

    wxDECLARE_EVENT_TABLE();
};
