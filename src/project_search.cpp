#include "project_search.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static std::string lowerAscii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out.push_back((char)std::tolower(c));
    return out;
}

bool ProjectSearchTextMatches(const std::string& searchableText,
                              const std::string& query) {
    std::string h = lowerAscii(searchableText);
    std::istringstream terms(lowerAscii(query));
    std::string term;
    bool sawTerm = false;
    while (terms >> term) {
        sawTerm = true;
        if (h.find(term) == std::string::npos)
            return false;
    }
    return sawTerm;
}

std::string BuildProjectSearchText(const std::string& name,
                                   const std::string& path,
                                   const std::string& source,
                                   const std::string& lastLLM) {
    std::string searchable = name + " " + path + " " + source + " " + lastLLM;
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(path, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".md")
            continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        std::ostringstream ss;
        ss << f.rdbuf();
        searchable += " " + entry.path().filename().string() + " " + ss.str();
    }
    return searchable;
}

bool ProjectMatchesSearch(const std::string& name,
                          const std::string& path,
                          const std::string& source,
                          const std::string& lastLLM,
                          const std::string& query) {
    if (query.empty()) return true;
    return ProjectSearchTextMatches(
        BuildProjectSearchText(name, path, source, lastLLM), query);
}
