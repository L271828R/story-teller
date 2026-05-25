#include "edit_panel.h"
#include "config.h"
#include "creator.h"
#include "quiz.h"
#include "edit_panel_html.h"
#include "editor.h"
#include "git_ops.h"
#include "llm.h"
#include "logger.h"
#include "meta.h"
#include "project.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <cctype>
#include <wx/app.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>
#include <wx/utils.h>

namespace fs = std::filesystem;

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string ExtractField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;
    std::string val;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < json.size()) {
            char e = json[pos++];
            switch (e) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                default:   val += e;    break;
            }
        } else {
            val += c;
        }
    }
    return val;
}

static int ExtractInt(const std::string& json, const std::string& key, int def = -1) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    if (pos >= json.size()) return def;
    try { return std::stoi(json.substr(pos)); } catch (...) { return def; }
}

// Escape a value for embedding in a JS single-quoted string literal.
static std::string jsq(const std::string& s) {
    std::string r = "'";
    for (char c : s) {
        if      (c == '\'') r += "\\'";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') { /* skip */ }
        else r += c;
    }
    return r + "'";
}

static std::string BuildDocumentPrompt(const std::string& content,
                                        const std::string& instruction) {
    return
        "Apply the following instruction to this document.\n"
        "Preserve all structural tokens exactly as written:\n"
        "  - :::tidbit[Name] ... ::: fences\n"
        "  - <!-- tb:N --> and <!-- ch:N --> HTML comments\n"
        "  - ## headings and --- separators\n"
        "Return ONLY the modified document. No code fences, no explanation.\n\n"
        "Instruction: " + instruction + "\n\n"
        "---\n\n" + content;
}

static std::string language_suffix(const std::string& language) {
    std::string out;
    for (unsigned char c : language) {
        if (std::isalnum(c)) out += (char)std::tolower(c);
        else if (!out.empty() && out.back() != '_') out += '_';
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "translated" : out;
}

static std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += "'";
    return out;
}

static std::string applescript_string(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

// ── LLM config helper ─────────────────────────────────────────────────────────

static LLMConfig llm_config_from_state(const AppState& st) {
    LLMConfig cfg;
    cfg.backend = BackendFromLabel(st.backend);
    if (!st.apiKey.empty())      cfg.apiKey      = st.apiKey;
    if (!st.ollamaModel.empty()) cfg.ollamaModel = st.ollamaModel;
    return cfg;
}

// ── Constructor ───────────────────────────────────────────────────────────────

EditPanel::EditPanel(wxWindow* parent, OpenCallback onFileChanged, bool darkMode)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onFileChanged))
    , m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("edit");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) {
            if (evt.GetMessageHandler() == "edit")
                HandleMessage(evt.GetString().ToStdString());
        });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    m_webView->SetPage(wxString::FromUTF8(BuildEditPanelHTML(m_darkMode)), "");
}

// ── Public interface ──────────────────────────────────────────────────────────

void EditPanel::RefreshChapters() {
    if (!m_ready) { m_pendingRefresh = true; return; }
    DoRefresh();
}

void EditPanel::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Run("setDarkMode(" + std::string(dark ? "true" : "false") + ");");
}

// ── WebView helpers ───────────────────────────────────────────────────────────

void EditPanel::Run(const std::string& js) {
    if (!m_ready || !m_webView) return;
    m_webView->RunScript(wxString::FromUTF8(js));
}

