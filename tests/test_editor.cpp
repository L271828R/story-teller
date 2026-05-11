#include "editor.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path make_temp_dir() {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto p = fs::temp_directory_path() / ("mdviewer_editor_" + std::to_string(ns));
    fs::create_directories(p);
    return p;
}

static const std::string kSampleChapter =
    "# Black Holes\n\n"
    "Some intro text.\n\n"
    "<!-- tb:3 -->\n"
    ":::tidbit[Albert Einstein]\n"
    "E=mc²\n"
    ":::\n\n"
    "Middle paragraph.\n\n"
    "<!-- tb:5 -->\n"
    ":::tidbit[Carl Sagan]\n"
    "We are star stuff.\n"
    ":::\n\n"
    "Final paragraph.\n";

int test_editor() {
    int failures = 0;

    // ExtractTidbit returns the :::tidbit...:::  block for a given ID.
    {
        std::string block = ExtractTidbit(kSampleChapter, 3);
        bool hasOpen    = block.find(":::tidbit[Albert Einstein]") != std::string::npos;
        bool hasContent = block.find("E=mc²")                     != std::string::npos;
        bool hasClose   = block.find(":::") != std::string::npos;
        bool noSagan    = block.find("Carl Sagan") == std::string::npos;
        if (!hasOpen || !hasContent || !hasClose || !noSagan) {
            std::cerr << "FAIL [extract-tidbit]: open=" << hasOpen
                      << " content=" << hasContent
                      << " close=" << hasClose
                      << " noSagan=" << noSagan
                      << "\n  got: '" << block << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-tidbit]\n";
        }
    }

    // ExtractTidbit returns "" for an unknown ID.
    {
        std::string block = ExtractTidbit(kSampleChapter, 99);
        if (!block.empty()) {
            std::cerr << "FAIL [extract-tidbit-missing]: expected empty, got '" << block << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-tidbit-missing]\n";
        }
    }

    // PatchTidbit replaces the target block and preserves surrounding content.
    {
        std::string replacement =
            ":::tidbit[Albert Einstein]\n"
            "Energy and mass are equivalent — a profound truth!\n"
            ":::";
        std::string patched = PatchTidbit(kSampleChapter, 3, replacement);

        bool hasNew     = patched.find("profound truth")         != std::string::npos;
        bool hasOldGone = patched.find("E=mc²")                  == std::string::npos;
        bool hasSagan   = patched.find("We are star stuff")      != std::string::npos;
        bool hasIntro   = patched.find("Some intro text")        != std::string::npos;
        bool hasFinal   = patched.find("Final paragraph")        != std::string::npos;
        if (!hasNew || !hasOldGone || !hasSagan || !hasIntro || !hasFinal) {
            std::cerr << "FAIL [patch-tidbit]: new=" << hasNew
                      << " oldGone=" << hasOldGone
                      << " sagan=" << hasSagan
                      << " intro=" << hasIntro
                      << " final=" << hasFinal << "\n";
            ++failures;
        } else {
            std::cout << "PASS [patch-tidbit]\n";
        }
    }

    // PatchTidbit preserves the <!-- tb:N --> marker in the output.
    {
        std::string replacement = ":::tidbit[Albert Einstein]\nNew content.\n:::";
        std::string patched = PatchTidbit(kSampleChapter, 3, replacement);
        bool hasMarker = patched.find("<!-- tb:3 -->") != std::string::npos;
        if (!hasMarker) {
            std::cerr << "FAIL [patch-preserves-marker]: <!-- tb:3 --> not found after patch\n";
            ++failures;
        } else {
            std::cout << "PASS [patch-preserves-marker]\n";
        }
    }

    // ApplyTidbitPatch reads a file, patches it, and writes it back.
    {
        auto tmp = make_temp_dir();
        fs::path filepath = tmp / "ch.md";
        { std::ofstream f(filepath); f << kSampleChapter; }

        std::string replacement = ":::tidbit[Albert Einstein]\nUpdated on disk.\n:::";
        bool ok = ApplyTidbitPatch(filepath.string(), 3, replacement);

        std::string onDisk;
        { std::ifstream f(filepath);
          onDisk.assign(std::istreambuf_iterator<char>(f), {}); }

        bool updated = onDisk.find("Updated on disk") != std::string::npos;
        bool sagan   = onDisk.find("We are star stuff") != std::string::npos;
        if (!ok || !updated || !sagan) {
            std::cerr << "FAIL [apply-patch]: ok=" << ok
                      << " updated=" << updated
                      << " sagan=" << sagan << "\n";
            ++failures;
        } else {
            std::cout << "PASS [apply-patch]\n";
        }
        fs::remove_all(tmp);
    }

    // ReplaceChapter writes entirely new content to a file.
    {
        auto tmp = make_temp_dir();
        fs::path filepath = tmp / "ch.md";
        { std::ofstream f(filepath); f << "old content\n"; }

        bool ok = ReplaceChapter(filepath.string(), "# New\n\nNew content.\n");
        std::string onDisk;
        { std::ifstream f(filepath);
          onDisk.assign(std::istreambuf_iterator<char>(f), {}); }

        bool hasNew = onDisk.find("New content") != std::string::npos;
        bool noOld  = onDisk.find("old content") == std::string::npos;
        if (!ok || !hasNew || !noOld) {
            std::cerr << "FAIL [replace-chapter]: ok=" << ok
                      << " hasNew=" << hasNew
                      << " noOld=" << noOld << "\n";
            ++failures;
        } else {
            std::cout << "PASS [replace-chapter]\n";
        }
        fs::remove_all(tmp);
    }

    return failures;
}
