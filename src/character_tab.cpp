#include "character_tab.h"
#include "character_tab_html.h"
#include "conversation.h"
#include "llm.h"
#include "config.h"
#include "markdown.h"
#include "persona.h"
#include "thumbnail.h"
#include "logger.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <thread>
#include <wx/config.h>
#include <wx/sizer.h>
#include <wx/tokenzr.h>
#include <wx/filedlg.h>

// ── JSON helpers ──────────────────────────────────────────────────────────────

static std::string CtField(const std::string& json, const std::string& key) {
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
                default:   val += e;    break;
            }
        } else val += c;
    }
    return val;
}

static bool CtBool(const std::string& json, const std::string& key) {
    for (const auto& pat : {"\"" + key + "\":","\"" + key + "\": "}) {
        size_t pos = json.find(pat);
        if (pos == std::string::npos) continue;
        pos += pat.size();
        while (pos < json.size() && json[pos] == ' ') ++pos;
        return pos < json.size() && json[pos] == 't';
    }
    return false;
}

static std::string Jq(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c < 0x20)  { char b[8]; snprintf(b, 8, "\\u%04x", c); o += b; }
        else                 o += (char)c;
    }
    return o + "\"";
}

// ── Default library ───────────────────────────────────────────────────────────

static const std::map<std::string, std::vector<std::string>>& DefaultLibrary() {
    static const std::map<std::string, std::vector<std::string>> lib = {
        {"Science",    {"Albert Einstein", "Marie Curie", "Carl Sagan",
                        "Richard Feynman", "Nikola Tesla", "Charles Darwin"}},
        {"Literature", {"Sherlock Holmes", "Agatha Christie", "Edgar Allan Poe"}},
        {"History",    {"Ada Lovelace", "Napoleon Bonaparte", "Cleopatra"}},
    };
    return lib;
}

// ── CharacterTab ──────────────────────────────────────────────────────────────

CharacterTab::CharacterTab(wxWindow* parent, bool darkMode)
    : wxPanel(parent, wxID_ANY), m_darkMode(darkMode)
{
    LoadState();

    m_webView = wxWebView::New(this, wxID_ANY);
    m_webView->AddScriptMessageHandler("characters");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) {
            HandleMessage(evt.GetString().ToStdString());
        }, wxID_ANY);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    m_webView->SetPage(wxString::FromUTF8(BuildCharacterTabHTML(darkMode)), "");
}

void CharacterTab::Run(const std::string& js) {
    if (!m_ready) return;
    m_webView->RunScript(wxString::FromUTF8(js));
}

void CharacterTab::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Run(std::string("setDarkMode(") + (dark ? "true" : "false") + ")");
}

void CharacterTab::Activate() {
    if (m_ready) PushState();
    else         m_pendingActivate = true;
}

// ── Persistence ───────────────────────────────────────────────────────────────

