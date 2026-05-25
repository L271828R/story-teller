#include "prompt_store.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Minimal JSON helpers (no external dependency)
// ---------------------------------------------------------------------------

static std::string jsEsc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r':              break;
            case '\t': o += "\\t";  break;
            default:   o += c;      break;
        }
    }
    return o;
}

// Read a JSON string value starting at pos (after the opening '"').
// Advances pos past the closing '"'.
static std::string readJsonString(const std::string& s, size_t& pos) {
    std::string val;
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < s.size()) {
            char e = s[pos++];
            switch (e) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                default:   val += e;    break;
            }
        } else {
            val += c;
        }
    }
    return val;
}

static std::string readJsonField(const std::string& json, size_t& pos,
                                  const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t kp = json.find(search, pos);
    if (kp == std::string::npos) return "";
    size_t p = kp + search.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == ':')) ++p;
    if (p >= json.size()) return "";
    if (json[p] == '"') { ++p; return readJsonString(json, p); }
    // Number
    std::string num;
    while (p < json.size() && (json[p] >= '0' && json[p] <= '9')) num += json[p++];
    return num;
}

// ---------------------------------------------------------------------------

std::string PromptsFilePath() {
    const char* home = std::getenv("HOME");
    std::string dir = (home ? std::string(home) : "/tmp")
                    + "/Library/Logs/StoryTeller";
    return dir + "/prompts.json";
}

static const std::string& resolvedPath(const std::string& arg) {
    static std::string def;
    if (!arg.empty()) return arg;
    def = PromptsFilePath();
    return def;
}

std::vector<SavedPrompt> LoadPrompts(const std::string& filePath) {
    const std::string& path = resolvedPath(filePath);
    std::ifstream f(path);
    if (!f) return {};
    std::string json{std::istreambuf_iterator<char>(f), {}};

    std::vector<SavedPrompt> result;
    size_t pos = 0;
    while (true) {
        size_t ob = json.find('{', pos);
        if (ob == std::string::npos) break;
        size_t cb = json.find('}', ob);
        if (cb == std::string::npos) break;
        std::string obj = json.substr(ob, cb - ob + 1);
        size_t p = 0;
        std::string idStr = readJsonField(obj, p, "id");
        p = 0;
        std::string title = readJsonField(obj, p, "title");
        p = 0;
        std::string text  = readJsonField(obj, p, "text");
        if (!idStr.empty()) {
            SavedPrompt sp;
            try { sp.id = std::stoi(idStr); } catch (...) { pos = cb + 1; continue; }
            sp.title = title;
            sp.text  = text;
            result.push_back(sp);
        }
        pos = cb + 1;
    }
    return result;
}

void SaveAllPrompts(const std::vector<SavedPrompt>& prompts,
                    const std::string& filePath) {
    const std::string& path = resolvedPath(filePath);
    std::ofstream f(path);
    f << "[\n";
    for (size_t i = 0; i < prompts.size(); ++i) {
        if (i) f << ",\n";
        f << "{\"id\":" << prompts[i].id
          << ",\"title\":\"" << jsEsc(prompts[i].title) << "\""
          << ",\"text\":\""  << jsEsc(prompts[i].text)  << "\"}";
    }
    f << "\n]\n";
}

SavedPrompt AddPrompt(const std::string& title, const std::string& text,
                      const std::string& filePath) {
    auto ps = LoadPrompts(filePath);
    int maxId = 0;
    for (auto& p : ps) if (p.id > maxId) maxId = p.id;
    SavedPrompt np{maxId + 1, title, text};
    ps.push_back(np);
    SaveAllPrompts(ps, filePath);
    return np;
}

bool DeletePrompt(int id, const std::string& filePath) {
    auto ps = LoadPrompts(filePath);
    auto it = std::find_if(ps.begin(), ps.end(),
                           [id](const SavedPrompt& p){ return p.id == id; });
    if (it == ps.end()) return false;
    ps.erase(it);
    SaveAllPrompts(ps, filePath);
    return true;
}

bool UpdatePrompt(int id, const std::string& title, const std::string& text,
                  const std::string& filePath) {
    auto ps = LoadPrompts(filePath);
    auto it = std::find_if(ps.begin(), ps.end(),
                           [id](const SavedPrompt& p){ return p.id == id; });
    if (it == ps.end()) return false;
    it->title = title;
    it->text  = text;
    SaveAllPrompts(ps, filePath);
    return true;
}
