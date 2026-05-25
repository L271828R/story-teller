#include "character_tab.h"
#include "character_tab_html.h"
#include "persona.h"
#include "thumbnail.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
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

    wxConfig dcfg("StoryTeller");
    dcfg.DeleteGroup("/charlib_descriptions");
    dcfg.SetPath("/charlib_descriptions");
    for (auto& [name, desc] : m_charDescriptions) {
        if (!desc.empty())
            dcfg.Write(wxString::FromUTF8(name), wxString::FromUTF8(desc));
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