void CharacterTab::LoadState() {
    {
        wxConfig cfg("StoryTeller");
        cfg.SetPath("/charlib");
        wxString catStr;
        if (!cfg.Read("categories", &catStr) || catStr.empty()) {
            m_charsByCategory = DefaultLibrary();
        } else {
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
        wxString cs;
        if (cfg.Read("checked", &cs) && !cs.empty()) {
            wxStringTokenizer tok(cs, "|");
            while (tok.HasMoreTokens())
                m_checkedChars.insert(tok.GetNextToken().ToStdString());
        }
    }
    {
        wxConfig dcfg("StoryTeller");
        dcfg.SetPath("/charlib_descriptions");
        wxString key, val;
        long idx = 0;
        if (dcfg.GetFirstEntry(key, idx)) {
            do {
                dcfg.Read(key, &val);
                m_charDescriptions[key.ToStdString()] = val.ToStdString();
            } while (dcfg.GetNextEntry(key, idx));
        }
    }
}

void CharacterTab::SaveState() const {
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
    wxString cs;
    for (auto& ch : m_checkedChars) {
        if (!cs.empty()) cs += "|";
        cs += wxString::FromUTF8(ch);
    }
    cfg.Write("checked", cs);

    cfg.DeleteGroup("/charlib_descriptions");
    cfg.SetPath("/charlib_descriptions");
    for (auto& [name, desc] : m_charDescriptions) {
        if (!desc.empty())
            cfg.Write(wxString::FromUTF8(name), wxString::FromUTF8(desc));
    }
}

void CharacterTab::PushState() {
    if (!m_ready) return;

    auto images = ToDataURLs(ScanPersonaImages());

    std::ostringstream j;
    j << "{\"cats\":{";
    bool fc = true;
    for (auto& [cat, chars] : m_charsByCategory) {
        if (!fc) j << ",";
        fc = false;
        j << Jq(cat) << ":[";
        bool fn = true;
        for (auto& ch : chars) {
            if (!fn) j << ",";
            fn = false;
            j << Jq(ch);
        }
        j << "]";
    }
    j << "},\"imgs\":{";
    bool fi = true;
    for (auto& [k, v] : images) {
        if (!fi) j << ",";
        fi = false;
        j << Jq(k) << ":" << Jq(v);
    }
    j << "},\"checked\":[";
    bool fch = true;
    for (auto& ch : m_checkedChars) {
        if (!fch) j << ",";
        fch = false;
        j << Jq(ch);
    }
    j << "],\"descs\":{";
    bool fd = true;
    for (auto& [name, desc] : m_charDescriptions) {
        if (!fd) j << ",";
        fd = false;
        j << Jq(name) << ":" << Jq(desc);
    }
    j << "}}";

    Run("setCharacters(" + j.str() + ")");
}

// ── Message dispatcher ────────────────────────────────────────────────────────

void CharacterTab::HandleMessage(const std::string& json) {
    std::string action = CtField(json, "action");
    if      (action == "ready")          { m_ready = true; if (m_pendingActivate) { m_pendingActivate = false; PushState(); } }
    else if (action == "toggle")         DoToggle(CtField(json,"name"), CtBool(json,"checked"));
    else if (action == "setDesc")        DoSetDesc(CtField(json,"name"), CtField(json,"description"));
    else if (action == "addCategory")    DoAddCategory(CtField(json,"name"));
    else if (action == "deleteCategory") DoDeleteCategory(CtField(json,"name"));
    else if (action == "addCharacter")   DoAddCharacter(CtField(json,"category"), CtField(json,"name"));
    else if (action == "deleteCharacter")DoDeleteCharacter(CtField(json,"category"), CtField(json,"name"));
    else if (action == "uploadImage")    DoUploadImage(CtField(json,"name"));
    else if (action == "renameCharacter")DoRenameCharacter(CtField(json,"oldName"), CtField(json,"newName"));
    else if (action == "openChat")        DoOpenChat(CtField(json,"name"));
    else if (action == "invitePersona")  DoInvitePersona(CtField(json,"name"));
    else if (action == "sendMessage")    DoChatSend(CtField(json,"text"), CtBool(json,"useArticle"), CtField(json,"directTo"));
    else if (action == "clearChat")      DoClearChat();
    else if (action == "closeChat")      DoCloseChat();
}

// ── Action handlers ───────────────────────────────────────────────────────────

void CharacterTab::DoToggle(const std::string& name, bool checked) {
    if (checked) m_checkedChars.insert(name);
    else         m_checkedChars.erase(name);
    SaveState();
}

void CharacterTab::DoSetDesc(const std::string& name, const std::string& desc) {
    if (name.empty()) return;
    if (desc.empty()) m_charDescriptions.erase(name);
    else              m_charDescriptions[name] = desc;
    SaveState();
}

void CharacterTab::DoAddCategory(const std::string& name) {
    if (name.empty() || m_charsByCategory.count(name)) return;
    m_charsByCategory[name];
    SaveState();
    PushState();
}

void CharacterTab::DoDeleteCategory(const std::string& name) {
    if (name.empty()) return;
    auto it = m_charsByCategory.find(name);
    if (it == m_charsByCategory.end()) return;
    for (auto& ch : it->second) {
        m_checkedChars.erase(ch);
        m_charDescriptions.erase(ch);
    }
    m_charsByCategory.erase(it);
    SaveState();
    PushState();
}

void CharacterTab::DoAddCharacter(const std::string& cat, const std::string& name) {
    if (cat.empty() || name.empty()) return;
    auto& vec = m_charsByCategory[cat];
    if (std::find(vec.begin(), vec.end(), name) != vec.end()) return;
    vec.push_back(name);
    m_checkedChars.insert(name);
    SaveState();
    PushState();
}

void CharacterTab::DoDeleteCharacter(const std::string& cat, const std::string& name) {
    auto it = m_charsByCategory.find(cat);
    if (it != m_charsByCategory.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), name), vec.end());
    }
    m_checkedChars.erase(name);
    m_charDescriptions.erase(name);
    SaveState();
    PushState();
}

