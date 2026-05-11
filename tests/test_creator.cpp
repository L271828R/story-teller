#include "creator.h"
#include "project.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path make_temp_dir() {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto p = fs::temp_directory_path() / ("mdviewer_creator_" + std::to_string(ns));
    fs::create_directories(p);
    return p;
}

int test_creator() {
    int failures = 0;

    // BuildPrompt includes the topic, style, and character names.
    {
        GenerationRequest req;
        req.topic      = "black holes";
        req.style      = "children's book";
        req.characters = {"Albert Einstein", "Carl Sagan"};
        req.projectContext = "";

        std::string prompt = BuildPrompt(req, "");
        bool hasTopic  = prompt.find("black holes")    != std::string::npos;
        bool hasStyle  = prompt.find("children")       != std::string::npos;
        bool hasEin    = prompt.find("Albert Einstein") != std::string::npos;
        bool hasSagan  = prompt.find("Carl Sagan")      != std::string::npos;
        if (!hasTopic || !hasStyle || !hasEin || !hasSagan) {
            std::cerr << "FAIL [build-prompt-fields]: topic=" << hasTopic
                      << " style=" << hasStyle
                      << " einstein=" << hasEin
                      << " sagan=" << hasSagan << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-prompt-fields]\n";
        }
    }

    // BuildPrompt explicitly instructs the LLM to use the characters for tidbits.
    {
        GenerationRequest req;
        req.topic      = "black holes";
        req.style      = "children's book";
        req.characters = {"Albert Einstein", "Carl Sagan"};

        std::string prompt = BuildPrompt(req, "");
        bool hasEin       = prompt.find("Albert Einstein") != std::string::npos;
        bool hasTidbitInstr = prompt.find("tidbit") != std::string::npos
                           && prompt.find("Albert Einstein") != std::string::npos;
        if (!hasEin || !hasTidbitInstr) {
            std::cerr << "FAIL [build-prompt-tidbit-instruction]: "
                      << "characters should appear alongside tidbit instruction\n";
            ++failures;
        } else {
            std::cout << "PASS [build-prompt-tidbit-instruction]\n";
        }
    }

    // BuildPrompt includes a reminder to use the mdviewer/story-teller skill.
    {
        GenerationRequest req;
        req.topic      = "black holes";
        req.style      = "children's book";
        req.characters = {};

        std::string prompt = BuildPrompt(req, "");
        bool hasSkillReminder = prompt.find("skill") != std::string::npos
                             || prompt.find("mdviewer") != std::string::npos
                             || prompt.find("StoryTeller") != std::string::npos;
        if (!hasSkillReminder) {
            std::cerr << "FAIL [build-prompt-skill-reminder]: "
                      << "no skill reminder found in prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-prompt-skill-reminder]\n";
        }
    }

    // BuildPrompt embeds the tidbit syntax reference so the LLM knows the format.
    {
        GenerationRequest req;
        req.topic      = "volcanoes";
        req.style      = "horror";
        req.characters = {};
        req.projectContext = "";

        std::string prompt = BuildPrompt(req, "");
        bool hasTidbitSyntax = prompt.find(":::tidbit") != std::string::npos;
        if (!hasTidbitSyntax) {
            std::cerr << "FAIL [build-prompt-syntax]: :::tidbit not found in prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-prompt-syntax]\n";
        }
    }

    // BuildPrompt prepends project context when provided.
    {
        GenerationRequest req;
        req.topic      = "the moon";
        req.style      = "noir";
        req.characters = {};
        req.projectContext = "This story takes place in 1920s Chicago.";

        std::string prompt = BuildPrompt(req, "");
        bool hasContext = prompt.find("1920s Chicago") != std::string::npos;
        if (!hasContext) {
            std::cerr << "FAIL [build-prompt-context]: project context not found in prompt\n";
            ++failures;
        } else {
            std::cout << "PASS [build-prompt-context]\n";
        }
    }

    // BuildPatchPrompt includes the original block, the instruction, and the syntax ref.
    {
        std::string original = ":::tidbit[Sherlock Holmes]\nElementary.\n:::";
        std::string instruction = "make this more dramatic";

        std::string prompt = BuildPatchPrompt(original, instruction, "");
        bool hasOriginal    = prompt.find("Elementary")          != std::string::npos;
        bool hasInstruction = prompt.find("more dramatic")       != std::string::npos;
        bool hasSyntax      = prompt.find(":::tidbit")           != std::string::npos;
        if (!hasOriginal || !hasInstruction || !hasSyntax) {
            std::cerr << "FAIL [build-patch-prompt]: original=" << hasOriginal
                      << " instruction=" << hasInstruction
                      << " syntax=" << hasSyntax << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-patch-prompt]\n";
        }
    }

    // BuildPrompt instructs the LLM to divide the story into numbered chapters.
    {
        GenerationRequest req;
        req.topic = "black holes";
        req.style = "children's book";

        std::string prompt = BuildPrompt(req, "");
        bool hasChapter   = prompt.find("## Chapter") != std::string::npos
                         || prompt.find("Chapter N")  != std::string::npos;
        bool hasChId      = prompt.find("ch:") != std::string::npos
                         || prompt.find("<!-- ch") != std::string::npos
                         || prompt.find("chapter id") != std::string::npos
                         || prompt.find("numbered") != std::string::npos;
        if (!hasChapter || !hasChId) {
            std::cerr << "FAIL [build-prompt-chapter-structure]: "
                      << "chapter=" << hasChapter << " id=" << hasChId << "\n";
            ++failures;
        } else {
            std::cout << "PASS [build-prompt-chapter-structure]\n";
        }
    }

    // StampChapters injects <!-- ch:N --> markers before ## Chapter headings.
    {
        std::string content =
            "# My Story\n\n"
            "## Chapter 1: The Start\n\nSome text.\n\n"
            "## Chapter 2: The End\n\nMore text.\n";

        auto [stamped, count] = StampChapters(content, 0);
        bool hasCh0   = stamped.find("<!-- ch:0 -->") != std::string::npos;
        bool hasCh1   = stamped.find("<!-- ch:1 -->") != std::string::npos;
        bool countOk  = count == 2;
        bool orderOk  = stamped.find("<!-- ch:0 -->") < stamped.find("## Chapter 1");
        if (!hasCh0 || !hasCh1 || !countOk || !orderOk) {
            std::cerr << "FAIL [stamp-chapters]: ch0=" << hasCh0
                      << " ch1=" << hasCh1 << " count=" << count
                      << " order=" << orderOk << "\n";
            ++failures;
        } else {
            std::cout << "PASS [stamp-chapters]\n";
        }
    }

    // ChapterFilename produces a slug from the topic with a chapter number prefix.
    {
        std::string name = ChapterFilename("Black Holes & Neutron Stars", 3);
        bool hasPrefix  = name.rfind("ch03_", 0) == 0;
        bool hasMd      = name.size() > 3 && name.substr(name.size() - 3) == ".md";
        bool noSpaces   = name.find(' ') == std::string::npos;
        bool noAmpersand = name.find('&') == std::string::npos;
        if (!hasPrefix || !hasMd || !noSpaces || !noAmpersand) {
            std::cerr << "FAIL [chapter-filename]: got '" << name << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [chapter-filename]\n";
        }
    }

    // SaveChapter writes the content to disk and returns the path.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "save-test");
        std::string proj = (base / "save-test").string();

        std::string content = "# Hello\n\nWorld\n";
        std::string path = SaveChapter(proj, "ch01_hello.md", content);
        bool fileExists = fs::exists(path);
        bool contentOk  = false;
        if (fileExists) {
            std::ifstream f(path);
            std::string got((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
            contentOk = (got == content);
        }
        if (!fileExists || !contentOk) {
            std::cerr << "FAIL [save-chapter]: exists=" << fileExists
                      << " content=" << contentOk << "\n";
            ++failures;
        } else {
            std::cout << "PASS [save-chapter]\n";
        }
        fs::remove_all(base);
    }

    return failures;
}
