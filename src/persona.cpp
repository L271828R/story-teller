#include "persona.h"
#include "conversation.h"
#include "thumbnail.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <dirent.h>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

std::string GetPersonasDir() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/story-teller/personas" : "";
}

std::string NormalizePersonaName(const std::string& name) {
    std::string out;
    const unsigned char* p   = reinterpret_cast<const unsigned char*>(name.c_str());
    const unsigned char* end = p + name.size();
    while (p < end) {
        unsigned char c = *p;
        if (c < 0x80) {
            // ASCII
            if (std::isalnum(c))
                out += (char)std::tolower(c);
            else if (std::isspace(c) || c == '_') {
                if (!out.empty() && out.back() != '_')
                    out += '_';
            }
            ++p;
        } else {
            // Multi-byte UTF-8: keep the whole sequence as-is.
            int bytes = (c & 0xF8) == 0xF0 ? 4 :
                        (c & 0xF0) == 0xE0 ? 3 :
                        (c & 0xE0) == 0xC0 ? 2 : 1;
            for (int i = 0; i < bytes && p < end; ++i, ++p)
                out += (char)*p;
        }
    }
    while (!out.empty() && out.back() == '_')
        out.pop_back();
    return out;
}

static std::string extLower(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return ext;
}

static std::string stem(const std::string& filename) {
    auto dot = filename.rfind('.');
    return dot == std::string::npos ? filename : filename.substr(0, dot);
}

static bool isImageExt(const std::string& ext) {
    return ext == "jpg" || ext == "jpeg" || ext == "png"
        || ext == "gif" || ext == "webp";
}

std::map<std::string, std::string> ScanPersonaImages() {
    std::map<std::string, std::string> result;
    std::string dir = GetPersonasDir();
    if (dir.empty()) return result;

    DIR* d = opendir(dir.c_str());
    if (!d) return result;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string filename(entry->d_name);
        std::string ext = extLower(filename);
        if (!isImageExt(ext)) continue;
        std::string norm = NormalizePersonaName(stem(filename));
        if (norm.empty()) continue;
        std::string thumbPath = EnsureThumb(dir + "/" + filename);
        result[norm] = "file://" + thumbPath;
    }
    closedir(d);
    return result;
}

std::vector<std::string> ExtractTidbitNames(const std::string& markdown) {
    std::vector<std::string> names;
    const std::string marker = ":::tidbit[";
    size_t pos = 0;
    while ((pos = markdown.find(marker, pos)) != std::string::npos) {
        pos += marker.size();
        auto end = markdown.find(']', pos);
        if (end == std::string::npos) break;
        std::string name = markdown.substr(pos, end - pos);
        if (!name.empty()) names.push_back(name);
        pos = end + 1;
    }
    return names;
}

static void walkMdFiles(const std::string& dir,
                        std::vector<std::string>& out,
                        std::set<std::string>& seen) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string name(entry->d_name);
        std::string fullPath = dir + "/" + name;
        struct stat st;
        if (::stat(fullPath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walkMdFiles(fullPath, out, seen);
        } else if (S_ISREG(st.st_mode) &&
                   name.size() >= 3 &&
                   name.compare(name.size() - 3, 3, ".md") == 0) {
            std::ifstream f(fullPath);
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            for (const auto& n : ExtractTidbitNames(content))
                if (seen.insert(n).second) out.push_back(n);
        }
    }
    closedir(d);
}

std::vector<std::string> ExtractTidbitNamesFromDir(const std::string& rootDir) {
    std::vector<std::string> result;
    std::set<std::string> seen;
    walkMdFiles(rootDir, result, seen);
    std::sort(result.begin(), result.end());
    return result;
}

static std::string base64Encode(const std::vector<uint8_t>& d) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((d.size() + 2) / 3) * 4);
    for (size_t i = 0; i < d.size(); i += 3) {
        uint32_t n = (uint32_t)d[i] << 16;
        if (i+1 < d.size()) n |= (uint32_t)d[i+1] << 8;
        if (i+2 < d.size()) n |= (uint32_t)d[i+2];
        out += T[(n>>18)&63]; out += T[(n>>12)&63];
        out += (i+1 < d.size()) ? T[(n>>6)&63] : '=';
        out += (i+2 < d.size()) ? T[n&63]      : '=';
    }
    return out;
}

static std::string fileToDataUrl(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    if (data.empty()) return "";
    std::string ext = extLower(path);
    std::string mime = (ext == "png")  ? "image/png"  :
                       (ext == "gif")  ? "image/gif"  :
                       (ext == "webp") ? "image/webp" : "image/jpeg";
    return "data:" + mime + ";base64," + base64Encode(data);
}

