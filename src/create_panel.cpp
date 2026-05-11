#include "create_panel.h"
#include "config.h"
#include "logger.h"
#include "creator.h"
#include "llm.h"
#include "markdown.h"
#include "project.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/config.h>
#include <wx/dataobj.h>
#include <wx/dirdlg.h>
#include <wx/listbox.h>
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
    EVT_BUTTON(ID_CP_OPEN_VIEW,     CreatePanel::OnOpenInView)
    EVT_LISTBOX_DCLICK(ID_CP_CHAPTER_LIST, CreatePanel::OnOpenInView)
wxEND_EVENT_TABLE()

static wxArrayString make_styles() {
    wxArrayString s;
    for (auto* n : {"Academic essay", "Children's book", "Crime noir",
                    "Fairy tale", "Horror", "Podcast transcript",
                    "Socratic dialogue", "Tech blog post"})
        s.Add(n);
    return s;
}

static wxArrayString make_backends() {
    wxArrayString s;
    for (auto* n : {"Clipboard (manual)", "claude -p", "Ollama (local)", "Anthropic API"})
        s.Add(n);
    return s;
}

static LLMBackend backend_from_index(int i) {
    switch (i) {
        case 1:  return LLMBackend::ClaudeP;
        case 2:  return LLMBackend::Ollama;
        case 3:  return LLMBackend::API;
        default: return LLMBackend::Clipboard;
    }
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
        m_projectPathLabel->SetForegroundColour(wxColour(100, 100, 100));
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

    // ── Style ─────────────────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Style:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        m_styleChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition,
                                     wxDefaultSize, make_styles());
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
        m_ollamaModel = new wxTextCtrl(this, wxID_ANY, "llama3",
                                       wxDefaultPosition, wxSize(160, -1));
        ollamaRow->Add(m_ollamaModel, 0, wxALIGN_CENTER_VERTICAL);
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
        inner->Add(new wxStaticText(this, wxID_ANY, "Chapters in project:"),
                   0, wxBOTTOM, 4);
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        m_chapterListBox = new wxListBox(this, ID_CP_CHAPTER_LIST,
                                         wxDefaultPosition, wxSize(-1, 100));
        row->Add(m_chapterListBox, 1, wxEXPAND | wxRIGHT, 6);
        row->Add(new wxButton(this, ID_CP_OPEN_VIEW, "Open in View",
                              wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
                 0, wxALIGN_TOP);
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
    }

    SetBackgroundColour(wxColour(235, 235, 228));
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
    int sel = m_backendChoice->GetSelection();
    m_ollamaSizer->Show(sel == 2);
    m_apiKeySizer->Show(sel == 3);
    if (GetSizer()) GetSizer()->Layout();
}

// ---------------------------------------------------------------------------
void CreatePanel::LoadChapters() {
    m_chapterListBox->Clear();
    wxString projPath = CurrentProjectPath();
    if (projPath.empty()) return;
    std::error_code ec;
    std::vector<std::string> files;
    for (auto& e : fs::directory_iterator(projPath.ToStdString(), ec))
        if (e.path().extension() == ".md" && e.path().filename().string()[0] != '.')
            files.push_back(e.path().filename().string());
    std::sort(files.begin(), files.end());
    for (auto& f : files)
        m_chapterListBox->Append(wxString::FromUTF8(f));
}

void CreatePanel::LoadProjects() {
    AppConfig cfg = LoadConfig();
    m_projectChoice->Clear();
    if (cfg.defaultFolder.empty()) return;

    namespace fs = std::filesystem;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(cfg.defaultFolder, ec)) {
        if (entry.is_directory(ec))
            m_projectChoice->Append(wxString::FromUTF8(entry.path().filename().string()));
    }
}

