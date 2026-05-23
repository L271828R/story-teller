#pragma once
#include <string>
#include <fstream>

// Appends timestamped lines to ~/Library/Logs/StoryTeller/story-teller-<PID>.log.
class Logger {
public:
    static Logger& get();
    void log(const std::string& msg);
    const std::string& filePath() const;
private:
    Logger();
    std::ofstream m_file;
    std::string   m_path;
};
