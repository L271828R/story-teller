#pragma once
#include <string>
#include <fstream>

// Appends timestamped lines to ~/Library/Logs/StoryTeller/story-teller.log.
class Logger {
public:
    static Logger& get();
    void log(const std::string& msg);
private:
    Logger();
    std::ofstream m_file;
};
