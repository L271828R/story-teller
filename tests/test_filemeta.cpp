#include "filemeta.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static fs::path make_temp_dir() {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto p = fs::temp_directory_path() / ("fm_test_" + std::to_string(ns));
    fs::create_directories(p);
    return p;
}

static void write_file(const fs::path& p, const std::string& content) {
    std::ofstream f(p);
    f << content;
}

static std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    return {std::istreambuf_iterator<char>(f), {}};
}

int test_filemeta() {
    int failures = 0;

    // ReadLanguage returns the language from a <!-- language: X --> comment.
    {
        auto dir = make_temp_dir();
        write_file(dir / "ch.md",
            "<!-- language: Spanish -->\n\n## Chapter 1\n\nBody text.\n");
        std::string lang = ReadLanguage((dir / "ch.md").string());
        if (lang != "Spanish") {
            std::cerr << "FAIL [read-language]: got '" << lang << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [read-language]\n";
        }
        fs::remove_all(dir);
    }

    // ReadLanguage returns empty string when no comment is present.
    {
        auto dir = make_temp_dir();
        write_file(dir / "ch.md", "## Chapter 1\n\nNo metadata here.\n");
        std::string lang = ReadLanguage((dir / "ch.md").string());
        if (!lang.empty()) {
            std::cerr << "FAIL [read-language-absent]: got '" << lang << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [read-language-absent]\n";
        }
        fs::remove_all(dir);
    }

    // WriteLanguage adds the comment to a file that doesn't have one.
    {
        auto dir = make_temp_dir();
        write_file(dir / "ch.md", "## Chapter 1\n\nBody.\n");
        bool ok = WriteLanguage((dir / "ch.md").string(), "English");
        std::string lang = ReadLanguage((dir / "ch.md").string());
        std::string content = read_file(dir / "ch.md");
        bool bodyPreserved = content.find("## Chapter") != std::string::npos;
        if (!ok || lang != "English" || !bodyPreserved) {
            std::cerr << "FAIL [write-language-new]: ok=" << ok
                      << " lang='" << lang << "' body=" << bodyPreserved << "\n";
            ++failures;
        } else {
            std::cout << "PASS [write-language-new]\n";
        }
        fs::remove_all(dir);
    }

    // WriteLanguage updates an existing comment without duplicating it.
    {
        auto dir = make_temp_dir();
        write_file(dir / "ch.md",
            "<!-- language: English -->\n\n## Chapter 1\n\nBody.\n");
        WriteLanguage((dir / "ch.md").string(), "Chinese");
        std::string content = read_file(dir / "ch.md");
        std::string lang = ReadLanguage((dir / "ch.md").string());
        // Only one language comment must appear.
        auto first  = content.find("<!-- language:");
        auto second = content.find("<!-- language:", first + 1);
        if (lang != "Chinese" || second != std::string::npos) {
            std::cerr << "FAIL [write-language-update]: lang='" << lang
                      << "' duplicate=" << (second != std::string::npos) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [write-language-update]\n";
        }
        fs::remove_all(dir);
    }

    // FileModifiedTime returns a non-empty string in YYYY-MM-DD HH:MM format.
    {
        auto dir = make_temp_dir();
        write_file(dir / "ch.md", "## Ch\n");
        std::string ts = FileModifiedTime((dir / "ch.md").string());
        bool looks_right = ts.size() == 16 &&
                           ts[4] == '-' && ts[7] == '-' &&
                           ts[10] == ' ' && ts[13] == ':';
        if (!looks_right) {
            std::cerr << "FAIL [file-modified-time]: got '" << ts << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [file-modified-time]\n";
        }
        fs::remove_all(dir);
    }

    // FileCreatedTime returns a non-empty string in YYYY-MM-DD HH:MM format.
    {
        auto dir = make_temp_dir();
        write_file(dir / "ch.md", "## Ch\n");
        std::string ts = FileCreatedTime((dir / "ch.md").string());
        bool looks_right = ts.size() == 16 &&
                           ts[4] == '-' && ts[7] == '-' &&
                           ts[10] == ' ' && ts[13] == ':';
        if (!looks_right) {
            std::cerr << "FAIL [file-created-time]: got '" << ts << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [file-created-time]\n";
        }
        fs::remove_all(dir);
    }

    return failures;
}
