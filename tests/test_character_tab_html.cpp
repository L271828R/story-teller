#include "../src/character_tab_html.h"
#include <iostream>
#include <string>

int test_character_tab_html() {
    int failures = 0;

    std::string light = BuildCharacterTabHTML(false);
    std::string dark  = BuildCharacterTabHTML(true);

    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [" << name << "]\n";
    };

    check("char-tab-doctype",
          light.find("<!DOCTYPE html>") != std::string::npos);

    check("char-tab-search",
          light.find("id=\"search\"") != std::string::npos);

    check("char-tab-grid",
          light.find("id=\"grid\"") != std::string::npos);

    check("char-tab-add-card",
          light.find("add-card") != std::string::npos);

    check("char-tab-message-handler",
          light.find("messageHandlers.characters") != std::string::npos);

    check("char-tab-setCharacters",
          light.find("setCharacters") != std::string::npos);

    check("char-tab-dark-mode",
          dark.find("class=\"dark\"")  != std::string::npos &&
          light.find("class=\"dark\"") == std::string::npos);

    check("char-tab-toggle",
          light.find("toggleChar") != std::string::npos);

    check("char-tab-desc-textarea",
          light.find("desc-ta") != std::string::npos ||
          light.find("saveDesc") != std::string::npos);

    check("char-tab-filter-pills",
          light.find("setFilter") != std::string::npos);

    return failures;
}
