#include "social_results.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

#define CHECK(label, cond) \
    do { if (cond) { std::cout << "PASS [" label "]\n"; } \
         else { std::cerr << "FAIL [" label "]\n"; ++failures; } } while(0)

struct TmpDir {
    std::string path;
    TmpDir() {
        path = (fs::temp_directory_path() /
                ("sres_test_" + std::to_string(getpid()))).string();
        fs::create_directories(path);
    }
    ~TmpDir() { fs::remove_all(path); }
};

int test_social_results() {
    int failures = 0;

    // Empty project → empty map
    {
        TmpDir d;
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-empty-project", all.empty());
    }

    // Append one hook, load it back
    {
        TmpDir d;
        AppendSocialResult(d.path, "darwin.md", "hooks",
                           {"Did you know a single boat trip changed science forever?", "english/high"});
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-one-hook-doc",      all.count("darwin.md") == 1);
        CHECK("sres-one-hook-count",    all["darwin.md"].hooks.size() == 1);
        CHECK("sres-one-hook-text",
            all["darwin.md"].hooks[0].text == "Did you know a single boat trip changed science forever?");
        CHECK("sres-one-hook-meta",     all["darwin.md"].hooks[0].meta == "english/high");
    }

    // Multiple appends accumulate in order
    {
        TmpDir d;
        AppendSocialResult(d.path, "darwin.md", "hooks", {"Hook one", "english/low"});
        AppendSocialResult(d.path, "darwin.md", "hooks", {"Hook two", "chinese/medium"});
        AppendSocialResult(d.path, "darwin.md", "hooks", {"Hook three", "bilingual/high"});
        auto all = LoadAllSocialResults(d.path);
        auto& hooks = all["darwin.md"].hooks;
        CHECK("sres-multi-count",  hooks.size() == 3);
        CHECK("sres-multi-order0", hooks[0].text == "Hook one");
        CHECK("sres-multi-order1", hooks[1].text == "Hook two");
        CHECK("sres-multi-order2", hooks[2].text == "Hook three");
    }

    // Different sections are independent
    {
        TmpDir d;
        AppendSocialResult(d.path, "darwin.md", "hooks",       {"A hook", "english/high"});
        AppendSocialResult(d.path, "darwin.md", "packaging",   {"TITLE: Darwin\nDESCRIPTION: ...", ""});
        AppendSocialResult(d.path, "darwin.md", "atomization", {"CLIP 1: Intro\n...", ""});
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-sections-hooks",       all["darwin.md"].hooks.size() == 1);
        CHECK("sres-sections-packaging",   all["darwin.md"].packaging.size() == 1);
        CHECK("sres-sections-atomization", all["darwin.md"].atomization.size() == 1);
    }

    // Multiple docs are independent
    {
        TmpDir d;
        AppendSocialResult(d.path, "darwin.md",  "hooks", {"Darwin hook", "english/high"});
        AppendSocialResult(d.path, "newton.md",  "hooks", {"Newton hook", "chinese/low"});
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-multi-doc-darwin", all["darwin.md"].hooks[0].text == "Darwin hook");
        CHECK("sres-multi-doc-newton", all["newton.md"].hooks[0].text == "Newton hook");
    }

    // ClearSocialSection removes entries for that section only
    {
        TmpDir d;
        AppendSocialResult(d.path, "darwin.md", "hooks",     {"Hook A", "english/high"});
        AppendSocialResult(d.path, "darwin.md", "hooks",     {"Hook B", "chinese/low"});
        AppendSocialResult(d.path, "darwin.md", "packaging", {"Packaging A", ""});
        ClearSocialSection(d.path, "darwin.md", "hooks");
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-clear-hooks-gone",      all["darwin.md"].hooks.empty());
        CHECK("sres-clear-packaging-kept",  all["darwin.md"].packaging.size() == 1);
    }

    // Text with newlines and special chars round-trips correctly
    {
        TmpDir d;
        std::string multiline = "TITLE: Darwin's Finches\nDESCRIPTION: Line one.\nLine two.";
        AppendSocialResult(d.path, "darwin.md", "packaging", {multiline, ""});
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-multiline", all["darwin.md"].packaging[0].text == multiline);
    }

    // Text with quotes round-trips correctly
    {
        TmpDir d;
        std::string quoted = "He said \"evolution\" and changed the world.";
        AppendSocialResult(d.path, "darwin.md", "hooks", {quoted, "english/medium"});
        auto all = LoadAllSocialResults(d.path);
        CHECK("sres-quotes", all["darwin.md"].hooks[0].text == quoted);
    }

    return failures;
}
