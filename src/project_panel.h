#pragma once
#include <wx/panel.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <functional>
#include <string>

class ProjectPanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string&)>;

    ProjectPanel(wxWindow* parent, OpenCallback onProjectActivated);

    void RefreshProjects();

private:
    void OnProjectSelected(wxCommandEvent& evt);
    void OnProjectActivated(wxCommandEvent& evt);
    void OnActivateBtn(wxCommandEvent& evt);
    void OnRefreshBtn(wxCommandEvent& evt);

    void ActivateSelectedProject();

    wxListBox*    m_projectList;
    wxStaticText* m_projectPathLabel;
    wxButton*     m_activateBtn;
    OpenCallback  m_openCallback;

    wxDECLARE_EVENT_TABLE();
};