void EditPanel::HandleMessage(const std::string& json) {
    std::string action = ExtractField(json, "action");

    if (action == "ready") {
        m_ready = true;
        DoRefresh();
        return;
    }
    if (action == "refresh")             { DoRefresh(); return; }
    if (action == "select_file")         { OnSelectFile(ExtractField(json,"file")); return; }
    if (action == "open_file")           { OnOpenFile(ExtractField(json,"file"), ExtractField(json,"mode")); return; }
    if (action == "move_file")           { OnMoveFile(ExtractField(json,"file"), ExtractField(json,"direction")); return; }
    if (action == "rename_file")         { OnRenameFile(); return; }
    if (action == "delete_file")         { OnDeleteFile(); return; }
    if (action == "set_rewrite_target")  {
        m_rewriteTarget = ExtractField(json, "target");
        m_showSections  = (m_rewriteTarget == "chapter");
        if (m_rewriteTarget != "document") PushItems();
        return;
    }
    if (action == "rewrite")             { OnRewrite(json); return; }
    if (action == "translate")           { OnTranslate(json); return; }
    if (action == "commit")              { OnCommit(ExtractField(json,"file"), ExtractField(json,"message")); return; }
    if (action == "view_version")        { OnViewVersion(ExtractField(json,"commit")); return; }
    if (action == "diff")                { OnDiff(ExtractField(json,"file"), ExtractField(json,"commit1"), ExtractField(json,"commit2")); return; }
    if (action == "restore")             { OnRestore(ExtractField(json,"file"), ExtractField(json,"commit"), ExtractField(json,"shortHash"), ExtractField(json,"subject")); return; }
    if (action == "checkout")            { OnCheckout(ExtractField(json,"commit"), ExtractField(json,"shortHash"), ExtractField(json,"subject")); return; }
    if (action == "stash")               { OnStash(); return; }
    if (action == "unstash")             { OnUnstash(); return; }
    if (action == "quiz")                { OnQuiz(json); return; }
}

// ── State pushers ─────────────────────────────────────────────────────────────

void EditPanel::PushFiles() {
    std::string js = "setFiles([";
    for (size_t i = 0; i < m_files.size(); ++i) {
        if (i) js += ",";
        js += jsq(m_files[i]);
    }
    js += "]," + std::to_string(m_selFile) + ");";
    Run(js);
}

void EditPanel::PushItems() {
    const bool sections = m_showSections;
    std::string js = "setItems([";
    if (sections) {
        for (size_t i = 0; i < m_sections.size(); ++i) {
            if (i) js += ",";
            js += "{id:" + std::to_string(m_sections[i].id) +
                  ",label:" + jsq("ch:" + std::to_string(m_sections[i].id)
                                  + "  " + m_sections[i].preview) +
                  ",type:'chapter'}";
        }
    } else {
        for (size_t i = 0; i < m_tidbits.size(); ++i) {
            if (i) js += ",";
            js += "{id:" + std::to_string(m_tidbits[i].id) +
                  ",label:" + jsq("tb:" + std::to_string(m_tidbits[i].id)
                                  + "  " + m_tidbits[i].preview) +
                  ",type:'tidbit'}";
        }
    }
    js += "]," + jsq(m_rewriteTarget) + ");";
    Run(js);
}

void EditPanel::PushHistory() {
    std::string js = "setHistory([";
    for (size_t i = 0; i < m_commits.size(); ++i) {
        if (i) js += ",";
        js += "{hash:" + jsq(m_commits[i].hash) +
              ",shortHash:" + jsq(m_commits[i].shortHash) +
              ",date:" + jsq(m_commits[i].date) +
              ",subject:" + jsq(m_commits[i].subject) + "}";
    }
    js += "]);";
    Run(js);
}

void EditPanel::PushStatus(const std::string& msg) {
    Run("setStatus(" + jsq(msg) + ");");
}

void EditPanel::PushBusy(bool on) {
    m_busy = on;
    Run(std::string("setBusy(") + (on ? "true" : "false") + ");");
}

// ── Business logic ────────────────────────────────────────────────────────────

std::string EditPanel::CurrentProjectPath() const {
    AppConfig cfg = LoadConfig();
    AppState  st  = LoadAppState();
    if (cfg.defaultFolder.empty() || st.currentProject.empty()) return "";

    // st.currentProject may be just the base name ("darwin") or a full
    // relative path ("Literature/darwin") — try direct first, then search.
    std::string direct = cfg.defaultFolder + "/" + st.currentProject;
    if (fs::exists(direct)) return direct;

    std::string base = fs::path(st.currentProject).filename().string();
    for (const auto& rel : ListAllProjects(cfg.defaultFolder)) {
        if (fs::path(rel).filename().string() == base)
            return cfg.defaultFolder + "/" + rel;
    }
    return "";
}

