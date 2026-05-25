#include "prompts_tab.h"
#include "prompts_tab_html.h"
#include "prompt_store.h"
#include <wx/webview.h>
#include <sstream>

static std::string ptField(const std::string& json, const std::string& key) {
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

static int ptInt(const std::string& json, const std::string& key) {
    std::string v = ptField(json, key);
    if (v.empty()) return 0;
    try { return std::stoi(v); } catch (...) { return 0; }
}

// ---------------------------------------------------------------------------

PromptsTab::PromptsTab(wxWindow* parent, bool darkMode)
    : wxPanel(parent, wxID_ANY)
    , m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("promptstab");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) { OnScriptMessage(evt); });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    Reload();
}

void PromptsTab::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Reload();
}

void PromptsTab::Reload() {
    auto prompts = LoadPrompts();
    std::string html = BuildPromptsTabHTML(prompts, m_darkMode);
    m_webView->SetPage(wxString::FromUTF8(html), "");
}

void PromptsTab::SetOnChanged(std::function<void()> cb) {
    m_onChanged = std::move(cb);
}

void PromptsTab::OnScriptMessage(wxWebViewEvent& evt) {
    std::string payload = evt.GetString().ToStdString();
    std::string action  = ptField(payload, "action");

    if (action == "addPrompt") {
        std::string title = ptField(payload, "title");
        std::string text  = ptField(payload, "text");
        if (title.empty() || text.empty()) return;
        AddPrompt(title, text);
        if (m_onChanged) m_onChanged();
        Reload();

    } else if (action == "deletePrompt") {
        int id = ptInt(payload, "id");
        if (id <= 0) return;
        DeletePrompt(id);
        if (m_onChanged) m_onChanged();
        Reload();

    } else if (action == "updatePrompt") {
        int id        = ptInt(payload, "id");
        std::string title = ptField(payload, "title");
        std::string text  = ptField(payload, "text");
        if (id <= 0 || title.empty()) return;
        UpdatePrompt(id, title, text);
        if (m_onChanged) m_onChanged();
        Reload();
    }
}
