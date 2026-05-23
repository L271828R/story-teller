#pragma once
#include <wx/wx.h>
#include <wx/webview.h>
#include <functional>
#include <string>
#include <vector>
#include "conversation.h"
#include "llm.h"

// Embeddable chat panel — sits in a splitter alongside the main webview.
// Call Open() each time the user clicks a chapter's chat button.
class ChatPanel : public wxPanel {
public:
    // onClose    — called when the user clicks ×; caller should unsplit
    // onConvSaved — called after each turn is persisted; caller should re-render doc
    ChatPanel(wxWindow*             parent,
              bool                  darkMode,
              std::function<void()> onClose,
              std::function<void()> onConvSaved);

    void Open(const std::string& filePath, int chId,
              const std::string& chTitle, const LLMConfig& llmCfg);

    void SetDarkMode(bool dark);

private:
    std::string   m_filePath;
    int           m_chId    = -1;
    std::string   m_chTitle;
    LLMConfig     m_llmCfg;
    bool          m_darkMode;
    bool          m_busy    = false;
    bool          m_isOpen  = false;

    std::function<void()>  m_onClose;
    std::function<void()>  m_onSaved;

    std::vector<ConversationTurn> m_turns;

    wxStaticText* m_titleLabel = nullptr;
    wxWebView*    m_webView    = nullptr;

    void ApplyTheme();
    void LoadHistory();
    void Render(const std::string& pendingQuestion = "");
    void DoSend(const std::string& question);
    void OnScriptMessage(wxWebViewEvent&);

    wxDECLARE_EVENT_TABLE();
};
