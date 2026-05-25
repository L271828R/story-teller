#include "quiz.h"
#include <sstream>
#include <algorithm>

std::string StripTidbits(const std::string& md) {
    std::string result;
    result.reserve(md.size());
    size_t i = 0;
    const size_t n = md.size();

    while (i < n) {
        // Look for a line starting with ":::tidbit"
        size_t lineEnd = md.find('\n', i);
        if (lineEnd == std::string::npos) lineEnd = n - 1;
        std::string line = md.substr(i, lineEnd - i + 1);

        // Check for opening fence
        size_t colon = 0;
        while (colon < line.size() && line[colon] == ':') ++colon;
        bool isTidbitOpen = colon >= 3
            && line.size() > colon + 6
            && line.substr(colon, 6) == "tidbit";

        if (isTidbitOpen) {
            // Skip until we find the closing ":::" fence on its own line
            size_t j = lineEnd + 1;
            while (j < n) {
                size_t end = md.find('\n', j);
                if (end == std::string::npos) end = n - 1;
                std::string closeLine = md.substr(j, end - j + 1);
                // Count leading colons
                size_t cc = 0;
                while (cc < closeLine.size() && closeLine[cc] == ':') ++cc;
                bool rest = (cc == closeLine.size()
                             || closeLine[cc] == '\n'
                             || closeLine[cc] == '\r');
                if (cc >= 3 && rest) {
                    i = end + 1;
                    break;
                }
                j = end + 1;
            }
            if (j >= n) i = n; // unterminated tidbit — skip to end
            // Remove trailing blank line left behind the block
            while (i < n && md[i] == '\n') { ++i; }
            if (i > 0 && i < n) result += '\n'; // keep paragraph separation
        } else {
            result += line;
            i = lineEnd + 1;
        }
    }
    return result;
}

std::string BuildQuizPrompt(const std::string& md, int n, const std::string& extra) {
    std::ostringstream out;
    out << "Here is a document. Please create a multiple-choice quiz with exactly "
        << n << " questions to test comprehension.\n\n"
        << "Format each question like this:\n\n"
        << "**Q1:** Question text?\n\n"
        << "A) option  B) option  C) option  D) option\n\n"
        << "**Answer:** X\n\n"
        << "Output only the quiz questions and answers — no preamble, no explanation.\n\n";
    if (!extra.empty())
        out << "Additional instructions: " << extra << "\n\n";
    out << "---\n\n" << md;
    return out.str();
}

std::string AppendQuizToMarkdown(const std::string& md,
                                 const std::string& quizMarkdown) {
    // Find and remove any existing "## Quiz" section
    const std::string marker = "## Quiz";
    std::string base = md;

    size_t pos = base.find('\n' + marker);
    if (pos == std::string::npos && base.substr(0, marker.size()) == marker)
        pos = std::string::npos, base = base; // handle file starting with ## Quiz
    // Also check start-of-file
    if (pos == std::string::npos && base.size() >= marker.size()
            && base.substr(0, marker.size()) == marker) {
        base.clear();
    } else if (pos != std::string::npos) {
        base = base.substr(0, pos + 1); // keep the newline before ## Quiz
    }

    // Strip trailing whitespace/newlines from base
    while (!base.empty() && (base.back() == '\n' || base.back() == '\r'))
        base.pop_back();

    return base + "\n\n" + marker + "\n\n" + quizMarkdown;
}

std::string RemoveEntireQuiz(const std::string& md) {
    const std::string marker = "## Quiz";
    // Check start-of-file
    if (md.size() >= marker.size() && md.substr(0, marker.size()) == marker) {
        return "";
    }
    size_t pos = md.find('\n' + marker);
    if (pos == std::string::npos) return md; // no quiz section
    std::string result = md.substr(0, pos);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result + "\n";
}

static std::string trimTrailing(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end-1] == '\n' || s[end-1] == '\r' || s[end-1] == ' '))
        --end;
    return s.substr(0, end);
}

std::vector<QuizQuestion> ParseQuizQuestions(const std::string& md) {
    const std::string marker = "## Quiz";
    std::string section;
    if (md.size() >= marker.size() && md.substr(0, marker.size()) == marker) {
        section = md.substr(marker.size());
    } else {
        size_t pos = md.find('\n' + marker);
        if (pos == std::string::npos) return {};
        section = md.substr(pos + 1 + marker.size());
    }

    std::vector<QuizQuestion> result;
    // Split by "\n**Q" followed by digit
    size_t i = 0;
    while (i < section.size() && (section[i] == '\n' || section[i] == '\r')) ++i;

    while (i < section.size()) {
        // Find start of "**Q{N}:**"
        if (section[i] != '*') { ++i; continue; }
        size_t blockStart = i;
        // Find the end of this block = start of next "\n**Q" or end of section
        size_t next = section.find("\n**Q", i + 1);
        size_t blockEnd = (next == std::string::npos) ? section.size() : next;
        std::string block = trimTrailing(section.substr(blockStart, blockEnd - blockStart));
        if (!block.empty()) {
            QuizQuestion q;
            // Parse number from "**Q{N}:**"
            q.num = 0;
            if (block.size() > 3 && block[0] == '*' && block[1] == '*' && block[2] == 'Q') {
                size_t j = 3;
                while (j < block.size() && block[j] >= '0' && block[j] <= '9') {
                    q.num = q.num * 10 + (block[j] - '0');
                    ++j;
                }
            }
            if (q.num > 0) {
                q.rawBlock = block;
                result.push_back(q);
            }
        }
        i = (next == std::string::npos) ? section.size() : next + 1;
    }
    return result;
}

static std::string rebuildQuiz(const std::string& md,
                               const std::vector<QuizQuestion>& questions) {
    if (questions.empty()) return RemoveEntireQuiz(md);
    std::string quizMd;
    for (size_t i = 0; i < questions.size(); ++i) {
        if (i > 0) quizMd += "\n\n";
        quizMd += questions[i].rawBlock;
    }
    return AppendQuizToMarkdown(md, quizMd);
}

std::string RemoveQuizQuestion(const std::string& md, int qNum) {
    auto qs = ParseQuizQuestions(md);
    std::vector<QuizQuestion> remaining;
    for (auto& q : qs)
        if (q.num != qNum) remaining.push_back(q);
    // Renumber sequentially
    for (int i = 0; i < (int)remaining.size(); ++i) {
        int newNum = i + 1;
        if (remaining[i].num != newNum) {
            std::string oldPfx = "**Q" + std::to_string(remaining[i].num) + ":**";
            std::string newPfx = "**Q" + std::to_string(newNum) + ":**";
            auto& raw = remaining[i].rawBlock;
            size_t pos = raw.find(oldPfx);
            if (pos != std::string::npos) raw.replace(pos, oldPfx.size(), newPfx);
            remaining[i].num = newNum;
        }
    }
    return rebuildQuiz(md, remaining);
}

std::string ReplaceQuizQuestion(const std::string& md, int qNum,
                                const std::string& newBlock) {
    auto qs = ParseQuizQuestions(md);
    for (auto& q : qs)
        if (q.num == qNum) { q.rawBlock = trimTrailing(newBlock); break; }
    return rebuildQuiz(md, qs);
}
