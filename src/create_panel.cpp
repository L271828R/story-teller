#include "create_panel.h"
#include "config.h"
#include "create_panel_html.h"
#include "creator.h"
#include "filemeta.h"
#include "llm.h"
#include "llm_response.h"
#include "logger.h"
#include "markdown.h"
#include "meta.h"
#include "project.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <wx/clipbrd.h>
#include <wx/config.h>
#include <wx/dataobj.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/tokenzr.h>
#include <wx/webview.h>

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
            char esc = json[pos++];
            switch (esc) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                default:   val += esc;  break;
            }
        } else {
            val += c;
        }
    }
    return val;
}

static bool ExtractBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    return pos < json.size() && json[pos] == 't';
}

// Escape a C++ string for embedding in a single-quoted JS string literal.
static std::string JsStr(const std::string& s) {
    std::string r = "'";
    for (char c : s) {
        if      (c == '\'') r += "\\'";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') { /* skip */ }
        else r += c;
    }
    r += "'";
    return r;
}

// ── Ollama helper ─────────────────────────────────────────────────────────────

static std::vector<std::string> load_ollama_models() {
    FILE* pipe = popen("curl -s --max-time 1 http://localhost:11434/api/tags 2>/dev/null", "r");
    if (!pipe) return {};
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    return ParseOllamaTags(out);
}

static std::string language_from_topic(const std::string& topic) {
    std::string lower = topic;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    const std::string needle = "language:";
    auto pos = lower.find(needle);
    if (pos == std::string::npos) return "(not specified)";
    pos += needle.size();
    while (pos < topic.size() && topic[pos] == ' ') ++pos;
    auto end = topic.find_first_of(".\n\r", pos);
    std::string lang = topic.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    while (!lang.empty() && std::isspace((unsigned char)lang.back())) lang.pop_back();
    return lang.empty() ? "(not specified)" : lang;
}

// ── Constructor ───────────────────────────────────────────────────────────────

CreatePanel::CreatePanel(wxWindow* parent, OpenCallback onFileGenerated, bool darkMode)
    : wxPanel(parent, wxID_ANY)
    , m_openCallback(std::move(onFileGenerated))
    , m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("create");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) {
            if (evt.GetMessageHandler() == "create")
                HandleMessage(evt.GetString().ToStdString());
        });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    m_webView->SetPage(wxString::FromUTF8(BuildCreatePanelHTML(darkMode)), "");
}

// ── Webview bridge ────────────────────────────────────────────────────────────

void CreatePanel::Run(const std::string& js) {
    if (m_webView && m_ready)
        m_webView->RunScript(wxString::FromUTF8(js));
}

void CreatePanel::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Run(std::string("setDarkMode(") + (dark ? "true" : "false") + ")");
}

// ── State push ────────────────────────────────────────────────────────────────

void CreatePanel::PushStatus(const std::string& msg) {
    Run("setStatus(" + JsStr(msg) + ")");
}

void CreatePanel::PushGenerating(bool on) {
    m_generating = on;
    Run(std::string("setGenerating(") + (on ? "true" : "false") + ")");
}

void CreatePanel::PushTranslating(bool on) {
    m_translating = on;
    Run(std::string("setTranslating(") + (on ? "true" : "false") + ")");
}

void CreatePanel::PushProjectList() {
    AppConfig cfg = LoadConfig();
    auto projects = ListAllProjects(cfg.defaultFolder);

    std::string json = "[";
    for (size_t i = 0; i < projects.size(); ++i) {
        if (i) json += ",";
        json += JsStr(projects[i]);
    }
    json += "]";

    Run("setProjectList(" + json + ","
        + JsStr(m_currentProject) + ","
        + JsStr(CurrentProjectPath()) + ")");
}

void CreatePanel::PushChapters() {
    std::string projPath = CurrentProjectPath();
    std::string json = "[";
    if (!projPath.empty()) {
        std::error_code ec;
        std::vector<std::string> files;
        for (auto& entry : fs::directory_iterator(projPath, ec)) {
            std::string fn = entry.path().filename().string();
            if (entry.path().extension() == ".md" && fn[0] != '.' && fn != "context.md")
                files.push_back(fn);
        }
        std::sort(files.begin(), files.end());
        bool first = true;
        for (auto& fn : files) {
            if (!first) json += ",";
            first = false;
            std::string fp = projPath + "/" + fn;
            json += "{\"name\":"     + JsStr(fn)
                 + ",\"language\":"  + JsStr(ReadLanguage(fp))
                 + ",\"created\":"   + JsStr(FileCreatedTime(fp))
                 + ",\"updated\":"   + JsStr(FileModifiedTime(fp))
                 + "}";
        }
    }
    json += "]";
    Run("setChapterList(" + json + ")");
}

