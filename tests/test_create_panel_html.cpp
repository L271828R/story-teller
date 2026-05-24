#include "../src/create_panel_html.h"
#include <iostream>
#include <string>

int test_create_panel_html() {
    int failures = 0;

    std::string light = BuildCreatePanelHTML(false);
    std::string dark  = BuildCreatePanelHTML(true);

    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [" << name << "]\n";
    };

    check("create-html-doctype",
          light.find("<!DOCTYPE html>") != std::string::npos);

    check("create-html-project-select",
          light.find("id=\"projectChoice\"") != std::string::npos);

    check("create-html-topic-textarea",
          light.find("id=\"topic\"") != std::string::npos);

    check("create-html-style-input",
          light.find("id=\"style\"") != std::string::npos &&
          light.find("id=\"styleList\"") != std::string::npos);

    check("create-html-characters",
          light.find("id=\"catList\"") != std::string::npos &&
          light.find("id=\"charList\"") != std::string::npos);

    check("create-html-backend-select",
          light.find("id=\"backend\"") != std::string::npos &&
          light.find("Anthropic API") != std::string::npos);

    check("create-html-generate-btn",
          light.find("id=\"generateBtn\"") != std::string::npos);

    check("create-html-file-table",
          light.find("id=\"fileTable\"") != std::string::npos &&
          light.find("id=\"fileBody\"")  != std::string::npos);

    // Translate language picker includes English and Chinese
    check("create-html-translate-langs",
          light.find("id=\"translateLang\"") != std::string::npos &&
          light.find(">English<")            != std::string::npos &&
          light.find(">Chinese (Mandarin)<") != std::string::npos);

    // Dark mode: body has 'dark' class only when requested
    check("create-html-dark-mode",
          dark.find("class=\"dark\"")  != std::string::npos &&
          light.find("class=\"dark\"") == std::string::npos);

    // All JS postMessage calls go through the 'create' handler
    check("create-html-message-handler",
          light.find("messageHandlers.create") != std::string::npos);

    // Generate button calls generate()
    check("create-html-generate-action",
          light.find("generate()") != std::string::npos);

    return failures;
}