std::string EditPanel::ChapterPath(const std::string& filename) const {
    std::string proj = CurrentProjectPath();
    if (proj.empty() || filename.empty()) return "";
    return proj + "/" + filename;
}

void EditPanel::DoRefresh() {
    m_files.clear();
    m_tidbits.clear();
    m_sections.clear();
    m_commits.clear();
    m_selFile = -1;

    std::string proj = CurrentProjectPath();
    if (proj.empty()) {
        PushFiles();
        PushItems();
        PushHistory();
        PushStatus("No project selected — use the Create tab to open a project.");
        return;
    }

    std::error_code ec;
    for (auto& e : fs::directory_iterator(proj, ec)) {
        std::string name = e.path().filename().string();
        if (e.path().extension() == ".md" && name[0] != '.')
            m_files.push_back(name);
    }
    std::sort(m_files.begin(), m_files.end());
    m_files = ApplyFileOrder(m_files, LoadFileOrder(proj));

    if (!m_files.empty()) {
        // Pick a sensible default: re-use prev selection, else prefer a
        // non-context story file (one that has tidbit markers).
        int pick = 0;
        if (m_selFile >= 0 && m_selFile < (int)m_files.size()) {
            pick = m_selFile;  // keep previous
        } else {
            for (int i = 0; i < (int)m_files.size(); ++i) {
                if (m_files[i] != "context.md") { pick = i; break; }
            }
        }
        m_selFile = pick;
        LoadItemsForFile(m_files[m_selFile]);
        PushFiles();
        PushItems();
        PushStatus(std::to_string(m_files.size()) + " file(s). "
                   "Double-click to open in View tab.");
        LoadHistoryForProject();
        PushHistory();
    } else {
        PushFiles();
        PushItems();
        PushHistory();
        PushStatus("No .md files found in: " + proj);
    }
}

void EditPanel::LoadItemsForFile(const std::string& filename) {
    m_tidbits.clear();
    m_sections.clear();

    std::string path = ChapterPath(filename);
    if (path.empty()) return;

    std::ifstream f(path);
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)), {});

    // Tidbits
    {
        const std::string prefix = "<!-- tb:";
        size_t pos = 0;
        while ((pos = content.find(prefix, pos)) != std::string::npos) {
            auto end = content.find(" -->", pos);
            if (end == std::string::npos) break;
            std::string id_str = content.substr(pos + prefix.size(), end - pos - prefix.size());
            int id = 0;
            try { id = std::stoi(id_str); } catch (...) { pos = end; continue; }

            std::string block = ExtractTidbit(content, id);
            std::string preview = "[empty]";
            if (!block.empty()) {
                std::istringstream ss(block);
                std::string line;
                while (std::getline(ss, line))
                    if (!line.empty() && line.substr(0, 3) != ":::")
                        { preview = line; break; }
            }
            if (preview.size() > 80) {
                // Truncate at a UTF-8 character boundary (continuation bytes are 10xxxxxx).
                size_t cut = 77;
                while (cut > 0 && (preview[cut] & 0xC0) == 0x80) --cut;
                preview = preview.substr(0, cut) + "...";
            }
            m_tidbits.push_back({id, preview});
            pos = end;
        }
    }

    // Sections / chapters
    {
        const std::string prefix = "<!-- ch:";
        size_t pos = 0;
        while ((pos = content.find(prefix, pos)) != std::string::npos) {
            auto end = content.find(" -->", pos);
            if (end == std::string::npos) break;
            std::string id_str = content.substr(pos + prefix.size(), end - pos - prefix.size());
            int id = 0;
            try { id = std::stoi(id_str); } catch (...) { pos = end; continue; }

            std::string block = ExtractChapter(content, id);
            std::string preview = "[empty]";
            if (!block.empty()) {
                std::istringstream ss(block);
                std::string line;
                while (std::getline(ss, line))
                    if (line.rfind("## ", 0) == 0) { preview = line.substr(3); break; }
            }
            if (preview.size() > 80) {
                size_t cut = 77;
                while (cut > 0 && (preview[cut] & 0xC0) == 0x80) --cut;
                preview = preview.substr(0, cut) + "...";
            }
            m_sections.push_back({id, preview});
            pos = end;
        }
    }
}