void CreatePanel::PushContext() {
    std::string projPath = CurrentProjectPath();
    if (projPath.empty()) { Run("setContext('')"); return; }
    fs::path ctx = fs::path(projPath) / "context.md";
    std::ifstream f(ctx);
    if (!f) { Run("setContext('')"); return; }
    std::string text{std::istreambuf_iterator<char>(f), {}};
    Run("setContext(" + JsStr(text) + ")");
}

void CreatePanel::PushCharLibrary() {
    // Ensure selected category is valid
    if (m_charsByCategory.find(m_selectedCat) == m_charsByCategory.end())
        m_selectedCat = m_charsByCategory.empty() ? "" : m_charsByCategory.begin()->first;

    std::string json = "{\"categories\":[";
    bool first = true;
    for (auto& [cat, _] : m_charsByCategory) {
        if (!first) json += ",";
        first = false;
        json += JsStr(cat);
    }
    json += "],\"selected\":" + JsStr(m_selectedCat) + ",\"chars\":[";

    auto it = m_charsByCategory.find(m_selectedCat);
    first = true;
    if (it != m_charsByCategory.end()) {
        for (auto& ch : it->second) {
            if (!first) json += ",";
            first = false;
            bool checked = m_checkedChars.count(ch) > 0;
            json += "{\"name\":" + JsStr(ch)
                 + ",\"checked\":" + (checked ? "true" : "false") + "}";
        }
    }
    json += "]}";
    Run("setCharLibrary(" + json + ")");
}

// ── Initial state ─────────────────────────────────────────────────────────────

void CreatePanel::PushInitialState() {
    AppConfig cfg = LoadConfig();
    AppState  st  = LoadAppState();

    // Restore form fields
    std::string stateJs = "restoreFormState({";
    if (!st.topic.empty())
        stateJs += "topic:"       + JsStr(st.topic) + ",";
    else if (!cfg.defaultPrompt.empty())
        stateJs += "topic:"       + JsStr(cfg.defaultPrompt) + ",";
    if (!st.style.empty())
        stateJs += "style:"       + JsStr(st.style) + ",";
    if (!st.backend.empty())
        stateJs += "backend:"     + JsStr(st.backend) + ",";
    if (!st.apiKey.empty())
        stateJs += "apiKey:"      + JsStr(st.apiKey) + ",";
    if (!st.ollamaModel.empty())
        stateJs += "ollamaModel:" + JsStr(st.ollamaModel) + ",";
    stateJs += "})";
    Run(stateJs);

    // Load and push character library (with saved checked chars)
    LoadCharLibrary();
    if (!st.checkedChars.empty()) {
        wxStringTokenizer tok(wxString::FromUTF8(st.checkedChars), "|");
        while (tok.HasMoreTokens())
            m_checkedChars.insert(tok.GetNextToken().ToStdString());
    }
    PushCharLibrary();

    // Select project
    if (!st.currentProject.empty())
        SelectProject(st.currentProject);
    else if (!cfg.defaultFolder.empty()) {
        auto projects = ListAllProjects(cfg.defaultFolder);
        if (!projects.empty()) SelectProject(projects[0]);
    }

    PushProjectList();
    PushChapters();
    PushContext();
    PushStatus("Ready.");
}

// ── SyncProject (called by MDViewerFrame on file open) ────────────────────────

void CreatePanel::SyncProject(const std::string& filePath) {
    if (!m_ready) {
        m_pendingSync = filePath;
        return;
    }
    AppConfig cfg = LoadConfig();
    if (!filePath.empty()) {
        std::string rel = ProjectNameFromFilePath(filePath, cfg.defaultFolder);
        if (!rel.empty()) {
            SelectProject(rel);
            PushProjectList();
            PushChapters();
            PushContext();
            return;
        }
    }
    AppState st = LoadAppState();
    if (!st.currentProject.empty()) {
        SelectProject(st.currentProject);
        PushProjectList();
        PushChapters();
        PushContext();
    }
}

