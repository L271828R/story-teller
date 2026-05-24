#include "process_monitor.h"
#include <csignal>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

// Read the first N bytes of a file.
static std::string read_head(const std::string& path, size_t n = 200) {
    std::ifstream f(path);
    std::string buf(n, '\0');
    f.read(&buf[0], static_cast<std::streamsize>(n));
    buf.resize(static_cast<size_t>(f.gcount()));
    return buf;
}

// Read a key=value text file, return map of entries.
static std::map<std::string, std::string> read_meta(const std::string& path) {
    std::map<std::string, std::string> m;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq != std::string::npos)
            m[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return m;
}

// Classify and summarise what the prompt is doing (fallback, no .meta file).
static std::string classify_prompt(const std::string& path) {
    std::string head = read_head(path);
    if (head.find("Translate") == 0) {
        auto into = head.find("into ");
        auto dot  = head.find(".\n", into);
        if (into != std::string::npos && dot != std::string::npos)
            return "translate \xe2\x86\x92 " + head.substr(into + 5, dot - into - 5);
        return "translate";
    }
    return "generate";
}

// Extract the story-teller_prompt_*.txt path from a ps command string.
// Codex commands contain two occurrences (output .codex.md + input .txt);
// we prefer the .txt input file because that is where the .meta lives.
static std::string extract_prompt_path(const std::string& cmd) {
    const std::string marker = "story-teller_prompt_";
    std::string fallback;
    size_t search = 0;
    while (true) {
        auto pos = cmd.find(marker, search);
        if (pos == std::string::npos) break;
        size_t start = pos;
        while (start > 0 && cmd[start - 1] != '\'' && cmd[start - 1] != ' ') --start;
        size_t end = cmd.find_first_of("' ", pos);
        if (end == std::string::npos) end = cmd.size();
        std::string candidate = cmd.substr(start, end - start);
        if (candidate.size() >= 4 &&
                candidate.compare(candidate.size() - 4, 4, ".txt") == 0)
            return candidate;   // exact match — done
        if (fallback.empty()) fallback = candidate;
        search = end;
    }
    return fallback;
}

std::vector<SpawnedProcess> ScanSpawnedProcesses() {
    std::vector<SpawnedProcess> result;

    // ps -eo pid,ppid,etime,command — etime format: [[DD-]HH:]MM:SS
    FILE* pipe = popen("ps -eo pid,ppid,etime,command 2>/dev/null", "r");
    if (!pipe) return result;

    char buf[2048];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::string line(buf);
        if (line.find("story-teller_prompt_") == std::string::npos) continue;
        if (line.find("bash -l -c") == std::string::npos) continue;

        std::istringstream iss(line);
        int pid = 0, ppid = 0;
        std::string etime, rest;
        if (!(iss >> pid >> ppid >> etime)) continue;
        std::getline(iss, rest);

        // rest has a leading space; skip sh -c wrappers (which also contain
        // "bash -l -c") by requiring the command itself to start with "bash".
        auto cmdStart = rest.find_first_not_of(' ');
        if (cmdStart == std::string::npos) continue;
        if (rest.compare(cmdStart, 4, "bash") != 0) continue;

        // rest is the full command string; find the prompt file in it.
        std::string promptFile = extract_prompt_path(rest);
        if (promptFile.empty()) continue;

        SpawnedProcess p;
        p.pid        = pid;
        p.ppid       = ppid;
        p.elapsed    = etime;
        p.promptFile = promptFile;
        p.orphaned   = (ppid == 1);

        auto meta = read_meta(promptFile + ".meta");
        p.project = meta.count("project") ? meta["project"] : "";
        p.backend = meta.count("backend") ? meta["backend"] : "";
        p.action  = meta.count("action")  ? meta["action"]
                                          : classify_prompt(promptFile);
        result.push_back(p);
    }
    pclose(pipe);
    return result;
}

bool KillSpawnedProcess(int pid) {
    if (pid <= 1) return false;
    // Kill the entire process group so the LLM child (claude -p, codex, etc.)
    // is also terminated and doesn't become an orphan.
    ::killpg(pid, SIGTERM);
    // Fall back to killing just the pid if it's not a group leader.
    return ::kill(pid, SIGTERM) == 0;
}

int KillOrphanedProcesses() {
    int count = 0;
    for (const auto& p : ScanSpawnedProcesses()) {
        if (p.orphaned && KillSpawnedProcess(p.pid))
            ++count;
    }
    return count;
}
