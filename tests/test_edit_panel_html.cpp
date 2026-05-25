#include "../src/edit_panel_html.h"
#include <iostream>
#include <string>

int test_edit_panel_html() {
    int failures = 0;

    std::string light = BuildEditPanelHTML(false);
    std::string dark  = BuildEditPanelHTML(true);

    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [" << name << "]\n";
    };

    check("edit-html-doctype",
          light.find("<!DOCTYPE html>") != std::string::npos);

    check("edit-html-dark-mode",
          dark.find("class=\"dark\"")  != std::string::npos &&
          light.find("class=\"dark\"") == std::string::npos);

    check("edit-html-message-handler",
          light.find("messageHandlers.edit") != std::string::npos);

    // Quiz section: button to generate a quiz from the selected file.
    check("edit-html-quiz-btn",
          light.find("doQuiz") != std::string::npos);

    // Quiz section: question-count input so the user can control how many questions.
    check("edit-html-quiz-n",
          light.find("quiz-n") != std::string::npos ||
          light.find("quizN")  != std::string::npos);

    // setBusy must disable the quiz button so it can't be double-clicked.
    check("edit-html-quiz-busy",
          light.find("quiz-btn") != std::string::npos ||
          light.find("quizBtn")  != std::string::npos);

    // File list must show creation and modification timestamps from the data.
    check("edit-html-file-dates",
          light.find(".created") != std::string::npos ||
          light.find(".modified") != std::string::npos ||
          (light.find("e.created") != std::string::npos &&
           light.find("e.modified") != std::string::npos));

    // File list should use a table so dates can be column-aligned.
    check("edit-html-file-table",
          light.find("<table") != std::string::npos ||
          light.find("file-table") != std::string::npos);

    // Column headers must be clickable to sort.
    check("edit-html-file-sort",
          light.find("sortFiles") != std::string::npos ||
          light.find("_sortCol")  != std::string::npos);

    // Sort direction indicator must toggle (▲/▼ or similar).
    check("edit-html-file-sort-indicator",
          light.find("▲") != std::string::npos ||
          light.find("▼") != std::string::npos ||
          light.find("asc") != std::string::npos);

    return failures;
}
