#include "chat_panel.h"
#include "conversation.h"
#include "llm.h"
#include "config.h"
#include "meta.h"
#include <chrono>
#include <filesystem>
#include <thread>
#include <fstream>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/webview.h>

namespace fs = std::filesystem;

wxBEGIN_EVENT_TABLE(ChatPanel, wxPanel)
wxEND_EVENT_TABLE()

// ---------------------------------------------------------------------------
ChatPanel::ChatPanel(wxWindow*             parent,
                     bool                  darkMode,
                     std::function<void()> onClose,
                     std::function<void()> onConvSaved)
    : wxPanel(parent, wxID_ANY)
    , m_darkMode(darkMode)
    , m_onClose(std::move(onClose))
    , m_onSaved(std::move(onConvSaved))
{
    // Header: title + close button
    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
    m_titleLabel = new wxStaticText(this, wxID_ANY, "Chat");
    auto* closeBtn = new wxButton(this, wxID_ANY, "\xc3\x97",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    headerRow->Add(m_titleLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    headerRow->Add(closeBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    closeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_onClose) m_onClose();
    });

    auto* line = new wxStaticLine(this, wxID_ANY);

    // Chat history webview
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("deleteTurn");
    m_webView->AddScriptMessageHandler("chatSend");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
                    &ChatPanel::OnScriptMessage, this);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(headerRow, 0, wxEXPAND | wxTOP | wxBOTTOM, 4);
    outer->Add(line, 0, wxEXPAND);
    outer->Add(m_webView, 1, wxEXPAND);
    SetSizer(outer);

    ApplyTheme();
}

// ---------------------------------------------------------------------------
void ChatPanel::ApplyTheme() {
    SetBackgroundColour(m_darkMode ? wxColour(13, 17, 23) : wxNullColour);
    Refresh();
}

// ---------------------------------------------------------------------------
void ChatPanel::Open(const std::string& filePath, int chId,
                     const std::string& chTitle, const LLMConfig& llmCfg) {
    m_filePath = filePath;
    m_chId     = chId;
    m_chTitle  = chTitle;
    m_llmCfg   = llmCfg;
    m_isOpen   = true;
    m_titleLabel->SetLabel(wxString::FromUTF8("Chat \xe2\x80\x94 " + chTitle));
    LoadHistory();
    Render();
}

// ---------------------------------------------------------------------------
void ChatPanel::SetDarkMode(bool dark) {
    m_darkMode = dark;
    ApplyTheme();
    if (m_chId >= 0) Render();
}

// ---------------------------------------------------------------------------
void ChatPanel::LoadHistory() {
    m_turns = LoadConversation(m_filePath, m_chId);
}

// ---------------------------------------------------------------------------
void ChatPanel::Render(const std::string& pendingQ) {
    std::string html = BuildChatHTML(m_chTitle, m_turns, pendingQ, m_darkMode);
    m_webView->SetPage(wxString::FromUTF8(html), wxEmptyString);
}

// ---------------------------------------------------------------------------
void ChatPanel::OnScriptMessage(wxWebViewEvent& evt) {
    const wxString handler = evt.GetMessageHandler();
    if (handler == "chatSend") {
        std::string question = evt.GetString().Trim().ToStdString();
        if (!question.empty()) DoSend(question);
        return;
    }
    // handler == "deleteTurn"
    if (m_busy) return;
    long idx = -1;
    if (!evt.GetString().ToLong(&idx) || idx < 0 || idx >= (long)m_turns.size()) return;
    m_turns.erase(m_turns.begin() + (size_t)idx);
    DeleteTurn(m_filePath, m_chId, (int)idx);
    if (m_onSaved) m_onSaved();
    Render();
}

// ---------------------------------------------------------------------------
void ChatPanel::DoSend(const std::string& question) {
    if (m_busy || !m_isOpen) return;

    m_busy = true;
    Render(question);

    std::string filePath = m_filePath;
    int         chId     = m_chId;
    std::string chTitle  = m_chTitle;
    LLMConfig   cfg      = m_llmCfg;

    std::string docMarkdown;
    {
        std::ifstream f(filePath);
        if (f) docMarkdown.assign(std::istreambuf_iterator<char>(f), {});
    }

    std::vector<ConversationTurn> history = m_turns;
    std::string prompt = BuildQAPrompt(docMarkdown, chTitle, history, question);

    std::thread([this, prompt, cfg, filePath, chId, chTitle, question]() mutable {
        auto started = std::chrono::steady_clock::now();
        LLMResult res = InvokeLLM(prompt, cfg);
        int durationSeconds = (int)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - started).count();
        if (res.ok) {
            RecordLLMTiming(fs::path(filePath).parent_path().string(),
                            "chat", chTitle, durationSeconds,
                            BackendLabel(cfg.backend));
        }

        wxTheApp->CallAfter([this, res, filePath, chId, chTitle, question]() {
            m_busy = false;

            // Discard if the user switched to a different chapter while waiting.
            if (m_chId != chId) return;

            std::string answer = res.ok ? res.text : ("Error: " + res.error);
            if (answer.rfind("A: ", 0) == 0) answer = answer.substr(3);
            if (answer.rfind("A:", 0) == 0)  answer = answer.substr(2);
            while (!answer.empty() && (answer.front() == ' ' || answer.front() == '\n'))
                answer.erase(answer.begin());

            ConversationTurn turn{question, answer};
            m_turns.push_back(turn);
            bool saved = AppendTurn(filePath, chId, chTitle, turn);
            if (saved && m_onSaved) m_onSaved();
            Render();
        });
    }).detach();
}
