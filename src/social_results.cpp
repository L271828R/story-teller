#include "social_results.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static const std::string kSocialDir = ".social";

static std::string jsonEsc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else                 o += (char)c;
    }
    return o;
}

static std::string jsonUnescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case '"':  o += '"';  break;
                case '\\': o += '\\'; break;
                case 'n':  o += '\n'; break;
                case 'r':  o += '\r'; break;
                default:   o += s[i]; break;
            }
        } else {
            o += s[i];
        }
    }
    return o;
}

// Extract the value of a string field from a single JSON object line.
static std::string extractField(const std::string& line, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = line.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    std::string val;
    while (pos < line.size()) {
        char c = line[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < line.size()) {
            char e = line[pos++];
            switch (e) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                default:   val += e;    break;
            }
        } else val += c;
    }
    return val;
}

static std::string sectionPath(const std::string& projectDir,
                                const std::string& docName,
                                const std::string& section) {
    return (fs::path(projectDir) / kSocialDir / (docName + "." + section + ".jsonl")).string();
}

static std::vector<SocialEntry> loadSection(const std::string& projectDir,
                                             const std::string& docName,
                                             const std::string& section) {
    std::vector<SocialEntry> out;
    std::ifstream f(sectionPath(projectDir, docName, section));
    if (!f) return out;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] != '{') continue;
        SocialEntry e;
        e.text = extractField(line, "text");
        e.meta = extractField(line, "meta");
        out.push_back(std::move(e));
    }
    return out;
}

std::map<std::string,SocialDocResults> LoadAllSocialResults(const std::string& projectDir) {
    std::map<std::string,SocialDocResults> out;
    fs::path dir = fs::path(projectDir) / kSocialDir;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        // Expect: <docname>.<section>.jsonl
        auto dot1 = name.rfind(".jsonl");
        if (dot1 == std::string::npos) continue;
        name = name.substr(0, dot1);
        auto dot2 = name.rfind('.');
        if (dot2 == std::string::npos) continue;
        std::string section = name.substr(dot2 + 1);
        std::string docName = name.substr(0, dot2);
        if (section != "hooks" && section != "packaging" && section != "atomization") continue;

        auto entries = loadSection(projectDir, docName, section);
        if (entries.empty()) continue;
        if (section == "hooks")       out[docName].hooks       = std::move(entries);
        else if (section == "packaging")   out[docName].packaging   = std::move(entries);
        else if (section == "atomization") out[docName].atomization = std::move(entries);
    }
    return out;
}

void AppendSocialResult(const std::string& projectDir,
                         const std::string& docName,
                         const std::string& section,
                         const SocialEntry& entry) {
    fs::path dir = fs::path(projectDir) / kSocialDir;
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::ofstream f(sectionPath(projectDir, docName, section),
                    std::ios::app);
    if (!f) return;
    f << "{\"text\":\"" << jsonEsc(entry.text)
      << "\",\"meta\":\"" << jsonEsc(entry.meta) << "\"}\n";
}

void ClearSocialSection(const std::string& projectDir,
                         const std::string& docName,
                         const std::string& section) {
    std::error_code ec;
    fs::remove(sectionPath(projectDir, docName, section), ec);
}