std::map<std::string, std::string> ToDataURLs(
    const std::map<std::string, std::string>& fileUrls)
{
    std::map<std::string, std::string> result;
    for (const auto& kv : fileUrls) {
        const std::string& url = kv.second;
        std::string path = (url.substr(0, 7) == "file://") ? url.substr(7) : url;
        std::string data = fileToDataUrl(path);
        if (!data.empty())
            result[kv.first] = data;
    }
    return result;
}

std::map<std::string, std::vector<std::string>> ExtractTidbitNamesByCategory(
    const std::string& rootDir)
{
    std::map<std::string, std::vector<std::string>> result;
    DIR* d = opendir(rootDir.c_str());
    if (!d) return result;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string name(entry->d_name);
        std::string fullPath = rootDir + "/" + name;
        struct stat st;
        if (::stat(fullPath.c_str(), &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) continue;

        std::vector<std::string> names;
        std::set<std::string> seen;
        walkMdFiles(fullPath, names, seen);
        std::sort(names.begin(), names.end());
        result[name] = std::move(names);
    }
    closedir(d);
    return result;
}

std::string AddPersonaImage(const std::string& personaName,
                             const std::string& srcImagePath) {
    std::string dir = GetPersonasDir();
    if (dir.empty()) return "";

    mkdir(dir.c_str(), 0755);

    std::string norm = NormalizePersonaName(personaName);
    if (norm.empty()) return "";

    std::string ext = extLower(srcImagePath);
    if (ext.empty()) ext = "jpg";

    std::string destPath = dir + "/" + norm + "." + ext;

    std::ifstream src(srcImagePath, std::ios::binary);
    std::ofstream dst(destPath,     std::ios::binary | std::ios::trunc);
    if (!src || !dst) return "";
    dst << src.rdbuf();
    return destPath;
}

// ── Persona chat persistence ──────────────────────────────────────────────────

std::string GetPersonaChatsDir() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/story-teller/persona_chats" : "";
}

static std::string chatFilePath(const std::string& personaName,
                                const std::string& chatsDir) {
    std::string dir = chatsDir.empty() ? GetPersonaChatsDir() : chatsDir;
    if (dir.empty()) return "";
    return dir + "/" + NormalizePersonaName(personaName) + ".md";
}

std::vector<ConversationTurn> LoadPersonaConversation(
    const std::string& personaName, const std::string& chatsDir)
{
    std::string path = chatFilePath(personaName, chatsDir);
    if (path.empty()) return {};
    std::ifstream f(path);
    if (!f) return {};
    std::string body((std::istreambuf_iterator<char>(f)), {});
    return ParseConversation(body);
}

void SavePersonaConversation(
    const std::string& personaName,
    const std::vector<ConversationTurn>& turns,
    const std::string& chatsDir)
{
    std::string dir  = chatsDir.empty() ? GetPersonaChatsDir() : chatsDir;
    std::string path = chatFilePath(personaName, chatsDir);
    if (path.empty()) return;

    if (turns.empty()) {
        ::unlink(path.c_str());
        return;
    }

    mkdir(dir.c_str(), 0755);
    std::ofstream f(path, std::ios::trunc);
    if (f) f << SerializeConversationBody(turns);
}

void RenamePersonaChat(
    const std::string& oldName,
    const std::string& newName,
    const std::string& chatsDir)
{
    std::string oldPath = chatFilePath(oldName, chatsDir);
    std::string newPath = chatFilePath(newName, chatsDir);
    if (oldPath.empty() || newPath.empty() || oldPath == newPath) return;
    struct stat st;
    if (::stat(oldPath.c_str(), &st) == 0)
        ::rename(oldPath.c_str(), newPath.c_str());
}

bool RenamePersonaImage(const std::string& oldName,
                        const std::string& newName,
                        const std::string& personasDir) {
    std::string dir = personasDir.empty() ? GetPersonasDir() : personasDir;
    if (dir.empty()) return false;

    std::string oldNorm = NormalizePersonaName(oldName);
    std::string newNorm = NormalizePersonaName(newName);
    if (oldNorm.empty() || newNorm.empty()) return false;

    static const char* exts[] = {"jpg", "jpeg", "png", "gif", "webp", nullptr};
    for (int i = 0; exts[i]; ++i) {
        std::string src = dir + "/" + oldNorm + "." + exts[i];
        struct stat st;
        if (::stat(src.c_str(), &st) != 0) continue;
        std::string dst = dir + "/" + newNorm + "." + exts[i];
        return ::rename(src.c_str(), dst.c_str()) == 0;
    }
    return false;
}

// ── Group chats ───────────────────────────────────────────────────────────────

