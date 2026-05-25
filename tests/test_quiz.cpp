#include "quiz.h"
#include <iostream>
#include <string>

int test_quiz() {
    int failures = 0;

    // [strip-tidbits-single] — removes a single tidbit block
    {
        std::string md =
            "## Chapter 1\n\n"
            "Some prose here.\n\n"
            ":::tidbit[Einstein]\n"
            "E=mc²\n"
            ":::\n\n"
            "More prose.\n";
        std::string result = StripTidbits(md);
        bool noTidbit  = result.find(":::tidbit") == std::string::npos;
        bool hasProse  = result.find("Some prose here") != std::string::npos;
        bool hasMore   = result.find("More prose") != std::string::npos;
        bool noEinstein = result.find("Einstein") == std::string::npos;
        if (!noTidbit || !hasProse || !hasMore || !noEinstein) {
            std::cerr << "FAIL [strip-tidbits-single]: noTidbit=" << noTidbit
                      << " hasProse=" << hasProse << " hasMore=" << hasMore
                      << " noEinstein=" << noEinstein
                      << "\n  result: '" << result << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [strip-tidbits-single]\n";
        }
    }

    // [strip-tidbits-multiple] — removes multiple tidbit blocks
    {
        std::string md =
            "## Chapter 1\n\n"
            "Prose A.\n\n"
            ":::tidbit[Darwin]\n"
            "Natural selection.\n"
            ":::\n\n"
            "## Chapter 2\n\n"
            "Prose B.\n\n"
            ":::tidbit[Newton]\n"
            "Gravity.\n"
            ":::\n\n"
            "End.\n";
        std::string result = StripTidbits(md);
        bool noTidbits = result.find(":::tidbit") == std::string::npos;
        bool hasA      = result.find("Prose A") != std::string::npos;
        bool hasB      = result.find("Prose B") != std::string::npos;
        bool noNewton  = result.find("Newton") == std::string::npos;
        if (!noTidbits || !hasA || !hasB || !noNewton) {
            std::cerr << "FAIL [strip-tidbits-multiple]: noTidbits=" << noTidbits
                      << " hasA=" << hasA << " hasB=" << hasB
                      << " noNewton=" << noNewton << "\n";
            ++failures;
        } else {
            std::cout << "PASS [strip-tidbits-multiple]\n";
        }
    }

    // [strip-tidbits-none] — no-op when no tidbits present
    {
        std::string md = "## Chapter 1\n\nJust prose.\n";
        std::string result = StripTidbits(md);
        if (result != md) {
            std::cerr << "FAIL [strip-tidbits-none]: document changed when it shouldn't\n";
            ++failures;
        } else {
            std::cout << "PASS [strip-tidbits-none]\n";
        }
    }

    // [build-quiz-prompt-n] — prompt includes the document and question count
    {
        std::string md = "## Chapter 1\n\nSome prose.\n";
        std::string prompt = BuildQuizPrompt(md, 5);
        bool hasDoc  = prompt.find("Some prose") != std::string::npos;
        bool hasN    = prompt.find("5") != std::string::npos;
        bool hasABC  = prompt.find("A)") != std::string::npos
                    || prompt.find("A.") != std::string::npos
                    || prompt.find("multiple choice") != std::string::npos
                    || prompt.find("multiple-choice") != std::string::npos;
        if (!hasDoc || !hasN || !hasABC) {
            std::cerr << "FAIL [build-quiz-prompt-n]: hasDoc=" << hasDoc
                      << " hasN=" << hasN << " hasABC=" << hasABC
                      << "\n  prompt: '" << prompt.substr(0, 300) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [build-quiz-prompt-n]\n";
        }
    }

    // [build-quiz-prompt-extra] — extra instructions are included in the prompt
    {
        std::string md = "## Chapter 1\n\nSome prose.\n";
        std::string prompt = BuildQuizPrompt(md, 4, "Write the quiz in Spanish.");
        bool hasExtra = prompt.find("Write the quiz in Spanish") != std::string::npos;
        bool hasDoc   = prompt.find("Some prose") != std::string::npos;
        if (!hasExtra || !hasDoc) {
            std::cerr << "FAIL [build-quiz-prompt-extra]: hasExtra=" << hasExtra
                      << " hasDoc=" << hasDoc
                      << "\n  prompt: '" << prompt.substr(0, 300) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [build-quiz-prompt-extra]\n";
        }
    }

    // [build-quiz-prompt-extra-empty] — empty extra instructions adds no garbage
    {
        std::string md = "## Chapter 1\n\nSome prose.\n";
        std::string promptWith    = BuildQuizPrompt(md, 3, "");
        std::string promptWithout = BuildQuizPrompt(md, 3);
        if (promptWith != promptWithout) {
            std::cerr << "FAIL [build-quiz-prompt-extra-empty]: empty extra instructions"
                      << " changed the prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-quiz-prompt-extra-empty]\n";
        }
    }

    // [build-quiz-prompt-no-instructions] — prompt must NOT contain LLM syntax instructions
    {
        std::string md = "## Chapter 1\n\nSome prose.\n";
        std::string prompt = BuildQuizPrompt(md, 3);
        bool noTidbitSyntax = prompt.find(":::tidbit") == std::string::npos;
        bool noPersona      = prompt.find("persona") == std::string::npos
                           && prompt.find("character") == std::string::npos;
        if (!noTidbitSyntax || !noPersona) {
            std::cerr << "FAIL [build-quiz-prompt-no-instructions]:"
                      << " noTidbitSyntax=" << noTidbitSyntax
                      << " noPersona=" << noPersona << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-quiz-prompt-no-instructions]\n";
        }
    }

    // [append-quiz-fresh] — appends Quiz section when none exists
    {
        std::string md =
            "# My Book\n\n"
            "## Chapter 1\n\n"
            "Some text.\n";
        std::string quiz =
            "**Q1:** What is 2+2?\n\n"
            "A) 3  B) 4  C) 5  D) 6\n\n"
            "**Answer:** B\n";
        std::string result = AppendQuizToMarkdown(md, quiz);
        bool hasOriginal = result.find("Some text") != std::string::npos;
        bool hasSection  = result.find("## Quiz") != std::string::npos;
        bool hasQ1       = result.find("Q1") != std::string::npos;
        bool quizAtEnd   = result.rfind("## Quiz") > result.rfind("Some text");
        if (!hasOriginal || !hasSection || !hasQ1 || !quizAtEnd) {
            std::cerr << "FAIL [append-quiz-fresh]: hasOriginal=" << hasOriginal
                      << " hasSection=" << hasSection << " hasQ1=" << hasQ1
                      << " quizAtEnd=" << quizAtEnd << "\n";
            ++failures;
        } else {
            std::cout << "PASS [append-quiz-fresh]\n";
        }
    }

    // [append-quiz-replaces-existing] — replaces existing Quiz section
    {
        std::string md =
            "# My Book\n\n"
            "## Chapter 1\n\n"
            "Some text.\n\n"
            "## Quiz\n\n"
            "**Q1:** Old question?\n\n"
            "A) Old  B) Older\n";
        std::string quiz =
            "**Q1:** New question?\n\n"
            "A) New  B) Newer\n";
        std::string result = AppendQuizToMarkdown(md, quiz);
        bool hasNew  = result.find("New question") != std::string::npos;
        bool noOld   = result.find("Old question") == std::string::npos;
        // exactly one ## Quiz heading
        size_t first = result.find("## Quiz");
        bool oneQuiz = first != std::string::npos
                    && result.find("## Quiz", first + 1) == std::string::npos;
        if (!hasNew || !noOld || !oneQuiz) {
            std::cerr << "FAIL [append-quiz-replaces-existing]: hasNew=" << hasNew
                      << " noOld=" << noOld << " oneQuiz=" << oneQuiz << "\n";
            ++failures;
        } else {
            std::cout << "PASS [append-quiz-replaces-existing]\n";
        }
    }

    // [parse-quiz-questions-empty] — no quiz section → empty vector
    {
        std::string md = "## Chapter 1\n\nSome text.\n";
        auto qs = ParseQuizQuestions(md);
        if (!qs.empty()) {
            std::cerr << "FAIL [parse-quiz-questions-empty]: expected 0 questions, got "
                      << qs.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-quiz-questions-empty]\n";
        }
    }

    // [parse-quiz-questions-one] — single question parsed correctly
    {
        std::string md =
            "## Chapter 1\n\nSome text.\n\n"
            "## Quiz\n\n"
            "**Q1:** What is 2+2?\n\n"
            "A) 3  B) 4  C) 5  D) 6\n\n"
            "**Answer:** B\n";
        auto qs = ParseQuizQuestions(md);
        bool hasOne  = qs.size() == 1;
        bool num1    = hasOne && qs[0].num == 1;
        bool hasText = hasOne && qs[0].rawBlock.find("What is 2+2") != std::string::npos;
        bool hasAns  = hasOne && qs[0].rawBlock.find("Answer") != std::string::npos;
        if (!hasOne || !num1 || !hasText || !hasAns) {
            std::cerr << "FAIL [parse-quiz-questions-one]: hasOne=" << hasOne
                      << " num1=" << num1 << " hasText=" << hasText
                      << " hasAns=" << hasAns << "\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-quiz-questions-one]\n";
        }
    }

    // [parse-quiz-questions-two] — two questions, correct nums and content
    {
        std::string md =
            "## Quiz\n\n"
            "**Q1:** First question?\n\n"
            "A) a  B) b  C) c  D) d\n\n"
            "**Answer:** A\n\n"
            "**Q2:** Second question?\n\n"
            "A) w  B) x  C) y  D) z\n\n"
            "**Answer:** C\n";
        auto qs = ParseQuizQuestions(md);
        bool twoQs   = qs.size() == 2;
        bool num12   = twoQs && qs[0].num == 1 && qs[1].num == 2;
        bool hasQ1   = twoQs && qs[0].rawBlock.find("First question") != std::string::npos;
        bool hasQ2   = twoQs && qs[1].rawBlock.find("Second question") != std::string::npos;
        if (!twoQs || !num12 || !hasQ1 || !hasQ2) {
            std::cerr << "FAIL [parse-quiz-questions-two]: twoQs=" << twoQs
                      << " num12=" << num12 << " hasQ1=" << hasQ1 << " hasQ2=" << hasQ2
                      << "\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-quiz-questions-two]\n";
        }
    }

    // [remove-quiz-question] — removes Q2 from a 3-question quiz, renumbers Q3→Q2
    {
        std::string md =
            "## Chapter\n\nText.\n\n"
            "## Quiz\n\n"
            "**Q1:** First?\n\nA) a  B) b  C) c  D) d\n\n**Answer:** A\n\n"
            "**Q2:** Second?\n\nA) a  B) b  C) c  D) d\n\n**Answer:** B\n\n"
            "**Q3:** Third?\n\nA) a  B) b  C) c  D) d\n\n**Answer:** C\n";
        std::string result = RemoveQuizQuestion(md, 2);
        bool noQ2    = result.find("Second?") == std::string::npos;
        bool hasQ1   = result.find("First?")  != std::string::npos;
        bool hasQ2   = result.find("Third?")  != std::string::npos;
        // After removal, "Third" should now be Q2
        bool renumbered = result.find("**Q2:** Third") != std::string::npos;
        if (!noQ2 || !hasQ1 || !hasQ2 || !renumbered) {
            std::cerr << "FAIL [remove-quiz-question]: noQ2=" << noQ2
                      << " hasQ1=" << hasQ1 << " hasQ2(was Q3)=" << hasQ2
                      << " renumbered=" << renumbered << "\n";
            ++failures;
        } else {
            std::cout << "PASS [remove-quiz-question]\n";
        }
    }

    // [remove-quiz-question-last] — removing the only question removes the quiz section
    {
        std::string md =
            "## Chapter\n\nText.\n\n"
            "## Quiz\n\n"
            "**Q1:** Only question?\n\nA) a  B) b\n\n**Answer:** A\n";
        std::string result = RemoveQuizQuestion(md, 1);
        bool noQuiz  = result.find("## Quiz") == std::string::npos;
        bool hasText = result.find("Text.") != std::string::npos;
        if (!noQuiz || !hasText) {
            std::cerr << "FAIL [remove-quiz-question-last]: noQuiz=" << noQuiz
                      << " hasText=" << hasText << "\n";
            ++failures;
        } else {
            std::cout << "PASS [remove-quiz-question-last]\n";
        }
    }

    // [replace-quiz-question] — replaces Q1 text, preserves Q2
    {
        std::string md =
            "## Quiz\n\n"
            "**Q1:** Old question?\n\nA) a  B) b\n\n**Answer:** A\n\n"
            "**Q2:** Second?\n\nA) a  B) b\n\n**Answer:** B\n";
        std::string newBlock =
            "**Q1:** New question?\n\nA) x  B) y\n\n**Answer:** B";
        std::string result = ReplaceQuizQuestion(md, 1, newBlock);
        bool hasNew  = result.find("New question") != std::string::npos;
        bool noOld   = result.find("Old question") == std::string::npos;
        bool hasQ2   = result.find("Second?")      != std::string::npos;
        if (!hasNew || !noOld || !hasQ2) {
            std::cerr << "FAIL [replace-quiz-question]: hasNew=" << hasNew
                      << " noOld=" << noOld << " hasQ2=" << hasQ2 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [replace-quiz-question]\n";
        }
    }

    // [remove-entire-quiz] — removes ## Quiz section, keeps chapter content
    {
        std::string md =
            "## Chapter 1\n\nImportant text.\n\n"
            "## Quiz\n\n"
            "**Q1:** Question?\n\nA) a  B) b\n\n**Answer:** A\n";
        std::string result = RemoveEntireQuiz(md);
        bool noQuiz  = result.find("## Quiz")     == std::string::npos;
        bool noQ1    = result.find("Question?")   == std::string::npos;
        bool hasText = result.find("Important text") != std::string::npos;
        if (!noQuiz || !noQ1 || !hasText) {
            std::cerr << "FAIL [remove-entire-quiz]: noQuiz=" << noQuiz
                      << " noQ1=" << noQ1 << " hasText=" << hasText << "\n";
            ++failures;
        } else {
            std::cout << "PASS [remove-entire-quiz]\n";
        }
    }

    return failures;
}
