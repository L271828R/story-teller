#pragma once
#include <string>
#include <vector>
#include <functional>

struct ConversationTurn {
    std::string question;
    std::string answer;   // empty while LLM has not yet responded
};

// A single exchange in a multi-persona chat: one user message and one response
// per active persona (in the order they were added to the conversation).
struct MultiChatTurn {
    std::string userMessage;
    std::vector<std::pair<std::string, std::string>> responses; // {personaName, answer}
};

// Parse Q:/A: pairs from the body of a :::conversation block.
std::vector<ConversationTurn> ParseConversation(const std::string& body);

// Serialize turns to the Q:/A: block body (no opening/closing ::: lines).
std::string SerializeConversationBody(const std::vector<ConversationTurn>& turns);

// Read the existing conversation turns for chId from filePath.
// Returns empty if the file has no :::conversation block for that chapter.
std::vector<ConversationTurn> LoadConversation(const std::string& filePath, int chId);

// Append a completed turn to the :::conversation block for chId.
// Creates the block if none exists, placed before the next chapter separator.
// Returns false on I/O error.
bool AppendTurn(const std::string& filePath, int chId,
                const std::string& chTitle, const ConversationTurn& turn);

// Delete the turn at zero-based index from the :::conversation block for chId.
// If it is the last turn the entire block is removed.
// Returns false on I/O error or out-of-range index.
bool DeleteTurn(const std::string& filePath, int chId, int index);

// Build the LLM prompt for a Q&A request.
std::string BuildQAPrompt(const std::string& docMarkdown,
                          const std::string& chTitle,
                          const std::vector<ConversationTurn>& history,
                          const std::string& question);

// Build a prompt asking the LLM to fix a broken Mermaid diagram.
// Returns a prompt expecting only the corrected raw diagram syntax in response.
std::string BuildMermaidRepairPrompt(const std::string& source);

// Build the LLM prompt for a single-persona conversation.
// mode: "technical" injects table guidance, "creative" injects ASCII art guidance.
std::string BuildPersonaPrompt(const std::string& personaName,
                               const std::string& personaDesc,
                               const std::string& docMarkdown,
                               const std::vector<ConversationTurn>& history,
                               const std::string& message,
                               bool useArticleContext,
                               const std::string& mode = "general");

// Build the LLM prompt for one persona's turn in a multi-persona conversation.
// history contains the full exchange so far (all participants labeled).
// The responding persona sees the other participants' names and their prior responses.
// mode: "technical" injects table guidance, "creative" injects ASCII art guidance.
std::string BuildPersonaPromptMulti(const std::string& personaName,
                                    const std::string& personaDesc,
                                    const std::string& docMarkdown,
                                    const std::vector<MultiChatTurn>& history,
                                    const std::string& message,
                                    bool useArticleContext,
                                    const std::string& mode = "general");

// Convert a single-persona history (ConversationTurn) to MultiChatTurn format
// so a solo chat can be promoted to a group chat.
std::vector<MultiChatTurn> ToMultiChatHistory(
    const std::string& personaName,
    const std::vector<ConversationTurn>& turns);

// Convert a multi-persona history back to single-persona format (only the
// responses by personaName are kept). Used when saving a solo chat.
std::vector<ConversationTurn> FromMultiChatHistory(
    const std::string& personaName,
    const std::vector<MultiChatTurn>& history);