std::string GroupChatKey(const std::vector<std::string>& names) {
    std::vector<std::string> normalized;
    normalized.reserve(names.size());
    for (const auto& n : names)
        normalized.push_back(NormalizePersonaName(n));
    std::sort(normalized.begin(), normalized.end());
    std::string key;
    for (size_t i = 0; i < normalized.size(); ++i) {
        if (i > 0) key += '+';
        key += normalized[i];
    }
    return key;
}

std::string SerializeGroupChat(const std::vector<MultiChatTurn>& turns,
                               const std::vector<std::string>& participants) {
    std::ostringstream out;
    out << "Participants:";
    for (size_t i = 0; i < participants.size(); ++i)
        out << (i == 0 ? " " : ", ") << participants[i];
    out << "\n\n";
    for (const auto& t : turns) {
        out << "Q: " << t.userMessage << "\n";
        for (const auto& r : t.responses)
            out << r.first << ": " << r.second << "\n";
        out << "\n";
    }
    return out.str();
}

std::vector<MultiChatTurn> ParseGroupChat(const std::string& body,
                                          std::vector<std::string>& outParticipants) {
    outParticipants.clear();
    std::vector<MultiChatTurn> turns;
    std::istringstream ss(body);
    std::string line;
    bool headerDone = false;
    MultiChatTurn cur;
    bool inTurn = false;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (!headerDone) {
            if (line.rfind("Participants: ", 0) == 0) {
                std::istringstream ns(line.substr(14));
                std::string name;
                while (std::getline(ns, name, ',')) {
                    size_t start = name.find_first_not_of(' ');
                    size_t end   = name.find_last_not_of(' ');
                    if (start != std::string::npos)
                        outParticipants.push_back(name.substr(start, end - start + 1));
                }
            } else if (line.empty() && !outParticipants.empty()) {
                headerDone = true;
            }
            continue;
        }

        if (line.rfind("Q: ", 0) == 0) {
            if (inTurn) turns.push_back(cur);
            cur = {line.substr(3), {}};
            inTurn = true;
        } else if (inTurn && !line.empty()) {
            bool isSpeaker = false;
            for (const auto& p : outParticipants) {
                std::string prefix = p + ": ";
                if (line.rfind(prefix, 0) == 0) {
                    cur.responses.push_back({p, line.substr(prefix.size())});
                    isSpeaker = true;
                    break;
                }
            }
            if (!isSpeaker && !cur.responses.empty())
                cur.responses.back().second += "\n" + line;
        }
    }
    if (inTurn && !cur.userMessage.empty()) turns.push_back(cur);
    return turns;
}

void SaveGroupConversation(const std::vector<std::string>& participants,
                           const std::vector<MultiChatTurn>& turns,
                           const std::string& chatsDir) {
    std::string dir  = chatsDir.empty() ? GetPersonaChatsDir() : chatsDir;
    std::string key  = GroupChatKey(participants);
    std::string path = dir + "/group_" + key + ".md";

    if (turns.empty()) {
        ::unlink(path.c_str());
        return;
    }
    mkdir(dir.c_str(), 0755);
    std::ofstream f(path, std::ios::trunc);
    if (f) f << SerializeGroupChat(turns, participants);
}

std::vector<MultiChatTurn> LoadGroupConversation(const std::vector<std::string>& participants,
                                                  const std::string& chatsDir) {
    std::string dir  = chatsDir.empty() ? GetPersonaChatsDir() : chatsDir;
    std::string key  = GroupChatKey(participants);
    std::string path = dir + "/group_" + key + ".md";

    std::ifstream f(path);
    if (!f) return {};
    std::string content((std::istreambuf_iterator<char>(f)), {});
    std::vector<std::string> outParts;
    return ParseGroupChat(content, outParts);
}

std::vector<GroupChatInfo> ListGroupChats(const std::string& chatsDir) {
    std::string dir = chatsDir.empty() ? GetPersonaChatsDir() : chatsDir;
    std::vector<GroupChatInfo> result;

    DIR* d = opendir(dir.c_str());
    if (!d) return result;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string fname(entry->d_name);
        if (fname.rfind("group_", 0) != 0) continue;
        if (fname.size() < 10) continue;
        if (fname.compare(fname.size() - 3, 3, ".md") != 0) continue;

        std::string key  = fname.substr(6, fname.size() - 9); // strip "group_" + ".md"
        std::string path = dir + "/" + fname;
        std::ifstream f(path);
        if (!f) continue;
        std::string content((std::istreambuf_iterator<char>(f)), {});
        std::vector<std::string> parts;
        ParseGroupChat(content, parts);
        if (!parts.empty())
            result.push_back({key, parts});
    }
    closedir(d);
    return result;
}
