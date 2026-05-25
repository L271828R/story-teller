#pragma once
#include <string>
#include <vector>

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

// Removes the entire ## Quiz section (everything from ## Quiz to end of file).
std::string RemoveEntireQuiz(const std::string& md);

struct QuizQuestion {
    int num;
    std::string rawBlock; // raw markdown for this question (no trailing newlines)
};

// Parses all questions from a markdown document that has a ## Quiz section.
// Returns empty vector if no quiz section exists.
std::vector<QuizQuestion> ParseQuizQuestions(const std::string& md);

// Removes one question (1-based qNum), renumbers the remaining ones.
// If removing the last question, removes the entire ## Quiz section.
std::string RemoveQuizQuestion(const std::string& md, int qNum);

// Replaces one question's raw block (1-based qNum).
std::string ReplaceQuizQuestion(const std::string& md, int qNum,
                                const std::string& newBlock);
