#include "project_search.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void write_file(const fs::path& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

int test_project_search() {
    int failures = 0;

    // Plain text search is not fuzzy: "stream" must appear as text.
    {
        bool ok = ProjectSearchTextMatches("a project about streaming data", "stream")
               && !ProjectSearchTextMatches("s t r e a m letters spread out", "stream");
        if (!ok) {
            std::cerr << "FAIL [project-search-not-fuzzy]\n";
            ++failures;
        } else {
            std::cout << "PASS [project-search-not-fuzzy]\n";
        }
    }

    // Multiple words require all terms, case-insensitively.
    {
        bool ok = ProjectSearchTextMatches("Kafka Stream processing notes", "stream kafka")
               && ProjectSearchTextMatches("Kafka Stream processing notes", "STREAM")
               && !ProjectSearchTextMatches("Kafka notes", "stream kafka");
        if (!ok) {
            std::cerr << "FAIL [project-search-all-terms]\n";
            ++failures;
        } else {
            std::cout << "PASS [project-search-all-terms]\n";
        }
    }

    // Project search indexes .md filenames and document contents.
    {
        auto dir = fs::temp_directory_path() / "st_project_search";
        fs::remove_all(dir);
        fs::create_directories(dir);
        write_file(dir / "chapter.md", "# Pipes\n\nThis chapter discusses stream flow.");
        write_file(dir / "notes.txt", "stream should not be needed from txt");

        bool ok = ProjectMatchesSearch("water", dir.string(), "Codex CLI", "",
                                       "stream flow")
               && ProjectMatchesSearch("water", dir.string(), "Codex CLI", "",
                                       "chapter.md")
               && !ProjectMatchesSearch("water", dir.string(), "Codex CLI", "",
                                        "unrelated");
        if (!ok) {
            std::cerr << "FAIL [project-search-md-content]\n";
            ++failures;
        } else {
            std::cout << "PASS [project-search-md-content]\n";
        }
        fs::remove_all(dir);
    }

    return failures;
}
