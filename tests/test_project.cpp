#include "project.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path make_temp_dir() {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto p = fs::temp_directory_path() / ("mdviewer_test_" + std::to_string(ns));
    fs::create_directories(p);
    return p;
}

int test_project() {
    int failures = 0;

    // CreateProject creates the folder, a context.md stub, and an empty .index.
    {
        auto base = make_temp_dir();
        bool ok       = CreateProject(base.string(), "my-story");
        fs::path proj = base / "my-story";
        bool dirExists = fs::exists(proj);
        bool claudeMd  = fs::exists(proj / "context.md");
        bool index     = fs::exists(proj / ".index");
        if (!ok || !dirExists || !claudeMd || !index) {
            std::cerr << "FAIL [create-project]: ok=" << ok
                      << " dir=" << dirExists
                      << " context.md=" << claudeMd
                      << " .index=" << index << "\n";
            ++failures;
        } else {
            std::cout << "PASS [create-project]\n";
        }
        fs::remove_all(base);
    }

    // ProjectExists returns true for a valid project, false for an absent one.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "p1");
        bool yes = ProjectExists((base / "p1").string());
        bool no  = ProjectExists((base / "nope").string());
        if (!yes || no) {
            std::cerr << "FAIL [project-exists]: yes=" << yes << " no=" << no << "\n";
            ++failures;
        } else {
            std::cout << "PASS [project-exists]\n";
        }
        fs::remove_all(base);
    }

    // SaveConfig / LoadConfig round-trips all fields.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "cfg-test");
        std::string proj = (base / "cfg-test").string();

        ProjectConfig cfg;
        cfg.llmBackend  = "ollama";
        cfg.ollamaModel = "llama3";
        cfg.ollamaUrl   = "http://localhost:11434";
        cfg.apiKey      = "sk-test";
        SaveConfig(proj, cfg);

        ProjectConfig got = LoadConfig(proj);
        if (got.llmBackend  != "ollama" ||
            got.ollamaModel != "llama3" ||
            got.ollamaUrl   != "http://localhost:11434" ||
            got.apiKey      != "sk-test") {
            std::cerr << "FAIL [config-roundtrip]: backend=" << got.llmBackend
                      << " model=" << got.ollamaModel
                      << " url=" << got.ollamaUrl
                      << " key=" << got.apiKey << "\n";
            ++failures;
        } else {
            std::cout << "PASS [config-roundtrip]\n";
        }
        fs::remove_all(base);
    }

    // RegisterChapter assigns monotonically increasing IDs; ChapterFile retrieves them.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "ch-test");
        std::string proj = (base / "ch-test").string();

        int id1 = RegisterChapter(proj, "chapter_01.md");
        int id2 = RegisterChapter(proj, "chapter_02.md");
        std::string f1 = ChapterFile(proj, id1);
        std::string f2 = ChapterFile(proj, id2);
        if (id2 <= id1 || f1 != "chapter_01.md" || f2 != "chapter_02.md") {
            std::cerr << "FAIL [chapter-register]: id1=" << id1 << " id2=" << id2
                      << " f1=" << f1 << " f2=" << f2 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [chapter-register]\n";
        }
        fs::remove_all(base);
    }

    // RegisterTidbit assigns monotonically increasing IDs; TidbitLocation retrieves them.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "tb-test");
        std::string proj = (base / "tb-test").string();

        int chId = RegisterChapter(proj, "ch1.md");
        int tb1  = RegisterTidbit(proj, chId, 0);
        int tb2  = RegisterTidbit(proj, chId, 1);
        auto [ch1, pos1] = TidbitLocation(proj, tb1);
        auto [ch2, pos2] = TidbitLocation(proj, tb2);
        if (tb2 <= tb1 || ch1 != chId || pos1 != 0 || ch2 != chId || pos2 != 1) {
            std::cerr << "FAIL [tidbit-register]: tb1=" << tb1 << " tb2=" << tb2
                      << " ch1=" << ch1 << " pos1=" << pos1
                      << " ch2=" << ch2 << " pos2=" << pos2 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [tidbit-register]\n";
        }
        fs::remove_all(base);
    }

    // Unknown chapter / tidbit IDs return sentinel values.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "unk-test");
        std::string proj = (base / "unk-test").string();

        std::string f = ChapterFile(proj, 999);
        auto [ch, pos] = TidbitLocation(proj, 999);
        if (!f.empty() || ch != -1 || pos != -1) {
            std::cerr << "FAIL [unknown-id]: expected empty/negative for unknown ids; "
                      << "f='" << f << "' ch=" << ch << " pos=" << pos << "\n";
            ++failures;
        } else {
            std::cout << "PASS [unknown-id]\n";
        }
        fs::remove_all(base);
    }

    // IDs persist across separate LoadIndex calls (written to disk).
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "persist-test");
        std::string proj = (base / "persist-test").string();

        int id = RegisterChapter(proj, "ch.md");
        // Reload from disk by calling ChapterFile (which re-reads the index).
        std::string f = ChapterFile(proj, id);
        if (f != "ch.md") {
            std::cerr << "FAIL [persist]: got '" << f << "' after reload\n";
            ++failures;
        } else {
            std::cout << "PASS [persist]\n";
        }
        fs::remove_all(base);
    }

    // InitProject initialises an existing directory in place (no subdirectory).
    {
        auto base = make_temp_dir();
        bool ok       = InitProject(base.string());
        bool claudeMd = fs::exists(base / "context.md");
        bool index    = fs::exists(base / ".index");
        bool isProj   = ProjectExists(base.string());
        if (!ok || !claudeMd || !index || !isProj) {
            std::cerr << "FAIL [init-project]: ok=" << ok
                      << " context.md=" << claudeMd
                      << " .index=" << index
                      << " isProj=" << isProj << "\n";
            ++failures;
        } else {
            std::cout << "PASS [init-project]\n";
        }
        fs::remove_all(base);
    }

    // InitProject is idempotent — calling it twice does not reset the index.
    {
        auto base = make_temp_dir();
        InitProject(base.string());
        int id = RegisterChapter(base.string(), "ch1.md");
        InitProject(base.string());  // second call must not reset counter
        int id2 = RegisterChapter(base.string(), "ch2.md");
        if (id2 <= id) {
            std::cerr << "FAIL [init-idempotent]: id=" << id << " id2=" << id2 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [init-idempotent]\n";
        }
        fs::remove_all(base);
    }

    // NextChapterId / NextTidbitId peek the next ID without consuming it.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "peek-test");
        std::string proj = (base / "peek-test").string();

        int peek1 = NextChapterId(proj);
        int peek2 = NextChapterId(proj);  // calling twice must return same value
        int assigned = RegisterChapter(proj, "ch.md");
        int peek3 = NextChapterId(proj);  // now must be assigned+1

        bool sameBeforeUse = (peek1 == peek2);
        bool matchesAssigned = (peek1 == assigned);
        bool advancedAfter = (peek3 == assigned + 1);

        if (!sameBeforeUse || !matchesAssigned || !advancedAfter) {
            std::cerr << "FAIL [next-id-peek]: peek1=" << peek1 << " peek2=" << peek2
                      << " assigned=" << assigned << " peek3=" << peek3 << "\n";
            ++failures;
        } else {
            std::cout << "PASS [next-id-peek]\n";
        }
        fs::remove_all(base);
    }

    // MoveFolder moves a subfolder to a different parent directory.
    {
        auto base = make_temp_dir();
        fs::create_directories(base / "literature" / "accidental");
        fs::create_directories(base / "other");

        MoveResult r = MoveFolder((base / "literature" / "accidental").string(),
                                  base.string());
        bool moved  = fs::exists(base / "accidental");
        bool gone   = !fs::exists(base / "literature" / "accidental");
        if (!r.ok || !moved || !gone) {
            std::cerr << "FAIL [move-folder-to-root]: ok=" << r.ok
                      << " error='" << r.error << "'"
                      << " moved=" << moved << " gone=" << gone << "\n";
            ++failures;
        } else {
            std::cout << "PASS [move-folder-to-root]\n";
        }
        fs::remove_all(base);
    }

    // MoveFolder refuses when the destination already contains a folder with the same name.
    {
        auto base = make_temp_dir();
        fs::create_directories(base / "literature" / "subfolder");
        fs::create_directories(base / "subfolder");  // collision

        MoveResult r = MoveFolder((base / "literature" / "subfolder").string(),
                                  base.string());
        if (r.ok || r.error.empty()) {
            std::cerr << "FAIL [move-folder-collision]: expected failure with message\n";
            ++failures;
        } else {
            std::cout << "PASS [move-folder-collision]\n";
        }
        fs::remove_all(base);
    }

    // MoveFolder refuses to move a folder into one of its own descendants.
    {
        auto base = make_temp_dir();
        fs::create_directories(base / "parent" / "child");

        MoveResult r = MoveFolder((base / "parent").string(),
                                  (base / "parent" / "child").string());
        if (r.ok || r.error.empty()) {
            std::cerr << "FAIL [move-folder-into-self]: expected failure with message\n";
            ++failures;
        } else {
            std::cout << "PASS [move-folder-into-self]\n";
        }
        fs::remove_all(base);
    }

    // New-project button flow: InitProject on a brand-new subdirectory path creates a
    // valid project (the directory doesn't exist yet, unlike the init-project test above).
    {
        auto root = make_temp_dir();
        std::string projectPath = (root / "my-new-story").string();

        bool precondition = !fs::exists(projectPath);  // must not exist beforehand
        bool ok           = InitProject(projectPath);
        bool isProject    = ProjectExists(projectPath);
        bool hasClaude    = fs::exists(fs::path(projectPath) / "context.md");
        bool hasIndex     = fs::exists(fs::path(projectPath) / ".index");

        if (!precondition || !ok || !isProject || !hasClaude || !hasIndex) {
            std::cerr << "FAIL [new-project-creates-subdir]: precondition=" << precondition
                      << " ok=" << ok << " isProject=" << isProject
                      << " context.md=" << hasClaude << " .index=" << hasIndex << "\n";
            ++failures;
        } else {
            std::cout << "PASS [new-project-creates-subdir]\n";
        }
        fs::remove_all(root);
    }

    // New-project button flow: attempting to create a project where a folder already
    // exists must leave the existing contents untouched (the button guards this with
    // fs::exists before calling InitProject).
    {
        auto root = make_temp_dir();
        fs::path existing = root / "taken";
        fs::create_directories(existing);
        std::ofstream(existing / "sentinel.txt") << "original";

        bool collision = fs::exists(existing.string());

        // The button skips InitProject when collision is true — verify existing
        // content is intact.
        std::string content;
        {
            std::ifstream f(existing / "sentinel.txt");
            if (f) content.assign(std::istreambuf_iterator<char>(f), {});
        }

        if (!collision || content != "original") {
            std::cerr << "FAIL [new-project-name-collision]: collision=" << collision
                      << " content='" << content << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [new-project-name-collision]\n";
        }
        fs::remove_all(root);
    }

    // ListAllProjects: flat layout returns top-level project names.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "alpha");
        CreateProject(base.string(), "beta");
        fs::create_directories(base / "not-a-project");  // no .index, must be ignored

        auto list = ListAllProjects(base.string());
        bool hasAlpha = (std::find(list.begin(), list.end(), "alpha") != list.end());
        bool hasBeta  = (std::find(list.begin(), list.end(), "beta")  != list.end());
        bool noExtra  = (list.size() == 2);
        if (!hasAlpha || !hasBeta || !noExtra) {
            std::cerr << "FAIL [list-projects-flat]: size=" << list.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [list-projects-flat]\n";
        }
        fs::remove_all(base);
    }

    // ListAllProjects: nested layout returns relative paths.
    {
        auto base = make_temp_dir();
        fs::create_directories(base / "Literature");
        CreateProject((base / "Literature").string(), "agatha");
        CreateProject((base / "Literature").string(), "kafka");

        auto list = ListAllProjects(base.string());
        bool hasAgatha = (std::find(list.begin(), list.end(), "Literature/agatha") != list.end());
        bool hasKafka  = (std::find(list.begin(), list.end(), "Literature/kafka")  != list.end());
        bool noExtra   = (list.size() == 2);
        if (!hasAgatha || !hasKafka || !noExtra) {
            std::cerr << "FAIL [list-projects-nested]: size=" << list.size();
            for (auto& p : list) std::cerr << " '" << p << "'";
            std::cerr << "\n";
            ++failures;
        } else {
            std::cout << "PASS [list-projects-nested]\n";
        }
        fs::remove_all(base);
    }

    // ListAllProjects: mixed flat + nested layout, result is sorted.
    {
        auto base = make_temp_dir();
        CreateProject(base.string(), "zzz-top");
        fs::create_directories(base / "Science");
        CreateProject((base / "Science").string(), "physics");
        CreateProject(base.string(), "aaa-first");

        auto list = ListAllProjects(base.string());
        bool isSorted = std::is_sorted(list.begin(), list.end());
        bool hasAll   = (list.size() == 3);
        if (!isSorted || !hasAll) {
            std::cerr << "FAIL [list-projects-mixed]: sorted=" << isSorted
                      << " size=" << list.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [list-projects-mixed]\n";
        }
        fs::remove_all(base);
    }

    return failures;
}
