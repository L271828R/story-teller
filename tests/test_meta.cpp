#include "meta.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int test_meta() {
    int failures = 0;

    // RecordOpen writes lastOpened; reload confirms it is set.
    {
        auto dir = fs::temp_directory_path() / "st_meta_open";
        fs::create_directories(dir);
        EnsureProjectMeta(dir.string(), "Claude -p");
        RecordOpen(dir.string());
        auto meta = LoadProjectMeta(dir.string());
        bool ok = meta.lastOpened.size() == 19   // "YYYY-MM-DDTHH:MM:SS"
               && meta.lastOpened[4] == '-'
               && meta.lastOpened[10] == 'T'
               && meta.created.size() == 19
               && meta.source == "Claude -p";
        if (!ok) {
            std::cerr << "FAIL [meta-record-open]: lastOpened='" << meta.lastOpened << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-record-open]\n";
        }
        fs::remove_all(dir);
    }

    // RecordLLMTiming appends an entry that round-trips correctly.
    {
        auto dir = fs::temp_directory_path() / "st_meta_timing";
        fs::create_directories(dir);
        RecordLLMTiming(dir.string(), "generate", "JK Rowling Ch1", 87);
        RecordLLMTiming(dir.string(), "patch",    "fix chapter 2",  12);
        auto meta = LoadProjectMeta(dir.string());
        bool ok = meta.timings.size() == 2
               && meta.timings[0].operation       == "generate"
               && meta.timings[0].topic           == "JK Rowling Ch1"
               && meta.timings[0].durationSeconds == 87
               && meta.timings[1].operation       == "patch"
               && meta.timings[1].durationSeconds == 12;
        if (!ok) {
            std::cerr << "FAIL [meta-record-timing]: got " << meta.timings.size() << " entries\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-record-timing]\n";
        }
        fs::remove_all(dir);
    }

    // RecordLLMTiming stores backend and round-trips it.
    {
        auto dir = fs::temp_directory_path() / "st_meta_timing_backend";
        fs::create_directories(dir);
        RecordLLMTiming(dir.string(), "generate", "Elon ch1", 42, "Claude -p");
        RecordLLMTiming(dir.string(), "generate", "Elon ch2", 31, "Codex CLI");
        auto meta = LoadProjectMeta(dir.string());
        bool ok = meta.timings.size() == 2
               && meta.timings[0].backend == "Claude -p"
               && meta.timings[1].backend == "Codex CLI";
        if (!ok) {
            std::cerr << "FAIL [meta-timing-backend]: t0.backend='"
                      << (meta.timings.size() > 0 ? meta.timings[0].backend : "?")
                      << "' t1.backend='"
                      << (meta.timings.size() > 1 ? meta.timings[1].backend : "?") << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-timing-backend]\n";
        }
        fs::remove_all(dir);
    }

    // ScanAllTimings aggregates timings across multiple project dirs, newest-first.
    {
        auto root = fs::temp_directory_path() / "st_scan_timings";
        auto p1   = root / "projA";
        auto p2   = root / "projB";
        fs::create_directories(p1);
        fs::create_directories(p2);
        RecordLLMTiming(p1.string(), "generate", "chapter1", 10, "Claude -p");
        RecordLLMTiming(p2.string(), "generate", "chapter2", 20, "Codex CLI");
        auto all = ScanAllTimings(root.string());
        bool hasBoth  = all.size() >= 2;
        bool hasClaude = false, hasCodex = false;
        for (auto& t : all) {
            if (t.backend == "Claude -p") hasClaude = true;
            if (t.backend == "Codex CLI") hasCodex  = true;
        }
        if (!hasBoth || !hasClaude || !hasCodex) {
            std::cerr << "FAIL [meta-scan-all-timings]: size=" << all.size()
                      << " hasClaude=" << hasClaude << " hasCodex=" << hasCodex << "\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-scan-all-timings]\n";
        }
        fs::remove_all(root);
    }

    // SaveProjectMeta / LoadProjectMeta round-trip preserves all fields.
    {
        auto dir = fs::temp_directory_path() / "st_meta_roundtrip";
        fs::create_directories(dir);
        ProjectMeta m;
        m.created = "2026-05-16T14:00:00";
        m.lastOpened = "2026-05-16T14:23:00";
        m.source = "Codex CLI";
        LLMTiming t;
        t.timestamp = "2026-05-16T14:20:00";
        t.operation = "generate";
        t.topic = "topic with \"quotes\"";
        t.durationSeconds = 99;
        t.backend = "Codex CLI";
        m.timings.push_back(t);
        SaveProjectMeta(dir.string(), m);
        auto m2 = LoadProjectMeta(dir.string());
        bool ok = m2.created == m.created
               && m2.lastOpened == m.lastOpened
               && m2.source == m.source
               && m2.timings.size() == 1
               && m2.timings[0].topic           == "topic with \"quotes\""
               && m2.timings[0].durationSeconds == 99
               && m2.timings[0].backend         == "Codex CLI";
        if (!ok) {
            std::cerr << "FAIL [meta-roundtrip]\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-roundtrip]\n";
        }
        fs::remove_all(dir);
    }

    // RecordProjectSource updates source without losing existing metadata.
    {
        auto dir = fs::temp_directory_path() / "st_meta_source";
        fs::create_directories(dir);
        EnsureProjectMeta(dir.string(), "Claude -p");
        RecordProjectSource(dir.string(), "Codex CLI");
        auto meta = LoadProjectMeta(dir.string());
        bool ok = meta.created.size() == 19 && meta.source == "Codex CLI";
        if (!ok) {
            std::cerr << "FAIL [meta-source]: source='" << meta.source << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-source]\n";
        }
        fs::remove_all(dir);
    }

    // ArchiveTimingEntry marks an entry archived and persists it.
    {
        auto root = fs::temp_directory_path() / "st_meta_archive";
        auto proj = root / "History" / "Mozart";
        fs::create_directories(proj);
        RecordLLMTiming((root / "History" / "Mozart").string(), "generate", "mozart ch1", 30, "claude -p");
        RecordLLMTiming((root / "History" / "Mozart").string(), "generate", "mozart ch2", 25, "claude -p");
        auto before = LoadProjectMeta(proj.string());

        // Archive the first entry
        std::string ts = before.timings[0].timestamp;
        ArchiveTimingEntry(root.string(), "History/Mozart", ts, true);
        auto after = LoadProjectMeta(proj.string());
        bool firstArchived  = after.timings[0].archived;
        bool secondNotArchived = !after.timings[1].archived;

        // Unarchive it
        ArchiveTimingEntry(root.string(), "History/Mozart", ts, false);
        auto unarchived = LoadProjectMeta(proj.string());
        bool restored = !unarchived.timings[0].archived;

        bool ok = firstArchived && secondNotArchived && restored;
        if (!ok) {
            std::cerr << "FAIL [meta-archive-entry]: firstArchived=" << firstArchived
                      << " secondNotArchived=" << secondNotArchived
                      << " restored=" << restored << "\n";
            ++failures;
        } else {
            std::cout << "PASS [meta-archive-entry]\n";
        }
        fs::remove_all(root);
    }

    return failures;
}
