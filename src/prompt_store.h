#pragma once
#include <string>
#include <vector>

struct SavedPrompt {
    int id;
    std::string title;
    std::string text;
};

// Returns the default path for the prompts JSON file.
std::string PromptsFilePath();

// Load all saved prompts from filePath (defaults to PromptsFilePath()).
std::vector<SavedPrompt> LoadPrompts(const std::string& filePath = "");

// Persist a full list to filePath.
void SaveAllPrompts(const std::vector<SavedPrompt>& prompts,
                    const std::string& filePath = "");

// Add a new prompt, auto-assign an ID, persist, return the new entry.
SavedPrompt AddPrompt(const std::string& title, const std::string& text,
                      const std::string& filePath = "");

// Delete by id. Returns true if the id was found and removed.
bool DeletePrompt(int id, const std::string& filePath = "");

// Update title and text for an existing id. Returns true if found.
bool UpdatePrompt(int id, const std::string& title, const std::string& text,
                  const std::string& filePath = "");
