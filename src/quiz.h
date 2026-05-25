#pragma once
#include <string>

// Removes all :::tidbit[...] ... ::: blocks from markdown content.
std::string StripTidbits(const std::string& md);

// Builds the LLM prompt for quiz generation.
// md should be pre-stripped of tidbits.
// n is the number of questions to request.
// extra is optional free-text instruction appended to the prompt (e.g. "in Spanish").
std::string BuildQuizPrompt(const std::string& md, int n,
                            const std::string& extra = "");

// Appends (or replaces) a "## Quiz" section at the end of md with quizMarkdown.
std::string AppendQuizToMarkdown(const std::string& md,
                                 const std::string& quizMarkdown);
