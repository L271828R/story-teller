#include "persona.h"
#include "persona_panel_html.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int test_persona() {
    int failures = 0;

    // NormalizePersonaName: basic ASCII
    {
        std::string r = NormalizePersonaName("Agatha Christie");
        if (r != "agatha_christie") {
            std::cerr << "FAIL [normalize-name]: got '" << r << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [normalize-name]\n";
        }
    }

    // NormalizePersonaName: multiple spaces collapse to single underscore
    {
        std::string r = NormalizePersonaName("Albert  Einstein");
        if (r != "albert_einstein") {
            std::cerr << "FAIL [normalize-spaces]: got '" << r << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [normalize-spaces]\n";
        }
    }

    // NormalizePersonaName: punctuation stripped
    {
        std::string r = NormalizePersonaName("Sir Arthur Conan Doyle");
        if (r != "sir_arthur_conan_doyle") {
            std::cerr << "FAIL [normalize-multi-word]: got '" << r << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [normalize-multi-word]\n";
        }
    }

    // NormalizePersonaName: trailing spaces don't leave trailing underscore
    {
        std::string r = NormalizePersonaName("Darwin ");
        if (!r.empty() && r.back() == '_') {
            std::cerr << "FAIL [normalize-trailing]: got '" << r << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [normalize-trailing]\n";
        }
    }

    // GetPersonasDir: returns non-empty string
    {
        std::string d = GetPersonasDir();
        if (d.empty()) {
            std::cerr << "FAIL [personas-dir-nonempty]\n";
            ++failures;
        } else {
            std::cout << "PASS [personas-dir-nonempty]  (" << d << ")\n";
        }
    }

    // ScanPersonaImages: non-existent dir returns empty map without crashing
    {
        auto m = ScanPersonaImages();
        std::cout << "PASS [scan-personas-no-crash]  (" << m.size() << " found)\n";
    }

    // ExtractTidbitNames: finds :::tidbit[Name] patterns in markdown
    {
        std::string md =
            "## Chapter\n\n"
            "Some text.\n\n"
            ":::tidbit[Agatha Christie]\nSome tidbit content.\n:::\n\n"
            ":::tidbit[Albert Einstein]\nAnother tidbit.\n:::\n";
        auto names = ExtractTidbitNames(md);
        bool hasAgatha  = false, hasAlbert = false;
        for (const auto& n : names) {
            if (n == "Agatha Christie") hasAgatha = true;
            if (n == "Albert Einstein") hasAlbert = true;
        }
        if (!hasAgatha || !hasAlbert || names.size() < 2) {
            std::cerr << "FAIL [extract-tidbit-names]: got " << names.size()
                      << " names; agatha=" << hasAgatha << " albert=" << hasAlbert << "\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-tidbit-names]\n";
        }
    }

    // ExtractTidbitNames: empty document returns empty vector
    {
        auto names = ExtractTidbitNames("");
        if (!names.empty()) {
            std::cerr << "FAIL [extract-tidbit-empty]: expected 0 got " << names.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-tidbit-empty]\n";
        }
    }

    // ExtractTidbitNamesFromDir: non-existent dir returns empty without crash
    {
        auto names = ExtractTidbitNamesFromDir("/nonexistent/path/xyz");
        if (!names.empty()) {
            std::cerr << "FAIL [extract-dir-nonexistent]: expected empty, got " << names.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-dir-nonexistent]\n";
        }
    }

    // ExtractTidbitNamesFromDir: finds names across multiple files in a dir
    {
        // Write two temp files with different personas.
        std::string dir = "/tmp/st_persona_test_" + std::to_string(getpid());
        mkdir(dir.c_str(), 0755);
        {
            std::ofstream f(dir + "/a.md");
            f << ":::tidbit[Ada Lovelace]\nContent.\n:::\n";
        }
        {
            std::ofstream f(dir + "/b.md");
            f << ":::tidbit[Charles Babbage]\nContent.\n:::\n";
        }
        auto names = ExtractTidbitNamesFromDir(dir);
        bool hasAda = false, hasBabbage = false;
        for (const auto& n : names) {
            if (n == "Ada Lovelace")    hasAda     = true;
            if (n == "Charles Babbage") hasBabbage = true;
        }
        // cleanup
        ::unlink((dir + "/a.md").c_str());
        ::unlink((dir + "/b.md").c_str());
        ::rmdir(dir.c_str());
        if (!hasAda || !hasBabbage) {
            std::cerr << "FAIL [extract-dir-multi-file]: ada=" << hasAda
                      << " babbage=" << hasBabbage << "\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-dir-multi-file]\n";
        }
    }

    // ExtractTidbitNamesByCategory: groups by top-level subdir
    {
        std::string root = "/tmp/st_cat_test_" + std::to_string(getpid());
        mkdir(root.c_str(), 0755);
        mkdir((root + "/History").c_str(), 0755);
        mkdir((root + "/Literature").c_str(), 0755);
        { std::ofstream f(root + "/History/darwin.md");
          f << ":::tidbit[Charles Darwin]\nText.\n:::\n"; }
        { std::ofstream f(root + "/Literature/neruda.md");
          f << ":::tidbit[Pablo Neruda]\nText.\n:::\n"; }

        auto cats = ExtractTidbitNamesByCategory(root);

        ::unlink((root + "/History/darwin.md").c_str());
        ::unlink((root + "/Literature/neruda.md").c_str());
        ::rmdir((root + "/History").c_str());
        ::rmdir((root + "/Literature").c_str());
        ::rmdir(root.c_str());

        bool ok = cats.count("History") && cats.count("Literature") &&
                  !cats["History"].empty() && !cats["Literature"].empty() &&
                  cats["History"][0] == "Charles Darwin" &&
                  cats["Literature"][0] == "Pablo Neruda";
        if (!ok) {
            std::cerr << "FAIL [extract-by-category]\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-by-category]\n";
        }
    }

    // BuildPersonaPanelHTML: contains structural elements and persona data
    {
        std::map<std::string, std::vector<std::string>> cats = {
            {"History",    {"Charles Darwin"}},
            {"Literature", {"Pablo Neruda"}}
        };
        std::map<std::string, std::string> imgs;
        std::string html = BuildPersonaPanelHTML(cats, imgs, false);
        bool hasDoctype = html.find("<!DOCTYPE html>")  != std::string::npos;
        bool hasDarwin  = html.find("Charles Darwin")   != std::string::npos;
        bool hasHistory = html.find("History")           != std::string::npos;
        bool hasUpload  = html.find("upload")            != std::string::npos;
        if (!hasDoctype || !hasDarwin || !hasHistory || !hasUpload) {
            std::cerr << "FAIL [persona-panel-html]: doctype=" << hasDoctype
                      << " darwin=" << hasDarwin << " history=" << hasHistory
                      << " upload=" << hasUpload << "\n";
            ++failures;
        } else {
            std::cout << "PASS [persona-panel-html]\n";
        }
    }

    return failures;
}
