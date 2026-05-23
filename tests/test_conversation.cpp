#include "conversation.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

namespace fs = std::filesystem;

int test_conversation() {
    int failures = 0;

    // ParseConversation: basic Q/A parsing
    {
        std::string body =
            "Q: What is a stack frame?\n"
            "A: A contiguous region of stack memory.\n\n"
            "Q: Does Valgrind see them?\n"
            "A: No, only heap.\n\n";
        auto turns = ParseConversation(body);
        if (turns.size() != 2
            || turns[0].question != "What is a stack frame?"
            || turns[0].answer   != "A contiguous region of stack memory."
            || turns[1].question != "Does Valgrind see them?"
            || turns[1].answer   != "No, only heap.") {
            std::cerr << "FAIL [parse-conversation]: got " << turns.size() << " turns\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-conversation]\n";
        }
    }

    // SerializeConversationBody: roundtrip
    {
        std::vector<ConversationTurn> turns = {
            {"What is RAII?", "Resource Acquisition Is Initialisation."},
            {"Why use it?",   "Guarantees cleanup on scope exit."},
        };
        std::string body = SerializeConversationBody(turns);
        auto reparsed = ParseConversation(body);
        if (reparsed.size() != 2
            || reparsed[0].question != turns[0].question
            || reparsed[1].answer   != turns[1].answer) {
            std::cerr << "FAIL [serialize-conversation-roundtrip]\n";
            ++failures;
        } else {
            std::cout << "PASS [serialize-conversation-roundtrip]\n";
        }
    }

    // AppendTurn: creates a block when none exists
    {
        auto tmp = fs::temp_directory_path() / "test_conv_append.md";
        std::ofstream f(tmp);
        f << "<!-- ch:2 -->\n## Chapter 2: The Heap\n\nContent here.\n\n---\n\n"
             "<!-- ch:3 -->\n## Chapter 3: RAII\n\nMore content.\n";
        f.close();

        ConversationTurn t{"What is the heap?", "A pool of unstructured memory."};
        bool ok = AppendTurn(tmp.string(), 2, "Chapter 2: The Heap", t);
        if (!ok) {
            std::cerr << "FAIL [append-turn-creates]: AppendTurn returned false\n";
            ++failures;
        } else {
            auto loaded = LoadConversation(tmp.string(), 2);
            if (loaded.size() != 1
                || loaded[0].question != t.question
                || loaded[0].answer   != t.answer) {
                std::cerr << "FAIL [append-turn-creates]: loaded " << loaded.size() << " turns\n";
                ++failures;
            } else {
                std::cout << "PASS [append-turn-creates]\n";
            }
        }
        fs::remove(tmp);
    }

    // AppendTurn: appends to existing block
    {
        auto tmp = fs::temp_directory_path() / "test_conv_existing.md";
        std::ofstream f(tmp);
        f << "<!-- ch:0 -->\n## Chapter 1: Basics\n\n"
             ":::conversation[Chapter 1: Basics]\n"
             "Q: First question?\nA: First answer.\n\n"
             ":::\n";
        f.close();

        ConversationTurn t{"Second question?", "Second answer."};
        AppendTurn(tmp.string(), 0, "Chapter 1: Basics", t);
        auto loaded = LoadConversation(tmp.string(), 0);
        if (loaded.size() != 2
            || loaded[1].question != "Second question?"
            || loaded[1].answer   != "Second answer.") {
            std::cerr << "FAIL [append-turn-existing]: loaded " << loaded.size() << " turns\n";
            ++failures;
        } else {
            std::cout << "PASS [append-turn-existing]\n";
        }
        fs::remove(tmp);
    }

    // DeleteTurn: removes middle turn; block and remaining turns survive
    {
        auto tmp = fs::temp_directory_path() / "test_conv_delete_mid.md";
        std::ofstream f(tmp);
        f << "<!-- ch:1 -->\n## Chapter 1\n\n"
             ":::conversation[Chapter 1]\n"
             "Q: First?\nA: First answer.\n\n"
             "Q: Second?\nA: Second answer.\n\n"
             "Q: Third?\nA: Third answer.\n\n"
             ":::\n";
        f.close();

        bool ok = DeleteTurn(tmp.string(), 1, 1); // delete "Second?"
        if (!ok) {
            std::cerr << "FAIL [delete-turn-middle]: DeleteTurn returned false\n";
            ++failures;
        } else {
            auto loaded = LoadConversation(tmp.string(), 1);
            if (loaded.size() != 2
                || loaded[0].question != "First?"
                || loaded[1].question != "Third?") {
                std::cerr << "FAIL [delete-turn-middle]: got " << loaded.size() << " turns\n";
                ++failures;
            } else {
                std::cout << "PASS [delete-turn-middle]\n";
            }
        }
        fs::remove(tmp);
    }

    // DeleteTurn: deleting the only turn removes the whole :::conversation block
    {
        auto tmp = fs::temp_directory_path() / "test_conv_delete_last.md";
        std::ofstream f(tmp);
        f << "<!-- ch:0 -->\n## Chapter 0\n\nParagraph.\n\n"
             ":::conversation[Chapter 0]\n"
             "Q: Only question?\nA: Only answer.\n\n"
             ":::\n"
             "<!-- ch:1 -->\n## Chapter 1\n\nNext.\n";
        f.close();

        bool ok = DeleteTurn(tmp.string(), 0, 0);
        if (!ok) {
            std::cerr << "FAIL [delete-turn-only]: DeleteTurn returned false\n";
            ++failures;
        } else {
            auto loaded = LoadConversation(tmp.string(), 0);
            // Read file to confirm block is gone
            std::ifstream rf(tmp);
            std::string contents((std::istreambuf_iterator<char>(rf)), {});
            bool blockGone = contents.find(":::conversation[") == std::string::npos;
            bool ch1intact = contents.find("<!-- ch:1 -->") != std::string::npos;
            if (!loaded.empty() || !blockGone || !ch1intact) {
                std::cerr << "FAIL [delete-turn-only]: block not removed; turns=" << loaded.size() << "\n";
                ++failures;
            } else {
                std::cout << "PASS [delete-turn-only]\n";
            }
        }
        fs::remove(tmp);
    }

    // BuildQAPrompt: contains chapter title and question
    {
        std::vector<ConversationTurn> history = {{"Q1", "A1"}};
        std::string prompt = BuildQAPrompt("doc content", "Chapter 2: RAII",
                                           history, "What is RAII?");
        bool hasTitle    = prompt.find("Chapter 2: RAII") != std::string::npos;
        bool hasQuestion = prompt.find("What is RAII?")   != std::string::npos;
        bool hasHistory  = prompt.find("Q1") != std::string::npos;
        if (!hasTitle || !hasQuestion || !hasHistory) {
            std::cerr << "FAIL [build-qa-prompt]\n";
            ++failures;
        } else {
            std::cout << "PASS [build-qa-prompt]\n";
        }
    }

    // BuildChatHTML: input textarea must be embedded in the HTML (not a wx widget).
    {
        std::vector<ConversationTurn> turns;
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasTextarea = html.find("<textarea") != std::string::npos;
        if (!hasTextarea) {
            std::cerr << "FAIL [chat-html-has-textarea]: no <textarea in HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-html-has-textarea]\n";
        }
    }

    // BuildChatHTML: JS must post to the chatSend script message handler.
    {
        std::vector<ConversationTurn> turns;
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasChatSend = html.find("chatSend") != std::string::npos;
        if (!hasChatSend) {
            std::cerr << "FAIL [chat-html-send-js]: 'chatSend' not found in HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-html-send-js]\n";
        }
    }

    // BuildChatHTML: textarea and button must be disabled when pendingQ is set (LLM in flight).
    {
        std::vector<ConversationTurn> turns = {{"Hi", "Hello!"}};
        std::string html = BuildChatHTML("Ch1", turns, "waiting for response", false);
        bool disabled = html.find("<textarea") != std::string::npos &&
                        html.find("disabled") != std::string::npos;
        if (!disabled) {
            std::cerr << "FAIL [chat-html-input-disabled-when-busy]: input not disabled during pending\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-html-input-disabled-when-busy]\n";
        }
    }

    // BuildChatHTML: multi-paragraph answer must produce <p> tags, not raw newlines.
    // Without RenderMarkdown the answer div contains a flat string with no <p>.
    {
        std::vector<ConversationTurn> turns = {{
            "Tell me about memory.",
            "First paragraph here.\n\nSecond paragraph here."
        }};
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasPTag = html.find("<p>") != std::string::npos;
        if (!hasPTag) {
            std::cerr << "FAIL [chat-answer-paragraphs]: no <p> tag found in answer HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-answer-paragraphs]\n";
        }
    }

    // BuildChatHTML: bold markdown (**text**) in answer must render as <strong>.
    {
        std::vector<ConversationTurn> turns = {{
            "What is special?",
            "This is **very** important."
        }};
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasStrong = html.find("<strong>") != std::string::npos;
        if (!hasStrong) {
            std::cerr << "FAIL [chat-answer-bold]: no <strong> tag found in answer HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-answer-bold]\n";
        }
    }

    // BuildChatHTML: bullet list in answer must render <ul>/<li>.
    {
        std::vector<ConversationTurn> turns = {{
            "List things.",
            "- Apple\n- Banana\n- Cherry"
        }};
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasUl = html.find("<ul>") != std::string::npos;
        bool hasLi = html.find("<li>") != std::string::npos;
        if (!hasUl || !hasLi) {
            std::cerr << "FAIL [chat-answer-list]: no <ul>/<li> found in answer HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-answer-list]\n";
        }
    }

    return failures;
}
