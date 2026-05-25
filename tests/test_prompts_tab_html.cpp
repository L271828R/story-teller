#include "prompts_tab_html.h"
#include <iostream>
#include <string>

int test_prompts_tab_html() {
    int failures = 0;

    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [" << name << "]\n";
    };

    std::vector<SavedPrompt> prompts = {
        {1, "Python quiz",  "Create a quiz about python basics"},
        {2, "Essay prompt", "Write a comprehensive essay about..."},
    };

    std::string light = BuildPromptsTabHTML(prompts, false);
    std::string dark  = BuildPromptsTabHTML(prompts, true);
    std::string empty = BuildPromptsTabHTML({}, false);

    check("prompts-tab-doctype",
          light.find("<!DOCTYPE html>") != std::string::npos);

    check("prompts-tab-dark-class",
          dark.find("class=\"dark\"") != std::string::npos);

    check("prompts-tab-no-dark-in-light",
          light.find("class=\"dark\"") == std::string::npos);

    // Prompt titles and text appear in the rendered HTML
    check("prompts-tab-titles",
          light.find("Python quiz")  != std::string::npos &&
          light.find("Essay prompt") != std::string::npos);

    check("prompts-tab-text-content",
          light.find("Create a quiz about python") != std::string::npos);

    // Message handler registered for "promptstab"
    check("prompts-tab-message-handler",
          light.find("promptstab") != std::string::npos);

    // Delete action present
    check("prompts-tab-delete",
          light.find("deletePrompt") != std::string::npos);

    // Edit/save action present
    check("prompts-tab-edit",
          light.find("editPrompt") != std::string::npos ||
          light.find("savePrompt") != std::string::npos);

    // Add new prompt form present
    check("prompts-tab-add-form",
          light.find("addPrompt")  != std::string::npos ||
          light.find("new-title")  != std::string::npos ||
          light.find("newPrompt")  != std::string::npos);

    // Empty state shows a hint rather than crashing
    check("prompts-tab-empty",
          empty.find("<!DOCTYPE html>") != std::string::npos);

    return failures;
}
