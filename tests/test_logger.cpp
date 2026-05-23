#include "../src/logger.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

int test_logger() {
    int failures = 0;

    // Log file path must contain the current PID.
    {
        std::string path = Logger::get().filePath();
        std::string pid  = std::to_string(getpid());
        if (path.find(pid) == std::string::npos) {
            std::cerr << "FAIL [logger-pid-in-path]: path '" << path
                      << "' does not contain PID " << pid << "\n";
            ++failures;
        } else {
            std::cout << "PASS [logger-pid-in-path]\n";
        }
    }

    // Logger writes to the PID-named file.
    {
        const std::string marker = "test-logger-pid-marker-xyz";
        Logger::get().log(marker);

        std::string path = Logger::get().filePath();
        std::ifstream f(path);
        bool found = false;
        std::string line;
        while (std::getline(f, line))
            if (line.find(marker) != std::string::npos) { found = true; break; }

        if (!found) {
            std::cerr << "FAIL [logger-writes-pid-file]: marker not found in " << path << "\n";
            ++failures;
        } else {
            std::cout << "PASS [logger-writes-pid-file]\n";
        }
    }

    return failures;
}
