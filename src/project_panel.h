#pragma once
#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <functional>
#include <string>
#include <vector>

class ProjectPanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string&)>;

    ProjectPanel(wxWindow* parent, OpenCallback onProjectActivated);

    void RefreshProjects();

private:
    struct ProjectRow {
        std::string name;
        std::string path;
    };

    void OnProjectSelected(wxListEvent& evt);
    void OnProjectActivated(wxListEvent& evt);
    void OnColumnClicked(wxListEvent& evt);
    void OnSearchChanged(wxCommandEvent& evt);
    void OnActivateBtn(wxCommandEvent& evt);
    void OnRenameBtn(wxCommandEvent& evt);
    void OnRefreshBtn(wxCommandEvent& evt);

    void ApplyProjectFilter();
    std::string SortableValue(const ProjectRow& project, int column) const;
    int SelectedProjectIndex() const;
    void ActivateSelectedProject();

    wxTextCtrl*   m_searchCtrl;
    wxListCtrl*   m_projectList;
    wxStaticText* m_projectPathLabel;
    wxStaticText* m_statsLabel;
    wxButton*     m_activateBtn;
    wxButton*     m_renameBtn;
    std::vector<ProjectRow> m_allProjects;
    std::vector<ProjectRow> m_projects;
    int           m_sortColumn = 0;
    bool          m_sortAscending = true;
    OpenCallback  m_openCallback;

    wxDECLARE_EVENT_TABLE();
};
