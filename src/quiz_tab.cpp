#include "quiz_tab.h"
#include "quiz_tab_html.h"
#include "quiz.h"
#include "config.h"
#include "llm.h"
#include "creator.h"
#include <wx/webview.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Minimal JSON field extractor (key → string value)
// ---------------------------------------------------------------------------
static std::string qjField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
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
                    default:   val += e;    break;
                }
            } else val += c;
        }
        return val;
    }
    std::string val;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}')
        val += json[pos++];
    return val;
}

static int qjInt(const std::string& json, const std::string& key, int def = 0) {
    std::string v = qjField(json, key);
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

// ---------------------------------------------------------------------------

static LLMConfig llmCfg() {
    AppState st = LoadAppState();
    LLMConfig cfg;
    cfg.backend = BackendFromLabel(st.backend);
    if (!st.apiKey.empty())      cfg.apiKey      = st.apiKey;
    if (!st.ollamaModel.empty()) cfg.ollamaModel = st.ollamaModel;
    return cfg;
}

// ---------------------------------------------------------------------------

QuizTab::QuizTab(wxWindow* parent, bool darkMode)
    : wxPanel(parent, wxID_ANY)
    , m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("quiztab");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) { OnScriptMessage(evt); });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    Reload();
}

void QuizTab::SetProject(const std::string& projDir,
                         const std::string& selectedDoc,
                         std::function<void()> onChanged) {
    m_projectDir  = projDir;
    m_selectedDoc = selectedDoc;
    m_onChanged   = std::move(onChanged);
    Reload();
}

void QuizTab::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Reload();
}

void QuizTab::Reload() {
    std::vector<QuizTabDoc> docs;
    if (!m_projectDir.empty()) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(m_projectDir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            // Only .md files that are not purely a quiz file
            std::string ext = name.size() > 3 ? name.substr(name.size() - 3) : "";
            if (ext != ".md") continue;
            QuizTabDoc doc;
            doc.name = name;
            std::string content = readFile(entry.path().string());
            auto qs = ParseQuizQuestions(content);
            for (auto& q : qs) {
                QuizTabQuestion qq;
                qq.num      = q.num;
                qq.rawBlock = q.rawBlock;
                doc.questions.push_back(qq);
            }
            docs.push_back(doc);
        }
        std::sort(docs.begin(), docs.end(),
                  [](const QuizTabDoc& a, const QuizTabDoc& b) {
                      return a.name < b.name; });
    }

    std::string html = BuildQuizTabHTML(docs, m_selectedDoc, m_darkMode);
    m_webView->SetPage(wxString::FromUTF8(html), "");
}

std::string QuizTab::readFile(const std::string& path) const {
    std::ifstream f(path);
    if (!f) return "";
    return {(std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()};
}

void QuizTab::writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) {
        wxMessageBox("Could not write: " + wxString::FromUTF8(path),
                     "StoryTeller", wxOK | wxICON_ERROR, this);
        return;
    }
    f << content;
}

void QuizTab::OnScriptMessage(wxWebViewEvent& evt) {
    std::string payload = evt.GetString().ToStdString();
    std::string action  = qjField(payload, "action");
    std::string docName = qjField(payload, "doc");

    if (m_projectDir.empty() || docName.empty()) return;
    std::string docPath = (fs::path(m_projectDir) / docName).string();

    if (action == "generate") {
        int         n     = qjInt(payload, "n", 5);
        std::string extra = qjField(payload, "extra");

        std::string content = readFile(docPath);
        std::string stripped = StripTidbits(content);
        std::string prompt   = BuildQuizPrompt(stripped, n, extra);

        LLMConfig cfg = llmCfg();
        if (cfg.backend == LLMBackend::Clipboard) {
            if (wxTheClipboard->Open()) {
                wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
                wxTheClipboard->Close();
            }
            m_webView->RunScript(wxString::FromUTF8(
                "setStatus('Quiz prompt copied to clipboard. Paste into an LLM, then paste the result back into the document manually.');"
                "setBusy(false);"));
            return;
        }

        std::thread([this, prompt, cfg, docPath, content, n]() mutable {
            LLMResult res = InvokeLLM(prompt, cfg);
            wxTheApp->CallAfter([this, res, docPath, content]() mutable {
                m_webView->RunScript("setBusy(false);");
                if (!res.ok) {
                    m_webView->RunScript(wxString::FromUTF8(
                        "setStatus('Error: " + res.error + "');"));
                    return;
                }
                std::string updated = AppendQuizToMarkdown(
                    content, CleanMarkdownResponse(res.text));
                writeFile(docPath, updated);
                if (m_onChanged) m_onChanged();
                Reload();
            });
        }).detach();

    } else if (action == "removequiz") {
        std::string content = readFile(docPath);
        writeFile(docPath, RemoveEntireQuiz(content));
        if (m_onChanged) m_onChanged();
        Reload();

    } else if (action == "removequestion") {
        int qnum = qjInt(payload, "qnum", 0);
        if (qnum < 1) return;
        std::string content = readFile(docPath);
        writeFile(docPath, RemoveQuizQuestion(content, qnum));
        if (m_onChanged) m_onChanged();
        Reload();

    } else if (action == "editquestion") {
        int qnum = qjInt(payload, "qnum", 0);
        if (qnum < 1) return;
        std::string newText = qjField(payload, "text");
        if (newText.empty()) return;
        std::string content = readFile(docPath);
        writeFile(docPath, ReplaceQuizQuestion(content, qnum, newText));
        if (m_onChanged) m_onChanged();
        Reload();
    }
}
