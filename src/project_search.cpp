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
                                   const std::string& /*path*/,
                                   const std::string& source,
                                   const std::string& lastLLM) {
    // Project-level search covers project metadata only. Article filenames and
    // contents are searched separately by ArticleMatchesSearch so the caller
    // can surface *which* article matched.
    return name + " " + source + " " + lastLLM;
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

bool ArticleMatchesSearch(const std::string& articlePath,
                          const std::string& query) {
    if (query.empty()) return true;
    fs::path p(articlePath);
    std::string searchable = p.filename().string();
    std::ifstream f(p);
    if (f) {
        std::ostringstream ss;
        ss << f.rdbuf();
        searchable += " " + ss.str();
    }
    return ProjectSearchTextMatches(searchable, query);
}