void EditPanel::LoadHistoryForProject() {
    m_commits.clear();
    std::string proj = CurrentProjectPath();
    if (proj.empty()) return;
    auto log = GitLogProject(proj);
    for (auto& c : log)
        m_commits.push_back({c.hash, c.shortHash, c.date, c.subject});
}

void EditPanel::SaveCurrentFileOrder() const {
    std::string proj = CurrentProjectPath();
    if (proj.empty()) return;
    SaveFileOrder(proj, m_files);
}

void EditPanel::OpenInVim(const std::string& path) {
    std::string vimCmd = "vim " + shell_quote(path);
    std::string itermScript =
        "tell application \"iTerm2\"\n"
        "  activate\n"
        "  create window with default profile\n"
        "  tell current session of current window\n"
        "    write text " + applescript_string(vimCmd) + "\n"
        "  end tell\n"
        "end tell";
    std::string termScript =
        "tell application \"Terminal\" to activate\n"
        "tell application \"Terminal\" to do script \"vim \" & quoted form of "
        + applescript_string(path);
    std::string cmd = "osascript -e " + shell_quote(itermScript)
                    + " || osascript -e " + shell_quote(termScript);
    long pid = wxExecute(wxString::FromUTF8(cmd), wxEXEC_ASYNC);
    if (pid == 0) PushStatus("Could not open Terminal with Vim.");
}

// ── Action handlers ───────────────────────────────────────────────────────────

void EditPanel::OnSelectFile(const std::string& filename) {
    auto it = std::find(m_files.begin(), m_files.end(), filename);
    if (it == m_files.end()) return;
    m_selFile = (int)(it - m_files.begin());
    LoadItemsForFile(filename);
    PushItems();  // show items immediately; don't block on git
    int nb = (int)(m_showSections ? m_sections.size() : m_tidbits.size());
    PushStatus(filename + " — " + std::to_string(nb) + " item(s) found.");
    LoadHistoryForProject();
    PushHistory();
}

void EditPanel::OnOpenFile(const std::string& filename, const std::string& mode) {
    std::string path = ChapterPath(filename);
    if (path.empty()) { PushStatus("Select a file first."); return; }
    if (mode == "vim") { OpenInVim(path); return; }
    if (m_openCallback) m_openCallback(path);
    PushStatus("Opened in View tab: " + filename);
}

void EditPanel::OnMoveFile(const std::string& filename, const std::string& direction) {
    auto it = std::find(m_files.begin(), m_files.end(), filename);
    if (it == m_files.end()) return;
    int idx = (int)(it - m_files.begin());
    if (direction == "up" && idx > 0) {
        std::swap(m_files[idx], m_files[idx - 1]);
        m_selFile = idx - 1;
    } else if (direction == "down" && idx + 1 < (int)m_files.size()) {
        std::swap(m_files[idx], m_files[idx + 1]);
        m_selFile = idx + 1;
    } else {
        return;
    }
    SaveCurrentFileOrder();
    PushFiles();
}

void EditPanel::OnRenameFile() {
    if (m_selFile < 0 || m_selFile >= (int)m_files.size()) {
        PushStatus("Select a file first.");
        return;
    }
    std::string oldName = m_files[m_selFile];
    wxString newNameWx = wxGetTextFromUser(
        "New filename:", "Rename File",
        wxString::FromUTF8(oldName), this).Trim();
    if (newNameWx.empty() || newNameWx.ToStdString() == oldName) return;

    std::string newName = newNameWx.ToStdString();
    fs::path newFs(newName);
    if (newFs.has_parent_path()) { PushStatus("Enter a filename only, not a path."); return; }
    if (newFs.extension().empty()) newFs += ".md";
    if (newFs.extension() != ".md") { PushStatus("Filename must end in .md."); return; }

    std::string proj = CurrentProjectPath();
    if (proj.empty()) return;
    fs::path oldFull(proj + "/" + oldName);
    fs::path newFull = oldFull.parent_path() / newFs;
    if (fs::exists(newFull)) { PushStatus("Rename failed: target already exists."); return; }

    std::error_code ec;
    fs::rename(oldFull, newFull, ec);
    if (ec) { PushStatus("Rename failed: " + ec.message()); return; }

    Logger::get().log("EditPanel renamed: " + oldName + " -> " + newFs.string());
    m_files[m_selFile] = newFs.filename().string();
    SaveCurrentFileOrder();
    PushFiles();
    PushStatus("Renamed to: " + newFs.filename().string());
}

