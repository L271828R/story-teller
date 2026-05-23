#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/checklst.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include "config.h"
#include "creator.h"

// Self-contained form panel for content generation.
// When a chapter is saved, onFileGenerated is called with its absolute path.
class CreatePanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string& filepath)>;
    CreatePanel(wxWindow* parent, OpenCallback onFileGenerated);
    void SyncProject(const std::string& filePath = "");

private:
    OpenCallback   m_openCallback;

    // ── Form fields ───────────────────────────────────────────────────────
    wxChoice*        m_projectChoice;
    wxStaticText*    m_projectPathLabel;
    wxTextCtrl*      m_topicCtrl;
    wxComboBox*      m_styleChoice;

    // ── Character library ─────────────────────────────────────────────────
    wxListBox*       m_catList;     // left: category names
    wxCheckListBox*  m_charList;    // right: characters in selected category

    // category → ordered list of character names
    std::map<std::string, std::vector<std::string>> m_charsByCategory;
    // names that are checked for inclusion (survives category switches)
    std::set<std::string> m_checkedChars;

    // ── Project context (context.md) ───────────────────────────────────────
    wxTextCtrl*      m_contextCtrl;

    // ── Backend ───────────────────────────────────────────────────────────
    wxChoice*        m_backendChoice;
    wxTextCtrl*      m_apiKeyCtrl;
    wxSizerItem*     m_apiKeySizer  = nullptr;
    wxComboBox*      m_ollamaModel;
    wxSizerItem*     m_ollamaSizer  = nullptr;

    wxButton*        m_generateBtn;
    wxButton*        m_translateBtn = nullptr;
    wxListCtrl*      m_chapterListBox;
    wxTextCtrl*      m_statusCtrl;

    bool m_generating = false;

    // ── Character library helpers ─────────────────────────────────────────
    void LoadCharLibrary();
    void SaveCharLibrary() const;
    void RefreshCharList();          // rebuild right panel from selected category
    std::string SelectedCategory() const;

    // ── Event handlers ────────────────────────────────────────────────────
    void OnNewProject(wxCommandEvent&);
    void OnProjectSelected(wxCommandEvent&);
    void OnSave(wxCommandEvent&);
    void OnCatSelected(wxCommandEvent&);
    void OnCharToggled(wxCommandEvent&);
    void OnAddCategory(wxCommandEvent&);
    void OnDeleteCategory(wxCommandEvent&);
    void OnAddCharacter(wxCommandEvent&);
    void OnDeleteCharacter(wxCommandEvent&);
    void OnSaveContext(wxCommandEvent&);
    void OnBackendChanged(wxCommandEvent&);
    void OnGenerate(wxCommandEvent&);
    void OnCopyPrompt(wxCommandEvent&);
    void OnOpenInView(wxCommandEvent&);
    void OnChapterActivated(wxListEvent&);
    void OnTranslate(wxCommandEvent&);
    void OnDeleteFile(wxCommandEvent&);

    GenerationRequest BuildRequest() const;
    void UpdateBackendFields();
    void SetStatus(const wxString& msg);
    void SetGenerating(bool on);

    void LoadProjects();
    void LoadChapters();
    void LoadContext();
    void SaveContext();
    void SelectProject(const wxString& name);
    wxString CurrentProjectPath() const;
    void SaveFormState() const;
    void RestoreFormState(const AppState& st);

    wxDECLARE_EVENT_TABLE();
};
