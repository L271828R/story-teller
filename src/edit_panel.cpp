#include "edit_panel.h"
#include "config.h"
#include "creator.h"
#include "editor.h"
#include "git_ops.h"
#include "llm.h"
#include "logger.h"
#include "project.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/choice.h>
#include <wx/dataobj.h>
#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace fs = std::filesystem;

enum {
    ID_EP_REFRESH        = wxID_HIGHEST + 300,
    ID_EP_CHAPTER,
    ID_EP_REWRITE,
    ID_EP_RADIO_TIDBIT,
    ID_EP_RADIO_CHAPTER,
    ID_EP_COMMIT,
    ID_EP_VIEW_VER,
    ID_EP_DIFF,
    ID_EP_RESTORE,
};

wxBEGIN_EVENT_TABLE(EditPanel, wxPanel)
    EVT_BUTTON(ID_EP_REFRESH,           EditPanel::OnRefresh)
    EVT_LISTBOX(ID_EP_CHAPTER,          EditPanel::OnChapterSelected)
    EVT_RADIOBUTTON(ID_EP_RADIO_TIDBIT,  EditPanel::OnTargetChanged)
    EVT_RADIOBUTTON(ID_EP_RADIO_CHAPTER, EditPanel::OnTargetChanged)
    EVT_BUTTON(ID_EP_REWRITE,           EditPanel::OnRewrite)
    EVT_BUTTON(ID_EP_COMMIT,            EditPanel::OnCommit)
    EVT_BUTTON(ID_EP_VIEW_VER,          EditPanel::OnViewVersion)
    EVT_BUTTON(ID_EP_DIFF,              EditPanel::OnDiff)
    EVT_BUTTON(ID_EP_RESTORE,           EditPanel::OnRestore)
wxEND_EVENT_TABLE()

