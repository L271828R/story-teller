#include "edit_panel.h"
#include "config.h"
#include "creator.h"
#include "editor.h"
#include "llm.h"
#include "logger.h"
#include "project.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace fs = std::filesystem;

enum {
    ID_EP_REFRESH       = wxID_HIGHEST + 300,
    ID_EP_CHAPTER,
    ID_EP_REWRITE,
    ID_EP_RADIO_TIDBIT,
    ID_EP_RADIO_CHAPTER,
};

wxBEGIN_EVENT_TABLE(EditPanel, wxPanel)
    EVT_BUTTON(ID_EP_REFRESH,           EditPanel::OnRefresh)
    EVT_LISTBOX(ID_EP_CHAPTER,          EditPanel::OnChapterSelected)
    EVT_RADIOBUTTON(ID_EP_RADIO_TIDBIT,  EditPanel::OnTargetChanged)
    EVT_RADIOBUTTON(ID_EP_RADIO_CHAPTER, EditPanel::OnTargetChanged)
    EVT_BUTTON(ID_EP_REWRITE,           EditPanel::OnRewrite)
wxEND_EVENT_TABLE()

EditPanel::EditPanel(wxWindow* parent, OpenCallback onFileChanged)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onFileChanged))
{
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* inner = new wxBoxSizer(wxVERTICAL);

    // ── File list (left) + right list (tidbits or chapters) ──────────────
    {
        auto* cols = new wxBoxSizer(wxHORIZONTAL);

        auto* leftCol = new wxBoxSizer(wxVERTICAL);
        leftCol->Add(new wxStaticText(this, wxID_ANY, "File:"), 0, wxBOTTOM, 4);
        m_chapterList = new wxListBox(this, ID_EP_CHAPTER,
                                      wxDefaultPosition, wxSize(200, 160));
        leftCol->Add(m_chapterList, 1, wxEXPAND);
        cols->Add(leftCol, 0, wxEXPAND | wxRIGHT, 12);

        auto* rightCol = new wxBoxSizer(wxVERTICAL);
        m_rightLabel = new wxStaticText(this, wxID_ANY, "Tidbits:");
        rightCol->Add(m_rightLabel, 0, wxBOTTOM, 4);
        m_tidbitList = new wxListBox(this, wxID_ANY,
                                     wxDefaultPosition, wxSize(-1, 160));
        rightCol->Add(m_tidbitList, 1, wxEXPAND);
        cols->Add(rightCol, 1, wxEXPAND);

        inner->Add(cols, 0, wxEXPAND | wxBOTTOM, 6);

        auto* refreshRow = new wxBoxSizer(wxHORIZONTAL);
        refreshRow->AddStretchSpacer();
        refreshRow->Add(new wxButton(this, ID_EP_REFRESH, "↺ Refresh",
                                     wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0);
        inner->Add(refreshRow, 0, wxEXPAND | wxBOTTOM, 10);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Rewrite target ────────────────────────────────────────────────────
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, "Rewrite:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
        m_radioTidbit  = new wxRadioButton(this, ID_EP_RADIO_TIDBIT, "Selected tidbit",
                                           wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        m_radioChapter = new wxRadioButton(this, ID_EP_RADIO_CHAPTER, "Selected chapter");
        m_radioTidbit->SetValue(true);
        row->Add(m_radioTidbit,  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
        row->Add(m_radioChapter, 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxBOTTOM, 10);
    }

    // ── Instruction ───────────────────────────────────────────────────────
    inner->Add(new wxStaticText(this, wxID_ANY, "Instruction:"), 0, wxBOTTOM, 6);
    m_instructCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxSize(-1, 72),
                                    wxTE_MULTILINE | wxTE_RICH2 | wxTE_WORDWRAP);
    m_instructCtrl->SetHint("e.g. Make this shorter and funnier, translate to Spanish…");
    inner->Add(m_instructCtrl, 0, wxEXPAND | wxBOTTOM, 10);
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Buttons ───────────────────────────────────────────────────────────
    m_rewriteBtn = new wxButton(this, ID_EP_REWRITE, "Rewrite");
    inner->Add(m_rewriteBtn, 0, wxBOTTOM, 8);
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 8);

    // ── Status ────────────────────────────────────────────────────────────
    m_statusCtrl = new wxTextCtrl(this, wxID_ANY, "Select a file to begin.",
                                  wxDefaultPosition, wxSize(-1, 80),
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    inner->Add(m_statusCtrl, 1, wxEXPAND);

    outer->Add(inner, 1, wxEXPAND | wxALL, 14);
    SetSizer(outer);
    SetBackgroundColour(wxColour(228, 232, 235));

    RefreshChapters();
}

// ---------------------------------------------------------------------------

std::string EditPanel::CurrentProjectPath() const {
    AppConfig  cfg = LoadConfig();
    AppState   st  = LoadAppState();
    if (cfg.defaultFolder.empty() || st.currentProject.empty()) return "";
    return cfg.defaultFolder + "/" + st.currentProject;
}

std::string EditPanel::CurrentChapterPath() const {
    int sel = m_chapterList->GetSelection();
    if (sel == wxNOT_FOUND) return "";
    std::string proj = CurrentProjectPath();
    if (proj.empty()) return "";
    return proj + "/" + m_chapterList->GetString(sel).ToStdString();
}

void EditPanel::RefreshChapters() {
    m_chapterList->Clear();
    m_tidbitList->Clear();
    m_tidbits.clear();
    m_sections.clear();

    std::string proj = CurrentProjectPath();
    if (proj.empty()) { SetStatus("No project selected — use the Create tab first."); return; }

    std::error_code ec;
    std::vector<std::string> files;
    for (auto& e : fs::directory_iterator(proj, ec))
        if (e.path().extension() == ".md" && e.path().filename().string()[0] != '.')
            files.push_back(e.path().filename().string());
    std::sort(files.begin(), files.end());

    for (auto& f : files)
        m_chapterList->Append(wxString::FromUTF8(f));

    if (m_chapterList->GetCount() > 0) {
        m_chapterList->SetSelection(0);
        ReloadRightList();
    } else {
        SetStatus("No files yet — generate one in the Create tab.");
    }
}

// Reload the right list based on the active radio button.
void EditPanel::ReloadRightList() {
    if (m_radioChapter->GetValue()) {
        m_rightLabel->SetLabel("Chapters:");
        LoadSections();
    } else {
        m_rightLabel->SetLabel("Tidbits:");
        LoadTidbits();
    }
}

void EditPanel::LoadTidbits() {
    m_tidbitList->Clear();
    m_tidbits.clear();

    std::string path = CurrentChapterPath();
    if (path.empty()) return;

    std::ifstream f(path);
    if (!f) { SetStatus("Cannot read: " + path); return; }
    std::string content((std::istreambuf_iterator<char>(f)), {});

    std::string marker_prefix = "<!-- tb:";
    std::size_t pos = 0;
    while ((pos = content.find(marker_prefix, pos)) != std::string::npos) {
        auto end = content.find(" -->", pos);
        if (end == std::string::npos) break;
        std::string id_str = content.substr(pos + marker_prefix.size(),
                                            end - pos - marker_prefix.size());
        int id = 0;
        try { id = std::stoi(id_str); } catch (...) { pos = end; continue; }

        std::string block = ExtractTidbit(content, id);
        std::string preview = "[empty]";
        if (!block.empty()) {
            std::istringstream ss(block);
            std::string line;
            while (std::getline(ss, line))
                if (!line.empty() && line.substr(0, 3) != ":::") { preview = line; break; }
            if (preview.size() > 80) preview = preview.substr(0, 80) + "…";
        }

        m_tidbits.push_back({id, preview});
        m_tidbitList->Append(wxString::Format("tb:%d  %s", id, wxString::FromUTF8(preview)));
        pos = end;
    }

    SetStatus(m_tidbits.empty() ? "No tidbits found in this file."
                                 : wxString::Format("%d tidbit(s) found.", (int)m_tidbits.size()));
}

void EditPanel::LoadSections() {
    m_tidbitList->Clear();
    m_sections.clear();

    std::string path = CurrentChapterPath();
    if (path.empty()) return;

    std::ifstream f(path);
    if (!f) { SetStatus("Cannot read: " + path); return; }
    std::string content((std::istreambuf_iterator<char>(f)), {});

    std::string marker_prefix = "<!-- ch:";
    std::size_t pos = 0;
    while ((pos = content.find(marker_prefix, pos)) != std::string::npos) {
        auto end = content.find(" -->", pos);
        if (end == std::string::npos) break;
        std::string id_str = content.substr(pos + marker_prefix.size(),
                                            end - pos - marker_prefix.size());
        int id = 0;
        try { id = std::stoi(id_str); } catch (...) { pos = end; continue; }

        // Preview: the ## Chapter N: Title heading within this section.
        std::string block = ExtractChapter(content, id);
        std::string preview = "[empty]";
        if (!block.empty()) {
            std::istringstream ss(block);
            std::string line;
            while (std::getline(ss, line))
                if (line.rfind("## ", 0) == 0) { preview = line.substr(3); break; }
            if (preview.size() > 80) preview = preview.substr(0, 80) + "…";
        }

        m_sections.push_back({id, preview});
        m_tidbitList->Append(wxString::Format("ch:%d  %s", id, wxString::FromUTF8(preview)));
        pos = end;
    }

    SetStatus(m_sections.empty()
              ? "No chapters found — generate with a newer prompt."
              : wxString::Format("%d chapter(s) found.", (int)m_sections.size()));
}

// ---------------------------------------------------------------------------

void EditPanel::OnRefresh(wxCommandEvent&)        { RefreshChapters(); }
void EditPanel::OnChapterSelected(wxCommandEvent&) { ReloadRightList(); }
void EditPanel::OnTargetChanged(wxCommandEvent&)   { ReloadRightList(); }

void EditPanel::SetStatus(const wxString& msg) { m_statusCtrl->SetValue(msg); }
void EditPanel::SetBusy(bool on) {
    m_rewriteBtn->Enable(!on);
    m_rewriteBtn->SetLabel(on ? "Rewriting…" : "Rewrite");
}

void EditPanel::OnRewrite(wxCommandEvent&) {
    wxString instr = m_instructCtrl->GetValue().Trim();
    if (instr.empty()) { SetStatus("Enter an instruction first."); return; }

    std::string chapterPath = CurrentChapterPath();
    if (chapterPath.empty()) { SetStatus("Select a file first."); return; }

    std::string chapterContent;
    {
        std::ifstream f(chapterPath);
        if (!f) { SetStatus("Cannot read file."); return; }
        chapterContent.assign(std::istreambuf_iterator<char>(f), {});
    }

    bool chapterMode = m_radioChapter->GetValue();
    int tidbitId  = -1;
    int sectionId = -1;
    std::string originalBlock;

    if (chapterMode) {
        int sel = m_tidbitList->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)m_sections.size()) {
            SetStatus("Select a chapter from the list."); return;
        }
        sectionId     = m_sections[sel].id;
        originalBlock = ExtractChapter(chapterContent, sectionId);
        if (originalBlock.empty()) {
            SetStatus("Could not extract chapter — try Refresh."); return;
        }
    } else {
        int sel = m_tidbitList->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)m_tidbits.size()) {
            SetStatus("Select a tidbit from the list."); return;
        }
        tidbitId      = m_tidbits[sel].id;
        originalBlock = ExtractTidbit(chapterContent, tidbitId);
        if (originalBlock.empty()) {
            SetStatus("Could not extract tidbit — try Refresh."); return;
        }
    }

    AppState st = LoadAppState();
    LLMConfig cfg;
    cfg.backend = LLMBackend::Clipboard;
    if      (st.backend == "claude -p")      cfg.backend = LLMBackend::ClaudeP;
    else if (st.backend == "Ollama (local)") cfg.backend = LLMBackend::Ollama;
    else if (st.backend == "Anthropic API")  cfg.backend = LLMBackend::API;

    std::string prompt   = BuildPatchPrompt(originalBlock, instr.ToStdString(), "", chapterContent);
    std::string chapPath = chapterPath;
    OpenCallback cb      = m_openCallback;

    Logger::get().log("EditPanel rewrite: file=" + chapPath
                      + "  mode=" + (chapterMode ? "chapter" : "tidbit")
                      + "  id=" + std::to_string(chapterMode ? sectionId : tidbitId)
                      + "  backend=" + st.backend);

    SetBusy(true);
    SetStatus("Sending to LLM…");

    std::thread([this, prompt, cfg, chapPath, chapterMode, tidbitId, sectionId, cb]() mutable {
        LLMResult res = InvokeLLM(prompt, cfg);

        wxTheApp->CallAfter([this, res, chapPath, chapterMode, tidbitId, sectionId, cb]() mutable {
            SetBusy(false);
            if (!res.ok) {
                Logger::get().log("EditPanel rewrite FAILED: " + res.error);
                SetStatus("Error: " + wxString::FromUTF8(res.error));
                return;
            }

            bool ok = chapterMode
                      ? ApplyChapterPatch(chapPath, sectionId, res.text)
                      : ApplyTidbitPatch(chapPath, tidbitId, res.text);

            if (!ok) { SetStatus("Rewrite failed — could not write file."); return; }

            ReloadRightList();
            SetStatus("Done. Rendering updated file.");
            if (cb) cb(chapPath);
        });
    }).detach();
}
