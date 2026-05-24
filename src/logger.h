#pragma once
#include <string>
#include <fstream>

// Appends timestamped lines to ~/Library/Logs/StoryTeller/story-teller-<PID>.log.
// Prompts can be saved alongside the log via savePrompt().
class Logger {
public:
    static Logger& get();
    void log(const std::string& msg);
    const std::string& filePath() const;
    const std::string& logDir() const;
    // Saves content to <logDir>/prompt-<timestamp>-<PID>.txt and returns the path.
    std::string savePrompt(const std::string& content);
private:
    Logger();
    std::ofstream m_file;
    std::string   m_path;
    std::string   m_dir;
    int           m_promptSeq = 0;
};