// ── Project helpers ───────────────────────────────────────────────────────────

std::string CreatePanel::CurrentProjectPath() const {
    AppConfig cfg = LoadConfig();
    if (cfg.defaultFolder.empty() || m_currentProject.empty()) return "";
    return cfg.defaultFolder + "/" + m_currentProject;
}

void CreatePanel::SelectProject(const std::string& nameOrRel) {
    AppConfig cfg = LoadConfig();
    auto projects = ListAllProjects(cfg.defaultFolder);

    // Exact match first
    for (auto& p : projects) {
        if (p == nameOrRel) { m_currentProject = p; goto save; }
    }
    // Last-component fallback (AppState compatibility)
    {
        std::string base = fs::path(nameOrRel).filename().string();
        for (auto& p : projects) {
            if (fs::path(p).filename().string() == base) { m_currentProject = p; goto save; }
        }
    }
    return;

save:
    AppState st = LoadAppState();
    st.currentProject = fs::path(m_currentProject).filename().string();
    SaveAppState(st);
}

// ── Character library ─────────────────────────────────────────────────────────

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
        return;
    }
    m_charsByCategory.clear();
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

void CreatePanel::SaveCharLibrary() const {
    wxConfig cfg("StoryTeller");
    cfg.SetPath("/charlib");
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

// ── Message dispatcher ────────────────────────────────────────────────────────

void CreatePanel::HandleMessage(const std::string& json) {
    auto f = [&](const std::string& key) { return ExtractField(json, key); };
    auto b = [&](const std::string& key) { return ExtractBool(json, key); };

    std::string action = f("action");

    if (action == "ready") {
        m_ready = true;
        PushInitialState();
        if (!m_pendingSync.empty()) {
            std::string path = m_pendingSync;
            m_pendingSync.clear();
            SyncProject(path);
        }
    }
    else if (action == "newProject")       DoNewProject(f("name"));
    else if (action == "selectProject")    DoSelectProject(f("name"));
    else if (action == "generate")         DoGenerate(f("topic"),f("style"),f("context"),f("backend"),f("apiKey"),f("ollamaModel"));
    else if (action == "copyPrompt")       DoCopyPrompt(f("topic"),f("style"),f("context"),f("backend"),f("apiKey"),f("ollamaModel"));
    else if (action == "saveContext")      DoSaveContext(f("text"));
    else if (action == "saveState")        DoSaveState(f("topic"),f("style"),f("backend"),f("apiKey"),f("ollamaModel"));
    else if (action == "backendChanged")   DoBackendChanged(f("backend"));
    else if (action == "refreshOllama")    DoRefreshOllama();
    else if (action == "selectCategory")   DoSelectCategory(f("name"));
    else if (action == "addCategory")      DoAddCategory(f("name"));
    else if (action == "deleteCategory")   DoDeleteCategory(f("name"));
    else if (action == "addCharacter")     DoAddCharacter(f("category"),f("name"));
    else if (action == "deleteCharacter")  DoDeleteCharacter(f("category"),f("name"));
    else if (action == "toggleCharacter")  DoToggleCharacter(f("name"), b("checked"));
    else if (action == "openFile")         DoOpenFile(f("name"));
    else if (action == "translateFile")    DoTranslateFile(f("name"),f("language"),f("backend"),f("apiKey"),f("ollamaModel"));
    else if (action == "deleteFile")       DoDeleteFile(f("name"));
}

// ── Action handlers ───────────────────────────────────────────────────────────

void CreatePanel::DoNewProject(const std::string& name) {
    if (name.empty()) return;
    AppConfig cfg = LoadConfig();
    if (cfg.defaultFolder.empty()) {
        PushStatus("Set defaultFolder in ~/.config/story-teller/config first.");
        return;
    }
    if (!CreateProject(cfg.defaultFolder, name)) {
        PushStatus("Could not create project folder.");
        return;
    }
    LoadCharLibrary();
    SelectProject(name);
    PushProjectList();
    PushChapters();
    PushContext();
}

void CreatePanel::DoSelectProject(const std::string& name) {
    SelectProject(name);
    PushProjectList();
    PushChapters();
    PushContext();
}

void CreatePanel::DoCopyPrompt(const std::string& topic, const std::string& style,
                               const std::string& context, const std::string& /*backend*/,
                               const std::string& /*apiKey*/, const std::string& /*ollamaModel*/) {
    if (topic.empty()) { PushStatus("Enter a topic first."); return; }
    GenerationRequest req;
    req.topic          = topic;
    req.style          = style;
    req.projectContext = context;
    for (auto& ch : m_checkedChars) req.characters.push_back(ch);
    std::string prompt = BuildPrompt(req, GetLLMReadme());
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
        wxTheClipboard->Close();
    }
    PushStatus("Prompt copied to clipboard.\n\nPaste into any LLM, then open with File > Open.");
}

