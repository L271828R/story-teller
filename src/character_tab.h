#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/webview.h>

class CharacterTab : public wxPanel {
public:
    explicit CharacterTab(wxWindow* parent, bool darkMode = false);

    void SetDarkMode(bool dark);
    void Activate();   // call when tab is first shown / switched to

    // Called by CreatePanel at generation time.
    std::set<std::string>              GetCheckedChars()     const { return m_checkedChars; }
    std::map<std::string, std::string> GetCharDescriptions() const { return m_charDescriptions; }

private:
    wxWebView* m_webView         = nullptr;
    bool       m_darkMode        = false;
    bool       m_ready           = false;
    bool       m_pendingActivate = false;

    std::map<std::string, std::vector<std::string>> m_charsByCategory;
    std::set<std::string>                            m_checkedChars;
    std::map<std::string, std::string>               m_charDescriptions;

    void Run(const std::string& js);
    void HandleMessage(const std::string& json);
    void LoadState();
    void SaveState() const;
    void PushState();

    void DoToggle(const std::string& name, bool checked);
    void DoSetDesc(const std::string& name, const std::string& desc);
    void DoAddCategory(const std::string& name);
    void DoDeleteCategory(const std::string& name);
    void DoAddCharacter(const std::string& cat, const std::string& name);
    void DoDeleteCharacter(const std::string& cat, const std::string& name);
    void DoUploadImage(const std::string& name);
};
