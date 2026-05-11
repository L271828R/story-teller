#include "git_ops.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path make_temp_dir() {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto p = fs::temp_directory_path() / ("st_git_" + std::to_string(ns));
    fs::create_directories(p);
    return p;
}

static void write_file(const fs::path& p, const std::string& content) {
    std::ofstream f(p);
    f << content;
}

static std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

// Run a shell command silently.
static void shell(const std::string& cmd) { ::system(cmd.c_str()); }

int test_git_ops() {
    int failures = 0;

    // GitInit creates a .git directory.
    {
        auto dir = make_temp_dir();
        bool ok = GitInit(dir.string());
        bool hasGit = fs::exists(dir / ".git");
        if (!ok || !hasGit) {
            std::cerr << "FAIL [git-init]: ok=" << ok << " hasGit=" << hasGit << "\n";
            ++failures;
        } else {
            std::cout << "PASS [git-init]\n";
        }
        fs::remove_all(dir);
    }

    // GitCommitFile stages and commits a file; log shows one entry.
    {
        auto dir = make_temp_dir();
        GitInit(dir.string());
        // Set local identity so commit works on machines without global git config.
        shell("git -C " + dir.string() + " config user.email 'test@test.com' 2>/dev/null");
        shell("git -C " + dir.string() + " config user.name 'Test' 2>/dev/null");

        write_file(dir / "ch01.md", "# Hello\n\nWorld.\n");
        bool ok = GitCommitFile(dir.string(), "ch01.md", "first draft");

        auto log = GitLogFile(dir.string(), "ch01.md");
        bool logOk = !log.empty() && log[0].subject.find("first draft") != std::string::npos;

        if (!ok || !logOk) {
            std::cerr << "FAIL [git-commit]: ok=" << ok << " logOk=" << logOk
                      << " entries=" << log.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [git-commit]\n";
        }
        fs::remove_all(dir);
    }

    // GitShowFile returns the content at a given commit.
    {
        auto dir = make_temp_dir();
        GitInit(dir.string());
        shell("git -C " + dir.string() + " config user.email 'test@test.com' 2>/dev/null");
        shell("git -C " + dir.string() + " config user.name 'Test' 2>/dev/null");

        write_file(dir / "story.md", "# Version 1\n");
        GitCommitFile(dir.string(), "story.md", "v1");

        auto log = GitLogFile(dir.string(), "story.md");
        std::string content = log.empty() ? "" : GitShowFile(dir.string(), log[0].hash, "story.md");
        bool ok = content.find("Version 1") != std::string::npos;

        if (!ok) {
            std::cerr << "FAIL [git-show]: content='" << content << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [git-show]\n";
        }
        fs::remove_all(dir);
    }

    // GitRestoreFile reverts the working copy to a previous commit.
    {
        auto dir = make_temp_dir();
        GitInit(dir.string());
        shell("git -C " + dir.string() + " config user.email 'test@test.com' 2>/dev/null");
        shell("git -C " + dir.string() + " config user.name 'Test' 2>/dev/null");

        write_file(dir / "ch.md", "original content\n");
        GitCommitFile(dir.string(), "ch.md", "original");
        auto log = GitLogFile(dir.string(), "ch.md");

        write_file(dir / "ch.md", "modified content\n");
        GitCommitFile(dir.string(), "ch.md", "modified");

        // Restore to the original commit (last in log = oldest).
        bool ok = !log.empty() && GitRestoreFile(dir.string(), log[0].hash, "ch.md");
        std::string content = read_file(dir / "ch.md");
        bool restored = content.find("original") != std::string::npos;

        if (!ok || !restored) {
            std::cerr << "FAIL [git-restore]: ok=" << ok << " content='" << content << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [git-restore]\n";
        }
        fs::remove_all(dir);
    }

    // GitDiffHTML returns non-empty HTML when there are differences.
    {
        auto dir = make_temp_dir();
        GitInit(dir.string());
        shell("git -C " + dir.string() + " config user.email 'test@test.com' 2>/dev/null");
        shell("git -C " + dir.string() + " config user.name 'Test' 2>/dev/null");

        write_file(dir / "doc.md", "line one\nline two\n");
        GitCommitFile(dir.string(), "doc.md", "v1");
        write_file(dir / "doc.md", "line one\nline TWO changed\n");
        GitCommitFile(dir.string(), "doc.md", "v2");

        auto log = GitLogFile(dir.string(), "doc.md");
        bool ok = log.size() >= 2;
        std::string html = ok ? GitDiffHTML(dir.string(), log[1].hash, log[0].hash, "doc.md") : "";
        bool hasHtml    = html.find("<!DOCTYPE") != std::string::npos;
        bool hasDiff    = html.find("TWO changed") != std::string::npos;

        if (!ok || !hasHtml || !hasDiff) {
            std::cerr << "FAIL [git-diff-html]: ok=" << ok
                      << " hasHtml=" << hasHtml << " hasDiff=" << hasDiff << "\n";
            ++failures;
        } else {
            std::cout << "PASS [git-diff-html]\n";
        }
        fs::remove_all(dir);
    }

    return failures;
}
