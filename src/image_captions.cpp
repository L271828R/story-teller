#include "image_captions.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static const std::string kCaptionsFile = ".image_captions.json";

// Minimal JSON string escaper.
static std::string jsonEscape(const std::string& s) {
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

// Minimal JSON string field extractor: finds "key":"value" pairs.
static std::map<std::string,std::string> parseJsonObject(const std::string& json) {
    std::map<std::string,std::string> out;
    size_t i = 0;
    const size_t n = json.size();

    auto skipWS = [&]() { while (i < n && (json[i]==' '||json[i]=='\t'||json[i]=='\n'||json[i]=='\r')) ++i; };

    auto readString = [&]() -> std::string {
        if (i >= n || json[i] != '"') return "";
        ++i;
        std::string val;
        while (i < n) {
            char c = json[i++];
            if (c == '"') break;
            if (c == '\\' && i < n) {
                char e = json[i++];
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
    };

    skipWS();
    if (i < n && json[i] == '{') ++i;

    while (i < n) {
        skipWS();
        if (i >= n || json[i] == '}') break;
        if (json[i] == ',') { ++i; continue; }
        if (json[i] != '"') { ++i; continue; }

        std::string key = readString();
        skipWS();
        if (i < n && json[i] == ':') ++i;
        skipWS();
        std::string val = readString();
        if (!key.empty()) out[key] = val;
    }
    return out;
}

static std::string captionsPath(const std::string& projectDir) {
    return (fs::path(projectDir) / kCaptionsFile).string();
}

static std::string serializeMap(const std::map<std::string,std::string>& m) {
    std::ostringstream ss;
    ss << "{\n";
    bool first = true;
    for (const auto& kv : m) {
        if (!first) ss << ",\n";
        first = false;
        ss << "  \"" << jsonEscape(kv.first) << "\": \""
           << jsonEscape(kv.second) << "\"";
    }
    ss << "\n}\n";
    return ss.str();
}

std::map<std::string,std::string> LoadImageCaptions(const std::string& projectDir) {
    std::ifstream f(captionsPath(projectDir));
    if (!f) return {};
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return parseJsonObject(content);
}

void SaveImageCaption(const std::string& projectDir,
                       const std::string& filename,
                       const std::string& caption) {
    auto caps = LoadImageCaptions(projectDir);
    caps[filename] = caption;
    std::ofstream f(captionsPath(projectDir), std::ios::trunc);
    if (f) f << serializeMap(caps);
}

std::string GetImageCaption(const std::string& projectDir,
                              const std::string& filename) {
    auto caps = LoadImageCaptions(projectDir);
    auto it = caps.find(filename);
    return it != caps.end() ? it->second : "";
}
