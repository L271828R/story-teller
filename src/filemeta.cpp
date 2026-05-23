#include "filemeta.h"
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

static const std::string kPrefix = "<!-- language: ";
static const std::string kSuffix = " -->";

std::string ReadLanguage(const std::string& filePath) {
    std::ifstream f(filePath);
    std::string line;
    int linesChecked = 0;
    while (std::getline(f, line) && linesChecked < 5) {
        ++linesChecked;
        if (line.rfind(kPrefix, 0) == 0 && line.size() > kPrefix.size() + kSuffix.size()) {
            std::size_t end = line.rfind(kSuffix);
            if (end != std::string::npos)
                return line.substr(kPrefix.size(), end - kPrefix.size());
        }
    }
    return "";
}

bool WriteLanguage(const std::string& filePath, const std::string& language) {
    // Read existing content.
    std::ifstream in(filePath);
    if (!in.is_open()) return false;
    std::string content{std::istreambuf_iterator<char>(in), {}};
    in.close();

    std::string newComment = kPrefix + language + kSuffix + "\n";

    // Replace existing comment if present; otherwise prepend.
    auto pos = content.find(kPrefix);
    if (pos != std::string::npos) {
        auto end = content.find('\n', pos);
        if (end == std::string::npos) end = content.size();
        content.replace(pos, end - pos, kPrefix + language + kSuffix);
    } else {
        content = newComment + content;
    }

    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open()) return false;
    out << content;
    return out.good();
}

static std::string format_time(std::time_t t) {
    std::tm* tm = std::localtime(&t);
    if (!tm) return "";
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    return buf;
}

std::string FileModifiedTime(const std::string& filePath) {
    struct stat st;
    if (::stat(filePath.c_str(), &st) != 0) return "";
    return format_time(st.st_mtime);
}

std::string FileCreatedTime(const std::string& filePath) {
    struct stat st;
    if (::stat(filePath.c_str(), &st) != 0) return "";
#ifdef __APPLE__
    return format_time(st.st_birthtime);
#else
    return format_time(st.st_mtime);
#endif
}
