#include "conversation.h"
#include "markdown.h"
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------------------
std::vector<ConversationTurn> ParseConversation(const std::string& body) {
    std::vector<ConversationTurn> turns;
    ConversationTurn cur;
    bool inAnswer = false;

    std::istringstream ss(body);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind("Q: ", 0) == 0) {
            if (!cur.question.empty()) turns.push_back(cur);
            cur = {line.substr(3), ""};
            inAnswer = false;
        } else if (line.rfind("A: ", 0) == 0) {
            cur.answer = line.substr(3);
            inAnswer = true;
        } else if (inAnswer && !line.empty()) {
            cur.answer += "\n" + line;
        }
    }
    if (!cur.question.empty()) turns.push_back(cur);
    return turns;
}

// ---------------------------------------------------------------------------
std::string SerializeConversationBody(const std::vector<ConversationTurn>& turns) {
    std::ostringstream out;
    for (const auto& t : turns) {
        out << "Q: " << t.question << "\n";
        out << "A: " << t.answer  << "\n\n";
    }
    return out.str();
}

// ---------------------------------------------------------------------------
// Scan the file to find the :::conversation block that belongs to chId.
// We track which <!-- ch:N --> precedes each :::conversation block.
std::vector<ConversationTurn> LoadConversation(const std::string& filePath, int chId) {
    std::ifstream f(filePath);
    if (!f) return {};
    std::string content((std::istreambuf_iterator<char>(f)), {});

    std::istringstream ss(content);
    std::string line;
    int curChId = -1;
    bool inConv = false;
    std::string convBody;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind("<!-- ch:", 0) == 0) {
            size_t end = line.find(" -->", 8);
            if (end != std::string::npos) {
                try { curChId = std::stoi(line.substr(8, end - 8)); } catch (...) {}
            }
            if (inConv) { inConv = false; convBody.clear(); } // reset on new chapter
        }

        if (!inConv && curChId == chId && line.rfind(":::conversation[", 0) == 0) {
            inConv = true;
            convBody.clear();
        } else if (inConv) {
            if (line == ":::") return ParseConversation(convBody);
            convBody += line + "\n";
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
bool AppendTurn(const std::string& filePath, int chId,
                const std::string& chTitle, const ConversationTurn& turn) {
    std::ifstream fin(filePath);
    if (!fin) return false;
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    fin.close();

    std::string chMarker = "<!-- ch:" + std::to_string(chId) + " -->";
    size_t chPos = content.find(chMarker);
    if (chPos == std::string::npos) return false;

    size_t nextChPos = content.find("\n<!-- ch:", chPos + chMarker.size());
    size_t chapterEnd = (nextChPos == std::string::npos) ? content.size() : nextChPos;

    std::string newEntry = "Q: " + turn.question + "\nA: " + turn.answer + "\n\n";

    size_t convPos = content.find(":::conversation[", chPos);
    if (convPos != std::string::npos && convPos < chapterEnd) {
        // Append inside existing block before its closing :::
        size_t closePos = content.find("\n:::", convPos);
        if (closePos == std::string::npos || closePos >= chapterEnd) return false;
        content.insert(closePos + 1, newEntry);
    } else {
        // Create new block — insert just before the next chapter section
        std::string block = "\n:::conversation[" + chTitle + "]\n" + newEntry + ":::\n";
        if (nextChPos != std::string::npos) {
            content.insert(nextChPos, block);
        } else {
            if (!content.empty() && content.back() != '\n') content += '\n';
            content += block + '\n';
        }
    }

    std::ofstream fout(filePath);
    if (!fout) return false;
    fout << content;
    return fout.good();
}

// ---------------------------------------------------------------------------
bool DeleteTurn(const std::string& filePath, int chId, int index) {
    std::ifstream fin(filePath);
    if (!fin) return false;
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    fin.close();

    std::string chMarker = "<!-- ch:" + std::to_string(chId) + " -->";
    size_t chPos = content.find(chMarker);
    if (chPos == std::string::npos) return false;

    size_t nextChPos = content.find("\n<!-- ch:", chPos + chMarker.size());
    size_t chapterEnd = (nextChPos == std::string::npos) ? content.size() : nextChPos;

    size_t convPos = content.find(":::conversation[", chPos);
    if (convPos == std::string::npos || convPos >= chapterEnd) return false;

    size_t headerEnd = content.find('\n', convPos);
    if (headerEnd == std::string::npos) return false;
    size_t bodyStart = headerEnd + 1;

    size_t closeNl = content.find("\n:::", convPos);
    if (closeNl == std::string::npos || closeNl >= chapterEnd) return false;

    // body includes up to and including the \n at closeNl
    std::string body = content.substr(bodyStart, closeNl + 1 - bodyStart);
    auto turns = ParseConversation(body);
    if (index < 0 || index >= (int)turns.size()) return false;
    turns.erase(turns.begin() + index);

    if (turns.empty()) {
        // Remove the whole :::conversation block (including the \n before it)
        size_t blockStart = (convPos > 0 && content[convPos - 1] == '\n') ? convPos - 1 : convPos;
        // closeNl points to \n, then ::: (3 chars), then \n
        size_t blockEnd = closeNl + 5;
        if (blockEnd > content.size()) blockEnd = content.size();
        content = content.substr(0, blockStart) + content.substr(blockEnd);
    } else {
        // Replace only the body, keeping the header and closing ::: intact
        std::string newBody = SerializeConversationBody(turns);
        // content.substr(closeNl + 1) starts at the first : of the closing :::
        content = content.substr(0, bodyStart) + newBody + content.substr(closeNl + 1);
    }

    std::ofstream fout(filePath);
    if (!fout) return false;
    fout << content;
    return fout.good();
}

// ---------------------------------------------------------------------------
std::string BuildQAPrompt(const std::string& docMarkdown,
                          const std::string& chTitle,
                          const std::vector<ConversationTurn>& history,
                          const std::string& question) {
    std::ostringstream out;
    out << "## Document\n\n"
        << "The following is the full document the reader is studying:\n\n"
        << "```\n" << docMarkdown << "\n```\n\n"
        << "## Your role\n\n"
        << "You are a knowledgeable and engaging conversation partner. ";
    if (chTitle == "Document") {
        out << "The reader is reviewing the full document.\n";
    } else {
        out << "The reader is currently reading the chapter: **" << chTitle << "**.\n";
    }
    out << "Answer their question naturally and helpfully. Use the document above as "
           "context when it is relevant, but do NOT limit yourself to it — draw freely "
           "on your broader knowledge whenever the question calls for it or when the "
           "document does not cover the topic. If the document is silent on something "
           "interesting, say so briefly and then share what you know. Keep answers clear "
           "and conversational — a few sentences to a short paragraph.\n\n";

    if (!history.empty()) {
        out << "## Conversation so far\n\n";
        for (const auto& t : history) {
            out << "Q: " << t.question << "\nA: " << t.answer << "\n\n";
        }
    }

    out << "## New question\n\nQ: " << question << "\nA:";
    return out.str();
}

// ---------------------------------------------------------------------------
std::string BuildChatHTML(const std::string& chTitle,
                          const std::vector<ConversationTurn>& turns,
                          const std::string& pendingQ,
                          bool darkMode) {
    const std::string bg      = darkMode ? "#0d1117" : "#ffffff";
    const std::string text    = darkMode ? "#e6edf3" : "#1a1a1a";
    const std::string qBg     = darkMode ? "#1c2a3a" : "#e3f2fd";
    const std::string aBg     = darkMode ? "#1c2a1c" : "#f1f8e9";
    const std::string mutedC  = darkMode ? "#8b949e" : "#666666";
    const std::string borderC = darkMode ? "#30363d" : "#d0d7de";
    const std::string inputBg = darkMode ? "#161b22"  : "#f6f8fa";
    const std::string linkC   = darkMode ? "#58a6ff"  : "#0969da";

    bool busy = !pendingQ.empty();
    const std::string dis = busy ? " disabled" : "";

    std::string body;
    int idx = 0;
    for (const auto& t : turns) {
        std::string i = std::to_string(idx++);
        body += "<div class='turn'>"
                "<button class='del-btn' onclick='delTurn(" + i + ")' "
                "title='Delete this exchange'>\xc3\x97</button>"
                "<div class='q'>" + EscapeHTML(t.question) + "</div>"
                "<div class='a'>" + RenderMarkdown(t.answer) + "</div>"
                "</div>\n";
    }
    if (!pendingQ.empty()) {
        body += "<div class='turn'>"
                "<div class='q'>" + EscapeHTML(pendingQ) + "</div>"
                "<div class='a thinking'>&#x22EF;</div>"
                "</div>\n";
    }
    if (body.empty()) {
        body = "<p class='empty'>Ask anything about <em>"
               + EscapeHTML(chTitle) + "</em>.</p>";
    }

    body += "<div id='chat-input'>"
            "<textarea id='chat-ans' placeholder='Ask something\xe2\x80\xa6'" + dis + "></textarea>"
            "<button id='chat-send' onclick='chatSend()'" + dis + ">Send</button>"
            "</div>";

    return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
           "<style>"
           "* { box-sizing: border-box; margin: 0; padding: 0 }"
           "body { font-family: -apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
           "  font-size: 14px; line-height: 1.6; background: " + bg + "; color: " + text + ";"
           "  padding: 12px 16px 90px; }"
           ".turn { position: relative; margin-bottom: 20px; }"
           ".del-btn { float: right; background: transparent;"
           "  border: 1px solid transparent; cursor: pointer;"
           "  color: " + mutedC + "; font-size: 13px; padding: 1px 5px;"
           "  border-radius: 3px; line-height: 1; opacity: 0; transition: opacity .15s; }"
           ".turn:hover .del-btn { opacity: 1; }"
           ".del-btn:hover { border-color: " + borderC + "; }"
           ".q { background: " + qBg + "; border-radius: 8px 8px 8px 2px;"
           "  padding: 10px 14px; margin-bottom: 6px; font-weight: 500; }"
           ".a { background: " + aBg + "; border-radius: 2px 8px 8px 8px;"
           "  padding: 10px 14px; }"
           ".a p { margin: 0 0 .6em 0; }"
           ".a p:last-child { margin-bottom: 0; }"
           ".a ul,.a ol { padding-left: 1.4em; margin: .4em 0; }"
           ".a li { margin-bottom: .2em; }"
           ".thinking { color: " + mutedC + "; font-style: italic; }"
           ".empty { color: " + mutedC + "; font-style: italic; padding: 8px 0; }"
           "code { background: rgba(128,128,128,.15); padding: .15em .35em; border-radius: 3px; }"
           "#chat-input { position:fixed; bottom:0; left:0; right:0;"
           "  background:" + bg + "; border-top:1px solid " + borderC + ";"
           "  padding:6px 8px; z-index:50; display:flex; gap:6px; align-items:flex-end; }"
           "#chat-ans { flex:1; min-height:50px; max-height:120px; resize:vertical;"
           "  padding:6px 8px; border:1px solid " + borderC + "; border-radius:4px;"
           "  background:" + inputBg + "; color:" + text + ";"
           "  font-family:inherit; font-size:.95em; line-height:1.4; }"
           "#chat-ans:focus { outline:2px solid " + linkC + "; outline-offset:-1px; }"
           "#chat-send { padding:6px 16px; border-radius:4px;"
           "  border:1px solid " + linkC + "; background:" + linkC + ";"
           "  color:#fff; cursor:pointer; font-weight:600; font-size:.9em;"
           "  white-space:nowrap; align-self:flex-end; }"
           "#chat-send:disabled { opacity:.38; cursor:default;"
           "  background:" + inputBg + "; color:" + mutedC + "; border-color:" + borderC + "; }"
           "</style>"
           "<script>"
           "function delTurn(i){"
           "if(window.webkit&&window.webkit.messageHandlers&&"
           "window.webkit.messageHandlers.deleteTurn)"
           "window.webkit.messageHandlers.deleteTurn.postMessage(''+i);}"
           "function chatSend(){"
           "var t=document.getElementById('chat-ans');"
           "var txt=t?t.value.trim():'';"
           "if(!txt)return;"
           "if(window.webkit&&window.webkit.messageHandlers&&"
           "window.webkit.messageHandlers.chatSend)"
           "window.webkit.messageHandlers.chatSend.postMessage(txt);"
           "if(t)t.value='';}"
           "</script>"
           "</head><body>" + body +
           "<script>(function(){"
           "var a=document.getElementById('chat-ans');"
           "if(!a)return;"
           "a.addEventListener('keydown',function(e){"
           "if((e.metaKey||e.ctrlKey)&&e.key==='Enter'){e.preventDefault();chatSend();}"
           "});"
           "if(!a.disabled){a.focus();}"
           "window.scrollTo(0,document.body.scrollHeight);"
           "})();</script>"
           "</body></html>";
}
