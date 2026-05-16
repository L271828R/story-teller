#include "project_panel.h"
#include "config.h"
#include "project.h"
#include "logger.h"
#include "meta.h"
#include "project_search.h"
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/textdlg.h>

namespace fs = std::filesystem;

enum {
    ID_PP_PROJECT_LIST = wxID_HIGHEST + 400,
    ID_PP_SEARCH,
    ID_PP_ACTIVATE,
    ID_PP_RENAME,
    ID_PP_REFRESH
};

wxBEGIN_EVENT_TABLE(ProjectPanel, wxPanel)
    EVT_TEXT(ID_PP_SEARCH,                 ProjectPanel::OnSearchChanged)
    EVT_LIST_ITEM_SELECTED(ID_PP_PROJECT_LIST, ProjectPanel::OnProjectSelected)
    EVT_LIST_ITEM_ACTIVATED(ID_PP_PROJECT_LIST, ProjectPanel::OnProjectActivated)
    EVT_LIST_COL_CLICK(ID_PP_PROJECT_LIST, ProjectPanel::OnColumnClicked)
    EVT_BUTTON(ID_PP_ACTIVATE,           ProjectPanel::OnActivateBtn)
    EVT_BUTTON(ID_PP_RENAME,             ProjectPanel::OnRenameBtn)
    EVT_BUTTON(ID_PP_REFRESH,            ProjectPanel::OnRefreshBtn)
wxEND_EVENT_TABLE()

ProjectPanel::ProjectPanel(wxWindow* parent, OpenCallback onProjectActivated)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onProjectActivated))
{
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* inner = new wxBoxSizer(wxVERTICAL);

    inner->Add(new wxStaticText(this, wxID_ANY, "Available Projects:"), 0, wxBOTTOM, 6);

    m_searchCtrl = new wxTextCtrl(this, ID_PP_SEARCH, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER);
    m_searchCtrl->SetHint("Search projects...");
    inner->Add(m_searchCtrl, 0, wxEXPAND | wxBOTTOM, 8);

    m_projectList = new wxListCtrl(this, ID_PP_PROJECT_LIST,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxLC_REPORT | wxLC_SINGLE_SEL);
    m_projectList->AppendColumn("Project", wxLIST_FORMAT_LEFT, 210);
    m_projectList->AppendColumn("Created", wxLIST_FORMAT_LEFT, 140);
    m_projectList->AppendColumn("Modified", wxLIST_FORMAT_LEFT, 140);
    m_projectList->AppendColumn("Source", wxLIST_FORMAT_LEFT, 130);
    m_projectList->AppendColumn("Last LLM", wxLIST_FORMAT_LEFT, 280);
    inner->Add(m_projectList, 1, wxEXPAND | wxBOTTOM, 10);

    m_projectPathLabel = new wxStaticText(this, wxID_ANY, "Select a project to see its path.");
    wxFont small = m_projectPathLabel->GetFont();
    small.SetPointSize(small.GetPointSize() - 1);
    m_projectPathLabel->SetFont(small);
    m_projectPathLabel->SetForegroundColour(wxColour(100, 100, 100));
    inner->Add(m_projectPathLabel, 0, wxBOTTOM, 4);

    m_statsLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_statsLabel->SetFont(small);
    m_statsLabel->SetForegroundColour(wxColour(80, 100, 130));
    inner->Add(m_statsLabel, 0, wxBOTTOM, 10);

    auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
    m_activateBtn = new wxButton(this, ID_PP_ACTIVATE, "Activate Project");
    m_activateBtn->Disable();
    btnRow->Add(m_activateBtn, 0, wxRIGHT, 10);

    m_renameBtn = new wxButton(this, ID_PP_RENAME, "Rename Project");
    m_renameBtn->Disable();
    btnRow->Add(m_renameBtn, 0, wxRIGHT, 10);

    btnRow->Add(new wxButton(this, ID_PP_REFRESH, "Refresh List"), 0);
    inner->Add(btnRow, 0, wxEXPAND | wxBOTTOM, 10);

    outer->Add(inner, 1, wxEXPAND | wxALL, 14);
    SetSizer(outer);

    SetBackgroundColour(wxColour(240, 240, 245));

    RefreshProjects();
}

static std::string fmtTs(const std::string& ts) {
    // "2026-05-16T14:23:00" → "2026-05-16 14:23"
    if (ts.size() < 16) return ts;
    return ts.substr(0, 10) + " " + ts.substr(11, 5);
}

static std::string fmtFileTime(fs::file_time_type ft) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t t = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

static std::string modifiedTime(const std::string& projectPath) {
    std::error_code ec;
    auto ft = fs::last_write_time(projectPath, ec);
    return ec ? "" : fmtFileTime(ft);
}

static std::string fmtSecs(int s) {
    if (s < 60) return std::to_string(s) + "s";
    return std::to_string(s / 60) + "m " + std::to_string(s % 60) + "s";
}

static std::string lastLLMSummary(const ProjectMeta& meta) {
    if (meta.timings.empty()) return "";
    const auto& last = meta.timings.back();
    std::string out = last.operation + ": " + fmtSecs(last.durationSeconds);
    if (!last.topic.empty()) out += " - " + last.topic;
    return out;
}

