#include "../src/process_monitor.h"
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

    return failures;
}
