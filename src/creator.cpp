#include "creator.h"
#include "markdown.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

static std::string slugify(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (!out.empty() && out.back() != '_')
            out += '_';
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

std::string BuildPrompt(const GenerationRequest& req, const std::string& llmReadme) {
    std::ostringstream out;

    if (!req.projectContext.empty())
        out << "## Project context\n\n" << req.projectContext << "\n\n";

    out << "## Task\n\n"
        << "Write a **" << req.style << "** explanation of the following topic:\n\n"
        << "> " << req.topic << "\n\n";

    if (!req.characters.empty()) {
        out << "## Tidbit characters\n\n"
            << "Include tidbits from the following characters. "
               "Give each one a voice consistent with who they are:\n\n";
        for (auto& ch : req.characters)
            out << "- " << ch << "\n";
        out << "\n";
    }

    out << "## Output requirements\n\n"
        << "- Return only valid MDViewer markdown — no prose before or after.\n"
        << "- Use the tidbit syntax below for character commentary.\n"
        << "- Every tidbit must have content; do not emit empty tidbits.\n\n";

    std::string ref = llmReadme.empty() ? GetLLMReadme() : llmReadme;
    out << "## MDViewer syntax reference\n\n" << ref;

    return out.str();
}

std::string BuildPatchPrompt(const std::string& originalBlock,
                             const std::string& instruction,
                             const std::string& llmReadme) {
    std::ostringstream out;

    out << "## Rewrite instruction\n\n"
        << instruction << "\n\n"
        << "## Original block\n\n"
        << "```\n" << originalBlock << "\n```\n\n"
        << "## Requirements\n\n"
        << "- Return only the rewritten block — nothing else.\n"
        << "- Preserve the block type (chapter section or tidbit).\n"
        << "- Use valid MDViewer syntax (reference below).\n\n";

    std::string ref = llmReadme.empty() ? GetLLMReadme() : llmReadme;
    out << "## MDViewer syntax reference\n\n" << ref;

    return out.str();
}

std::string ChapterFilename(const std::string& topic, int chapterNumber) {
    std::ostringstream prefix;
    prefix << "ch" << std::setw(2) << std::setfill('0') << chapterNumber << "_";
    return prefix.str() + slugify(topic) + ".md";
}

std::string SaveChapter(const std::string& projectDir,
                        const std::string& filename,
                        const std::string& content) {
    fs::path path = fs::path(projectDir) / filename;
    std::ofstream f(path);
    f << content;
    if (!f.good()) return "";
    return path.string();
}