void CharacterTab::DoRenameCharacter(const std::string& oldName, const std::string& newName) {
    if (oldName.empty() || newName.empty() || oldName == newName) return;

    // Update category map
    for (auto& kv : m_charsByCategory) {
        auto& vec = kv.second;
        auto it = std::find(vec.begin(), vec.end(), oldName);
        if (it != vec.end()) {
            *it = newName;
            break;
        }
    }
    // Update checked and description maps
    if (m_checkedChars.erase(oldName))
        m_checkedChars.insert(newName);
    auto dit = m_charDescriptions.find(oldName);
    if (dit != m_charDescriptions.end()) {
        m_charDescriptions[newName] = std::move(dit->second);
        m_charDescriptions.erase(dit);
    }

    RenamePersonaImage(oldName, newName);
    RenamePersonaChat(oldName, newName);
    // Update any active references to the renamed persona.
    for (auto& p : m_activePersonas)
        if (p == oldName) { p = newName; break; }
    for (auto& turn : m_chatHistory)
        for (auto& r : turn.responses)
            if (r.first == oldName) r.first = newName;
    SaveState();
    PushState();

    if (m_onRename) m_onRename(oldName, newName);
}

void CharacterTab::DoUploadImage(const std::string& name) {
    if (name.empty()) return;
    wxFileDialog dlg(this,
        "Choose image for \"" + wxString::FromUTF8(name) + "\"",
        "", "",
        "Image files (*.jpg;*.jpeg;*.png;*.webp;*.gif)|*.jpg;*.jpeg;*.png;*.webp;*.gif",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    std::string dest = AddPersonaImage(name, dlg.GetPath().ToStdString());
    if (dest.empty()) {
        wxMessageBox("Could not save image.", "StoryTeller", wxOK | wxICON_ERROR, this);
        return;
    }
    std::string key = NormalizePersonaName(name);
    std::string thumb = EnsureThumb(dest);
    auto converted = ToDataURLs({{key, "file://" + thumb}});
    if (converted.empty()) return;
    const std::string& url = converted.begin()->second;
    m_webView->RunScript(
        "window._u=" + wxString::FromUTF8("\"" + url + "\";") +
        "updatePersonaImage(\"" + wxString::FromUTF8(key) + "\",window._u);" +
        "delete window._u;");
}

// ── Persona conversation ──────────────────────────────────────────────────────

void CharacterTab::SetCurrentDocument(std::string markdown) {
    m_currentDocMarkdown = std::move(markdown);
}

static std::string StripLeadingLabel(const std::string& name, std::string answer) {
    if (answer.rfind(name + ":", 0) == 0)
        answer = answer.substr(name.size() + 1);
    while (!answer.empty() && (answer.front() == ' ' || answer.front() == '\n'))
        answer.erase(answer.begin());
    return answer;
}

void CharacterTab::RenderPersonaChat(const std::string& pendingQ) {
    if (!m_ready || m_activePersonas.empty()) return;

    bool multi = m_activePersonas.size() > 1;
    std::ostringstream html;
    for (auto& t : m_chatHistory) {
        html << "<div class='chat-turn'>"
             << "<div class='chat-q'>" << RenderMarkdown(t.userMessage) << "</div>";
        for (auto& r : t.responses) {
            html << "<div class='chat-a'>";
            if (multi)
                html << "<span class='chat-speaker'>" << EscapeHTML(r.first) << "</span>";
            html << RenderMarkdown(r.second) << "</div>";
        }
        html << "</div>";
    }
    if (!pendingQ.empty()) {
        html << "<div class='chat-turn'>"
             << "<div class='chat-q'>" << RenderMarkdown(pendingQ) << "</div>"
             << "<div class='chat-a chat-waiting'>\xe2\x80\xa6</div>"
             << "</div>";
    }

    Run("updateChatHistory(" + Jq(html.str()) + "," + (m_chatBusy ? "true" : "false") + ")");
}

void CharacterTab::PersistChat() {
    // Only persist single-persona conversations (group chats are session-only).
    if (m_activePersonas.size() != 1) return;
    const std::string& name = m_activePersonas[0];
    auto single = FromMultiChatHistory(name, m_chatHistory);
    SavePersonaConversation(name, single);
}

void CharacterTab::DoOpenChat(const std::string& name) {
    if (name.empty()) return;

    // If a chat is already open with this persona as host, do nothing.
    if (!m_activePersonas.empty() && m_activePersonas[0] == name) {
        Run("startChat(" + Jq(name) + ")");
        RenderPersonaChat();
        return;
    }

    m_activePersonas = {name};
    // Load persisted single-persona history and convert to MultiChatTurn format.
    auto saved = LoadPersonaConversation(name);
    m_chatHistory = ToMultiChatHistory(name, saved);

    Run("startChat(" + Jq(name) + ")");
    Run("setActivePersonas([" + Jq(name) + "])");
    RenderPersonaChat();
}

void CharacterTab::DoInvitePersona(const std::string& name) {
    if (name.empty() || m_activePersonas.empty() || m_chatBusy) return;
    // Don't invite duplicates.
    for (const auto& p : m_activePersonas)
        if (p == name) return;

    m_activePersonas.push_back(name);

    // The invited persona immediately reacts to the last user message (if any).
    std::string lastMsg;
    if (!m_chatHistory.empty())
        lastMsg = m_chatHistory.back().userMessage;

    // Notify JS to show the updated active-persona list in the header.
    std::ostringstream activeJs;
    activeJs << "[";
    for (size_t i = 0; i < m_activePersonas.size(); ++i) {
        if (i) activeJs << ",";
        activeJs << Jq(m_activePersonas[i]);
    }
    activeJs << "]";
    Run("setActivePersonas(" + activeJs.str() + ")");

    if (lastMsg.empty()) {
        // Nothing to react to yet; just show them as present.
        RenderPersonaChat();
        return;
    }

    m_chatBusy = true;
    RenderPersonaChat();

    std::string personaName = name;
    std::string personaDesc = m_charDescriptions.count(name) ? m_charDescriptions.at(name) : "";
    std::string docMarkdown = m_currentDocMarkdown;
    bool useArticle         = false; // article context state unknown; default off for invite
    auto history            = m_chatHistory;

    AppState st = LoadAppState();
    LLMConfig cfg;
    cfg.backend     = BackendFromLabel(st.backend);
    cfg.apiKey      = st.apiKey;
    cfg.ollamaModel = st.ollamaModel;

    std::string prompt = BuildPersonaPromptMulti(
        personaName, personaDesc, docMarkdown, history, lastMsg, useArticle);

    std::thread([this, prompt, cfg, personaName, lastMsg]() mutable {
        LLMResult res = InvokeLLM(prompt, cfg);
        wxTheApp->CallAfter([this, res, personaName]() {
            m_chatBusy = false;
            // Check invited persona is still active.
            bool stillActive = false;
            for (const auto& p : m_activePersonas) if (p == personaName) stillActive = true;
            if (!stillActive) return;

            std::string answer = res.ok ? res.text : ("Error: " + res.error);
            answer = StripLeadingLabel(personaName, std::move(answer));

            // Append this response to the last turn.
            if (!m_chatHistory.empty())
                m_chatHistory.back().responses.push_back({personaName, answer});

            PersistChat();
            RenderPersonaChat();
        });
    }).detach();
}

void CharacterTab::DoChatSend(const std::string& text, bool useArticle, const std::string& directTo) {
    if (text.empty() || m_activePersonas.empty() || m_chatBusy) return;

    Logger::get().log("CharacterTab::DoChatSend personas=" + std::to_string(m_activePersonas.size())
                    + " useArticle=" + std::to_string(useArticle)
                    + " directTo=" + directTo);

    m_chatBusy = true;
    RenderPersonaChat(text);

    std::string docMarkdown = useArticle ? m_currentDocMarkdown : "";
    auto history            = m_chatHistory;

    // If directed to a specific persona, only that persona replies.
    std::vector<std::string> personas;
    if (!directTo.empty()) {
        for (const auto& p : m_activePersonas)
            if (p == directTo) { personas.push_back(p); break; }
    }
    if (personas.empty()) personas = m_activePersonas;

    AppState st = LoadAppState();
    LLMConfig cfg;
    cfg.backend     = BackendFromLabel(st.backend);
    cfg.apiKey      = st.apiKey;
    cfg.ollamaModel = st.ollamaModel;

    std::vector<std::string> prompts;
    for (const auto& pName : personas) {
        std::string desc = m_charDescriptions.count(pName) ? m_charDescriptions.at(pName) : "";
        prompts.push_back(BuildPersonaPromptMulti(pName, desc, docMarkdown, history, text, useArticle));
    }

    std::thread([this, prompts, cfg, personas, text, useArticle]() mutable {
        std::vector<std::string> answers;
        for (size_t i = 0; i < personas.size(); ++i) {
            LLMResult res = InvokeLLM(prompts[i], cfg);
            std::string answer = res.ok ? res.text : ("Error: " + res.error);
            answer = StripLeadingLabel(personas[i], std::move(answer));
            answers.push_back(std::move(answer));
        }

        wxTheApp->CallAfter([this, personas, text, answers]() {
            m_chatBusy = false;
            if (m_activePersonas.empty()) return;

            MultiChatTurn turn;
            turn.userMessage = text;
            for (size_t i = 0; i < personas.size(); ++i)
                turn.responses.push_back({personas[i], answers[i]});
            m_chatHistory.push_back(std::move(turn));

            PersistChat();
            RenderPersonaChat();
        });
    }).detach();
}

void CharacterTab::DoClearChat() {
    if (m_activePersonas.empty()) return;
    m_chatHistory.clear();
    PersistChat();
    RenderPersonaChat();
}

void CharacterTab::DoCloseChat() {
    m_activePersonas.clear();
    m_chatHistory.clear();
    // No JS callback needed — the user closed the panel from the UI.
}