EditPanel::EditPanel(wxWindow* parent, OpenCallback onFileChanged)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onFileChanged))
{
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* inner = new wxBoxSizer(wxVERTICAL);

    // ── File list (left) + right list (tidbits or chapter sections) ──────
    {
        auto* cols = new wxBoxSizer(wxHORIZONTAL);

        auto* leftCol = new wxBoxSizer(wxVERTICAL);
        leftCol->Add(new wxStaticText(this, wxID_ANY, "File:"), 0, wxBOTTOM, 4);
        m_chapterList = new wxListBox(this, ID_EP_CHAPTER,
                                      wxDefaultPosition, wxSize(200, 140));
        leftCol->Add(m_chapterList, 1, wxEXPAND);
        cols->Add(leftCol, 0, wxEXPAND | wxRIGHT, 12);

        auto* rightCol = new wxBoxSizer(wxVERTICAL);
        m_rightLabel = new wxStaticText(this, wxID_ANY, "Tidbits:");
        rightCol->Add(m_rightLabel, 0, wxBOTTOM, 4);
        m_tidbitList = new wxListBox(this, wxID_ANY,
                                     wxDefaultPosition, wxSize(-1, 140));
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
                                    wxDefaultPosition, wxSize(-1, 60),
                                    wxTE_MULTILINE | wxTE_RICH2 | wxTE_WORDWRAP);
    m_instructCtrl->SetHint("e.g. Make this shorter and funnier, translate to Spanish…");
    inner->Add(m_instructCtrl, 0, wxEXPAND | wxBOTTOM, 8);

    m_rewriteBtn = new wxButton(this, ID_EP_REWRITE, "Rewrite");
    inner->Add(m_rewriteBtn, 0, wxBOTTOM, 10);
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    // ── Git version history ───────────────────────────────────────────────
    inner->Add(new wxStaticText(this, wxID_ANY, "Version history (select 1 or 2):"),
               0, wxBOTTOM, 4);

    // History list — LB_EXTENDED lets user ctrl/shift-click two items.
    m_historyList = new wxListBox(this, wxID_ANY,
                                  wxDefaultPosition, wxSize(-1, 110),
                                  0, nullptr, wxLB_EXTENDED);
    inner->Add(m_historyList, 0, wxEXPAND | wxBOTTOM, 6);

    {
        // Commit row: message field + Commit button
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        m_commitMsgCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                         wxDefaultPosition, wxDefaultSize, 0);
        m_commitMsgCtrl->SetHint("Commit message…");
        row->Add(m_commitMsgCtrl, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        m_commitBtn = new wxButton(this, ID_EP_COMMIT, "Save to git",
                                   wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        row->Add(m_commitBtn, 0, wxALIGN_CENTER_VERTICAL);
        inner->Add(row, 0, wxEXPAND | wxBOTTOM, 6);
    }

    {
        // Action buttons: View version | Diff | Restore
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        m_viewVerBtn = new wxButton(this, ID_EP_VIEW_VER, "View version",
                                    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_diffBtn    = new wxButton(this, ID_EP_DIFF,    "Diff selected vs current",
                                    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_restoreBtn = new wxButton(this, ID_EP_RESTORE, "Restore",
                                    wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        row->Add(m_viewVerBtn, 0, wxRIGHT, 8);
        row->Add(m_diffBtn,    0, wxRIGHT, 8);
        row->Add(m_restoreBtn, 0);
        inner->Add(row, 0, wxBOTTOM, 10);
    }
    inner->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 8);

    // ── Status ────────────────────────────────────────────────────────────
    m_statusCtrl = new wxTextCtrl(this, wxID_ANY, "Select a file to begin.",
                                  wxDefaultPosition, wxSize(-1, 70),
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
    m_historyList->Clear();
    m_tidbits.clear();
    m_sections.clear();
    m_commits.clear();

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
        LoadHistory();
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

void EditPanel::LoadHistory() {
    m_historyList->Clear();
    m_commits.clear();

    std::string proj = CurrentProjectPath();
    std::string path = CurrentChapterPath();
    if (proj.empty() || path.empty()) return;

    std::string relPath = fs::path(path).filename().string();
    auto log = GitLogFile(proj, relPath);
    for (auto& c : log) {
        m_historyList->Append(wxString::FromUTF8(
            c.date + "  " + c.shortHash + "  " + c.subject));
        m_commits.push_back({c.hash, c.shortHash, c.date, c.subject});
    }
}

// ---------------------------------------------------------------------------

void EditPanel::OnRefresh(wxCommandEvent&)         { RefreshChapters(); }
void EditPanel::OnChapterSelected(wxCommandEvent&) { ReloadRightList(); LoadHistory(); }
void EditPanel::OnTargetChanged(wxCommandEvent&)   { ReloadRightList(); }

void EditPanel::SetStatus(const wxString& msg) { m_statusCtrl->SetValue(msg); }
void EditPanel::SetBusy(bool on) {
    m_rewriteBtn->Enable(!on);
    m_rewriteBtn->SetLabel(on ? "Rewriting…" : "Rewrite");
    m_commitBtn->Enable(!on);
}

// ── Rewrite via LLM ──────────────────────────────────────────────────────────

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
    else if (st.backend == "Codex CLI")      cfg.backend = LLMBackend::CodexCLI;
    else if (st.backend == "Ollama (local)") cfg.backend = LLMBackend::Ollama;
    else if (st.backend == "Anthropic API")  cfg.backend = LLMBackend::API;
    if (!st.apiKey.empty())      cfg.apiKey      = st.apiKey;
    if (!st.ollamaModel.empty()) cfg.ollamaModel = st.ollamaModel;

    // Clipboard mode: copy the prompt so the user can paste it into any LLM manually.
    if (cfg.backend == LLMBackend::Clipboard) {
        std::string prompt = BuildPatchPrompt(originalBlock, instr.ToStdString(), "", chapterContent);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        SetStatus("Prompt copied to clipboard.\n\n"
                  "Paste into any LLM, copy the rewritten block, then paste it back\n"
                  "into the file manually — or set a direct backend in the Create tab\n"
                  "to have the rewrite applied automatically.");
        return;
    }

    std::string backendLabel = st.backend;
    std::string prompt   = BuildPatchPrompt(originalBlock, instr.ToStdString(), "", chapterContent);
    std::string chapPath = chapterPath;
    OpenCallback cb      = m_openCallback;

    Logger::get().log("EditPanel rewrite: file=" + chapPath
                      + "  mode=" + (chapterMode ? "chapter" : "tidbit")
                      + "  id=" + std::to_string(chapterMode ? sectionId : tidbitId)
                      + "  backend=" + st.backend);

    SetBusy(true);
    SetStatus("Sending to " + wxString::FromUTF8(backendLabel) + "…");

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
            LoadHistory();
            SetStatus("Done. Rendering updated file.");
            if (cb) cb(chapPath);
        });
    }).detach();
}

// ── Git operations ────────────────────────────────────────────────────────────

void EditPanel::OnCommit(wxCommandEvent&) {
    std::string proj = CurrentProjectPath();
    std::string path = CurrentChapterPath();
    if (proj.empty() || path.empty()) { SetStatus("Select a file first."); return; }

    wxString msg = m_commitMsgCtrl->GetValue().Trim();
    if (msg.empty()) { SetStatus("Enter a commit message first."); return; }

    std::string relPath = fs::path(path).filename().string();
    bool ok = GitCommitFile(proj, relPath, msg.ToStdString());
    if (!ok) {
        SetStatus("Commit failed — is git configured for this project?");
        return;
    }
    m_commitMsgCtrl->Clear();
    LoadHistory();
    SetStatus("Committed: " + msg);
    Logger::get().log("EditPanel committed: " + relPath + " — " + msg.ToStdString());
}

