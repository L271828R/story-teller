#include "conversation.h"
#include "html_template.h"
#include "markdown.h"
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

    // BuildChatHTML: chatSend JS must JSON.stringify the text so that newlines
    // survive the WKWebView postMessage bridge (plain postMessage strips \n).
    {
        std::vector<ConversationTurn> turns;
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasJsonStringify = html.find("JSON.stringify") != std::string::npos;
        if (!hasJsonStringify) {
            std::cerr << "FAIL [chat-html-json-send]: chatSend JS does not use JSON.stringify\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-html-json-send]\n";
        }
    }

    // BuildChatHTML: <pre> blocks must have a background and hljs must run.
    {
        std::vector<ConversationTurn> turns;
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        bool hasPreCSS  = html.find("pre{") != std::string::npos ||
                          html.find("pre {") != std::string::npos;
        bool hasHljs    = html.find("hljs") != std::string::npos;
        if (!hasPreCSS || !hasHljs) {
            std::cerr << "FAIL [chat-html-pre-hljs]:"
                      << " hasPreCSS=" << hasPreCSS
                      << " hasHljs=" << hasHljs << "\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-html-pre-hljs]\n";
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

    // AppendTurn: chId=-1 (document chat) persists even without a <!-- ch:-1 --> marker
    {
        auto tmp = fs::temp_directory_path() / "test_conv_doc_chat.md";
        std::ofstream f(tmp);
        f << "<!-- ch:0 -->\n## Chapter 1\n\nContent.\n";
        f.close();

        ConversationTurn t{"What is this about?", "It is about testing."};
        bool ok = AppendTurn(tmp.string(), -1, "Document", t);
        if (!ok) {
            std::cerr << "FAIL [append-turn-doc-chat]: AppendTurn returned false for chId=-1\n";
            ++failures;
        } else {
            auto loaded = LoadConversation(tmp.string(), -1);
            if (loaded.size() != 1
                || loaded[0].question != t.question
                || loaded[0].answer   != t.answer) {
                std::cerr << "FAIL [append-turn-doc-chat]: loaded " << loaded.size() << " turns\n";
                ++failures;
            } else {
                std::cout << "PASS [append-turn-doc-chat]\n";
            }
        }
        fs::remove(tmp);
    }

    // AppendTurn: chId=-1 second turn appends to existing block
    {
        auto tmp = fs::temp_directory_path() / "test_conv_doc_chat2.md";
        std::ofstream f(tmp);
        f << "<!-- ch:0 -->\n## Chapter 1\n\nContent.\n";
        f.close();

        ConversationTurn t1{"First question?", "First answer."};
        ConversationTurn t2{"Second question?", "Second answer."};
        AppendTurn(tmp.string(), -1, "Document", t1);
        bool ok = AppendTurn(tmp.string(), -1, "Document", t2);
        if (!ok) {
            std::cerr << "FAIL [append-turn-doc-chat-second]: AppendTurn returned false\n";
            ++failures;
        } else {
            auto loaded = LoadConversation(tmp.string(), -1);
            if (loaded.size() != 2
                || loaded[0].question != t1.question
                || loaded[1].question != t2.question) {
                std::cerr << "FAIL [append-turn-doc-chat-second]: loaded " << loaded.size() << " turns\n";
                ++failures;
            } else {
                std::cout << "PASS [append-turn-doc-chat-second]\n";
            }
        }
        fs::remove(tmp);
    }

    // DeleteTurn: chId=-1 delete works after persistence
    {
        auto tmp = fs::temp_directory_path() / "test_conv_doc_chat_del.md";
        std::ofstream f(tmp);
        f << "<!-- ch:0 -->\n## Chapter 1\n\nContent.\n";
        f.close();

        ConversationTurn t1{"First?", "First answer."};
        ConversationTurn t2{"Second?", "Second answer."};
        AppendTurn(tmp.string(), -1, "Document", t1);
        AppendTurn(tmp.string(), -1, "Document", t2);
        bool ok = DeleteTurn(tmp.string(), -1, 0);
        if (!ok) {
            std::cerr << "FAIL [delete-turn-doc-chat]: DeleteTurn returned false\n";
            ++failures;
        } else {
            auto loaded = LoadConversation(tmp.string(), -1);
            if (loaded.size() != 1 || loaded[0].question != "Second?") {
                std::cerr << "FAIL [delete-turn-doc-chat]: loaded " << loaded.size() << " turns\n";
                ++failures;
            } else {
                std::cout << "PASS [delete-turn-doc-chat]\n";
            }
        }
        fs::remove(tmp);
    }

    // RenderMarkdown: python fence with no blank line before it must still produce <pre><code>.
    {
        std::string q = "can you map this:\n```python\nprint(\"hello world\")\nval = 5 * 2\n```";
        std::string html = RenderMarkdown(q);
        bool hasPre     = html.find("<pre>") != std::string::npos;
        bool hasCode    = html.find("<code") != std::string::npos;
        bool noRawFence = html.find("```") == std::string::npos;
        if (!hasPre || !hasCode || !noRawFence) {
            std::cerr << "FAIL [chat-python-fence]:"
                      << " hasPre=" << hasPre
                      << " hasCode=" << hasCode
                      << " noRawFence=" << noRawFence
                      << "\n  html: " << html.substr(0, 300) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-python-fence]\n";
        }
    }

    // BuildChatHTML: question bubble must render markdown, not raw text.
    // A code fence in the question must produce <pre><code>, not literal backticks.
    // Check only the <body> section — hljs JS/CSS in <head> legitimately contains backticks.
    {
        std::vector<ConversationTurn> turns = {{
            "can you map this:\n\n```text\n 19 x 3,\nstep 1: 3*9=27\n```",
            "ok"
        }};
        std::string html = BuildChatHTML("Ch1", turns, "", false);
        size_t bodyPos  = html.find("<body>");
        std::string body = bodyPos != std::string::npos ? html.substr(bodyPos) : html;
        bool hasPre     = body.find("<pre>") != std::string::npos;
        bool hasCode    = body.find("<code") != std::string::npos;
        bool hasContent = body.find("19 x 3") != std::string::npos;
        bool noRawFence = body.find("```") == std::string::npos;
        if (!hasPre || !hasCode || !hasContent || !noRawFence) {
            std::cerr << "FAIL [chat-question-markdown]:"
                      << " hasPre=" << hasPre
                      << " hasCode=" << hasCode
                      << " hasContent=" << hasContent
                      << " noRawFence=" << noRawFence << "\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-question-markdown]\n";
        }
    }

    // BuildChatHTML: pending question bubble must also render markdown.
    {
        std::vector<ConversationTurn> turns;
        std::string pendingQ = "map this:\n\n```text\n 19 x 3\n```";
        std::string html = BuildChatHTML("Ch1", turns, pendingQ, false);
        size_t bodyPos  = html.find("<body>");
        std::string body = bodyPos != std::string::npos ? html.substr(bodyPos) : html;
        bool hasPre     = body.find("<pre>") != std::string::npos;
        bool hasCode    = body.find("<code") != std::string::npos;
        bool noRawFence = body.find("```") == std::string::npos;
        if (!hasPre || !hasCode || !noRawFence) {
            std::cerr << "FAIL [chat-pending-question-markdown]:"
                      << " hasPre=" << hasPre
                      << " hasCode=" << hasCode
                      << " noRawFence=" << noRawFence << "\n";
            ++failures;
        } else {
            std::cout << "PASS [chat-pending-question-markdown]\n";
        }
    }

    // BuildPersonaPromptMulti: persona name and question appear
    {
        MultiChatTurn prev;
        prev.userMessage = "Hello everyone.";
        prev.responses   = {{"Einstein", "Guten Tag!"}, {"Curie", "Bonjour!"}};

        std::string prompt = BuildPersonaPromptMulti(
            "Einstein", "Brilliant physicist.",
            "", {prev}, "What is mass?", false);

        bool hasName    = prompt.find("Einstein")      != std::string::npos;
        bool hasHistory = prompt.find("Hello everyone.") != std::string::npos;
        bool hasCurie   = prompt.find("Curie")          != std::string::npos;
        bool hasNewQ    = prompt.find("What is mass?")  != std::string::npos;
        if (!hasName || !hasHistory || !hasCurie || !hasNewQ) {
            std::cerr << "FAIL [build-persona-prompt-multi]\n"; ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-multi]\n";
        }
    }

    // BuildPersonaPromptMulti: article included only when useArticle=true
    {
        std::string prompt = BuildPersonaPromptMulti(
            "Curie", "", "## Radioactivity", {}, "Explain.", true);
        bool hasArticle = prompt.find("## Radioactivity") != std::string::npos;
        if (!hasArticle) {
            std::cerr << "FAIL [build-persona-prompt-multi-article]\n"; ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-multi-article]\n";
        }
    }

    // BuildPersonaPrompt: persona name and question appear in prompt
    {
        std::vector<ConversationTurn> history;
        std::string prompt = BuildPersonaPrompt("Albert Einstein", "Brilliant physicist.",
                                               "", history, "Hello?", false);
        bool hasName     = prompt.find("Albert Einstein") != std::string::npos;
        bool hasQuestion = prompt.find("Hello?") != std::string::npos;
        if (!hasName || !hasQuestion) {
            std::cerr << "FAIL [build-persona-prompt-basic]:"
                      << " hasName=" << hasName << " hasQuestion=" << hasQuestion << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-basic]\n";
        }
    }

    // BuildPersonaPrompt: article included when useArticle=true
    {
        std::vector<ConversationTurn> history;
        std::string prompt = BuildPersonaPrompt("Marie Curie", "",
                                               "## Radioactivity research", history, "Explain", true);
        bool hasArticle = prompt.find("## Radioactivity research") != std::string::npos;
        if (!hasArticle) {
            std::cerr << "FAIL [build-persona-prompt-article]: article text not found in prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-article]\n";
        }
    }

    // BuildPersonaPrompt: article excluded when useArticle=false
    {
        std::vector<ConversationTurn> history;
        std::string prompt = BuildPersonaPrompt("Marie Curie", "",
                                               "## Radioactivity research", history, "Explain", false);
        bool noArticle = prompt.find("## Radioactivity research") == std::string::npos;
        if (!noArticle) {
            std::cerr << "FAIL [build-persona-prompt-no-article]: article text leaked into prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-no-article]\n";
        }
    }

    // BuildPersonaPrompt: description included when non-empty
    {
        std::vector<ConversationTurn> history;
        std::string prompt = BuildPersonaPrompt("Sherlock Holmes", "Famous detective from Baker St.",
                                               "", history, "What do you observe?", false);
        bool hasDesc = prompt.find("Famous detective from Baker St.") != std::string::npos;
        if (!hasDesc) {
            std::cerr << "FAIL [build-persona-prompt-desc]: description not in prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-desc]\n";
        }
    }

    // BuildPersonaPrompt: conversation history appears in prompt
    {
        std::vector<ConversationTurn> history = {{"What is light?", "Electromagnetic waves."}};
        std::string prompt = BuildPersonaPrompt("Einstein", "",
                                               "", history, "What is mass?", false);
        bool hasHistQ = prompt.find("What is light?")      != std::string::npos;
        bool hasHistA = prompt.find("Electromagnetic waves.") != std::string::npos;
        bool hasNewQ  = prompt.find("What is mass?")       != std::string::npos;
        if (!hasHistQ || !hasHistA || !hasNewQ) {
            std::cerr << "FAIL [build-persona-prompt-history]\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-history]\n";
        }
    }

    // BuildPersonaPrompt: tidbits stripped from article context
    {
        std::vector<ConversationTurn> history;
        std::string docWithTidbit =
            "# Main Article\n\nSome content here.\n\n"
            ":::tidbit[Cavewoman]\nShe invented fire.\n:::\n\n"
            "More article text.";
        std::string prompt = BuildPersonaPrompt("Einstein", "", docWithTidbit, history, "Tell me", true);
        bool hasMain    = prompt.find("Some content here.") != std::string::npos;
        bool hasMore    = prompt.find("More article text.") != std::string::npos;
        bool noTidbit   = prompt.find("She invented fire.") == std::string::npos;
        bool noTidbitFn = prompt.find(":::tidbit")          == std::string::npos;
        if (!hasMain || !hasMore || !noTidbit || !noTidbitFn) {
            std::cerr << "FAIL [build-persona-prompt-strips-tidbits]: "
                      << "hasMain=" << hasMain << " hasMore=" << hasMore
                      << " noTidbit=" << noTidbit << " noTidbitFn=" << noTidbitFn << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-strips-tidbits]\n";
        }
    }

    // BuildPersonaPromptMulti: tidbits stripped from article context
    {
        std::vector<MultiChatTurn> history;
        std::string docWithTidbit =
            "# Chat Topic\n\n:::tidbit[Caveman]\nHe grunted.\n:::\n\nReal content.";
        std::string prompt = BuildPersonaPromptMulti("Curie", "", docWithTidbit, history, "Discuss", true);
        bool hasReal  = prompt.find("Real content.")  != std::string::npos;
        bool noTidbit = prompt.find("He grunted.")    == std::string::npos;
        if (!hasReal || !noTidbit) {
            std::cerr << "FAIL [build-persona-prompt-multi-strips-tidbits]: "
                      << "hasReal=" << hasReal << " noTidbit=" << noTidbit << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-multi-strips-tidbits]\n";
        }
    }

    // BuildPersonaPrompt: prompt encourages sharing opinion, not annoyance
    {
        std::vector<ConversationTurn> history;
        std::string prompt = BuildPersonaPrompt("Einstein", "", "", history, "Hello", false);
        // Should include guidance about opinion / engagement, not just "in-character"
        bool hasOpinion = prompt.find("opinion") != std::string::npos ||
                          prompt.find("perspective") != std::string::npos;
        bool noAnnoy    = prompt.find("annoyed") == std::string::npos &&
                          prompt.find("frustrated") == std::string::npos;
        if (!hasOpinion || !noAnnoy) {
            std::cerr << "FAIL [build-persona-prompt-opinion]: "
                      << "hasOpinion=" << hasOpinion << " noAnnoy=" << noAnnoy << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-opinion]\n";
        }
    }

    // BuildPersonaPrompt: repeated questions should be welcomed, not deflected
    {
        std::vector<ConversationTurn> history;
        std::string prompt = BuildPersonaPrompt("Einstein", "", "", history, "Hello", false);
        bool hasRepeated = prompt.find("repeated") != std::string::npos ||
                           prompt.find("revisit")  != std::string::npos ||
                           prompt.find("same question") != std::string::npos;
        if (!hasRepeated) {
            std::cerr << "FAIL [build-persona-prompt-repeated-questions]\n";
            ++failures;
        } else {
            std::cout << "PASS [build-persona-prompt-repeated-questions]\n";
        }
    }

    // Conversation style modes
    {
        auto check = [&](const std::string& name, const std::string& mode,
                         const std::string& needle, const std::string& label) {
            std::string p = BuildPersonaPrompt("Ada", "", "", {}, "hi", false, mode);
            bool ok = p.find(needle) != std::string::npos;
            if (!ok) std::cerr << "FAIL [" << label << "]: '" << needle << "' not found\n";
            else     std::cout << "PASS [" << label << "]\n";
            if (!ok) ++failures;
        };
        check("Ada", "technical", "prefer markdown tables",  "build-persona-prompt-mode-technical");
        check("Ada", "story",     "narrative",               "build-persona-prompt-mode-story");
        check("Ada", "debate",    "speak TO them",            "build-persona-prompt-mode-debate");
        check("Ada", "simple",    "ten-year-old",            "build-persona-prompt-mode-simple");
        check("Ada", "socratic",  "ask questions",           "build-persona-prompt-mode-socratic");
        check("Ada", "rant",      "unfiltered",              "build-persona-prompt-mode-rant");

        std::string gen = BuildPersonaPrompt("Ada", "", "", {}, "hi", false, "general");
        bool genClean = gen.find("visualization tools") == std::string::npos &&
                        gen.find("narrative") == std::string::npos &&
                        gen.find("unfiltered") == std::string::npos;
        if (!genClean) std::cerr << "FAIL [build-persona-prompt-mode-general-clean]\n";
        else           std::cout << "PASS [build-persona-prompt-mode-general-clean]\n";
        if (!genClean) ++failures;
    }
    {
        auto check = [&](const std::string& name, const std::string& mode,
                         const std::string& needle, const std::string& label) {
            std::string p = BuildPersonaPromptMulti("Ada", "", "", {}, "hi", false, mode);
            bool ok = p.find(needle) != std::string::npos;
            if (!ok) std::cerr << "FAIL [" << label << "]: '" << needle << "' not found\n";
            else     std::cout << "PASS [" << label << "]\n";
            if (!ok) ++failures;
        };
        check("Ada", "technical", "prefer markdown tables",  "build-persona-prompt-multi-mode-technical");
        check("Ada", "story",     "narrative",               "build-persona-prompt-multi-mode-story");
        check("Ada", "debate",    "committed position",      "build-persona-prompt-multi-mode-debate");
        check("Ada", "simple",    "ten-year-old",            "build-persona-prompt-multi-mode-simple");
        check("Ada", "socratic",  "ask questions",           "build-persona-prompt-multi-mode-socratic");
        check("Ada", "rant",      "unfiltered",              "build-persona-prompt-multi-mode-rant");

        std::string gen = BuildPersonaPromptMulti("Ada", "", "", {}, "hi", false, "general");
        bool genClean = gen.find("visualization tools") == std::string::npos &&
                        gen.find("narrative") == std::string::npos &&
                        gen.find("unfiltered") == std::string::npos;
        if (!genClean) std::cerr << "FAIL [build-persona-prompt-multi-mode-general-clean]\n";
        else           std::cout << "PASS [build-persona-prompt-multi-mode-general-clean]\n";
        if (!genClean) ++failures;
    }

    // BuildMermaidRepairPrompt: contains the source and asks for corrected syntax
    {
        std::string src = "flowchart LR\n  A --> B\n  B --bad syntax-- C\n";
        std::string prompt = BuildMermaidRepairPrompt(src);
        bool hasSrc   = prompt.find(src)        != std::string::npos;
        bool hasFix   = prompt.find("syntax")   != std::string::npos ||
                        prompt.find("corrected") != std::string::npos ||
                        prompt.find("fix")       != std::string::npos;
        bool noFences = prompt.find("```") == std::string::npos; // prompt should not wrap it
        if (!hasSrc || !hasFix) {
            std::cerr << "FAIL [build-mermaid-repair-prompt]: hasSrc=" << hasSrc
                      << " hasFix=" << hasFix << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-mermaid-repair-prompt]\n";
        }
        (void)noFences;
    }

    return failures;
}
