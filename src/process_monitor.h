#pragma once
#include <string>
#include <vector>

struct SpawnedProcess {
    int         pid;
    int         ppid;
    std::string elapsed;    // e.g. "03:42" or "1-02:03:04"
    std::string promptFile; // path to the temp prompt .txt
    std::string project;    // e.g. "History/Darwin"
    std::string action;     // e.g. "translate → French" or "generate"
    std::string backend;    // e.g. "claude -p" or "Codex CLI"
    bool        orphaned;   // ppid == 1 (parent story-teller died)
};

// Scans ps output for claude -p processes spawned by story-teller.
std::vector<SpawnedProcess> ScanSpawnedProcesses();

// Sends SIGTERM to pid (and its process group); returns true if delivered.
bool KillSpawnedProcess(int pid);

// Scans for orphaned LLM processes (ppid == 1) and kills them.
// Returns the number of processes killed. Safe to call at startup.
int KillOrphanedProcesses();