void EditPanel::OnDeleteFile() {
    if (m_selFile < 0 || m_selFile >= (int)m_files.size()) {
        PushStatus("Select a file first.");
        return;
    }
    std::string filename = m_files[m_selFile];
    wxString question = "Delete '" + wxString::FromUTF8(filename) + "'?\n\n"
        "This removes the file from disk. Git history may preserve older versions.";
    if (wxMessageBox(question, "Confirm delete",
                     wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
        return;

    std::string path = ChapterPath(filename);
    std::error_code ec;
    bool removed = fs::remove(path, ec);
    if (!removed || ec) { PushStatus("Delete failed: " + (ec ? ec.message() : "not removed")); return; }

    Logger::get().log("EditPanel deleted: " + filename);
    m_files.erase(m_files.begin() + m_selFile);
    if (m_selFile >= (int)m_files.size()) m_selFile = (int)m_files.size() - 1;
    if (m_selFile >= 0) LoadItemsForFile(m_files[m_selFile]);
    else { m_tidbits.clear(); m_sections.clear(); }
    SaveCurrentFileOrder();
    PushFiles();
    PushItems();
    PushStatus("Deleted: " + filename);
}

// ── Rewrite ───────────────────────────────────────────────────────────────────

void EditPanel::OnRewrite(const std::string& json) {
    std::string filename = ExtractField(json, "file");
    std::string target   = ExtractField(json, "target");
    int         id       = ExtractInt(json, "id");
    std::string instr    = ExtractField(json, "instruction");

    std::string path = ChapterPath(filename);
    if (path.empty() || instr.empty()) return;

    std::string content;
    { std::ifstream f(path); if (!f) { PushStatus("Cannot read file."); return; }
      content.assign(std::istreambuf_iterator<char>(f), {}); }

    // ── Document-wide rewrite ─────────────────────────────────────────────────
    if (target == "document") {
        AppState  st  = LoadAppState();
        LLMConfig cfg = llm_config_from_state(st);
        std::string prompt = BuildDocumentPrompt(content, instr);

        if (cfg.backend == LLMBackend::Clipboard) {
            if (wxTheClipboard->Open()) {
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
                wxTheClipboard->Close();
            }
            PushStatus("Document prompt copied to clipboard.\n"
                       "Paste into any LLM, copy the result, then paste it back as the file.");
            return;
        }

        Logger::get().log("EditPanel rewrite-document: file=" + filename);
        PushBusy(true);
        PushStatus("Sending entire document to " + st.backend + "…");

        std::string chapPath = path;
        OpenCallback cb = m_openCallback;

        std::string backend = st.backend;
        std::thread([this, prompt, cfg, chapPath, filename, cb, backend]() mutable {
            auto t0 = std::chrono::steady_clock::now();
            LLMResult res = InvokeLLM(prompt, cfg);
            int secs = (int)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - t0).count();
            if (res.ok)
                RecordLLMTiming(fs::path(chapPath).parent_path().string(),
                                "rewrite-doc", filename, secs, backend);

            wxTheApp->CallAfter([this, res, chapPath, filename, cb]() mutable {
                PushBusy(false);
                if (!res.ok) {
                    Logger::get().log("EditPanel rewrite-document FAILED: " + res.error);
                    PushStatus("Error: " + res.error);
                    return;
                }
                {
                    std::ofstream f(chapPath, std::ios::trunc);
                    f << CleanMarkdownResponse(res.text);
                    if (!f.good()) { PushStatus("Could not write file."); return; }
                }
                OnSelectFile(filename);
                PushStatus("Document rewrite done: " + filename);
                if (cb) cb(chapPath);
            });
        }).detach();
        return;
    }

    bool chapterMode = (target == "chapter");
    std::string originalBlock;
    if (chapterMode) {
        if (id < 0) { PushStatus("Select a chapter section first."); return; }
        originalBlock = ExtractChapter(content, id);
        if (originalBlock.empty()) { PushStatus("Could not extract chapter — try Refresh."); return; }
    } else {
        if (id < 0) { PushStatus("Select a tidbit first."); return; }
        originalBlock = ExtractTidbit(content, id);
        if (originalBlock.empty()) { PushStatus("Could not extract tidbit — try Refresh."); return; }
    }

    AppState st  = LoadAppState();
    LLMConfig cfg = llm_config_from_state(st);
    std::string prompt = BuildPatchPrompt(originalBlock, instr, "", content);

    if (cfg.backend == LLMBackend::Clipboard) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        PushStatus("Prompt copied to clipboard.\nPaste into any LLM, then copy the result "
                   "and paste it back manually — or set a direct backend in the Create tab.");
        return;
    }

    Logger::get().log("EditPanel rewrite: file=" + filename
                      + " mode=" + target + " id=" + std::to_string(id));
    PushBusy(true);
    PushStatus("Sending to " + st.backend + "…");

    std::string chapPath = path;
    OpenCallback cb = m_openCallback;

    std::string backend = st.backend;
    std::thread([this, prompt, cfg, chapPath, chapterMode, id, cb, filename, backend]() mutable {
        auto t0 = std::chrono::steady_clock::now();
        LLMResult res = InvokeLLM(prompt, cfg);
        int secs = (int)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - t0).count();
        if (res.ok)
            RecordLLMTiming(fs::path(chapPath).parent_path().string(),
                            "patch", (chapterMode?"chapter ":"tidbit ")+std::to_string(id), secs, backend);

        wxTheApp->CallAfter([this, res, chapPath, chapterMode, id, cb, filename]() mutable {
            PushBusy(false);
            if (!res.ok) {
                Logger::get().log("EditPanel rewrite FAILED: " + res.error);
                PushStatus("Error: " + res.error);
                return;
            }
            bool ok = chapterMode ? ApplyChapterPatch(chapPath, id, res.text)
                                  : ApplyTidbitPatch(chapPath, id, res.text);
            if (!ok) { PushStatus("Rewrite failed — could not write file."); return; }
            OnSelectFile(filename);
            PushStatus("Rewrite done.");
            if (cb) cb(chapPath);
        });
    }).detach();
}

