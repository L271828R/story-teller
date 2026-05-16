#pragma once
#include <string>
#include <vector>

struct LLMTiming {
    std::string timestamp;       // "YYYY-MM-DDTHH:MM:SS"
    std::string operation;       // "generate", "patch", "translate", "chat"
    std::string topic;
    int         durationSeconds = 0;
};

struct ProjectMeta {
    std::string            lastOpened;
    std::vector<LLMTiming> timings;
};

// Current local time as "YYYY-MM-DDTHH:MM:SS".
std::string MetaNow();

// Read/write .storyteller.json in projectDir.
ProjectMeta LoadProjectMeta(const std::string& projectDir);
void        SaveProjectMeta(const std::string& projectDir, const ProjectMeta& meta);

// Update lastOpened to now.
void RecordOpen(const std::string& projectDir);

// Append one LLM timing entry; keeps the most recent 100.
void RecordLLMTiming(const std::string& projectDir,
                     const std::string& operation,
                     const std::string& topic,
                     int durationSeconds);
