#pragma once
#include <string>
#include <vector>

struct CharacterEntry {
    std::string name;
    std::string description; // optional disambiguation shown to the LLM
    CharacterEntry() = default;
    CharacterEntry(const char* n) : name(n) {}  // allows {"Name"} initializer-list syntax
    CharacterEntry(std::string n, std::string d = "")
        : name(std::move(n)), description(std::move(d)) {}
};

struct GenerationRequest {
    std::string topic;
    std::string style;
    std::vector<CharacterEntry> characters;
    std::string projectContext;
    int tidbitsPerChapter = 1;
};

// Builds a full LLM prompt from the request.
// llmReadme is injected from GetLLMReadme(); callers may pass "" in tests.
std::string BuildPrompt(const GenerationRequest& req, const std::string& llmReadme);

struct ValidationResult {
    bool ok;
    std::string error;
};

// Checks that generated content has the minimum structure StoryTeller needs
// before it is stamped and saved.
ValidationResult ValidateGeneratedStory(const std::string& content);

// Builds a follow-up prompt after a generated story failed validation.
std::string BuildRepairPrompt(const std::string& originalPrompt,
                              const std::string& validationError);

// Builds a patch prompt asking the LLM to rewrite a single block per the instruction.
// chapterContext: the full chapter text — gives the LLM story context for the rewrite.
std::string BuildPatchPrompt(const std::string& originalBlock,
                             const std::string& instruction,
                             const std::string& llmReadme,
                             const std::string& chapterContext = "");

std::string BuildTranslationPrompt(const std::string& sourceMarkdown,
                                   const std::string& targetLanguage,
                                   const std::string& extraInstruction = "");

// Returns the extra-instruction string that asks the LLM to add ::pinyin tag lines.
std::string BuildPinyinInstruction();

// Removes lines starting with "::pinyin " from text, leaving the Chinese prose.
std::string StripPinyinLines(const std::string& text);

// Derives the output filename for a translated file.
// "the_ravens_shadow.md" + "Spanish"          -> "the_ravens_shadow_Spanish.md"
// "the_ravens_shadow.md" + "Chinese w/ Pinyin" -> "the_ravens_shadow_Chinese_Pinyin.md"
// "the_ravens_shadow.md" + "Chinese (Mandarin)"-> "the_ravens_shadow_Chinese.md"
std::string TranslationFilename(const std::string& sourceFilename,
                                const std::string& language);

// Removes common LLM wrappers around returned markdown, such as an outer
// ```markdown fence and short prose before the actual document begins.
std::string CleanMarkdownResponse(const std::string& response);

// Injects <!-- ch:N --> markers before each "## Chapter N:" heading.
// Returns the stamped text and the count of chapters found.
struct StampResult { std::string text; int count; };
StampResult StampChapters(const std::string& content, int baseId);

// Returns a filename like "ch03_black_holes.md" from a topic and chapter number.
std::string ChapterFilename(const std::string& topic, int chapterNumber);

// Derives a filename from the first ## heading in content (e.g. "the_ravens_shadow.md").
// Falls back to ChapterFilename(fallback, chapterNumber) when no usable heading is found.
std::string FilenameFromContent(const std::string& content,
                                const std::string& fallback,
                                int chapterNumber);

// Writes content to <projectDir>/<filename> and returns the full path.
// Creates the file; returns "" on failure.
std::string SaveChapter(const std::string& projectDir,
                        const std::string& filename,
                        const std::string& content);

// Returns the relative path from defaultFolder to the project directory containing
// filePath (e.g. "/base/Literature/agatha/ch01.md", "/base" -> "Literature/agatha").
// Returns "" when the file is not under defaultFolder or is directly in defaultFolder.
std::string ProjectNameFromFilePath(const std::string& filePath,
                                    const std::string& defaultFolder);