// ── Translate ─────────────────────────────────────────────────────────────────

void EditPanel::OnTranslate(const std::string& json) {
    std::string filename = ExtractField(json, "file");
    std::string language = ExtractField(json, "language");
    std::string instr    = ExtractField(json, "instruction");
    std::string commit   = ExtractField(json, "commit");

    std::string srcPath = ChapterPath(filename);
    if (srcPath.empty() || language.empty()) return;

    std::string sourceContent;
    if (!commit.empty()) {
        std::string proj = CurrentProjectPath();
        sourceContent = GitShowFile(proj, commit, filename);
        if (sourceContent.empty()) { PushStatus("Could not read selected version."); return; }
    } else {
        std::ifstream f(srcPath);
        if (!f) { PushStatus("Cannot read file."); return; }
        sourceContent.assign(std::istreambuf_iterator<char>(f), {});
    }

    AppState st   = LoadAppState();
    LLMConfig cfg = llm_config_from_state(st);
    std::string prompt = BuildTranslationPrompt(sourceContent, language, instr);

    if (cfg.backend == LLMBackend::Clipboard) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        PushStatus("Translation prompt copied to clipboard.");
        return;
    }

    fs::path src(srcPath);
    std::string suffix = language_suffix(language);
    fs::path outPath = src.parent_path() /
        (src.stem().string() + "_" + suffix + src.extension().string());
    OpenCallback cb = m_openCallback;

    Logger::get().log("EditPanel translate: file=" + filename + " lang=" + language);
    PushBusy(true);
    PushStatus("Translating to " + language + "…");

    std::string backend = st.backend;
    std::thread([this, prompt, cfg, outPath, cb, backend]() mutable {
        auto t0 = std::chrono::steady_clock::now();
        LLMResult res = InvokeLLM(prompt, cfg);
        int secs = (int)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - t0).count();
        if (res.ok)
            RecordLLMTiming(outPath.parent_path().string(),
                            "translate", outPath.filename().string(), secs, backend);
        wxTheApp->CallAfter([this, res, outPath, cb]() mutable {
            PushBusy(false);
            if (!res.ok) {
                Logger::get().log("EditPanel translate FAILED: " + res.error);
                PushStatus("Error: " + res.error);
                return;
            }
            { std::ofstream f(outPath); f << CleanMarkdownResponse(res.text);
              if (!f.good()) { PushStatus("Could not write translated file."); return; } }
            DoRefresh();
            PushStatus("Translated file saved: " + outPath.filename().string());
            if (cb) cb(outPath.string());
        });
    }).detach();
}

