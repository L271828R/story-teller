#include "project_panel.h"
#include "config.h"
#include "project.h"
#include "logger.h"
#include <filesystem>
#include <algorithm>
#include <wx/sizer.h>
#include <wx/statline.h>

namespace fs = std::filesystem;

enum {
    ID_PP_PROJECT_LIST = wxID_HIGHEST + 400,
    ID_PP_ACTIVATE,
    ID_PP_REFRESH
};

wxBEGIN_EVENT_TABLE(ProjectPanel, wxPanel)
    EVT_LISTBOX(ID_PP_PROJECT_LIST,      ProjectPanel::OnProjectSelected)
    EVT_LISTBOX_DCLICK(ID_PP_PROJECT_LIST, ProjectPanel::OnProjectActivated)
    EVT_BUTTON(ID_PP_ACTIVATE,           ProjectPanel::OnActivateBtn)
    EVT_BUTTON(ID_PP_REFRESH,            ProjectPanel::OnRefreshBtn)
wxEND_EVENT_TABLE()

ProjectPanel::ProjectPanel(wxWindow* parent, OpenCallback onProjectActivated)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onProjectActivated))
{
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* inner = new wxBoxSizer(wxVERTICAL);

    inner->Add(new wxStaticText(this, wxID_ANY, "Available Projects:"), 0, wxBOTTOM, 6);

    m_projectList = new wxListBox(this, ID_PP_PROJECT_LIST,
                                  wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SINGLE);
    inner->Add(m_projectList, 1, wxEXPAND | wxBOTTOM, 10);

    m_projectPathLabel = new wxStaticText(this, wxID_ANY, "Select a project to see its path.");
    wxFont small = m_projectPathLabel->GetFont();
    small.SetPointSize(small.GetPointSize() - 1);
    m_projectPathLabel->SetFont(small);
    m_projectPathLabel->SetForegroundColour(wxColour(100, 100, 100));
    inner->Add(m_projectPathLabel, 0, wxBOTTOM, 10);

    auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
    m_activateBtn = new wxButton(this, ID_PP_ACTIVATE, "Activate Project");
    m_activateBtn->Disable();
    btnRow->Add(m_activateBtn, 0, wxRIGHT, 10);

    btnRow->Add(new wxButton(this, ID_PP_REFRESH, "Refresh List"), 0);
    inner->Add(btnRow, 0, wxEXPAND | wxBOTTOM, 10);

    outer->Add(inner, 1, wxEXPAND | wxALL, 14);
    SetSizer(outer);

    SetBackgroundColour(wxColour(240, 240, 245));

    RefreshProjects();
}

void ProjectPanel::RefreshProjects() {
    m_projectList->Clear();
    m_activateBtn->Disable();
    m_projectPathLabel->SetLabel("Select a project to see its path.");

    AppConfig cfg = LoadConfig();
    if (cfg.defaultFolder.empty()) {
        m_projectPathLabel->SetLabel("Error: defaultFolder not set in config.");
        return;
    }

    if (!fs::exists(cfg.defaultFolder)) {
        m_projectPathLabel->SetLabel("Error: defaultFolder does not exist: " + cfg.defaultFolder);
        return;
    }

    std::vector<std::string> projects;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cfg.defaultFolder, ec)) {
        if (entry.is_directory(ec)) {
            if (ProjectExists(entry.path().string())) {
                projects.push_back(entry.path().filename().string());
            }
        }
    }

    std::sort(projects.begin(), projects.end());

    for (const auto& p : projects) {
        m_projectList->Append(wxString::FromUTF8(p));
    }

    // Try to select the current project if it exists in the list
    AppState st = LoadAppState();
    if (!st.currentProject.empty()) {
        int idx = m_projectList->FindString(wxString::FromUTF8(st.currentProject));
        if (idx != wxNOT_FOUND) {
            m_projectList->SetSelection(idx);
            m_activateBtn->Enable();
            m_projectPathLabel->SetLabel(cfg.defaultFolder + "/" + st.currentProject);
        }
    }
}

void ProjectPanel::OnProjectSelected(wxCommandEvent&) {
    int sel = m_projectList->GetSelection();
    if (sel == wxNOT_FOUND) {
        m_activateBtn->Disable();
        m_projectPathLabel->SetLabel("Select a project to see its path.");
        return;
    }

    m_activateBtn->Enable();
    AppConfig cfg = LoadConfig();
    m_projectPathLabel->SetLabel(cfg.defaultFolder + "/" + m_projectList->GetString(sel));
}

void ProjectPanel::OnProjectActivated(wxCommandEvent&) {
    ActivateSelectedProject();
}

void ProjectPanel::OnActivateBtn(wxCommandEvent&) {
    ActivateSelectedProject();
}

void ProjectPanel::OnRefreshBtn(wxCommandEvent&) {
    RefreshProjects();
}

void ProjectPanel::ActivateSelectedProject() {
    int sel = m_projectList->GetSelection();
    if (sel == wxNOT_FOUND) return;

    std::string projectName = m_projectList->GetString(sel).ToStdString();
    AppConfig cfg = LoadConfig();
    std::string projectPath = cfg.defaultFolder + "/" + projectName;

    // Save app state
    AppState st = LoadAppState();
    st.currentProject = projectName;
    SaveAppState(st);

    Logger::get().log("Activating project: " + projectName);

    // Find a file to open: prefer story chapters over claude.md.
    std::string fileToOpen;
    {
        std::error_code ec;
        std::vector<std::string> mdFiles;
        for (auto& entry : fs::directory_iterator(projectPath, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".md"
                    && entry.path().filename() != "claude.md") {
                mdFiles.push_back(entry.path().string());
            }
        }
        std::sort(mdFiles.begin(), mdFiles.end());
        if (!mdFiles.empty()) {
            fileToOpen = mdFiles[0];
        } else {
            // Fall back to claude.md if it's the only file present
            std::string claudePath = projectPath + "/claude.md";
            if (fs::exists(claudePath)) fileToOpen = claudePath;
        }
    }

    if (!fileToOpen.empty()) {
        if (m_openCallback) {
            m_openCallback(fileToOpen);
        }
    } else {
        Logger::get().log("Activated project " + projectName + " but no .md files found.");
    }
}
