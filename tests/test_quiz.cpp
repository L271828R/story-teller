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

    return failures;
}
