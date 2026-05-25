#pragma once
#include <string>
#include <vector>

struct LLMTiming {
    std::string timestamp;       // "YYYY-MM-DDTHH:MM:SS"
    std::string operation;       // "generate", "patch", "translate", "chat"
    std::string topic;
    int         durationSeconds = 0;
    std::string backend;
    bool        archived        = false;
    std::string project;         // populated by ScanAllTimings, empty otherwise
};

struct ProjectMeta {
    std::string            created;
    std::string            lastOpened;
    std::string            source;
    std::vector<LLMTiming> timings;
};

// Current local time as "YYYY-MM-DDTHH:MM:SS".
std::string MetaNow();

// Read/write .storyteller.json in projectDir.
ProjectMeta LoadProjectMeta(const std::string& projectDir);
void        SaveProjectMeta(const std::string& projectDir, const ProjectMeta& meta);

// Update lastOpened to now.
void RecordOpen(const std::string& projectDir);

// Populate stable project metadata when first seen.
void EnsureProjectMeta(const std::string& projectDir, const std::string& source = "");

// Record the LLM/backend source associated with the project.
void RecordProjectSource(const std::string& projectDir, const std::string& source);

// Append one LLM timing entry; keeps the most recent 100.
void RecordLLMTiming(const std::string& projectDir,
                     const std::string& operation,
                     const std::string& topic,
                     int durationSeconds,
                     const std::string& backend = "");

// Collect timings from all project subdirectories under defaultFolder,
// sorted newest-first. Each entry has its project field set to the folder name.
std::vector<LLMTiming> ScanAllTimings(const std::string& defaultFolder);

// Set the archived flag on the timing entry identified by (project, timestamp).
// project is a path relative to defaultFolder (e.g. "History/Mozart").
void ArchiveTimingEntry(const std::string& defaultFolder,
                        const std::string& project,
                        const std::string& timestamp,
                        bool archived);
