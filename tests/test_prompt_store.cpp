#include "prompt_store.h"
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int test_prompt_store() {
    int failures = 0;
    fs::path tmp = fs::temp_directory_path() / "st_prompt_test.json";
    std::string path = tmp.string();

    // Clean state
    fs::remove(tmp);

    // [prompt-load-empty] — missing file returns empty list
    {
        auto ps = LoadPrompts(path);
        if (!ps.empty()) {
            std::cerr << "FAIL [prompt-load-empty]: expected 0, got " << ps.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [prompt-load-empty]\n";
        }
    }

    // [prompt-add] — adds a prompt, assigns id, persists
    {
        auto p = AddPrompt("First title", "First body text", path);
        bool goodId    = p.id > 0;
        bool goodTitle = p.title == "First title";
        bool goodText  = p.text  == "First body text";
        auto loaded = LoadPrompts(path);
        bool persisted = loaded.size() == 1 && loaded[0].id == p.id;
        if (!goodId || !goodTitle || !goodText || !persisted) {
            std::cerr << "FAIL [prompt-add]: goodId=" << goodId
                      << " goodTitle=" << goodTitle << " goodText=" << goodText
                      << " persisted=" << persisted << "\n";
            ++failures;
        } else {
            std::cout << "PASS [prompt-add]\n";
        }
    }

    // [prompt-add-multiple] — IDs are unique and order is preserved
    {
        fs::remove(tmp);
        AddPrompt("A", "text A", path);
        AddPrompt("B", "text B", path);
        auto ps = LoadPrompts(path);
        bool twoItems = ps.size() == 2;
        bool uniqueIds = twoItems && ps[0].id != ps[1].id;
        bool order = twoItems && ps[0].title == "A" && ps[1].title == "B";
        if (!twoItems || !uniqueIds || !order) {
            std::cerr << "FAIL [prompt-add-multiple]: twoItems=" << twoItems
                      << " uniqueIds=" << uniqueIds << " order=" << order << "\n";
            ++failures;
        } else {
            std::cout << "PASS [prompt-add-multiple]\n";
        }
    }

    // [prompt-delete] — removes the matching id, leaves others
    {
        fs::remove(tmp);
        auto p1 = AddPrompt("Keep",   "keep body", path);
        auto p2 = AddPrompt("Delete", "del body",  path);
        bool ok = DeletePrompt(p2.id, path);
        auto ps = LoadPrompts(path);
        bool oneLeft = ps.size() == 1;
        bool hasKeep = oneLeft && ps[0].title == "Keep";
        if (!ok || !oneLeft || !hasKeep) {
            std::cerr << "FAIL [prompt-delete]: ok=" << ok
                      << " oneLeft=" << oneLeft << " hasKeep=" << hasKeep << "\n";
            ++failures;
        } else {
            std::cout << "PASS [prompt-delete]\n";
        }
    }

    // [prompt-delete-missing] — deleting nonexistent id returns false
    {
        bool ok = DeletePrompt(99999, path);
        if (ok) {
            std::cerr << "FAIL [prompt-delete-missing]: should return false\n";
            ++failures;
        } else {
            std::cout << "PASS [prompt-delete-missing]\n";
        }
    }

    // [prompt-update] — changes title and text of existing prompt
    {
        fs::remove(tmp);
        auto p = AddPrompt("Old title", "Old text", path);
        bool ok = UpdatePrompt(p.id, "New title", "New text", path);
        auto ps = LoadPrompts(path);
        bool hasNew = ps.size() == 1 && ps[0].title == "New title"
                   && ps[0].text == "New text" && ps[0].id == p.id;
        if (!ok || !hasNew) {
            std::cerr << "FAIL [prompt-update]: ok=" << ok << " hasNew=" << hasNew << "\n";
            ++failures;
        } else {
            std::cout << "PASS [prompt-update]\n";
        }
    }

    // [prompt-roundtrip-special-chars] — title/text with quotes and newlines survive
    {
        fs::remove(tmp);
        std::string weirdTitle = "He said \"hello\" & left";
        std::string weirdText  = "Line one\nLine two\nLine \"three\"";
        AddPrompt(weirdTitle, weirdText, path);
        auto ps = LoadPrompts(path);
        bool ok = ps.size() == 1 && ps[0].title == weirdTitle && ps[0].text == weirdText;
        if (!ok) {
            std::cerr << "FAIL [prompt-roundtrip-special-chars]\n";
            if (!ps.empty()) {
                std::cerr << "  title got: '" << ps[0].title << "'\n";
                std::cerr << "  text  got: '" << ps[0].text  << "'\n";
            }
            ++failures;
        } else {
            std::cout << "PASS [prompt-roundtrip-special-chars]\n";
        }
    }

    fs::remove(tmp);
    return failures;
}
