#include "quiz.h"
#include <sstream>

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