void CreatePanel::DoSaveContext(const std::string& text) {
    std::string projPath = CurrentProjectPath();
    if (projPath.empty()) return;
    std::ofstream f(fs::path(projPath) / "context.md", std::ios::trunc);
    if (f) f << text;
    PushStatus("Project context saved.");
}

void CreatePanel::DoSaveState(const std::string& topic, const std::string& style,
                              const std::string& backend, const std::string& apiKey,
                              const std::string& ollamaModel) {
    AppState st = LoadAppState();
    st.currentProject = fs::path(m_currentProject).filename().string();
    st.topic       = topic;
    st.style       = style;
    st.backend     = backend;
    st.apiKey      = apiKey;
    st.ollamaModel = ollamaModel;
    std::string chars;
    for (auto& ch : m_checkedChars) { if (!chars.empty()) chars += "|"; chars += ch; }
    st.checkedChars = chars;
    SaveAppState(st);
    PushStatus("Form saved — will be restored on next launch.");
}

void CreatePanel::DoBackendChanged(const std::string& backend) {
    AppState st = LoadAppState();
    st.backend = backend;
    SaveAppState(st);
}

void CreatePanel::DoRefreshOllama() {
    auto models = load_ollama_models();
    std::string json = "[";
    for (size_t i = 0; i < models.size(); ++i) {
        if (i) json += ",";
        json += JsStr(models[i]);
    }
    json += "]";
    Run("setOllamaModels(" + json + ")");
    PushStatus(models.empty() ? "No Ollama models found at localhost:11434."
                               : "Loaded Ollama models.");
}

void CreatePanel::DoSelectCategory(const std::string& cat) {
    m_selectedCat = cat;
    PushCharLibrary();
}

void CreatePanel::DoAddCategory(const std::string& name) {
    if (name.empty() || m_charsByCategory.count(name)) return;
    m_charsByCategory[name];
    m_selectedCat = name;
    SaveCharLibrary();
    PushCharLibrary();
}

void CreatePanel::DoDeleteCategory(const std::string& name) {
    if (name.empty()) return;
    m_charsByCategory.erase(name);
    m_selectedCat = m_charsByCategory.empty() ? "" : m_charsByCategory.begin()->first;
    SaveCharLibrary();
    PushCharLibrary();
}

void CreatePanel::DoAddCharacter(const std::string& cat, const std::string& name) {
    if (cat.empty() || name.empty()) return;
    auto& vec = m_charsByCategory[cat];
    if (std::find(vec.begin(), vec.end(), name) != vec.end()) return;
    vec.push_back(name);
    m_checkedChars.insert(name);
    SaveCharLibrary();
    PushCharLibrary();
}

void CreatePanel::DoDeleteCharacter(const std::string& cat, const std::string& name) {
    auto it = m_charsByCategory.find(cat);
    if (it == m_charsByCategory.end()) return;
    auto& vec = it->second;
    vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
    m_checkedChars.erase(name);
    SaveCharLibrary();
    PushCharLibrary();
}

void CreatePanel::DoToggleCharacter(const std::string& name, bool checked) {
    if (checked) m_checkedChars.insert(name);
    else         m_checkedChars.erase(name);
}

void CreatePanel::DoOpenFile(const std::string& filename) {
    std::string projPath = CurrentProjectPath();
    if (projPath.empty() || filename.empty()) return;
    if (m_openCallback) m_openCallback(projPath + "/" + filename);
}

