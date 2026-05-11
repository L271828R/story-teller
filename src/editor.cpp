#include "editor.h"
#include <fstream>
#include <sstream>
#include <string>

static std::string marker_for(int id) {
    return "<!-- tb:" + std::to_string(id) + " -->";
}

// Returns the position just past the end of the :::tidbit...:::  block
// that starts at `start` in `s`. Returns std::string::npos if not well-formed.
static std::size_t tidbit_end(const std::string& s, std::size_t start) {
    std::size_t p = start;
    // Skip to the closing line that is exactly ":::" (possibly followed by \n).
    while (p < s.size()) {
        std::size_t nl = s.find('\n', p);
        std::string line = (nl == std::string::npos)
                           ? s.substr(p)
                           : s.substr(p, nl - p);
        if (line == ":::") {
            return (nl == std::string::npos) ? s.size() : nl + 1;
        }
        p = (nl == std::string::npos) ? s.size() : nl + 1;
    }
    return std::string::npos;
}

std::string ExtractTidbit(const std::string& fileContent, int tidbitId) {
    std::string marker = marker_for(tidbitId);
    auto mpos = fileContent.find(marker);
    if (mpos == std::string::npos) return "";

    // Skip past marker line.
    auto after_marker = fileContent.find('\n', mpos);
    if (after_marker == std::string::npos) return "";
    after_marker += 1;

    // Skip blank lines to find :::tidbit[...].
    std::size_t block_start = after_marker;
    while (block_start < fileContent.size() && fileContent[block_start] == '\n')
        ++block_start;

    if (fileContent.compare(block_start, 9, ":::tidbit") != 0) return "";

    auto block_end = tidbit_end(fileContent, block_start);
    if (block_end == std::string::npos) return "";

    // Trim trailing newline from the returned block.
    std::size_t len = block_end - block_start;
    while (len > 0 && fileContent[block_start + len - 1] == '\n') --len;
    return fileContent.substr(block_start, len);
}

std::string PatchTidbit(const std::string& fileContent,
                        int tidbitId,
                        const std::string& newBlock) {
    std::string marker = marker_for(tidbitId);
    auto mpos = fileContent.find(marker);
    if (mpos == std::string::npos) return fileContent;

    auto after_marker = fileContent.find('\n', mpos);
    if (after_marker == std::string::npos) return fileContent;
    after_marker += 1;

    // Find start of tidbit block (skip blank lines).
    std::size_t block_start = after_marker;
    while (block_start < fileContent.size() && fileContent[block_start] == '\n')
        ++block_start;

    if (fileContent.compare(block_start, 9, ":::tidbit") != 0) return fileContent;

    auto block_end = tidbit_end(fileContent, block_start);
    if (block_end == std::string::npos) return fileContent;

    std::string result;
    result.reserve(fileContent.size());
    result += fileContent.substr(0, block_start);
    result += newBlock;
    result += '\n';
    result += fileContent.substr(block_end);
    return result;
}

bool ApplyTidbitPatch(const std::string& filepath,
                      int tidbitId,
                      const std::string& newBlock) {
    std::string content;
    {
        std::ifstream f(filepath);
        if (!f) return false;
        content.assign(std::istreambuf_iterator<char>(f), {});
    }
    std::string patched = PatchTidbit(content, tidbitId, newBlock);
    std::ofstream f(filepath, std::ios::trunc);
    f << patched;
    return f.good();
}

bool ReplaceChapter(const std::string& filepath, const std::string& newContent) {
    std::ofstream f(filepath, std::ios::trunc);
    f << newContent;
    return f.good();
}
