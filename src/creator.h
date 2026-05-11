#pragma once
#include <string>
#include <vector>

struct GenerationRequest {
    std::string topic;
    std::string style;
    std::vector<std::string> characters;
    std::string projectContext;
};

// Builds a full LLM prompt from the request.
// llmReadme is injected from GetLLMReadme(); callers may pass "" in tests.
std::string BuildPrompt(const GenerationRequest& req, const std::string& llmReadme);

// Builds a patch prompt asking the LLM to rewrite a single block per the instruction.
std::string BuildPatchPrompt(const std::string& originalBlock,
                             const std::string& instruction,
                             const std::string& llmReadme);

// Returns a filename like "ch03_black_holes.md" from a topic and chapter number.
std::string ChapterFilename(const std::string& topic, int chapterNumber);

// Writes content to <projectDir>/<filename> and returns the full path.
// Creates the file; returns "" on failure.
std::string SaveChapter(const std::string& projectDir,
                        const std::string& filename,
                        const std::string& content);