void CreatePanel::SelectProject(const wxString& name) {
    int idx = m_projectChoice->FindString(name);
    if (idx != wxNOT_FOUND) m_projectChoice->SetSelection(idx);

    wxString path = CurrentProjectPath();
    m_projectPathLabel->SetLabel(path.empty() ? wxString() : path);

    if (!name.empty()) {
        AppState st = LoadAppState();
        st.currentProject = name.ToStdString();
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

    // Refresh list and select the new project.
    LoadProjects();
    SelectProject(name);
}

void CreatePanel::OnProjectSelected(wxCommandEvent&) {
    int sel = m_projectChoice->GetSelection();
    if (sel == wxNOT_FOUND) return;
    SelectProject(m_projectChoice->GetString(sel));
    LoadChapters();
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

void CreatePanel::OnBackendChanged(wxCommandEvent&) { UpdateBackendFields(); }

// Build a GenerationRequest from the current form state.
GenerationRequest CreatePanel::BuildRequest() const {
    GenerationRequest req;
    req.topic = m_topicCtrl->GetValue().ToStdString();
    req.style = m_styleChoice->GetString(m_styleChoice->GetSelection()).ToStdString();
    for (auto& ch : m_checkedChars)
        req.characters.push_back(ch);
    std::string projDir = CurrentProjectPath().ToStdString();
    fs::path claudeMd = fs::path(projDir) / "claude.md";
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

    GenerationRequest req     = BuildRequest();
    int               chId    = NextChapterId(projDir.ToStdString());
    std::string       filename = ChapterFilename(req.topic, chId);
    std::string       prompt  = BuildPrompt(req, GetLLMReadme());
    int               bkIdx   = m_backendChoice->GetSelection();
    LLMBackend        backend  = backend_from_index(bkIdx);

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

    SetGenerating(true);
    SetStatus("Sending to " + m_backendChoice->GetString(bkIdx) + "…");
    Logger::get().log("Generate: backend=" + m_backendChoice->GetString(bkIdx).ToStdString()
                      + "  project=" + projDirStr + "  file=" + filename);
    OpenCallback cb = m_openCallback;

    std::thread([this, prompt, cfg, projDirStr, filename, chId, cb]() mutable {
        LLMResult res = InvokeLLM(prompt, cfg);

        wxTheApp->CallAfter([this, res, projDirStr, filename, chId, cb]() mutable {
            SetGenerating(false);
            if (!res.ok) {
                Logger::get().log("Generate FAILED: " + res.error);
                SetStatus("Error: " + wxString::FromUTF8(res.error));
                return;
            }

            // Stamp each :::tidbit block with a stable <!-- tb:N --> marker.
            std::string content = res.text;
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
    st.style   = m_styleChoice->GetString(m_styleChoice->GetSelection()).ToStdString();
    st.backend = m_backendChoice->GetString(m_backendChoice->GetSelection()).ToStdString();

    std::string chars;
    for (auto& ch : m_checkedChars) {
        if (!chars.empty()) chars += "|";
        chars += ch;
    }
    st.checkedChars = chars;

    SaveAppState(st);
    Logger::get().log("Form state saved  project=" + st.currentProject
                      + "  chars=" + st.checkedChars);
}

void CreatePanel::RestoreFormState(const AppState& st) {
    if (!st.style.empty()) {
        int idx = m_styleChoice->FindString(wxString::FromUTF8(st.style));
        if (idx != wxNOT_FOUND) m_styleChoice->SetSelection(idx);
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
}

void CreatePanel::OnSave(wxCommandEvent&) {
    SaveFormState();
    SetStatus("Form saved — will be restored on next launch.");
}

void CreatePanel::OnOpenInView(wxCommandEvent&) {
    int sel = m_chapterListBox->GetSelection();
    if (sel == wxNOT_FOUND) { SetStatus("Select a chapter first."); return; }
    wxString path = CurrentProjectPath() + "/" + m_chapterListBox->GetString(sel);
    if (m_openCallback) m_openCallback(path.ToStdString());
}
