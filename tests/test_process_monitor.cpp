#include "../src/process_monitor.h"
#include "../src/monitor_panel_html.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

int test_process_monitor() {
    int failures = 0;

    // ScanSpawnedProcesses returns a vector (possibly empty) without crashing.
    {
        auto procs = ScanSpawnedProcesses();
        // Just verify it ran — result depends on what's currently running.
        std::cout << "PASS [scan-spawned-no-crash]  (" << procs.size() << " found)\n";
    }

    // KillSpawnedProcess refuses pid <= 1.
    {
        bool refused = !KillSpawnedProcess(0) && !KillSpawnedProcess(1);
        if (!refused) {
            std::cerr << "FAIL [kill-refuses-pid-one]\n";
            ++failures;
        } else {
            std::cout << "PASS [kill-refuses-pid-one]\n";
        }
    }

    // KillSpawnedProcess can terminate a real process.
    {
        // Spawn a harmless sleep process.
        pid_t child = fork();
        if (child == 0) {
            // Child: sleep indefinitely.
            ::sleep(9999);
            _exit(0);
        }
        bool killed = KillSpawnedProcess(static_cast<int>(child));
        // Reap to avoid zombie.
        ::waitpid(child, nullptr, 0);
        if (!killed) {
            std::cerr << "FAIL [kill-live-process]\n";
            ++failures;
        } else {
            std::cout << "PASS [kill-live-process]\n";
        }
    }

    // KillOrphanedProcesses returns without crashing (count >= 0).
    {
        int killed = KillOrphanedProcesses();
        if (killed < 0) {
            std::cerr << "FAIL [kill-orphaned-no-crash]\n";
            ++failures;
        } else {
            std::cout << "PASS [kill-orphaned-no-crash]  (" << killed << " killed)\n";
        }
    }

    // Monitor HTML must include a timing log pane with setTimingLog JS function.
    {
        std::string html = BuildMonitorPanelHTML(false);
        bool hasPane       = html.find("timing-log") != std::string::npos ||
                             html.find("timingLog")   != std::string::npos ||
                             html.find("timing_log")  != std::string::npos;
        bool hasSetFn      = html.find("setTimingLog") != std::string::npos;
        bool hasBackendCol = html.find("Backend")      != std::string::npos ||
                             html.find("backend")      != std::string::npos;
        if (!hasPane || !hasSetFn || !hasBackendCol) {
            std::cerr << "FAIL [monitor-timing-log]: hasPane=" << hasPane
                      << " hasSetFn=" << hasSetFn
                      << " hasBackendCol=" << hasBackendCol << "\n";
            ++failures;
        } else {
            std::cout << "PASS [monitor-timing-log]\n";
        }
    }

    // Monitor HTML must have archive button and show-archived toggle in timing log.
    {
        std::string html = BuildMonitorPanelHTML(false);
        bool hasArchiveAction = html.find("archiveTiming") != std::string::npos;
        bool hasShowArchived  = html.find("showArchived")  != std::string::npos ||
                                html.find("show-archived") != std::string::npos ||
                                html.find("show_archived") != std::string::npos;
        if (!hasArchiveAction || !hasShowArchived) {
            std::cerr << "FAIL [monitor-archive-ui]: hasArchiveAction=" << hasArchiveAction
                      << " hasShowArchived=" << hasShowArchived << "\n";
            ++failures;
        } else {
            std::cout << "PASS [monitor-archive-ui]\n";
        }
    }

    return failures;
}