void EditPanel::OnViewVersion(wxCommandEvent&) {
    wxArrayInt sel;
    m_historyList->GetSelections(sel);
    if (sel.empty()) { SetStatus("Select one commit from history to view."); return; }

    int idx = sel[0];
    if (idx < 0 || idx >= (int)m_commits.size()) return;

    std::string proj    = CurrentProjectPath();
    std::string path    = CurrentChapterPath();
    if (proj.empty() || path.empty()) return;

    std::string relPath = fs::path(path).filename().string();
    std::string content = GitShowFile(proj, m_commits[idx].hash, relPath);
    if (content.empty()) { SetStatus("Could not retrieve that version."); return; }

    // Write to a temp file so the View tab can render it.
    fs::path tmp = fs::temp_directory_path() /
                   ("storyteller_ver_" + m_commits[idx].shortHash + "_" + relPath);
    {
        std::ofstream f(tmp);
        f << content;
    }
    if (m_openCallback) m_openCallback(tmp.string());
    SetStatus("Viewing " + m_commits[idx].shortHash + ": " + m_commits[idx].subject);
}

void EditPanel::OnDiff(wxCommandEvent&) {
    wxArrayInt sel;
    m_historyList->GetSelections(sel);
    if (sel.size() > 2) {
        SetStatus("Select 0, 1, or 2 commits to diff.");
        return;
    }

    std::string proj = CurrentProjectPath();
    std::string path = CurrentChapterPath();
    if (proj.empty() || path.empty()) { SetStatus("Select a file first."); return; }
    std::string relPath = fs::path(path).filename().string();

    std::string hash1, hash2;
    if (sel.empty()) {
        // Nothing selected: diff HEAD vs working copy (show uncommitted changes).
        hash1 = "HEAD";
        hash2 = "";
    } else if (sel.size() == 1) {
        int idx = sel[0];
        if (idx < 0 || idx >= (int)m_commits.size()) return;
        hash1 = m_commits[idx].hash;
        hash2 = "";  // diff vs working copy
    } else {
        // Ensure older commit is hash1 (higher index = older in log)
        int a = sel[0], b = sel[1];
        if (a > b) std::swap(a, b);
        if (a < 0 || b >= (int)m_commits.size()) return;
        hash1 = m_commits[b].hash;  // older
        hash2 = m_commits[a].hash;  // newer
    }

    std::string html = GitDiffHTML(proj, hash1, hash2, relPath);

    fs::path tmp = fs::temp_directory_path() / ("storyteller_diff_" + relPath + ".html");
    {
        std::ofstream f(tmp);
        f << html;
    }
    if (m_openCallback) m_openCallback(tmp.string());
    if (sel.empty()) {
        SetStatus("Diff opened: HEAD vs current file.");
    } else if (sel.size() == 1) {
        int idx = sel[0];
        SetStatus("Diff opened: " + m_commits[idx].shortHash + " vs current file.");
    } else {
        SetStatus("Diff opened between selected versions.");
    }
}

void EditPanel::OnRestore(wxCommandEvent&) {
    wxArrayInt sel;
    m_historyList->GetSelections(sel);
    if (sel.size() != 1) { SetStatus("Select exactly one commit to restore."); return; }

    int idx = sel[0];
    if (idx < 0 || idx >= (int)m_commits.size()) return;

    std::string proj    = CurrentProjectPath();
    std::string path    = CurrentChapterPath();
    if (proj.empty() || path.empty()) return;
    std::string relPath = fs::path(path).filename().string();

    wxString question = wxString::Format(
        "Restore '%s' to version %s (%s)?\nThis will overwrite the working copy.",
        relPath, m_commits[idx].shortHash, m_commits[idx].subject);
    if (wxMessageBox(question, "Confirm restore", wxYES_NO | wxICON_WARNING, this) != wxYES)
        return;

    bool ok = GitRestoreFile(proj, m_commits[idx].hash, relPath);
    if (!ok) { SetStatus("Restore failed."); return; }

    ReloadRightList();
    LoadHistory();
    if (m_openCallback) m_openCallback(path);
    SetStatus("Restored to " + m_commits[idx].shortHash + ": " + m_commits[idx].subject);
    Logger::get().log("EditPanel restored: " + relPath + " to " + m_commits[idx].hash);
}