void CreatePanel::DoDeleteFile(const std::string& filename) {
    if (filename.empty()) return;
    wxString name = wxString::FromUTF8(filename);
    if (wxMessageBox("Delete \"" + name + "\"?\n\nThis cannot be undone.",
                     "Confirm Delete", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                     this) != wxYES) return;
    std::string projPath = CurrentProjectPath();
    std::error_code ec;
    fs::remove(fs::path(projPath) / filename, ec);
    if (ec) { PushStatus("Could not delete: " + ec.message()); return; }
    PushStatus("Deleted: " + filename);
    PushChapters();
}

// ── Generate ──────────────────────────────────────────────────────────────────

void CreatePanel::DoGenerate(const std::string& topic, const std::string& style,
                             const std::string& context, const std::string& backend,
                             const std::string& apiKey, const std::string& ollamaModel) {
    if (m_generating) return;
    if (topic.empty()) { PushStatus("Enter a topic."); return; }

    std::string projPath = CurrentProjectPath();
    if (projPath.empty()) { PushStatus("Select or create a project first."); return; }
    if (!InitProject(projPath)) { PushStatus("Cannot initialise project folder."); return; }

    GenerationRequest req;
    req.topic          = topic;
    req.style          = style;
    req.projectContext = context;
    for (auto& ch : m_checkedChars) req.characters.push_back(ch);

    int         chId    = NextChapterId(projPath);
    std::string prompt  = BuildPrompt(req, GetLLMReadme());
    LLMBackend  bk      = BackendFromLabel(backend);

    if (bk == LLMBackend::Clipboard) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        PushStatus("Prompt copied to clipboard.\n\nPaste into any LLM, then open with File > Open.");
        return;
    }

    LLMConfig cfg;
    cfg.backend     = bk;
    cfg.apiKey      = apiKey;
    cfg.ollamaModel = ollamaModel;
    cfg.project     = m_currentProject;
    cfg.action      = "generate";

    Logger::get().log("Generate: backend=" + backend
                      + "  project=" + projPath
                      + "  language=" + language_from_topic(topic));

    PushGenerating(true);
    PushStatus("Sending to " + backend + "…");

    OpenCallback cb = m_openCallback;
    std::string  topicStr = topic;

    std::thread([this, prompt, cfg, projPath, chId, topicStr, cb]() mutable {
        auto t0 = std::chrono::steady_clock::now();
        LLMResult res = InvokeLLM(prompt, cfg);
        int elapsed = (int)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - t0).count();

        if (res.ok)
            RecordLLMTiming(projPath, "generate", topicStr, elapsed);

        wxTheApp->CallAfter([this, res, projPath, chId, topicStr, cb]() mutable {
            PushGenerating(false);
            if (!res.ok) {
                Logger::get().log("Generate FAILED: " + res.error);
                PushStatus("Error: " + res.error);
                return;
            }

            std::string content  = CleanMarkdownResponse(res.text);
            std::string filename = FilenameFromContent(content, topicStr, chId);

            // Stamp tidbit markers
            std::string stamped;
            int baseTbId = NextTidbitId(projPath), tbCount = 0;
            std::size_t pos = 0;
            while (pos < content.size()) {
                auto tbpos = content.find(":::tidbit[", pos);
                if (tbpos == std::string::npos) { stamped += content.substr(pos); break; }
                stamped += content.substr(pos, tbpos - pos);
                stamped += "<!-- tb:" + std::to_string(baseTbId + tbCount) + " -->\n";
                auto endpos = content.find("\n:::", tbpos);
                if (endpos == std::string::npos) {
                    stamped += content.substr(tbpos); pos = content.size();
                } else {
                    endpos += 4;
                    stamped += content.substr(tbpos, endpos - tbpos);
                    pos = endpos;
                }
                ++tbCount;
            }
            auto [chStamped, chCount] = StampChapters(stamped, 0);
            stamped = chStamped;

            std::string path = SaveChapter(projPath, filename, stamped);
            if (path.empty()) { PushStatus("Error: could not save chapter file."); return; }

            WriteLanguage(path, language_from_topic(topicStr));
            RegisterChapter(projPath, filename);
            for (int i = 0; i < tbCount; ++i) RegisterTidbit(projPath, chId, i);

            Logger::get().log("Generate saved: " + path);
            PushStatus("Saved: " + filename);
            PushChapters();
            if (cb) cb(path);
        });
    }).detach();
}

// ── Translate ─────────────────────────────────────────────────────────────────

