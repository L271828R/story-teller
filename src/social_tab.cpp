#include "social_tab.h"
#include "social_tab_html.h"
#include "social.h"
#include "social_results.h"
#include "image_captions.h"
#include "config.h"
#include "llm.h"
#include <wx/webview.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sstream>

namespace fs = std::filesystem;

static std::string sjField(const std::string& json, const std::string& key) {
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

static LLMConfig llmCfg() {
    AppState st = LoadAppState();
    LLMConfig cfg;
    cfg.backend = BackendFromLabel(st.backend);
    if (!st.apiKey.empty())      cfg.apiKey      = st.apiKey;
    if (!st.ollamaModel.empty()) cfg.ollamaModel = st.ollamaModel;
    return cfg;
}

static std::string escJs(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': o += "\\\\"; break;
            case '\'': o += "\\'";  break;
            case '\n': o += "\\n";  break;
            case '\r': break;
            default:   o += c;     break;
        }
    }
    return o;
}

SocialTab::SocialTab(wxWindow* parent, bool darkMode)
    : wxPanel(parent, wxID_ANY)
    , m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("socialtab");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) { OnScriptMessage(evt); });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    Reload();
}

void SocialTab::SetProject(const std::string& projDir,
                            const std::string& selectedDoc,
                            std::function<void()> onChanged) {
    m_projectDir  = projDir;
    m_selectedDoc = selectedDoc;
    m_onChanged   = std::move(onChanged);
    Reload();
}

void SocialTab::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Reload();
}

void SocialTab::Reload() {
    std::vector<std::string> docs;
    if (!m_projectDir.empty()) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(m_projectDir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            std::string ext = name.size() > 3 ? name.substr(name.size() - 3) : "";
            if (ext != ".md") continue;
            docs.push_back(name);
        }
        std::sort(docs.begin(), docs.end());
    }

    auto allResults = LoadAllSocialResults(m_projectDir);
    std::string html = BuildSocialTabHTML(docs, m_selectedDoc, m_darkMode, allResults);
    m_webView->SetPage(wxString::FromUTF8(html), "");
}

std::string SocialTab::readFile(const std::string& path) const {
    std::ifstream f(path);
    if (!f) return "";
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

void SocialTab::OnScriptMessage(wxWebViewEvent& evt) {
    std::string payload = evt.GetString().ToStdString();
    std::string action  = sjField(payload, "action");
    std::string docName = sjField(payload, "doc");

    if (action == "copy") {
        std::string text = sjField(payload, "text");
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(text)));
            wxTheClipboard->Close();
        }
        return;
    }

    if (m_projectDir.empty() || docName.empty()) return;
    std::string docPath = (fs::path(m_projectDir) / docName).string();
    std::string content = InjectImageCaptions(
        StripSocialNoise(readFile(docPath)), m_projectDir);

    LLMConfig cfg = llmCfg();

    auto clipboardFallback = [&](const std::string& prompt) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        m_webView->RunScript(
            "setStatus('Prompt copied to clipboard — paste into an LLM, "
            "then paste the result back manually.');"
            "setBusy('hook',false);setBusy('packaging',false);setBusy('atomize',false);");
    };

    auto dispatchResult = [this](const std::string& renderFn,
                                   const std::string& projDir,
                                   const std::string& doc,
                                   const std::string& section,
                                   const std::string& meta) {
        return [this, renderFn, projDir, doc, section, meta](const LLMResult& res) {
            if (!res.ok) {
                m_webView->RunScript(wxString::FromUTF8(
                    "setStatus('Error: " + res.error + "');"
                    "setBusy('hook',false);setBusy('packaging',false);setBusy('atomize',false);"));
                return;
            }
            if (!projDir.empty() && !doc.empty())
                AppendSocialResult(projDir, doc, section, {res.text, meta});
            std::string safe = escJs(res.text);
            m_webView->RunScript(wxString::FromUTF8(
                renderFn + "('" + safe + "');"
                "setStatus('');"
                "setBusy('hook',false);setBusy('packaging',false);setBusy('atomize',false);"));
        };
    };

    if (action == "hook") {
        std::string lang   = sjField(payload, "language");
        std::string energy = sjField(payload, "energy");
        std::string nudge  = sjField(payload, "nudge");
        auto savedResults  = LoadAllSocialResults(m_projectDir);
        std::vector<std::string> prevHooks;
        auto it = savedResults.find(docName);
        if (it != savedResults.end())
            for (const auto& e : it->second.hooks)
                prevHooks.push_back(e.text);
        std::string prompt = BuildHookPrompt(content, lang, energy, nudge, prevHooks);

        if (cfg.backend == LLMBackend::Clipboard) { clipboardFallback(prompt); return; }

        auto cb = dispatchResult("renderHookResult", m_projectDir, docName, "hooks", lang + "/" + energy);
        std::thread([this, prompt, cfg, cb]() mutable {
            LLMResult res = InvokeLLM(prompt, cfg);
            wxTheApp->CallAfter([this, res, cb]() mutable { cb(res); });
        }).detach();

    } else if (action == "packaging") {
        std::string prompt = BuildPackagingPrompt(content);

        if (cfg.backend == LLMBackend::Clipboard) { clipboardFallback(prompt); return; }

        auto cb = dispatchResult("renderPackagingResult", m_projectDir, docName, "packaging", "");
        std::thread([this, prompt, cfg, cb]() mutable {
            LLMResult res = InvokeLLM(prompt, cfg);
            wxTheApp->CallAfter([this, res, cb]() mutable { cb(res); });
        }).detach();

    } else if (action == "atomize") {
        std::string prompt = BuildAtomizationPrompt(content);

        if (cfg.backend == LLMBackend::Clipboard) { clipboardFallback(prompt); return; }

        auto cb = dispatchResult("renderAtomizationResult", m_projectDir, docName, "atomization", "");
        std::thread([this, prompt, cfg, cb]() mutable {
            LLMResult res = InvokeLLM(prompt, cfg);
            wxTheApp->CallAfter([this, res, cb]() mutable { cb(res); });
        }).detach();

    } else if (action == "clearsection") {
        std::string section = sjField(payload, "section");
        // Map JS section name → storage section name
        std::string storageSection = (section == "hook")       ? "hooks"
                                   : (section == "atomize")    ? "atomization"
                                   :                             section; // "packaging" → "packaging"
        ClearSocialSection(m_projectDir, docName, storageSection);
    }
}
