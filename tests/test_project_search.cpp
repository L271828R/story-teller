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

    // Project name and LLM source are still searchable.
    {
        auto dir = fs::temp_directory_path() / "st_search_metadata";
        fs::remove_all(dir);
        fs::create_directories(dir);
        write_file(dir / "ch01.md", "# Chapter\n\nSome content here.");

        bool ok = ProjectMatchesSearch("MyStory", dir.string(), "Anthropic API", "",
                                       "mystory")
               && ProjectMatchesSearch("MyStory", dir.string(), "Anthropic API", "",
                                       "anthropic")
               && !ProjectMatchesSearch("MyStory", dir.string(), "Anthropic API", "",
                                        "openai");
        if (!ok) {
            std::cerr << "FAIL [project-search-metadata]\n";
            ++failures;
        } else {
            std::cout << "PASS [project-search-metadata]\n";
        }
        fs::remove_all(dir);
    }

    // ArticleMatchesSearch matches by filename.
    {
        auto dir = fs::temp_directory_path() / "st_article_search_name";
        fs::remove_all(dir);
        fs::create_directories(dir);
        write_file(dir / "chapter-about-kafka.md", "no keyword here\n");

        bool ok = ArticleMatchesSearch((dir / "chapter-about-kafka.md").string(),
                                       "kafka")
               && !ArticleMatchesSearch((dir / "chapter-about-kafka.md").string(),
                                        "rabbit");
        if (!ok) {
            std::cerr << "FAIL [article-search-name]\n";
            ++failures;
        } else {
            std::cout << "PASS [article-search-name]\n";
        }
        fs::remove_all(dir);
    }

    // ArticleMatchesSearch matches by file contents.
    {
        auto dir = fs::temp_directory_path() / "st_article_search_content";
        fs::remove_all(dir);
        fs::create_directories(dir);
        write_file(dir / "ch02.md", "# Chapter 2\n\nThis chapter mentions building things.\n");

        bool ok = ArticleMatchesSearch((dir / "ch02.md").string(), "building")
               && !ArticleMatchesSearch((dir / "ch02.md").string(), "distributed");
        if (!ok) {
            std::cerr << "FAIL [article-search-content]\n";
            ++failures;
        } else {
            std::cout << "PASS [article-search-content]\n";
        }
        fs::remove_all(dir);
    }

    // ProjectMatchesSearch is metadata-only — article content is NOT searched
    // at the project level (that's the article search's job).
    {
        auto dir = fs::temp_directory_path() / "st_project_metadata_only";
        fs::remove_all(dir);
        fs::create_directories(dir);
        write_file(dir / "ch01.md", "# Chapter 1\n\nnothing special.");
        write_file(dir / "ch02.md", "# Chapter 2\n\nmentions building things.");

        bool contentTermMisses =
            !ProjectMatchesSearch("myproject", dir.string(), "", "", "building");
        bool metadataTermHits =
             ProjectMatchesSearch("myproject", dir.string(), "", "", "myproject");
        if (!contentTermMisses || !metadataTermHits) {
            std::cerr << "FAIL [project-search-metadata-only]: "
                      << "content-miss=" << contentTermMisses
                      << " metadata-hit=" << metadataTermHits << "\n";
            ++failures;
        } else {
            std::cout << "PASS [project-search-metadata-only]\n";
        }
        fs::remove_all(dir);
    }

    return failures;
}
