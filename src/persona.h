#pragma once
#include <map>
#include <string>
#include <vector>
#include "conversation.h"

// Global persona image store: ~/story-teller/personas/
std::string GetPersonasDir();

// Persistent per-persona conversation store: ~/story-teller/persona_chats/
// Each persona gets its own <normalized_name>.md file.
std::string GetPersonaChatsDir();

// Load saved conversation turns for a persona from chatsDir (defaults to
// GetPersonaChatsDir()). Returns empty if no file exists yet.
std::vector<ConversationTurn> LoadPersonaConversation(
    const std::string& personaName,
    const std::string& chatsDir = "");

// Save turns for a persona. An empty turns vector deletes the file.
void SavePersonaConversation(
    const std::string& personaName,
    const std::vector<ConversationTurn>& turns,
    const std::string& chatsDir = "");

// Rename a persona's chat file (no-op if no file exists).
void RenamePersonaChat(
    const std::string& oldName,
    const std::string& newName,
    const std::string& chatsDir = "");

// Lowercase, spaces→underscores, strip non-alphanumeric.
// Must match the JS normalization in html_template.cpp.
std::string NormalizePersonaName(const std::string& name);

// Scan personas dir; return map of normalized_name → file:// URL.
// Returns empty map if the directory doesn't exist.
std::map<std::string, std::string> ScanPersonaImages();

// Extract character names from :::tidbit[Name] blocks in markdown text.
std::vector<std::string> ExtractTidbitNames(const std::string& markdown);

// Walk all .md files under rootDir (recursively) and collect unique tidbit names.
// Returns an alphabetically sorted list. Safe if the directory doesn't exist.
std::vector<std::string> ExtractTidbitNamesFromDir(const std::string& rootDir);

// Scan one level of subdirectories of rootDir; return category_name → sorted tidbit names.
std::map<std::string, std::vector<std::string>> ExtractTidbitNamesByCategory(
    const std::string& rootDir);

// Copy srcImagePath into the personas dir under the given name.
// Creates the directory if needed. Returns destination path or "" on failure.
std::string AddPersonaImage(const std::string& personaName,
                             const std::string& srcImagePath);

// Convert a map of file:// URLs to data: URLs by reading and base64-encoding
// each file in memory. Entries that can't be read are omitted.
std::map<std::string, std::string> ToDataURLs(
    const std::map<std::string, std::string>& fileUrls);

// Rename a persona's image file from NormalizePersonaName(oldName).* to
// NormalizePersonaName(newName).* inside personasDir (defaults to GetPersonasDir()).
// Returns true if a file was found and renamed, false otherwise.
bool RenamePersonaImage(const std::string& oldName,
                        const std::string& newName,
                        const std::string& personasDir = "");
