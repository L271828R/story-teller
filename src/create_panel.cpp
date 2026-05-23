#include "create_panel.h"
#include "config.h"
#include "logger.h"
#include "creator.h"
#include "llm.h"
#include "llm_response.h"
#include "markdown.h"
#include "meta.h"
#include "project.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/clipbrd.h>
#include <wx/config.h>
#include <wx/dataobj.h>
#include <wx/dirdlg.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include "filemeta.h"
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/tokenzr.h>

namespace fs = std::filesystem;

enum {
    ID_CP_NEW_PROJECT = wxID_HIGHEST + 200,
    ID_CP_PROJECT_SEL,
    ID_CP_BACKEND,
    ID_CP_REFRESH_OLLAMA,
    ID_CP_GENERATE,
    ID_CP_COPY_PROMPT,
    ID_CP_SAVE,
    ID_CP_OPEN_VIEW,
    ID_CP_CHAPTER_LIST,
    ID_CP_CAT_LIST,
    ID_CP_ADD_CAT,
    ID_CP_DEL_CAT,
    ID_CP_CHAR_LIST,
    ID_CP_ADD_CHAR,
    ID_CP_DEL_CHAR,
    ID_CP_SAVE_CONTEXT,
    ID_CP_TRANSLATE,
    ID_CP_DELETE_FILE,
};

wxBEGIN_EVENT_TABLE(CreatePanel, wxPanel)
    EVT_BUTTON(ID_CP_NEW_PROJECT,    CreatePanel::OnNewProject)
    EVT_CHOICE(ID_CP_PROJECT_SEL,    CreatePanel::OnProjectSelected)
    EVT_BUTTON(ID_CP_SAVE,           CreatePanel::OnSave)
    EVT_LISTBOX(ID_CP_CAT_LIST,      CreatePanel::OnCatSelected)
    EVT_CHECKLISTBOX(ID_CP_CHAR_LIST, CreatePanel::OnCharToggled)
    EVT_BUTTON(ID_CP_ADD_CAT,     CreatePanel::OnAddCategory)
    EVT_BUTTON(ID_CP_DEL_CAT,     CreatePanel::OnDeleteCategory)
    EVT_BUTTON(ID_CP_ADD_CHAR,    CreatePanel::OnAddCharacter)
    EVT_BUTTON(ID_CP_DEL_CHAR,    CreatePanel::OnDeleteCharacter)
    EVT_CHOICE(ID_CP_BACKEND,     CreatePanel::OnBackendChanged)
    EVT_BUTTON(ID_CP_GENERATE,      CreatePanel::OnGenerate)
    EVT_BUTTON(ID_CP_COPY_PROMPT,   CreatePanel::OnCopyPrompt)
    EVT_BUTTON(ID_CP_SAVE_CONTEXT,  CreatePanel::OnSaveContext)
    EVT_BUTTON(ID_CP_TRANSLATE,     CreatePanel::OnTranslate)
    EVT_BUTTON(ID_CP_DELETE_FILE,   CreatePanel::OnDeleteFile)
    EVT_BUTTON(ID_CP_OPEN_VIEW,     CreatePanel::OnOpenInView)
    EVT_LIST_ITEM_ACTIVATED(ID_CP_CHAPTER_LIST, CreatePanel::OnChapterActivated)
wxEND_EVENT_TABLE()

static wxArrayString make_styles() {
    wxArrayString s;
    for (auto* n : {"Academic essay", "Children's book", "Crime noir",
                    "Fairy tale", "Horror", "Long-form essay",
                    "Podcast transcript", "Popular science",
                    "Socratic dialogue", "Tech blog post"})
        s.Add(n);
    return s;
}

static wxArrayString make_backends() {
    wxArrayString s;
    for (auto* n : {"Clipboard (manual)", "claude -p", "Codex CLI", "Gemini CLI", "Ollama (local)", "Anthropic API"})
        s.Add(n);
    return s;
}

static LLMBackend backend_from_label(const std::string& label) {
    return BackendFromLabel(label);
}

static std::vector<std::string> load_ollama_models() {
    FILE* pipe = popen("curl -s --max-time 1 http://localhost:11434/api/tags 2>/dev/null", "r");
    if (!pipe) return {};
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    return ParseOllamaTags(out);
}