static std::string buildStats(const std::string& projectPath) {
    auto meta = LoadProjectMeta(projectPath);
    if (meta.created.empty() && meta.lastOpened.empty() &&
        meta.source.empty() && meta.timings.empty()) return "";
    std::string out;
    if (!meta.created.empty())
        out += "Created: " + fmtTs(meta.created);
    if (!meta.lastOpened.empty())
        out += (out.empty() ? "" : "   ") +
               std::string("Opened: ") + fmtTs(meta.lastOpened);
    if (!meta.source.empty())
        out += (out.empty() ? "" : "   ") + std::string("Source: ") + meta.source;
    if (!meta.timings.empty()) {
        if (!out.empty()) out += "   ";
        out += "Last " + lastLLMSummary(meta);
    }
    return out;
}

static std::string sourceFromConfig(const std::string& projectPath) {
    ProjectConfig cfg = LoadConfig(projectPath);
    return cfg.llmBackend;
}

static bool validProjectName(const std::string& name) {
    return !name.empty() &&
           name.find('/') == std::string::npos &&
           name.find('\\') == std::string::npos &&
           name != "." && name != "..";
}

static std::string lowerAscii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out.push_back((char)std::tolower(c));
    return out;
}

static bool projectMatchesQuery(const std::string& name,
                                const std::string& path,
                                const std::string& query) {
    if (query.empty()) return true;
    ProjectMeta meta = LoadProjectMeta(path);
    return ProjectMatchesSearch(name, path, meta.source, lastLLMSummary(meta), query);
}

static void setProjectColumns(wxListCtrl* list, long row,
                              const std::string& projectPath,
                              const ProjectMeta& meta) {
    list->SetItem(row, 1, wxString::FromUTF8(fmtTs(meta.created)));
    list->SetItem(row, 2, wxString::FromUTF8(modifiedTime(projectPath)));
    list->SetItem(row, 3, wxString::FromUTF8(meta.source.empty() ? "Unknown" : meta.source));
    list->SetItem(row, 4, wxString::FromUTF8(lastLLMSummary(meta)));
}

void ProjectPanel::RefreshProjects() {
    m_projectList->DeleteAllItems();
    m_allProjects.clear();
    m_projects.clear();
    m_activateBtn->Disable();
    m_renameBtn->Disable();
    m_projectPathLabel->SetLabel("Select a project to see its path.");
    m_statsLabel->SetLabel(wxEmptyString);

    AppConfig cfg = LoadConfig();
    if (cfg.defaultFolder.empty()) {
        m_projectPathLabel->SetLabel("Error: defaultFolder not set in config.");
        m_statsLabel->SetLabel(wxEmptyString);
        m_renameBtn->Disable();
        return;
    }

    if (!fs::exists(cfg.defaultFolder)) {
        m_projectPathLabel->SetLabel("Error: defaultFolder does not exist: " + cfg.defaultFolder);
        m_statsLabel->SetLabel(wxEmptyString);
        m_renameBtn->Disable();
        return;
    }

    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cfg.defaultFolder, ec)) {
        if (entry.is_directory(ec)) {
            if (ProjectExists(entry.path().string())) {
                m_allProjects.push_back({entry.path().filename().string(), entry.path().string()});
            }
        }
    }

    std::sort(m_allProjects.begin(), m_allProjects.end(),
              [](const ProjectRow& a, const ProjectRow& b) { return a.name < b.name; });

    ApplyProjectFilter();
}

