#include "create_panel.h"
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
    ID_CP_BROWSE     = wxID_HIGHEST + 200,
    ID_CP_BACKEND,
    ID_CP_GENERATE,
    ID_CP_COPY_PROMPT,
    ID_CP_CAT_LIST,
    ID_CP_ADD_CAT,
    ID_CP_DEL_CAT,
    ID_CP_CHAR_LIST,
    ID_CP_ADD_CHAR,
    ID_CP_DEL_CHAR,
};

wxBEGIN_EVENT_TABLE(CreatePanel, wxPanel)
    EVT_BUTTON(ID_CP_BROWSE,      CreatePanel::OnBrowse)
    EVT_LISTBOX(ID_CP_CAT_LIST,   CreatePanel::OnCatSelected)
    EVT_CHECKLISTBOX(ID_CP_CHAR_LIST, CreatePanel::OnCharToggled)
    EVT_BUTTON(ID_CP_ADD_CAT,     CreatePanel::OnAddCategory)
    EVT_BUTTON(ID_CP_DEL_CAT,     CreatePanel::OnDeleteCategory)
    EVT_BUTTON(ID_CP_ADD_CHAR,    CreatePanel::OnAddCharacter)
    EVT_BUTTON(ID_CP_DEL_CHAR,    CreatePanel::OnDeleteCharacter)
    EVT_CHOICE(ID_CP_BACKEND,     CreatePanel::OnBackendChanged)
    EVT_BUTTON(ID_CP_GENERATE,    CreatePanel::OnGenerate)
    EVT_BUTTON(ID_CP_COPY_PROMPT, CreatePanel::OnCopyPrompt)
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

    // ── Project folder ────────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Project folder:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        m_projectDir = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxSize(320, -1));
        row->Add(m_projectDir, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        row->Add(new wxButton(this, ID_CP_BROWSE, "Browse…",
                              wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
                 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxEXPAND | wxBOTTOM, 8);
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
        row->Add(new wxButton(this, ID_CP_COPY_PROMPT, "Copy Prompt"), 0);
        inner->Add(row, 0, wxBOTTOM, 8);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 8);

    // ── Status output ─────────────────────────────────────────────────────
    m_statusCtrl = new wxTextCtrl(this, wxID_ANY, "Ready.",
                                  wxDefaultPosition, wxSize(-1, 80),
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    inner->Add(m_statusCtrl, 1, wxEXPAND);

    outer->Add(inner, 1, wxEXPAND | wxALL, 14);
    SetSizer(outer);
}

// ---------------------------------------------------------------------------
void CreatePanel::SetStatus(const wxString& msg) {
    m_statusCtrl->SetValue(msg);
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
    GetSizer()->Layout();
}

// ---------------------------------------------------------------------------
void CreatePanel::OnBrowse(wxCommandEvent&) {
    wxDirDialog dlg(this, "Choose project folder", m_projectDir->GetValue(),
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
        m_projectDir->SetValue(dlg.GetPath());
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
    wxConfig cfg("MDViewer");
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
    wxConfig cfg("MDViewer");
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
    std::string projDir = m_projectDir->GetValue().ToStdString();
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
    wxString projDir = m_projectDir->GetValue().Trim();
    if (topic.empty())   { SetStatus("Enter a topic."); return; }
    if (projDir.empty()) { SetStatus("Choose a project folder."); return; }

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

    SetGenerating(true);
    SetStatus("Sending to " + m_backendChoice->GetString(bkIdx) + "…");

    std::string projDirStr = projDir.ToStdString();
    OpenCallback cb = m_openCallback;

    std::thread([this, prompt, cfg, projDirStr, filename, chId, cb]() mutable {
        LLMResult res = InvokeLLM(prompt, cfg);

        wxTheApp->CallAfter([this, res, projDirStr, filename, chId, cb]() mutable {
            SetGenerating(false);
            if (!res.ok) { SetStatus("Error: " + wxString::FromUTF8(res.error)); return; }

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

            std::string path = SaveChapter(projDirStr, filename, stamped);
            if (path.empty()) { SetStatus("Error: could not save chapter file."); return; }

            RegisterChapter(projDirStr, filename);
            for (int i = 0; i < tbCount; ++i)
                RegisterTidbit(projDirStr, chId, i);

            SetStatus("Saved: " + wxString::FromUTF8(filename));
            if (cb) cb(path);
        });
    }).detach();
}
