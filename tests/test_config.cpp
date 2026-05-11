#include "config.h"
#include <iostream>

int test_config() {
    int failures = 0;

    // ParseState reads currentProject.
    {
        AppState st = ParseState("currentProject = my-story\n");
        if (st.currentProject != "my-story") {
            std::cerr << "FAIL [parse-state-project]: got '" << st.currentProject << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-state-project]\n";
        }
    }

    // ParseState returns empty when key absent.
    {
        AppState st = ParseState("# nothing\n");
        if (!st.currentProject.empty()) {
            std::cerr << "FAIL [parse-state-empty]: got '" << st.currentProject << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-state-empty]\n";
        }
    }

    // ParseState trims whitespace around = and value.
    {
        AppState st = ParseState("currentProject=space station\n");
        if (st.currentProject != "space station") {
            std::cerr << "FAIL [parse-state-trim]: got '" << st.currentProject << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-state-trim]\n";
        }
    }

    return failures;
}
