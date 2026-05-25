#include "quiz_tab_html.h"
#include <iostream>
#include <string>

int test_quiz_tab_html() {
    int failures = 0;

    // Helper: build a typical two-doc setup
    auto makeDocs = []() {
        std::vector<QuizTabDoc> docs;
        QuizTabDoc d1;
        d1.name = "chapter1.md";
        QuizTabQuestion q1; q1.num = 1; q1.rawBlock = "**Q1:** Capital of France?\n\nA) London  B) Paris\n\n**Answer:** B";
        QuizTabQuestion q2; q2.num = 2; q2.rawBlock = "**Q2:** H2O is?\n\nA) water  B) salt\n\n**Answer:** A";
        d1.questions = {q1, q2};
        QuizTabDoc d2;
        d2.name = "chapter2.md";
        // no questions
        docs = {d1, d2};
        return docs;
    };

    // DOCTYPE well-formed
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        if (html.find("<!DOCTYPE html>") == std::string::npos) {
            std::cerr << "FAIL [quiz-tab-doctype]\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-doctype]\n";
        }
    }

    // Dark mode class
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", true);
        if (html.find("class=\"dark\"") == std::string::npos) {
            std::cerr << "FAIL [quiz-tab-dark-class]\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-dark-class]\n";
        }
    }

    // Light mode must not carry dark class
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        if (html.find("class=\"dark\"") != std::string::npos) {
            std::cerr << "FAIL [quiz-tab-no-dark-class-in-light]\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-no-dark-class-in-light]\n";
        }
    }

    // Both doc names appear in the dropdown
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool has1 = html.find("chapter1.md") != std::string::npos;
        bool has2 = html.find("chapter2.md") != std::string::npos;
        if (!has1 || !has2) {
            std::cerr << "FAIL [quiz-tab-doc-selector]: has1=" << has1 << " has2=" << has2 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-doc-selector]\n";
        }
    }

    // Questions from selected doc are embedded in JS data
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool hasQ1 = html.find("Capital of France") != std::string::npos;
        bool hasQ2 = html.find("H2O is") != std::string::npos;
        if (!hasQ1 || !hasQ2) {
            std::cerr << "FAIL [quiz-tab-questions-embedded]: hasQ1=" << hasQ1
                      << " hasQ2=" << hasQ2 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-questions-embedded]\n";
        }
    }

    // Generate button present
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool hasBtn = html.find("generateQuiz") != std::string::npos ||
                      html.find("Generate Quiz") != std::string::npos;
        if (!hasBtn) {
            std::cerr << "FAIL [quiz-tab-generate-btn]\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-generate-btn]\n";
        }
    }

    // Extra instructions textarea present
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool hasTA = html.find("gen-extra") != std::string::npos ||
                     html.find("extra") != std::string::npos;
        if (!hasTA) {
            std::cerr << "FAIL [quiz-tab-extra-instructions]\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-extra-instructions]\n";
        }
    }

    // Script message handler registered for "quiztab"
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool hasHandler = html.find("quiztab") != std::string::npos;
        if (!hasHandler) {
            std::cerr << "FAIL [quiz-tab-message-handler]: 'quiztab' handler missing\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-message-handler]\n";
        }
    }

    // Edit and delete functions present
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool hasEdit   = html.find("editQ")   != std::string::npos;
        bool hasDelete = html.find("deleteQ") != std::string::npos;
        if (!hasEdit || !hasDelete) {
            std::cerr << "FAIL [quiz-tab-edit-delete]: hasEdit=" << hasEdit
                      << " hasDelete=" << hasDelete << "\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-edit-delete]\n";
        }
    }

    // removeQuiz function present (for removing entire quiz)
    {
        auto docs = makeDocs();
        std::string html = BuildQuizTabHTML(docs, "chapter1.md", false);
        bool hasRemove = html.find("removeQuiz") != std::string::npos;
        if (!hasRemove) {
            std::cerr << "FAIL [quiz-tab-remove-quiz]: 'removeQuiz' function missing\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-tab-remove-quiz]\n";
        }
    }

    return failures;
}