// ── Quiz ──────────────────────────────────────────────────────────────────────

void EditPanel::OnQuiz(const std::string& json) {
    std::string filename = ExtractField(json, "file");
    int         n        = ExtractInt(json, "n", 5);
    std::string extra    = ExtractField(json, "extra");

    std::string path = ChapterPath(filename);
    if (path.empty()) return;

    std::string content;
    { std::ifstream f(path); if (!f) { PushStatus("Cannot read file."); return; }
      content.assign(std::istreambuf_iterator<char>(f), {}); }

    AppState  st  = LoadAppState();
    LLMConfig cfg = llm_config_from_state(st);
    std::string stripped = StripTidbits(content);
    std::string prompt   = BuildQuizPrompt(stripped, n, extra);

    if (cfg.backend == LLMBackend::Clipboard) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        PushStatus("Quiz prompt copied to clipboard.\nPaste into any LLM, copy the result, "
                   "then paste it back and append it manually as a '## Quiz' section.");
        return;
    }

    Logger::get().log("EditPanel quiz: file=" + filename + " n=" + std::to_string(n));
    PushBusy(true);
    PushStatus("Generating " + std::to_string(n) + "-question quiz via " + st.backend + "…");

    OpenCallback cb = m_openCallback;

    std::thread([this, prompt, cfg, path, content, cb, filename]() mutable {
        LLMResult res = InvokeLLM(prompt, cfg);
        wxTheApp->CallAfter([this, res, path, content, cb, filename]() mutable {
            PushBusy(false);
            if (!res.ok) {
                Logger::get().log("EditPanel quiz FAILED: " + res.error);
                PushStatus("Error: " + res.error);
                return;
            }
            std::string updated = AppendQuizToMarkdown(content, CleanMarkdownResponse(res.text));
            { std::ofstream f(path, std::ios::trunc);
              f << updated;
              if (!f.good()) { PushStatus("Could not write file."); return; } }
            PushStatus("Quiz appended to " + filename);
            if (cb) cb(path);
        });
    }).detach();
}

// ── Git operations ────────────────────────────────────────────────────────────

void EditPanel::OnCommit(const std::string& filename, const std::string& message) {
    std::string proj = CurrentProjectPath();
    if (proj.empty() || filename.empty() || message.empty()) return;
    bool ok = GitCommitFile(proj, filename, message);
    if (!ok) { PushStatus("Commit failed — is git configured for this project?"); return; }
    Run("clearCommitMsg();");
    Logger::get().log("EditPanel committed: " + filename + " — " + message);
    PushStatus("Committed: " + message);
    // Refresh history after a short delay (git index may not be instant).
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        wxTheApp->CallAfter([this]() {
            LoadHistoryForProject();
            PushHistory();
        });
    }).detach();
}

