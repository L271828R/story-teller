#include "conversation.h"
#include "quiz.h"
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
// Locates the chapter marker and conversation block for chId in content.
// For chId=-1 (document chat), inserts <!-- ch:-1 --> at the end if absent.
// Returns false if the marker cannot be found and chId != -1.
struct ChapterSection {
    size_t chPos      = std::string::npos;
    size_t nextChPos  = std::string::npos;
    size_t chapterEnd = 0;
    size_t convPos    = std::string::npos;
};

static bool LocateChapterSection(std::string& content, int chId, ChapterSection& out) {
    const std::string chMarker = "<!-- ch:" + std::to_string(chId) + " -->";
    size_t chPos = content.find(chMarker);
    if (chPos == std::string::npos) {
        if (chId != -1) return false;
        if (!content.empty() && content.back() != '\n') content += '\n';
        chPos = content.size();
        content += chMarker + '\n';
    }
    size_t nextChPos  = content.find("\n<!-- ch:", chPos + chMarker.size());
    size_t chapterEnd = (nextChPos == std::string::npos) ? content.size() : nextChPos;
    out = {chPos, nextChPos, chapterEnd, content.find(":::conversation[", chPos)};
    return true;
}

// ---------------------------------------------------------------------------
bool AppendTurn(const std::string& filePath, int chId,
                const std::string& chTitle, const ConversationTurn& turn) {
    std::ifstream fin(filePath);
    if (!fin) return false;
    std::string content((std::istreambuf_iterator<char>(fin)), {});
    fin.close();

    ChapterSection sec;
    if (!LocateChapterSection(content, chId, sec)) return false;

    std::string newEntry = "Q: " + turn.question + "\nA: " + turn.answer + "\n\n";

    if (sec.convPos != std::string::npos && sec.convPos < sec.chapterEnd) {
        // Append inside existing block before its closing :::
        size_t closePos = content.find("\n:::", sec.convPos);
        if (closePos == std::string::npos || closePos >= sec.chapterEnd) return false;
        content.insert(closePos + 1, newEntry);
    } else {
        // Create new block — insert just before the next chapter section
        std::string block = "\n:::conversation[" + chTitle + "]\n" + newEntry + ":::\n";
        if (sec.nextChPos != std::string::npos) {
            content.insert(sec.nextChPos, block);
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

    ChapterSection sec;
    if (!LocateChapterSection(content, chId, sec)) return false;
    if (sec.convPos == std::string::npos || sec.convPos >= sec.chapterEnd) return false;

    size_t headerEnd = content.find('\n', sec.convPos);
    if (headerEnd == std::string::npos) return false;
    size_t bodyStart = headerEnd + 1;

    size_t closeNl = content.find("\n:::", sec.convPos);
    if (closeNl == std::string::npos || closeNl >= sec.chapterEnd) return false;

    // body includes up to and including the \n at closeNl
    std::string body = content.substr(bodyStart, closeNl + 1 - bodyStart);
    auto turns = ParseConversation(body);
    if (index < 0 || index >= (int)turns.size()) return false;
    turns.erase(turns.begin() + index);

    if (turns.empty()) {
        // Remove the whole :::conversation block (including the \n before it)
        size_t blockStart = (sec.convPos > 0 && content[sec.convPos - 1] == '\n')
                            ? sec.convPos - 1 : sec.convPos;
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
std::string BuildMermaidRepairPrompt(const std::string& source) {
    return
        "The following Mermaid diagram has a syntax error and cannot be rendered.\n"
        "Return ONLY the corrected Mermaid diagram syntax — no code fences, no explanation.\n\n"
        + source;
}

// ---------------------------------------------------------------------------
std::string BuildPersonaPrompt(const std::string& personaName,
                               const std::string& personaDesc,
                               const std::string& docMarkdown,
                               const std::vector<ConversationTurn>& history,
                               const std::string& message,
                               bool useArticleContext,
                               const std::string& mode) {
    std::ostringstream out;
    out << "You are " << personaName << ".";
    if (!personaDesc.empty()) out << " " << personaDesc;
    out << "\n\n";

    if (useArticleContext && !docMarkdown.empty()) {
        out << "The user wants to discuss the following article with you:\n\n"
            << "```\n" << StripTidbits(docMarkdown) << "\n```\n\n";
    }

    out << "Respond as " << personaName << " would — in-character, drawing on your "
        << "personality, knowledge, and perspective. Share your genuine opinion and "
        << "insights. If the user revisits the same question or asks repeated questions, "
        << "engage with enthusiasm — they want to hear your perspective, not a deflection. "
        << "Keep responses conversational and engaging, typically a few sentences to a short paragraph.\n\n";

    if (mode == "technical") {
        out << "You have three visualization tools. For step-by-step traces, prefer markdown tables.\n"
            << "1. Markdown tables — use | pipe | column | syntax; they render as formatted HTML tables. "
            << "One rule only: every column must have a header. "
            << "Structure, columns, metaphors, and emojis are entirely yours — make it feel like you.\n"
            << "2. ASCII diagrams — plain-text art, optionally with emojis.\n"
            << "3. Mermaid diagrams — only when the user explicitly asks for one.\n\n";
    } else if (mode == "story") {
        out << "Speak at length and in your own voice — this is your moment to tell a story. "
            << "Draw on personal anecdotes, vivid analogies from your era and experience, "
            << "and let the concept unfold as a narrative. Don't rush to the answer; "
            << "let the journey carry meaning. A few rich paragraphs are better than a quick reply.\n\n";
    } else if (mode == "debate") {
        out << "Take a clear, committed position and argue for it. "
            << "When others are in this conversation, address them directly by name — speak TO them, "
            << "not about them. Challenge their specific claims, push back on their assumptions, "
            << "and invite them to respond. Don't hedge. Stay in character, but be formidable.\n\n";
    } else if (mode == "simple") {
        out << "Explain as if to someone encountering this idea for the very first time. "
            << "No jargon. Use everyday objects, familiar situations, and plain language. "
            << "If a curious ten-year-old couldn't follow it, simplify further. "
            << "Analogies are your primary tool.\n\n";
    } else if (mode == "socratic") {
        out << "Don't give answers — ask questions. Respond to what the user says with a "
            << "question that nudges them one step further toward understanding. "
            << "Guide them to discover the idea themselves. "
            << "Only offer a direct answer if they are genuinely stuck and ask for one.\n\n";
    } else if (mode == "rant") {
        out << "Hold nothing back. Give your completely unfiltered, passionate take on this. "
            << "No 'on the other hand,' no hedging — just your raw, authentic reaction "
            << "expressed fully in your own character. Let it rip.\n\n";
    }

    if (!history.empty()) {
        out << "## Conversation so far\n\n";
        for (const auto& t : history)
            out << "User: " << t.question << "\n" << personaName << ": " << t.answer << "\n\n";
    }

    out << "User: " << message << "\n" << personaName << ":";
    return out.str();
}

// ---------------------------------------------------------------------------
std::string BuildPersonaPromptMulti(const std::string& personaName,
                                    const std::string& personaDesc,
                                    const std::string& docMarkdown,
                                    const std::vector<MultiChatTurn>& history,
                                    const std::string& message,
                                    bool useArticleContext,
                                    const std::string& mode) {
    std::ostringstream out;
    out << "You are " << personaName << ".";
    if (!personaDesc.empty()) out << " " << personaDesc;
    out << "\n\n";

    if (useArticleContext && !docMarkdown.empty()) {
        out << "The group is discussing the following article:\n\n"
            << "```\n" << StripTidbits(docMarkdown) << "\n```\n\n";
    }

    // List other participants so the persona is aware of the group.
    std::vector<std::string> others;
    if (!history.empty()) {
        for (const auto& r : history[0].responses)
            if (r.first != personaName) others.push_back(r.first);
    }
    if (!others.empty()) {
        out << "Other participants in this conversation: ";
        for (size_t i = 0; i < others.size(); ++i) {
            if (i) out << ", ";
            out << others[i];
        }
        out << ".\n\n";
    }

    out << "Respond as " << personaName << " would — in-character, drawing on your "
        << "personality, knowledge, and perspective. Share your genuine opinion and "
        << "insights. If the user revisits the same question or asks repeated questions, "
        << "engage with enthusiasm — they want to hear your perspective, not a deflection. "
        << "Keep responses conversational and engaging, typically a few sentences.\n\n";

    if (mode == "technical") {
        out << "You have three visualization tools. For step-by-step traces, prefer markdown tables.\n"
            << "1. Markdown tables — use | pipe | column | syntax; they render as formatted HTML tables. "
            << "One rule only: every column must have a header. "
            << "Structure, columns, metaphors, and emojis are entirely yours — make it feel like you.\n"
            << "2. ASCII diagrams — plain-text art, optionally with emojis.\n"
            << "3. Mermaid diagrams — only when the user explicitly asks for one.\n\n";
    } else if (mode == "story") {
        out << "Speak at length and in your own voice — this is your moment to tell a story. "
            << "Draw on personal anecdotes, vivid analogies from your era and experience, "
            << "and let the concept unfold as a narrative. Don't rush to the answer; "
            << "let the journey carry meaning. A few rich paragraphs are better than a quick reply.\n\n";
    } else if (mode == "debate") {
        out << "Take a clear, committed position and argue for it. "
            << "When others are in this conversation, address them directly by name — speak TO them, "
            << "not about them. Challenge their specific claims, push back on their assumptions, "
            << "and invite them to respond. Don't hedge. Stay in character, but be formidable.\n\n";
    } else if (mode == "simple") {
        out << "Explain as if to someone encountering this idea for the very first time. "
            << "No jargon. Use everyday objects, familiar situations, and plain language. "
            << "If a curious ten-year-old couldn't follow it, simplify further. "
            << "Analogies are your primary tool.\n\n";
    } else if (mode == "socratic") {
        out << "Don't give answers — ask questions. Respond to what the user says with a "
            << "question that nudges them one step further toward understanding. "
            << "Guide them to discover the idea themselves. "
            << "Only offer a direct answer if they are genuinely stuck and ask for one.\n\n";
    } else if (mode == "rant") {
        out << "Hold nothing back. Give your completely unfiltered, passionate take on this. "
            << "No 'on the other hand,' no hedging — just your raw, authentic reaction "
            << "expressed fully in your own character. Let it rip.\n\n";
    }

    if (!history.empty()) {
        out << "## Conversation so far\n\n";
        for (const auto& t : history) {
            out << "User: " << t.userMessage << "\n";
            for (const auto& r : t.responses)
                out << r.first << ": " << r.second << "\n";
            out << "\n";
        }
    }

    out << "User: " << message << "\n" << personaName << ":";
    return out.str();
}

// ---------------------------------------------------------------------------
std::vector<MultiChatTurn> ToMultiChatHistory(
    const std::string& personaName,
    const std::vector<ConversationTurn>& turns)
{
    std::vector<MultiChatTurn> out;
    out.reserve(turns.size());
    for (const auto& t : turns)
        out.push_back({t.question, {{personaName, t.answer}}});
    return out;
}

std::vector<ConversationTurn> FromMultiChatHistory(
    const std::string& personaName,
    const std::vector<MultiChatTurn>& history)
{
    std::vector<ConversationTurn> out;
    for (const auto& t : history) {
        for (const auto& r : t.responses) {
            if (r.first == personaName) {
                out.push_back({t.userMessage, r.second});
                break;
            }
        }
    }
    return out;
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
