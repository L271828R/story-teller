#pragma once
#include "corpus.h"
#include <sqlite3.h>
#include <string>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

// Tab for managing the RAG corpus: add PDF/text documents, delete, view stats.
class CorpusPanel : public wxPanel {
public:
    CorpusPanel(wxWindow* parent);
    ~CorpusPanel();

    // Called when the active project changes so the panel opens the right DB.
    void SetProjectDir(const std::string& projectDir);

private:
    void RefreshList();
    void SetStatus(const wxString& msg);

    void OnAddFile(wxCommandEvent&);
    void OnDeleteDoc(wxCommandEvent&);

    sqlite3*      m_db = nullptr;
    std::string   m_projectDir;

    wxListCtrl*   m_list;
    wxButton*     m_addBtn;
    wxButton*     m_deleteBtn;
    wxTextCtrl*   m_statusCtrl;

    wxDECLARE_EVENT_TABLE();
};
