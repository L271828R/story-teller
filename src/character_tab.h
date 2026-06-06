#pragma once
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <wx/wx.h>
#include <wx/webview.h>
#include "conversation.h"

class CharacterTab : public wxPanel {
public:
    using RenameCallback = std::function<void(const std::string& oldName,
                                              const std::string& newName)>;

    explicit CharacterTab(wxWindow* parent, bool darkMode = false);

    void SetDarkMode(bool dark);
    void Activate();
    void SetOnRenameCharacter(RenameCallback cb) { m_onRename = std::move(cb); }

    void SetCurrentDocument(std::string markdown);
    // Scope persona and group chat history to a project directory.
    // projectPath is the parent folder of the open document.
    void SetChatContext(const std::string& projectPath);

    std::set<std::string>              GetCheckedChars()     const { return m_checkedChars; }
    std::map<std::string, std::string> GetCharDescriptions() const { return m_charDescriptions; }

private:
    wxWebView*     m_webView         = nullptr;
    bool           m_darkMode        = false;
    bool           m_ready           = false;
    bool           m_pendingActivate = false;
    RenameCallback m_onRename;

    std::map<std::string, std::vector<std::string>> m_charsByCategory;
    std::set<std::string>                            m_checkedChars;
    std::map<std::string, std::string>               m_charDescriptions;

    // Persona conversation state
    std::string m_currentDocMarkdown;
    std::string m_chatContextDir;   // project-scoped chats dir; empty = global
    std::vector<std::string> m_activePersonas;   // [0] = host, rest = invited
    std::vector<MultiChatTurn> m_chatHistory;
    bool m_chatBusy = false;
    std::map<std::string, int> m_mermaidRepairs; // source → attempt count

    void Run(const std::string& js);
    void HandleMessage(const std::string& json);
    void LoadState();
    void SaveState() const;
    void PushState();
    void PushGroups();
    void RenderPersonaChat(const std::string& pendingQ = "");
    void PersistChat();

    void DoToggle(const std::string& name, bool checked);
    void DoSetDesc(const std::string& name, const std::string& desc);
    void DoAddCategory(const std::string& name);
    void DoDeleteCategory(const std::string& name);
    void DoAddCharacter(const std::string& cat, const std::string& name);
    void DoDeleteCharacter(const std::string& cat, const std::string& name);
    void DoUploadImage(const std::string& name);
    void DoRenameCharacter(const std::string& oldName, const std::string& newName);
    void DoOpenChat(const std::string& name);
    void DoOpenGroupChat(const std::string& key);
    void DoRepairMermaid(const std::string& source);
    void DoInvitePersona(const std::string& name);
    void DoChatSend(const std::string& text, bool useArticle, const std::string& directTo = "");
    void DoClearChat();
    void DoCloseChat();
};