void ProjectPanel::ApplyProjectFilter() {
    m_projectList->DeleteAllItems();
    m_projects.clear();
    m_activateBtn->Disable();
    m_renameBtn->Disable();
    m_projectPathLabel->SetLabel("Select a project to see its path.");
    m_statsLabel->SetLabel(wxEmptyString);

    std::string query = m_searchCtrl ? m_searchCtrl->GetValue().ToStdString() : "";
    for (const auto& p : m_allProjects) {
        if (projectMatchesQuery(p.name, p.path, query))
            m_projects.push_back(p);
    }

    std::sort(m_projects.begin(), m_projects.end(),
              [this](const ProjectRow& a, const ProjectRow& b) {
                  std::string av = SortableValue(a, m_sortColumn);
                  std::string bv = SortableValue(b, m_sortColumn);
                  if (av == bv)
                      return m_sortAscending ? a.name < b.name : a.name > b.name;
                  return m_sortAscending ? av < bv : av > bv;
              });

    for (std::size_t i = 0; i < m_projects.size(); ++i) {
        auto& p = m_projects[i];
        std::string source = sourceFromConfig(p.path);
        EnsureProjectMeta(p.path, source);
        ProjectMeta meta = LoadProjectMeta(p.path);
        long row = m_projectList->InsertItem((long)i, wxString::FromUTF8(p.name));
        m_projectList->SetItemData(row, (long)i);
        setProjectColumns(m_projectList, row, p.path, meta);
    }

    if (m_projects.empty()) {
        m_projectPathLabel->SetLabel(query.empty()
                                     ? "No projects found."
                                     : "No projects match the current search.");
        return;
    }

    // Try to select the current project if it is visible in the filtered list.
    AppState st = LoadAppState();
    if (!st.currentProject.empty()) {
        for (long idx = 0; idx < (long)m_projects.size(); ++idx) {
            if (m_projects[(std::size_t)idx].name != st.currentProject) continue;
            m_projectList->SetItemState(idx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_projectList->EnsureVisible(idx);
            m_activateBtn->Enable();
            m_renameBtn->Enable();
            std::string projectPath = m_projects[(std::size_t)idx].path;
            m_projectPathLabel->SetLabel(projectPath);
            m_statsLabel->SetLabel(wxString::FromUTF8(buildStats(projectPath)));
            break;
        }
    }
}

std::string ProjectPanel::SortableValue(const ProjectRow& project, int column) const {
    ProjectMeta meta = LoadProjectMeta(project.path);
    switch (column) {
        case 0: return lowerAscii(project.name);
        case 1: return meta.created;
        case 2: return modifiedTime(project.path);
        case 3: return lowerAscii(meta.source.empty() ? "Unknown" : meta.source);
        case 4: return lowerAscii(lastLLMSummary(meta));
        default: return lowerAscii(project.name);
    }
}

void ProjectPanel::OnSearchChanged(wxCommandEvent&) {
    ApplyProjectFilter();
}

void ProjectPanel::OnColumnClicked(wxListEvent& evt) {
    int column = evt.GetColumn();
    if (column < 0) return;
    if (column == m_sortColumn)
        m_sortAscending = !m_sortAscending;
    else {
        m_sortColumn = column;
        m_sortAscending = true;
    }
    ApplyProjectFilter();
}

int ProjectPanel::SelectedProjectIndex() const {
    long item = m_projectList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1) return wxNOT_FOUND;
    long data = m_projectList->GetItemData(item);
    if (data < 0 || data >= (long)m_projects.size()) return wxNOT_FOUND;
    return (int)data;
}

void ProjectPanel::OnProjectSelected(wxListEvent&) {
    int sel = SelectedProjectIndex();
    if (sel == wxNOT_FOUND) {
        m_activateBtn->Disable();
        m_renameBtn->Disable();
        m_projectPathLabel->SetLabel("Select a project to see its path.");
        m_statsLabel->SetLabel(wxEmptyString);
        return;
    }

    m_activateBtn->Enable();
    m_renameBtn->Enable();
    std::string projectPath = m_projects[(std::size_t)sel].path;
    m_projectPathLabel->SetLabel(projectPath);
    m_statsLabel->SetLabel(wxString::FromUTF8(buildStats(projectPath)));
    Layout();
}

void ProjectPanel::OnProjectActivated(wxListEvent&) {
    ActivateSelectedProject();
}

void ProjectPanel::OnActivateBtn(wxCommandEvent&) {
    ActivateSelectedProject();
}

void ProjectPanel::OnRenameBtn(wxCommandEvent&) {
    int sel = SelectedProjectIndex();
    if (sel == wxNOT_FOUND) return;

    std::string oldName = m_projects[(std::size_t)sel].name;
    std::string oldPath = m_projects[(std::size_t)sel].path;
    wxString entered = wxGetTextFromUser("Enter a new project name:",
                                         "Rename Project",
                                         wxString::FromUTF8(oldName),
                                         this).Trim();
    if (entered.empty()) return;

    std::string newName = entered.ToStdString();
    if (newName == oldName) return;
    if (!validProjectName(newName)) {
        wxMessageBox("Use a folder-safe project name without slashes.",
                     "Invalid Project Name", wxOK | wxICON_WARNING, this);
        return;
    }

    fs::path oldProject(oldPath);
    fs::path newProject = oldProject.parent_path() / newName;
    if (fs::exists(newProject)) {
        wxMessageBox("A project with that name already exists.",
                     "Rename Project", wxOK | wxICON_WARNING, this);
        return;
    }

    std::error_code ec;
    fs::rename(oldProject, newProject, ec);
    if (ec) {
        wxMessageBox(wxString::FromUTF8("Could not rename project: " + ec.message()),
                     "Rename Project", wxOK | wxICON_ERROR, this);
        return;
    }

    AppState st = LoadAppState();
    if (st.currentProject == oldName) {
        st.currentProject = newName;
        SaveAppState(st);
    }

    Logger::get().log("Renamed project: " + oldName + " -> " + newName);
    RefreshProjects();
}

void ProjectPanel::OnRefreshBtn(wxCommandEvent&) {
    RefreshProjects();
}

void ProjectPanel::ActivateSelectedProject() {
    int sel = SelectedProjectIndex();
    if (sel == wxNOT_FOUND) return;

    std::string projectName = m_projects[(std::size_t)sel].name;
    std::string projectPath = m_projects[(std::size_t)sel].path;

    // Save app state
    AppState st = LoadAppState();
    st.currentProject = projectName;
    SaveAppState(st);

    Logger::get().log("Activating project: " + projectName);
    RecordOpen(projectPath);

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
