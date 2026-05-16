// tests/main.cpp
// Run with: cmake --build build --target test_mdviewer && ./build/test_mdviewer

#include <iostream>

int test_config();
int test_markdown();
int test_html_template();
int test_mdviewer();
int test_project();
int test_creator();
int test_editor();
int test_git_ops();
int test_llm_error();
int test_llm_response();
int test_conversation();
int test_meta();
int test_project_search();

int main() {
    int failures = 0;
    failures += test_config();
    failures += test_markdown();
    failures += test_html_template();
    failures += test_mdviewer();
    failures += test_project();
    failures += test_creator();
    failures += test_editor();
    failures += test_git_ops();
    failures += test_llm_error();
    failures += test_llm_response();
    failures += test_conversation();
    failures += test_meta();
    failures += test_project_search();
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILED") << "\n";
    return failures > 0 ? 1 : 0;
}