static std::string trim_copy(const std::string& s) {
    const std::string ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

static std::string truncate_for_log(const std::string& s, std::size_t maxLen = 240) {
    return s.size() <= maxLen ? s : s.substr(0, maxLen) + "...";
}

static std::string language_from_topic(const std::string& topic) {
    std::string lower = topic;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    std::string needle = "language:";
    auto pos = lower.find(needle);
    if (pos == std::string::npos) return "(not specified)";
    pos += needle.size();
    auto end = topic.find_first_of(".\n\r", pos);
    return trim_copy(topic.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
}

// ---------------------------------------------------------------------------
CreatePanel::CreatePanel(wxWindow* parent, OpenCallback onFileGenerated)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onFileGenerated))
{
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* inner = new wxBoxSizer(wxVERTICAL);

    // ── Project selector ──────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Project:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        m_projectChoice = new wxChoice(this, ID_CP_PROJECT_SEL,
                                       wxDefaultPosition, wxSize(240, -1));
        row->Add(m_projectChoice, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        row->Add(new wxButton(this, ID_CP_NEW_PROJECT, "New…",
                              wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
                 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxEXPAND | wxBOTTOM, 4);

        m_projectPathLabel = new wxStaticText(this, wxID_ANY, wxEmptyString);
        wxFont small = m_projectPathLabel->GetFont();
        small.SetPointSize(small.GetPointSize() - 1);
        m_projectPathLabel->SetFont(small);
        inner->Add(m_projectPathLabel, 0, wxBOTTOM, 8);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Topic ─────────────────────────────────────────────────────────────
    {
        auto* label = new wxStaticText(this, wxID_ANY,
                                       "What do you want to learn?");
        wxFont lf = label->GetFont();
        lf.SetPointSize(lf.GetPointSize() + 5);
        lf.SetWeight(wxFONTWEIGHT_BOLD);
        label->SetFont(lf);
        inner->Add(label, 0, wxBOTTOM, 6);

        m_topicCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxSize(-1, 90),
                                     wxTE_MULTILINE | wxTE_RICH2 | wxTE_WORDWRAP);
        wxFont tf = m_topicCtrl->GetFont();
        tf.SetPointSize(tf.GetPointSize() + 2);
        m_topicCtrl->SetFont(tf);
        m_topicCtrl->SetHint("Describe your topic — be as specific or broad as you like…");
        inner->Add(m_topicCtrl, 0, wxEXPAND | wxBOTTOM, 10);
    }

    // ── Project context (context.md) ──────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Project context:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        row->AddStretchSpacer();
        row->Add(new wxButton(this, ID_CP_SAVE_CONTEXT, "Save",
                              wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
                 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxEXPAND | wxBOTTOM, 4);

        m_contextCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                       wxDefaultPosition, wxSize(-1, 72),
                                       wxTE_MULTILINE | wxTE_RICH2 | wxTE_WORDWRAP);
        m_contextCtrl->SetHint(
            "Describe this project — characters, setting, tone. "
            "Prepended to the LLM prompt for every generation.");
        inner->Add(m_contextCtrl, 0, wxEXPAND | wxBOTTOM, 10);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Style ─────────────────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Style:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        m_styleChoice = new wxComboBox(this, wxID_ANY, make_styles()[0],
                                       wxDefaultPosition, wxSize(200, -1),
                                       make_styles(), wxCB_DROPDOWN);
        m_styleChoice->SetSelection(0);
        row->Add(m_styleChoice, 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxBOTTOM, 8);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Characters ────────────────────────────────────────────────────────
    inner->Add(new wxStaticText(this, wxID_ANY, "Tidbit characters:"),
               0, wxBOTTOM, 6);
    {
        // Two-panel: categories (left) + characters in category (right)
        auto* cols = new wxBoxSizer(wxHORIZONTAL);

        // Left: category list + add/delete buttons
        auto* leftCol = new wxBoxSizer(wxVERTICAL);
        m_catList = new wxListBox(this, ID_CP_CAT_LIST,
                                  wxDefaultPosition, wxSize(140, 148));
        leftCol->Add(m_catList, 1, wxEXPAND | wxBOTTOM, 4);
        auto* catBtns = new wxBoxSizer(wxHORIZONTAL);
        catBtns->Add(new wxButton(this, ID_CP_ADD_CAT, "+ Category",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0, wxRIGHT, 4);
        catBtns->Add(new wxButton(this, ID_CP_DEL_CAT, "✕",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0);
        leftCol->Add(catBtns, 0);
        cols->Add(leftCol, 0, wxEXPAND | wxRIGHT, 10);

        // Right: character checklist + add/delete buttons
        auto* rightCol = new wxBoxSizer(wxVERTICAL);
        m_charList = new wxCheckListBox(this, ID_CP_CHAR_LIST,
                                        wxDefaultPosition, wxSize(-1, 148));
        rightCol->Add(m_charList, 1, wxEXPAND | wxBOTTOM, 4);
        auto* charBtns = new wxBoxSizer(wxHORIZONTAL);
        charBtns->Add(new wxButton(this, ID_CP_ADD_CHAR, "+ Character",
                                   wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0, wxRIGHT, 4);
        charBtns->Add(new wxButton(this, ID_CP_DEL_CHAR, "✕",
                                   wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0);
        rightCol->Add(charBtns, 0);
        cols->Add(rightCol, 1, wxEXPAND);

        inner->Add(cols, 0, wxEXPAND | wxBOTTOM, 8);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    LoadCharLibrary();

    // ── Backend ───────────────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "LLM backend:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        m_backendChoice = new wxChoice(this, ID_CP_BACKEND, wxDefaultPosition,
                                       wxDefaultSize, make_backends());
        m_backendChoice->SetSelection(0);
        row->Add(m_backendChoice, 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxBOTTOM, 6);

        auto* apiRow = new wxBoxSizer(wxHORIZONTAL);
        apiRow->Add(new wxStaticText(this, wxID_ANY, "API key:"),
                    0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        m_apiKeyCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxSize(280, -1),
                                      wxTE_PASSWORD);
        apiRow->Add(m_apiKeyCtrl, 0, wxALIGN_CENTER_VERTICAL);
        m_apiKeySizer = inner->Add(apiRow, 0, wxBOTTOM, 6);
        m_apiKeySizer->Show(false);

        auto* ollamaRow = new wxBoxSizer(wxHORIZONTAL);
        ollamaRow->Add(new wxStaticText(this, wxID_ANY, "Ollama model:"),
                       0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        m_ollamaModel = new wxComboBox(this, wxID_ANY, "llama3",
                                       wxDefaultPosition, wxSize(220, -1),
                                       0, nullptr, wxCB_DROPDOWN);
        ollamaRow->Add(m_ollamaModel, 0, wxALIGN_CENTER_VERTICAL);
        ollamaRow->AddSpacer(6);
        auto* refreshBtn = new wxButton(this, ID_CP_REFRESH_OLLAMA, "Refresh",
                                        wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        refreshBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            wxString current = m_ollamaModel->GetValue();
            m_ollamaModel->Clear();
            for (auto& name : load_ollama_models())
                m_ollamaModel->Append(wxString::FromUTF8(name));
            if (!current.empty())
                m_ollamaModel->SetValue(current);
            else if (m_ollamaModel->GetCount() > 0)
                m_ollamaModel->SetSelection(0);
            SetStatus(m_ollamaModel->GetCount() > 0
                      ? "Loaded Ollama models."
                      : "No Ollama models found at localhost:11434.");
        });
        ollamaRow->Add(refreshBtn, 0, wxALIGN_CENTER_VERTICAL);
        m_ollamaSizer = inner->Add(ollamaRow, 0, wxBOTTOM, 8);
        m_ollamaSizer->Show(false);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Buttons ───────────────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        m_generateBtn = new wxButton(this, ID_CP_GENERATE, "Generate");
        row->Add(m_generateBtn, 0, wxRIGHT, 8);
        row->Add(new wxButton(this, ID_CP_COPY_PROMPT, "Copy Prompt"), 0, wxRIGHT, 8);
        row->AddStretchSpacer();
        row->Add(new wxButton(this, ID_CP_SAVE, "Save"), 0);
        inner->Add(row, 0, wxEXPAND | wxBOTTOM, 8);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 8);

    // ── Chapter list ──────────────────────────────────────────────────────
    {
        inner->Add(new wxStaticText(this, wxID_ANY, "Files in project:"),
                   0, wxBOTTOM, 4);
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        m_chapterListBox = new wxListCtrl(this, ID_CP_CHAPTER_LIST,
                                          wxDefaultPosition, wxSize(-1, 110),
                                          wxLC_REPORT | wxLC_SINGLE_SEL);
        m_chapterListBox->InsertColumn(0, "File",     wxLIST_FORMAT_LEFT, 200);
        m_chapterListBox->InsertColumn(1, "Language", wxLIST_FORMAT_LEFT,  90);
        m_chapterListBox->InsertColumn(2, "Created",  wxLIST_FORMAT_LEFT, 130);
        m_chapterListBox->InsertColumn(3, "Updated",  wxLIST_FORMAT_LEFT, 130);
        row->Add(m_chapterListBox, 1, wxEXPAND | wxRIGHT, 6);
        auto* fileBtns = new wxBoxSizer(wxVERTICAL);
        fileBtns->Add(new wxButton(this, ID_CP_OPEN_VIEW, "Open in View",
                                   wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
                      0, wxBOTTOM, 4);
        m_translateBtn = new wxButton(this, ID_CP_TRANSLATE, "Translate…",
                                      wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        fileBtns->Add(m_translateBtn, 0, wxBOTTOM, 4);
        fileBtns->Add(new wxButton(this, ID_CP_DELETE_FILE, "Delete",
                                   wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
                      0);
        row->Add(fileBtns, 0, wxALIGN_TOP);
        inner->Add(row, 0, wxEXPAND | wxBOTTOM, 8);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 8);

    // ── Status output ─────────────────────────────────────────────────────
    m_statusCtrl = new wxTextCtrl(this, wxID_ANY, "Ready.",
                                  wxDefaultPosition, wxSize(-1, 60),
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    inner->Add(m_statusCtrl, 1, wxEXPAND);

    outer->Add(inner, 1, wxEXPAND | wxALL, 14);
    SetSizer(outer);

    // ── Restore last session (all widgets now exist) ───────────────────────
    {
        AppConfig cfg = LoadConfig();
        LoadProjects();

        AppState st = LoadAppState();

        if (!st.currentProject.empty())
            SelectProject(wxString::FromUTF8(st.currentProject));
        else if (m_projectChoice->GetCount() > 0)
            SelectProject(m_projectChoice->GetString(0));

        if (!st.topic.empty())
            m_topicCtrl->SetValue(wxString::FromUTF8(st.topic));
        else if (!cfg.defaultPrompt.empty())
            m_topicCtrl->SetValue(wxString::FromUTF8(cfg.defaultPrompt));

        RestoreFormState(st);
        LoadChapters();
        LoadContext();
    }

}

// ---------------------------------------------------------------------------
void CreatePanel::SetStatus(const wxString& msg) {
    m_statusCtrl->SetValue(msg);
    std::string s = msg.ToStdString();
    if (s.find("Error") != std::string::npos || s.find("Cannot") != std::string::npos
        || s.find("could not") != std::string::npos || s.find("not found") != std::string::npos)
        Logger::get().log("CreatePanel error: " + s);
}

void CreatePanel::SetGenerating(bool on) {
    m_generating = on;
    m_generateBtn->Enable(!on);
    m_generateBtn->SetLabel(on ? "Generating…" : "Generate");
}

void CreatePanel::UpdateBackendFields() {
    std::string label = m_backendChoice->GetString(m_backendChoice->GetSelection()).ToStdString();
    m_ollamaSizer->Show(label == "Ollama (local)");
    m_apiKeySizer->Show(label == "Anthropic API");
    if (GetSizer()) GetSizer()->Layout();
}

// ---------------------------------------------------------------------------
void CreatePanel::LoadChapters() {
    m_chapterListBox->DeleteAllItems();
    wxString projPath = CurrentProjectPath();
    if (projPath.empty()) return;
    std::error_code ec;
    std::vector<std::string> files;
    for (auto& e : fs::directory_iterator(projPath.ToStdString(), ec)) {
        std::string fname = e.path().filename().string();
        if (e.path().extension() == ".md" && fname[0] != '.' && fname != "context.md")
            files.push_back(fname);
    }
    std::sort(files.begin(), files.end());
    for (auto& f : files) {
        std::string fullPath = projPath.ToStdString() + "/" + f;
        long row = m_chapterListBox->InsertItem(m_chapterListBox->GetItemCount(),
                                                wxString::FromUTF8(f));
        m_chapterListBox->SetItem(row, 1,
            wxString::FromUTF8(ReadLanguage(fullPath)));
        m_chapterListBox->SetItem(row, 2,
            wxString::FromUTF8(FileCreatedTime(fullPath)));
        m_chapterListBox->SetItem(row, 3,
            wxString::FromUTF8(FileModifiedTime(fullPath)));
    }
}

void CreatePanel::LoadContext() {
    wxString projPath = CurrentProjectPath();
    if (projPath.empty()) { m_contextCtrl->Clear(); return; }
    fs::path claudeMd = fs::path(projPath.ToStdString()) / "context.md";
    std::ifstream f(claudeMd);
    if (!f) { m_contextCtrl->Clear(); return; }
    std::string content{std::istreambuf_iterator<char>(f), {}};
    m_contextCtrl->SetValue(wxString::FromUTF8(content));
}

void CreatePanel::SaveContext() {
    wxString projPath = CurrentProjectPath();
    if (projPath.empty()) return;
    fs::path claudeMd = fs::path(projPath.ToStdString()) / "context.md";
    std::ofstream f(claudeMd, std::ios::trunc);
    if (f) f << m_contextCtrl->GetValue().ToStdString();
}

void CreatePanel::OnSaveContext(wxCommandEvent&) {
    SaveContext();
    SetStatus("Project context saved.");
}

void CreatePanel::LoadProjects() {
    AppConfig cfg = LoadConfig();
    m_projectChoice->Clear();
    if (cfg.defaultFolder.empty()) return;

    for (auto& rel : ListAllProjects(cfg.defaultFolder))
        m_projectChoice->Append(wxString::FromUTF8(rel));
}

void CreatePanel::SelectProject(const wxString& name) {
    // Try exact match first (handles relative paths like "Literature/agatha").
    int idx = m_projectChoice->FindString(name);
    if (idx == wxNOT_FOUND) {
        // Fallback: match by last path component for AppState compatibility.
        wxString lastName = wxString::FromUTF8(
            fs::path(name.ToStdString()).filename().string());
        for (int i = 0; i < (int)m_projectChoice->GetCount(); ++i) {
            wxString item = m_projectChoice->GetString(i);
            if (wxString::FromUTF8(
                    fs::path(item.ToStdString()).filename().string()) == lastName) {
                idx = i;
                break;
            }
        }
    }
    if (idx != wxNOT_FOUND) m_projectChoice->SetSelection(idx);

    wxString path = CurrentProjectPath();
    m_projectPathLabel->SetLabel(path.empty() ? wxString() : path);

    if (!name.empty()) {
        AppState st = LoadAppState();
        // Save only the last path component for Projects tab compatibility.
        st.currentProject = fs::path(name.ToStdString()).filename().string();
        SaveAppState(st);
    }
}

wxString CreatePanel::CurrentProjectPath() const {
    AppConfig cfg = LoadConfig();
    int sel = m_projectChoice->GetSelection();
    if (cfg.defaultFolder.empty() || sel == wxNOT_FOUND) return wxEmptyString;
    return wxString::FromUTF8(cfg.defaultFolder) + "/" + m_projectChoice->GetString(sel);
}

void CreatePanel::OnNewProject(wxCommandEvent&) {
    wxString name = wxGetTextFromUser(
        "Enter a name for the new project:", "New Project", "", this).Trim();
    if (name.empty()) return;

    AppConfig cfg = LoadConfig();
    if (cfg.defaultFolder.empty()) {
        SetStatus("Set defaultFolder in ~/.config/story-teller/config first.");
        return;
    }

    if (!CreateProject(cfg.defaultFolder, name.ToStdString())) {
        SetStatus("Could not create project folder.");
        return;
    }
    std::string backendLabel =
        m_backendChoice->GetString(m_backendChoice->GetSelection()).ToStdString();
    RecordProjectSource((fs::path(cfg.defaultFolder) / name.ToStdString()).string(),
                        backendLabel);

    // Refresh list and select the new project.
    LoadProjects();
    SelectProject(name);
}

void CreatePanel::OnProjectSelected(wxCommandEvent&) {
    int sel = m_projectChoice->GetSelection();
    if (sel == wxNOT_FOUND) return;
    SelectProject(m_projectChoice->GetString(sel));
    LoadChapters();
    LoadContext();
}

// Called by LoadFile whenever the user opens a file (from any tab or on startup).
// Lookup chain:
//   1. Derive the project's relative path from the file path (handles nested projects).
//   2. Fallback: use the project name saved in AppState (last-component match).
void CreatePanel::SyncProject(const std::string& filePath) {
    if (!filePath.empty()) {
        AppConfig cfg = LoadConfig();
        std::string rel = ProjectNameFromFilePath(filePath, cfg.defaultFolder);
        if (!rel.empty()) {
            SelectProject(wxString::FromUTF8(rel));
            LoadChapters();
            LoadContext();
            return;
        }
    }
    AppState st = LoadAppState();
    if (!st.currentProject.empty()) {
        SelectProject(wxString::FromUTF8(st.currentProject));
        LoadChapters();
        LoadContext();
    }
}

// ---------------------------------------------------------------------------
// Character library — persist to wxConfig
// ---------------------------------------------------------------------------
static const std::map<std::string, std::vector<std::string>>& default_library() {
    static const std::map<std::string, std::vector<std::string>> lib = {
        {"Science",    {"Albert Einstein", "Marie Curie", "Carl Sagan",
                        "Richard Feynman", "Nikola Tesla", "Charles Darwin"}},
        {"Literature", {"Sherlock Holmes", "Agatha Christie", "Edgar Allan Poe"}},
        {"History",    {"Ada Lovelace", "Napoleon Bonaparte", "Cleopatra"}},
    };
    return lib;
}

void CreatePanel::LoadCharLibrary() {
    wxConfig cfg("StoryTeller");
    cfg.SetPath("/charlib");

    wxString catStr;
    if (!cfg.Read("categories", &catStr) || catStr.empty()) {
        m_charsByCategory = default_library();
    } else {
        wxStringTokenizer tok(catStr, ",");
        while (tok.HasMoreTokens()) {
            std::string cat = tok.GetNextToken().ToStdString();
            wxString charStr;
            cfg.Read(wxString::FromUTF8(cat), &charStr);
            auto& vec = m_charsByCategory[cat];
            wxStringTokenizer ctok(charStr, "|");
            while (ctok.HasMoreTokens())
                vec.push_back(ctok.GetNextToken().ToStdString());
        }
    }

    m_catList->Clear();
    for (auto& [cat, _] : m_charsByCategory)
        m_catList->Append(wxString::FromUTF8(cat));
    if (m_catList->GetCount() > 0) {
        m_catList->SetSelection(0);
        RefreshCharList();
    }
}

void CreatePanel::SaveCharLibrary() const {
    wxConfig cfg("StoryTeller");
    cfg.SetPath("/charlib");

    // Build comma-separated category list.
    wxString catStr;
    for (auto& [cat, chars] : m_charsByCategory) {
        if (!catStr.empty()) catStr += ",";
        catStr += wxString::FromUTF8(cat);

        wxString charStr;
        for (auto& ch : chars) {
            if (!charStr.empty()) charStr += "|";
            charStr += wxString::FromUTF8(ch);
        }
        cfg.Write(wxString::FromUTF8(cat), charStr);
    }
    cfg.Write("categories", catStr);
}

std::string CreatePanel::SelectedCategory() const {
    int sel = m_catList->GetSelection();
    if (sel == wxNOT_FOUND) return "";
    return m_catList->GetString(sel).ToStdString();
}

void CreatePanel::RefreshCharList() {
    m_charList->Clear();
    std::string cat = SelectedCategory();
    auto it = m_charsByCategory.find(cat);
    if (it == m_charsByCategory.end()) return;
    for (auto& ch : it->second) {
        unsigned int idx = m_charList->Append(wxString::FromUTF8(ch));
        m_charList->Check(idx, m_checkedChars.count(ch) > 0);
    }
}

void CreatePanel::OnCatSelected(wxCommandEvent&) { RefreshCharList(); }

void CreatePanel::OnCharToggled(wxCommandEvent& evt) {
    unsigned int idx = (unsigned int)evt.GetInt();
    std::string name = m_charList->GetString(idx).ToStdString();
    if (m_charList->IsChecked(idx))
        m_checkedChars.insert(name);
    else
        m_checkedChars.erase(name);
}

void CreatePanel::OnAddCategory(wxCommandEvent&) {
    wxString name = wxGetTextFromUser("Category name:", "Add Category", "", this).Trim();
    if (name.empty() || m_charsByCategory.count(name.ToStdString())) return;
    m_charsByCategory[name.ToStdString()];        // insert empty
    m_catList->Append(name);
    m_catList->SetSelection((int)m_catList->GetCount() - 1);
    RefreshCharList();
    SaveCharLibrary();
}

void CreatePanel::OnDeleteCategory(wxCommandEvent&) {
    std::string cat = SelectedCategory();
    if (cat.empty()) return;
    if (wxMessageBox("Delete category \"" + cat + "\" and all its characters?",
                     "Confirm", wxYES_NO | wxNO_DEFAULT, this) != wxYES) return;
    m_charsByCategory.erase(cat);
    int sel = m_catList->GetSelection();
    m_catList->Delete((unsigned int)sel);
    if (m_catList->GetCount() > 0)
        m_catList->SetSelection(std::min(sel, (int)m_catList->GetCount() - 1));
    RefreshCharList();
    SaveCharLibrary();
}

void CreatePanel::OnAddCharacter(wxCommandEvent&) {
    std::string cat = SelectedCategory();
    if (cat.empty()) { SetStatus("Select a category first."); return; }
    wxString name = wxGetTextFromUser("Character name:", "Add Character", "", this).Trim();
    if (name.empty()) return;
    std::string ch = name.ToStdString();
    auto& vec = m_charsByCategory[cat];
    if (std::find(vec.begin(), vec.end(), ch) != vec.end()) return; // duplicate
    vec.push_back(ch);
    unsigned int idx = m_charList->Append(name);
    m_charList->Check(idx, true);
    m_checkedChars.insert(ch);
    SaveCharLibrary();
}

void CreatePanel::OnDeleteCharacter(wxCommandEvent&) {
    int idx = m_charList->GetSelection();
    if (idx == wxNOT_FOUND) return;
    std::string cat = SelectedCategory();
    std::string ch  = m_charList->GetString(idx).ToStdString();
    auto& vec = m_charsByCategory[cat];
    vec.erase(std::remove(vec.begin(), vec.end(), ch), vec.end());
    m_checkedChars.erase(ch);
    m_charList->Delete((unsigned int)idx);
    SaveCharLibrary();
}

void CreatePanel::OnBackendChanged(wxCommandEvent&) {
    UpdateBackendFields();
    if (m_backendChoice->GetString(m_backendChoice->GetSelection()) == "Ollama (local)"
        && m_ollamaModel->GetCount() == 0) {
        for (auto& name : load_ollama_models())
            m_ollamaModel->Append(wxString::FromUTF8(name));
    }
    // Auto-persist the backend selection so the Edit tab can pick it up immediately.
    AppState st = LoadAppState();
    st.backend = m_backendChoice->GetString(m_backendChoice->GetSelection()).ToStdString();
    SaveAppState(st);
}

// Build a GenerationRequest from the current form state.
GenerationRequest CreatePanel::BuildRequest() const {
    GenerationRequest req;
    req.topic = m_topicCtrl->GetValue().ToStdString();
    req.style = m_styleChoice->GetValue().ToStdString();
    for (auto& ch : m_checkedChars)
        req.characters.push_back(ch);
    std::string projDir = CurrentProjectPath().ToStdString();
    fs::path claudeMd = fs::path(projDir) / "context.md";
    if (fs::exists(claudeMd)) {
        std::ifstream f(claudeMd);
        req.projectContext.assign(std::istreambuf_iterator<char>(f), {});
    }
    return req;
}

void CreatePanel::OnCopyPrompt(wxCommandEvent&) {
    if (m_topicCtrl->GetValue().empty()) { SetStatus("Enter a topic first."); return; }
    std::string prompt = BuildPrompt(BuildRequest(), GetLLMReadme());
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
        wxTheClipboard->Close();
    }
    SetStatus("Prompt copied to clipboard.\n\n"
              "Paste into any LLM, then open the result with File > Open.");
}

void CreatePanel::OnGenerate(wxCommandEvent&) {
    if (m_generating) return;

    wxString topic   = m_topicCtrl->GetValue().Trim();
    wxString projDir = CurrentProjectPath();
    if (topic.empty())   { SetStatus("Enter a topic."); return; }
    if (projDir.empty()) { SetStatus("Select or create a project first."); return; }

    if (!InitProject(projDir.ToStdString())) {
        SetStatus("Cannot initialise project folder: " + projDir); return;
    }

    GenerationRequest req    = BuildRequest();
    int               chId   = NextChapterId(projDir.ToStdString());
    std::string       prompt = BuildPrompt(req, GetLLMReadme());
    int               bkIdx   = m_backendChoice->GetSelection();
    std::string       bkLabel = m_backendChoice->GetString(bkIdx).ToStdString();
    LLMBackend        backend  = backend_from_label(bkLabel);

    // Clipboard — copy and return immediately.
    if (backend == LLMBackend::Clipboard) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        SetStatus("Prompt copied to clipboard.\n\n"
                  "Paste into any LLM, then open the result with File > Open.");
        return;
    }

    LLMConfig cfg;
    cfg.backend     = backend;
    cfg.apiKey      = m_apiKeyCtrl->GetValue().ToStdString();
    cfg.ollamaModel = m_ollamaModel->GetValue().ToStdString();

    std::string projDirStr = projDir.ToStdString();
    std::string topicStr = req.topic;

    SetGenerating(true);
    SetStatus("Sending to " + m_backendChoice->GetString(bkIdx) + "…");
    Logger::get().log("Generate: backend=" + bkLabel
                      + "  project=" + projDirStr
                      + "  model=" + (backend == LLMBackend::Ollama ? cfg.ollamaModel : "(n/a)")
                      + "  language=" + language_from_topic(req.topic)
                      + "  topic=" + truncate_for_log(req.topic));
    OpenCallback cb = m_openCallback;

    std::thread([this, prompt, cfg, projDirStr, chId, topicStr, cb]() mutable {
        auto started = std::chrono::steady_clock::now();
        LLMResult res = InvokeLLM(prompt, cfg);
        int durationSeconds = (int)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - started).count();
        if (res.ok) {
            RecordProjectSource(projDirStr, cfg.backend == LLMBackend::ClaudeP ? "Claude -p" :
                                           cfg.backend == LLMBackend::CodexCLI ? "Codex CLI" :
                                           cfg.backend == LLMBackend::GeminiCLI ? "Gemini CLI" :
                                           cfg.backend == LLMBackend::Ollama ? "Ollama (local)" :
                                           cfg.backend == LLMBackend::API ? "Anthropic API" :
                                           "Clipboard");
            RecordLLMTiming(projDirStr, "generate", topicStr, durationSeconds);
        }

        wxTheApp->CallAfter([this, res, projDirStr, chId, topicStr, cb]() mutable {
            SetGenerating(false);
            if (!res.ok) {
                Logger::get().log("Generate FAILED: " + res.error);
                SetStatus("Error: " + wxString::FromUTF8(res.error));
                return;
            }

            // Stamp each :::tidbit block with a stable <!-- tb:N --> marker.
            std::string content = CleanMarkdownResponse(res.text);
            std::string filename = FilenameFromContent(content, topicStr, chId);
            std::string stamped;
            int baseTbId = NextTidbitId(projDirStr);
            int tbCount  = 0;
            std::size_t pos = 0;
            while (pos < content.size()) {
                auto tbpos = content.find(":::tidbit[", pos);
                if (tbpos == std::string::npos) { stamped += content.substr(pos); break; }
                stamped += content.substr(pos, tbpos - pos);
                stamped += "<!-- tb:" + std::to_string(baseTbId + tbCount) + " -->\n";
                auto endpos = content.find("\n:::", tbpos);
                if (endpos == std::string::npos) {
                    stamped += content.substr(tbpos);
                    pos = content.size();
                } else {
                    endpos += 4;
                    stamped += content.substr(tbpos, endpos - tbpos);
                    pos = endpos;
                }
                ++tbCount;
            }

            // Stamp each ## Chapter N: heading with a <!-- ch:N --> marker.
            auto [chStamped, chCount] = StampChapters(stamped, 0);
            stamped = chStamped;

            std::string path = SaveChapter(projDirStr, filename, stamped);
            if (path.empty()) { SetStatus("Error: could not save chapter file."); return; }

            WriteLanguage(path, language_from_topic(topicStr));
            RegisterChapter(projDirStr, filename);
            for (int i = 0; i < tbCount; ++i)
                RegisterTidbit(projDirStr, chId, i);

            SetStatus("Saved: " + wxString::FromUTF8(filename));
            LoadChapters();
            if (cb) cb(path);
        });
    }).detach();
}

// ---------------------------------------------------------------------------
// Form state persistence
// ---------------------------------------------------------------------------
void CreatePanel::SaveFormState() const {
    AppState st = LoadAppState();

    int sel = m_projectChoice->GetSelection();
    st.currentProject = (sel != wxNOT_FOUND)
                        ? m_projectChoice->GetString(sel).ToStdString() : "";
    st.topic   = m_topicCtrl->GetValue().ToStdString();
    st.style   = m_styleChoice->GetValue().ToStdString();
    st.backend = m_backendChoice->GetString(m_backendChoice->GetSelection()).ToStdString();

    std::string chars;
    for (auto& ch : m_checkedChars) {
        if (!chars.empty()) chars += "|";
        chars += ch;
    }
    st.checkedChars  = chars;
    st.apiKey        = m_apiKeyCtrl->GetValue().ToStdString();
    st.ollamaModel   = m_ollamaModel->GetValue().ToStdString();

    SaveAppState(st);
    Logger::get().log("Form state saved  project=" + st.currentProject
                      + "  chars=" + st.checkedChars);
}

void CreatePanel::RestoreFormState(const AppState& st) {
    if (!st.style.empty()) {
        int idx = m_styleChoice->FindString(wxString::FromUTF8(st.style));
        if (idx != wxNOT_FOUND)
            m_styleChoice->SetSelection(idx);
        else
            m_styleChoice->SetValue(wxString::FromUTF8(st.style));
    }
    if (!st.backend.empty()) {
        int idx = m_backendChoice->FindString(wxString::FromUTF8(st.backend));
        if (idx != wxNOT_FOUND) {
            m_backendChoice->SetSelection(idx);
            UpdateBackendFields();
        }
    }
    if (!st.checkedChars.empty()) {
        wxStringTokenizer tok(wxString::FromUTF8(st.checkedChars), "|");
        while (tok.HasMoreTokens())
            m_checkedChars.insert(tok.GetNextToken().ToStdString());
        RefreshCharList();
    }
    if (!st.apiKey.empty())
        m_apiKeyCtrl->SetValue(wxString::FromUTF8(st.apiKey));
    if (!st.ollamaModel.empty())
        m_ollamaModel->SetValue(wxString::FromUTF8(st.ollamaModel));
}

void CreatePanel::OnSave(wxCommandEvent&) {
    SaveFormState();
    SetStatus("Form saved — will be restored on next launch.");
}

void CreatePanel::OnOpenInView(wxCommandEvent&) {
    long sel = m_chapterListBox->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == wxNOT_FOUND) { SetStatus("Select a file first."); return; }
    wxString path = CurrentProjectPath() + "/" + m_chapterListBox->GetItemText(sel, 0);
    if (m_openCallback) m_openCallback(path.ToStdString());
}

void CreatePanel::OnChapterActivated(wxListEvent& evt) {
    wxString path = CurrentProjectPath() + "/" + evt.GetText();
    if (m_openCallback) m_openCallback(path.ToStdString());
}

// ---------------------------------------------------------------------------
void CreatePanel::OnDeleteFile(wxCommandEvent&) {
    long sel = m_chapterListBox->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == wxNOT_FOUND) { SetStatus("Select a file to delete first."); return; }

    wxString filename = m_chapterListBox->GetItemText(sel, 0);
    wxString fullPath = CurrentProjectPath() + "/" + filename;

    if (wxMessageBox("Delete \"" + filename + "\"?\n\nThis cannot be undone.",
                     "Confirm Delete", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
        return;

    std::error_code ec;
    fs::remove(fullPath.ToStdString(), ec);
    if (ec) {
        SetStatus("Could not delete file: " + wxString::FromUTF8(ec.message()));
        return;
    }
    SetStatus("Deleted: " + filename);
    LoadChapters();
}

// ---------------------------------------------------------------------------
void CreatePanel::OnTranslate(wxCommandEvent&) {
    long sel = m_chapterListBox->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == wxNOT_FOUND) { SetStatus("Select a file to translate first."); return; }

    wxString projDir = CurrentProjectPath();
    if (projDir.empty()) { SetStatus("No project selected."); return; }

    wxString sourceFile = m_chapterListBox->GetItemText(sel, 0);
    std::string sourcePath = projDir.ToStdString() + "/" + sourceFile.ToStdString();

    wxArrayString langs;
    for (auto* l : {"English", "Spanish", "French", "German", "Italian", "Portuguese",
                    "Japanese", "Korean", "Chinese (Mandarin)", "Arabic", "Russian"})
        langs.Add(l);

    wxSingleChoiceDialog dlg(this, "Translate to:", "Translate", langs);
    if (dlg.ShowModal() != wxID_OK) return;
    std::string language = dlg.GetStringSelection().ToStdString();

    std::ifstream ifs(sourcePath);
    if (!ifs) { SetStatus("Cannot read source file."); return; }
    std::string sourceText{std::istreambuf_iterator<char>(ifs), {}};

    int bkIdx = m_backendChoice->GetSelection();
    std::string bkLabel = m_backendChoice->GetString(bkIdx).ToStdString();
    LLMBackend backend = backend_from_label(bkLabel);

    LLMConfig cfg;
    cfg.backend     = backend;
    cfg.apiKey      = m_apiKeyCtrl->GetValue().ToStdString();
    cfg.ollamaModel = m_ollamaModel->GetValue().ToStdString();

    bool isChinese = (language == "Chinese (Mandarin)");
    std::string sourceFilename = sourceFile.ToStdString();
    std::string projDirStr     = projDir.ToStdString();
    std::string llmReadme      = GetLLMReadme();
    OpenCallback cb            = m_openCallback;

    // Chinese always gets ::pinyin annotations so we can produce two files from one call.
    std::string extraInstr = isChinese ? BuildPinyinInstruction() : "";

    if (backend == LLMBackend::Clipboard) {
        std::string prompt = BuildTranslationPrompt(sourceText, language, llmReadme, extraInstr);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        SetStatus("Translation prompt copied to clipboard.");
        return;
    }

    std::string bkLabelCopy = bkLabel;
    Logger::get().log("Translate: backend=" + bkLabelCopy
                      + "  source=" + sourceFilename
                      + "  language=" + language
                      + "  source_bytes=" + std::to_string(sourceText.size()));
    if (m_translateBtn) {
        m_translateBtn->Enable(false);
        m_translateBtn->SetLabel("Translating…");
    }
    SetStatus("Translating to " + wxString::FromUTF8(language) + "…");

    std::thread([this, sourceText, language, isChinese,
                 sourceFilename, projDirStr, llmReadme, extraInstr, cfg, cb]() mutable {
        std::string prompt = BuildTranslationPrompt(sourceText, language, llmReadme, extraInstr);
        LLMResult res = InvokeLLM(prompt, cfg);

        if (!res.ok) {
            Logger::get().log("Translate FAILED: " + res.error);
            wxTheApp->CallAfter([this, err = res.error]() {
                if (m_translateBtn) {
                    m_translateBtn->Enable(true);
                    m_translateBtn->SetLabel("Translate…");
                }
                SetStatus("Translation error: " + wxString::FromUTF8(err));
            });
            return;
        }

        Logger::get().log("Translate LLM OK: response_bytes=" + std::to_string(res.text.size()));
        std::string tagged = CleanMarkdownResponse(res.text);

        // For Chinese: parse the single tagged response into two files.
        // tagged  → _Chinese_Pinyin.md (Chinese lines + ::pinyin annotation lines)
        // stripped → _Chinese.md (Chinese lines only)
        std::string pinyinPath;
        std::string mainPath;

        if (isChinese) {
            std::string pinyinFile = TranslationFilename(sourceFilename, "Chinese w/ Pinyin");
            pinyinPath = SaveChapter(projDirStr, pinyinFile, tagged);
            if (!pinyinPath.empty()) {
                WriteLanguage(pinyinPath, "Chinese w/ Pinyin");
                Logger::get().log("Translate saved (pinyin): " + pinyinPath);
            } else {
                Logger::get().log("Translate ERROR: SaveChapter failed for " + pinyinFile + " in " + projDirStr);
            }

            std::string stripped = StripPinyinLines(tagged);
            std::string chineseFile = TranslationFilename(sourceFilename, "Chinese (Mandarin)");
            mainPath = SaveChapter(projDirStr, chineseFile, stripped);
            if (!mainPath.empty()) {
                WriteLanguage(mainPath, "Chinese (Mandarin)");
                Logger::get().log("Translate saved (Chinese): " + mainPath);
            } else {
                Logger::get().log("Translate ERROR: SaveChapter failed for " + chineseFile + " in " + projDirStr);
            }
        } else {
            std::string outFile = TranslationFilename(sourceFilename, language);
            mainPath = SaveChapter(projDirStr, outFile, tagged);
            if (!mainPath.empty()) {
                WriteLanguage(mainPath, language);
                Logger::get().log("Translate saved: " + mainPath);
            } else {
                Logger::get().log("Translate ERROR: SaveChapter failed for " + outFile + " in " + projDirStr);
            }
        }

        wxTheApp->CallAfter([this, mainPath, pinyinPath, isChinese, cb]() {
            if (m_translateBtn) {
                m_translateBtn->Enable(true);
                m_translateBtn->SetLabel("Translate…");
            }
            LoadChapters();
            if (isChinese && !pinyinPath.empty()) {
                SetStatus("Translated: " +
                    wxString::FromUTF8(fs::path(mainPath).filename().string()) +
                    " and " +
                    wxString::FromUTF8(fs::path(pinyinPath).filename().string()));
                if (cb) cb(pinyinPath);
            } else if (!mainPath.empty()) {
                SetStatus("Translated: " +
                    wxString::FromUTF8(fs::path(mainPath).filename().string()));
                if (cb) cb(mainPath);
            } else {
                SetStatus("Error: translation completed but file could not be saved. Check View Logs.");
            }
        });
    }).detach();
}
