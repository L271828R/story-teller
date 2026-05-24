#include "persona_panel.h"
#include "persona_panel_html.h"
#include "persona.h"
#include "logger.h"
#include <wx/webview.h>
#include <string>

PersonaPanel::PersonaPanel(wxWindow* parent, bool darkMode,
                           const std::map<std::string, std::vector<std::string>>& categories,
                           const std::map<std::string, std::string>& images,
                           std::function<void()> onImageAdded)
    : wxFrame(parent, wxID_ANY, "Manage Personas",
              wxDefaultPosition, wxSize(780, 560),
              wxDEFAULT_FRAME_STYLE)
{
    m_html          = BuildPersonaPanelHTML(categories, ToDataURLs(images), darkMode);
    m_onImageAdded  = std::move(onImageAdded);

    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("persona");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) { OnScriptMessage(evt); });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    m_webView->SetPage(wxString::FromUTF8(m_html), "");
}

static std::string jsonField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
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
        } else {
            val += c;
        }
    }
    return val;
}

void PersonaPanel::OnScriptMessage(wxWebViewEvent& evt) {
    std::string payload = evt.GetString().ToStdString();
    std::string action  = jsonField(payload, "action");
    if (action != "upload") return;

    std::string name = jsonField(payload, "name");
    if (name.empty()) return;

    wxFileDialog dlg(this,
                     "Choose image for \"" + wxString::FromUTF8(name) + "\"",
                     "", "",
                     "Image files (*.jpg;*.jpeg;*.png;*.webp;*.gif)"
                     "|*.jpg;*.jpeg;*.png;*.webp;*.gif|All files (*)|*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    std::string dest = AddPersonaImage(name, dlg.GetPath().ToStdString());
    if (dest.empty()) {
        wxMessageBox("Could not save image.", "StoryTeller", wxOK | wxICON_ERROR, this);
        return;
    }

    // Convert saved file to data: URL and update the panel in place.
    std::map<std::string, std::string> tmp = {{"file://" + dest, "file://" + dest}};
    // Use ToDataURLs via a single-entry map keyed by normalized name.
    std::string key     = NormalizePersonaName(name);
    std::string fileUrl = "file://" + dest;
    auto converted = ToDataURLs({{key, fileUrl}});
    if (converted.empty()) {
        wxMessageBox("Could not read saved image.", "StoryTeller", wxOK | wxICON_ERROR, this);
        return;
    }
    const std::string& dataUrl = converted.begin()->second;

    if (m_onImageAdded) m_onImageAdded();

    m_webView->RunScript(
        "window._u=" + wxString::FromUTF8("\"" + dataUrl + "\";") +
        "updatePersonaImage(\"" + wxString::FromUTF8(key) + "\",window._u);"
        "delete window._u;");
}
