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
        if (out.size() >= 60) break;
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

std::string BuildPrompt(const GenerationRequest& req, const std::string& llmReadme) {
    std::ostringstream out;

    if (!req.projectContext.empty())
        out << "## Project context\n\n" << req.projectContext << "\n\n";

    out << "## Request\n\n"
        << req.topic << "\n\n";

    if (!req.style.empty())
        out << "Style: **" << req.style << "**\n\n";

    if (!req.characters.empty()) {
        out << "## Tidbit characters\n\n"
            << "Use the following characters for the `:::tidbit` sections. "
               "Each character must appear at least once; give them a distinct voice:\n\n";
        for (auto& ch : req.characters)
            out << "- " << ch << "\n";
        out << "\n";
    }

    out << "## Story structure\n\n"
        << "Divide the story into numbered chapters. Each chapter **must** start with a "
           "level-2 heading in exactly this format:\n\n"
           "```\n## Chapter N: Title\n```\n\n"
           "where N is a sequential integer starting at 1. "
           "Keep chapters in order with no gaps. "
           "Every chapter must contain at least one `:::tidbit` block.\n\n";

    out << "## Skill reminder\n\n"
        << "Use your **mdviewer skill** before generating — it contains the full syntax "
           "reference for the markdown this app renders. "
           "Return only valid mdviewer markdown with no prose before or after the document. "
           "Every tidbit must have content; do not emit empty tidbits.\n\n";

    std::string ref = llmReadme.empty() ? GetLLMReadme() : llmReadme;
    out << "## MDViewer syntax reference\n\n" << ref;

    return out.str();
}

std::string BuildPatchPrompt(const std::string& originalBlock,
                             const std::string& instruction,
                             const std::string& llmReadme,
                             const std::string& chapterContext) {
    std::ostringstream out;

    if (!chapterContext.empty()) {
        out << "## Story context\n\n"
            << "The following is the full chapter that contains the block you are rewriting. "
               "Use it to understand tone, characters, and what has already been said:\n\n"
            << "```\n" << chapterContext << "\n```\n\n";
    }

    out << "## Rewrite instruction\n\n"
        << instruction << "\n\n"
        << "## Original block\n\n"
        << "```\n" << originalBlock << "\n```\n\n"
        << "## Skill reminder\n\n"
        << "Use your **mdviewer skill** for the syntax reference. "
           "Return only the rewritten block — no prose before or after.\n\n";

    std::string ref = llmReadme.empty() ? GetLLMReadme() : llmReadme;
    out << "## MDViewer syntax reference\n\n" << ref;

    return out.str();
}

StampResult StampChapters(const std::string& content, int baseId) {
    std::string stamped;
    int count = 0;
    std::size_t pos = 0;
    const std::string needle = "\n## Chapter ";

    // Handle content that begins with a chapter heading (no leading newline).
    if (content.rfind("## Chapter ", 0) == 0) {
        stamped += "<!-- ch:" + std::to_string(baseId + count++) + " -->\n";
    }

    while (pos < content.size()) {
        auto found = content.find(needle, pos);
        if (found == std::string::npos) { stamped += content.substr(pos); break; }
        stamped += content.substr(pos, found + 1 - pos); // include the \n before ##
        stamped += "<!-- ch:" + std::to_string(baseId + count++) + " -->\n";
        pos = found + 1; // resume from ## (skip past the \n we already appended)
    }
    return {stamped, count};
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
