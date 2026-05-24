#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

Logger& Logger::get() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    m_dir  = std::string(getenv("HOME") ?: "") + "/Library/Logs/StoryTeller";
    ::system(("mkdir -p \"" + m_dir + "\"").c_str());
    m_path = m_dir + "/story-teller-" + std::to_string(getpid()) + ".log";
    m_file.open(m_path, std::ios::app);
}

const std::string& Logger::filePath() const { return m_path; }
const std::string& Logger::logDir()   const { return m_dir; }

std::string Logger::savePrompt(const std::string& content) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", std::localtime(&t));
    std::string path = m_dir + "/prompt-" + ts
                     + "-" + std::to_string(getpid())
                     + "-" + std::to_string(++m_promptSeq) + ".txt";
    std::ofstream f(path);
    f << content;
    return f.good() ? path : "";
}

void Logger::log(const std::string& msg) {
    if (!m_file.is_open()) return;
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream line;
    line << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
         << "  " << msg << "\n";
    m_file << line.str();
    m_file.flush();
}