void EditPanel::OnViewVersion(const std::string& commitHash) {
    if (commitHash.empty() || m_selFile < 0) {
        PushStatus("Select a file and a commit first.");
        return;
    }
    std::string proj    = CurrentProjectPath();
    std::string relPath = m_files[m_selFile];
    std::string content = GitShowFile(proj, commitHash, relPath);
    if (content.empty()) { PushStatus("Could not retrieve that version."); return; }

    std::string shortHash = commitHash.size() >= 7 ? commitHash.substr(0, 7) : commitHash;
    fs::path tmp = fs::temp_directory_path() /
                   ("storyteller_ver_" + shortHash + "_" + relPath);
    { std::ofstream f(tmp); f << content; }
    if (m_openCallback) m_openCallback(tmp.string());
    PushStatus("Viewing " + shortHash + ": " + relPath);
}

void EditPanel::OnDiff(const std::string& filename,
                       const std::string& hash1, const std::string& hash2) {
    std::string proj = CurrentProjectPath();
    if (proj.empty() || filename.empty()) { PushStatus("Select a file first."); return; }
    std::string html = GitDiffHTML(proj, hash1, hash2, filename);
    fs::path tmp = fs::temp_directory_path() / ("storyteller_diff_" + filename + ".html");
    { std::ofstream f(tmp); f << html; }
    if (m_openCallback) m_openCallback(tmp.string());
    PushStatus("Diff opened: " + filename);
}

void EditPanel::OnRestore(const std::string& filename, const std::string& commitHash,
                          const std::string& shortHash, const std::string& subject) {
    if (filename.empty() || commitHash.empty()) return;
    wxString question = wxString::Format(
        "Restore '%s' to version %s (%s)?\nThis overwrites the working copy.",
        filename, shortHash, subject);
    if (wxMessageBox(question, "Confirm restore",
                     wxYES_NO | wxICON_WARNING, this) != wxYES) return;

    std::string proj = CurrentProjectPath();
    bool ok = GitRestoreFile(proj, commitHash, filename);
    if (!ok) { PushStatus("Restore failed."); return; }
    OnSelectFile(filename);
    if (m_openCallback) m_openCallback(ChapterPath(filename));
    PushStatus("Restored to " + shortHash + ": " + subject);
    Logger::get().log("EditPanel restored: " + filename + " to " + commitHash);
}

void EditPanel::OnCheckout(const std::string& commitHash,
                           const std::string& shortHash, const std::string& subject) {
    if (commitHash.empty()) return;
    wxString question = wxString::Format(
        "Checkout the whole project folder to version %s (%s)?\n\n"
        "This affects every file and may leave git in detached HEAD.\n"
        "Use Stash first if you have local changes to keep.",
        shortHash, subject);
    if (wxMessageBox(question, "Confirm checkout",
                     wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;

    std::string proj = CurrentProjectPath();
    bool ok = GitCheckoutCommit(proj, commitHash);
    if (!ok) { PushStatus("Checkout failed. Stash or commit local changes first."); return; }
    DoRefresh();
    if (m_selFile >= 0 && m_openCallback)
        m_openCallback(ChapterPath(m_files[m_selFile]));
    PushStatus("Checked out at " + shortHash + ": " + subject);
    Logger::get().log("EditPanel checkout: " + proj + " to " + commitHash);
}

void EditPanel::OnStash() {
    std::string proj = CurrentProjectPath();
    if (proj.empty()) return;
    bool ok = GitStashProject(proj, "StoryTeller stash");
    DoRefresh();
    PushStatus(ok ? "Stashed local project-folder changes."
                  : "Stash failed or there were no changes to stash.");
    Logger::get().log(std::string("EditPanel stash ") + (ok ? "OK: " : "FAILED: ") + proj);
}

void EditPanel::OnUnstash() {
    std::string proj = CurrentProjectPath();
    if (proj.empty()) return;
    bool ok = GitUnstashProject(proj);
    DoRefresh();
    if (m_selFile >= 0 && m_openCallback)
        m_openCallback(ChapterPath(m_files[m_selFile]));
    PushStatus(ok ? "Unstashed latest project-folder changes."
                  : "Unstash failed. There may be no stash, or there may be conflicts.");
    Logger::get().log(std::string("EditPanel unstash ") + (ok ? "OK: " : "FAILED: ") + proj);
}