void CreatePanel::DoTranslateFile(const std::string& filename, const std::string& language,
                                  const std::string& backend, const std::string& apiKey,
                                  const std::string& ollamaModel) {
    if (filename.empty() || language.empty()) return;
    std::string projPath = CurrentProjectPath();
    if (projPath.empty()) { PushStatus("No project selected."); return; }

    std::string sourcePath = projPath + "/" + filename;
    std::ifstream ifs(sourcePath);
    if (!ifs) { PushStatus("Cannot read source file."); return; }
    std::string sourceText{std::istreambuf_iterator<char>(ifs), {}};

    LLMBackend bk = BackendFromLabel(backend);
    LLMConfig  cfg;
    cfg.backend     = bk;
    cfg.apiKey      = apiKey;
    cfg.ollamaModel = ollamaModel;
    cfg.project     = m_currentProject;
    cfg.action      = "translate \xe2\x86\x92 " + language;  // → (UTF-8)

    bool        isChinese  = (language == "Chinese (Mandarin)");
    std::string extraInstr = isChinese ? BuildPinyinInstruction() : "";
    OpenCallback cb        = m_openCallback;

    if (bk == LLMBackend::Clipboard) {
        std::string prompt = BuildTranslationPrompt(sourceText, language, extraInstr);
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        PushStatus("Translation prompt copied to clipboard.");
        return;
    }

    Logger::get().log("Translate: backend=" + backend
                      + "  source=" + filename
                      + "  language=" + language
                      + "  source_bytes=" + std::to_string(sourceText.size()));
    PushTranslating(true);
    PushStatus("Translating to " + language + "…");

    std::thread([this, sourceText, language, isChinese, filename,
                 projPath, extraInstr, cfg, cb]() mutable {
        std::string prompt = BuildTranslationPrompt(sourceText, language, extraInstr);
        LLMResult res = InvokeLLM(prompt, cfg);

        if (!res.ok) {
            Logger::get().log("Translate FAILED: " + res.error);
            std::string errMsg = res.error;
            wxTheApp->CallAfter([this, errMsg]() {
                PushTranslating(false);
                PushStatus("Translation error: " + errMsg);
            });
            return;
        }

        Logger::get().log("Translate LLM OK: response_bytes=" + std::to_string(res.text.size()));
        std::string tagged = CleanMarkdownResponse(res.text);
        std::string pinyinPath, mainPath;

        if (isChinese) {
            std::string pinyinFile = TranslationFilename(filename, "Chinese w/ Pinyin");
            pinyinPath = SaveChapter(projPath, pinyinFile, tagged);
            if (!pinyinPath.empty()) {
                WriteLanguage(pinyinPath, "Chinese w/ Pinyin");
                Logger::get().log("Translate saved (pinyin): " + pinyinPath);
            } else {
                Logger::get().log("Translate ERROR: SaveChapter failed for " + pinyinFile);
            }
            std::string stripped    = StripPinyinLines(tagged);
            std::string chineseFile = TranslationFilename(filename, "Chinese (Mandarin)");
            mainPath = SaveChapter(projPath, chineseFile, stripped);
            if (!mainPath.empty()) {
                WriteLanguage(mainPath, "Chinese (Mandarin)");
                Logger::get().log("Translate saved (Chinese): " + mainPath);
            } else {
                Logger::get().log("Translate ERROR: SaveChapter failed for " + chineseFile);
            }
        } else {
            std::string outFile = TranslationFilename(filename, language);
            mainPath = SaveChapter(projPath, outFile, tagged);
            if (!mainPath.empty()) {
                WriteLanguage(mainPath, language);
                Logger::get().log("Translate saved: " + mainPath);
            } else {
                Logger::get().log("Translate ERROR: SaveChapter failed for " + outFile);
            }
        }

        wxTheApp->CallAfter([this, mainPath, pinyinPath, isChinese, cb]() {
            PushTranslating(false);
            PushChapters();
            if (isChinese && !pinyinPath.empty()) {
                PushStatus("Translated: "
                    + fs::path(mainPath).filename().string()
                    + " and "
                    + fs::path(pinyinPath).filename().string());
                if (cb) cb(pinyinPath);
            } else if (!mainPath.empty()) {
                PushStatus("Translated: " + fs::path(mainPath).filename().string());
                if (cb) cb(mainPath);
            } else {
                PushStatus("Error: translation completed but file could not be saved. Check View Logs.");
            }
        });
    }).detach();
}
