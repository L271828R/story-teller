#include "persona.h"
#include "persona_panel_html.h"
#include "conversation.h"
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

    // RenamePersonaImage: renames the image file on disk
    {
        // Create a temp personas dir and a fake image file
        std::string tmpDir = "/tmp/st_persona_rename_test";
        mkdir(tmpDir.c_str(), 0755);
        std::string oldFile = tmpDir + "/alice.png";
        std::string newFile = tmpDir + "/alicia.png";
        { std::ofstream f(oldFile); f << "fake"; }

        bool ok = RenamePersonaImage("alice", "Alicia", tmpDir);
        struct stat st;
        bool newExists = (::stat(newFile.c_str(), &st) == 0);
        bool oldGone   = (::stat(oldFile.c_str(), &st) != 0);

        if (!ok || !newExists || !oldGone) {
            std::cerr << "FAIL [rename-persona-image]: ok=" << ok
                      << " newExists=" << newExists << " oldGone=" << oldGone << "\n";
            ++failures;
        } else {
            std::cout << "PASS [rename-persona-image]\n";
            ::unlink(newFile.c_str());
        }
        ::rmdir(tmpDir.c_str());
    }

    // RenamePersonaImage returns false when source doesn't exist
    {
        bool ok = RenamePersonaImage("nobody", "someone", "/tmp/no_such_dir_st");
        if (ok) {
            std::cerr << "FAIL [rename-persona-image-missing]: expected false\n";
            ++failures;
        } else {
            std::cout << "PASS [rename-persona-image-missing]\n";
        }
    }

    // GetPersonaChatsDir: returns a non-empty path
    {
        std::string d = GetPersonaChatsDir();
        if (d.empty()) {
            std::cerr << "FAIL [persona-chats-dir]\n"; ++failures;
        } else {
            std::cout << "PASS [persona-chats-dir]  (" << d << ")\n";
        }
    }

    // LoadPersonaConversation: non-existent returns empty
    {
        auto turns = LoadPersonaConversation("no_such_persona_xyz", "/tmp/no_such_dir_pchat");
        if (!turns.empty()) {
            std::cerr << "FAIL [persona-chat-load-empty]: expected 0 turns\n"; ++failures;
        } else {
            std::cout << "PASS [persona-chat-load-empty]\n";
        }
    }

    // SavePersonaConversation + LoadPersonaConversation: roundtrip
    {
        std::string tmpDir = "/tmp/st_pchat_" + std::to_string(getpid());
        mkdir(tmpDir.c_str(), 0755);

        std::vector<ConversationTurn> turns = {
            {"What is E=mc²?",  "Mass-energy equivalence."},
            {"Who discovered it?", "I did, in 1905."},
        };
        SavePersonaConversation("Albert Einstein", turns, tmpDir);

        auto loaded = LoadPersonaConversation("Albert Einstein", tmpDir);

        // cleanup
        std::string f = tmpDir + "/albert_einstein.md";
        ::unlink(f.c_str());
        ::rmdir(tmpDir.c_str());

        bool ok = loaded.size() == 2 &&
                  loaded[0].question == turns[0].question &&
                  loaded[0].answer   == turns[0].answer   &&
                  loaded[1].question == turns[1].question  &&
                  loaded[1].answer   == turns[1].answer;
        if (!ok) {
            std::cerr << "FAIL [persona-chat-roundtrip]: got " << loaded.size() << " turns\n";
            ++failures;
        } else {
            std::cout << "PASS [persona-chat-roundtrip]\n";
        }
    }

    // SavePersonaConversation empty vector: removes the file
    {
        std::string tmpDir = "/tmp/st_pchat2_" + std::to_string(getpid());
        mkdir(tmpDir.c_str(), 0755);

        std::vector<ConversationTurn> turns = {{"Q", "A"}};
        SavePersonaConversation("Marie Curie", turns, tmpDir);
        SavePersonaConversation("Marie Curie", {}, tmpDir);

        auto loaded = LoadPersonaConversation("Marie Curie", tmpDir);
        ::rmdir(tmpDir.c_str());   // file gone so rmdir should succeed

        if (!loaded.empty()) {
            std::cerr << "FAIL [persona-chat-clear]: expected 0 turns, got " << loaded.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [persona-chat-clear]\n";
        }
    }

    // RenamePersonaChat: moves the chat file
    {
        std::string tmpDir = "/tmp/st_pchat3_" + std::to_string(getpid());
        mkdir(tmpDir.c_str(), 0755);

        std::vector<ConversationTurn> turns = {{"Hello?", "Greetings."}};
        SavePersonaConversation("Old Name", turns, tmpDir);
        RenamePersonaChat("Old Name", "New Name", tmpDir);

        auto loaded = LoadPersonaConversation("New Name", tmpDir);

        std::string oldFile = tmpDir + "/old_name.md";
        std::string newFile = tmpDir + "/new_name.md";
        ::unlink(oldFile.c_str());
        ::unlink(newFile.c_str());
        ::rmdir(tmpDir.c_str());

        bool ok = loaded.size() == 1 && loaded[0].question == "Hello?";
        if (!ok) {
            std::cerr << "FAIL [persona-chat-rename]: got " << loaded.size() << " turns\n";
            ++failures;
        } else {
            std::cout << "PASS [persona-chat-rename]\n";
        }
    }

    // Project-scoped isolation: chat saved in project dir is invisible from global dir
    {
        std::string globalDir = "/tmp/st_proj_global_" + std::to_string(getpid());
        std::string projDir   = "/tmp/st_proj_darwin_" + std::to_string(getpid());
        mkdir(globalDir.c_str(), 0755);
        mkdir(projDir.c_str(),   0755);

        std::vector<ConversationTurn> turns = {{"Darwin?", "Evolution!"}};
        SavePersonaConversation("Colombian fiero", turns, projDir);

        auto fromGlobal = LoadPersonaConversation("Colombian fiero", globalDir);
        auto fromProj   = LoadPersonaConversation("Colombian fiero", projDir);

        std::string file = projDir + "/colombian_fiero.md";
        ::unlink(file.c_str());
        ::rmdir(projDir.c_str());
        ::rmdir(globalDir.c_str());

        bool ok = fromGlobal.empty() && fromProj.size() == 1 &&
                  fromProj[0].question == "Darwin?";
        if (!ok) {
            std::cerr << "FAIL [persona-chat-project-isolation]: "
                      << "global=" << fromGlobal.size() << " proj=" << fromProj.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [persona-chat-project-isolation]\n";
        }
    }

    // GroupChatKey: sorts and normalizes participant names
    {
        std::string key = GroupChatKey({"Caveman", "Cavewoman"});
        if (key != "caveman+cavewoman") {
            std::cerr << "FAIL [group-chat-key]: got '" << key << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [group-chat-key]\n";
        }
    }

    // GroupChatKey: input order doesn't matter
    {
        std::string k1 = GroupChatKey({"Alice", "Bob", "Charlie"});
        std::string k2 = GroupChatKey({"Charlie", "Alice", "Bob"});
        if (k1 != k2 || k1 != "alice+bob+charlie") {
            std::cerr << "FAIL [group-chat-key-order]: got '" << k1 << "' vs '" << k2 << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [group-chat-key-order]\n";
        }
    }

    // SerializeGroupChat / ParseGroupChat roundtrip
    {
        std::vector<std::string> participants = {"Alice", "Bob"};
        std::vector<MultiChatTurn> turns = {
            {"Hello!", {{"Alice", "Hi there."}, {"Bob", "Greetings!"}}},
            {"How are you?", {{"Alice", "Fine."}, {"Bob", "Great."}}}
        };
        std::string body = SerializeGroupChat(turns, participants);
        std::vector<std::string> outParts;
        auto loaded = ParseGroupChat(body, outParts);
        bool ok = outParts == participants &&
                  loaded.size() == 2 &&
                  loaded[0].userMessage == "Hello!" &&
                  loaded[0].responses.size() == 2 &&
                  loaded[0].responses[0].first  == "Alice" &&
                  loaded[0].responses[0].second == "Hi there." &&
                  loaded[1].userMessage == "How are you?" &&
                  loaded[1].responses[1].second == "Great.";
        if (!ok) {
            std::cerr << "FAIL [group-chat-roundtrip]: parts=" << outParts.size()
                      << " turns=" << loaded.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [group-chat-roundtrip]\n";
        }
    }

    // SaveGroupConversation / LoadGroupConversation / ListGroupChats
    {
        std::string tmpDir = "/tmp/st_gchat_" + std::to_string(getpid());
        mkdir(tmpDir.c_str(), 0755);

        std::vector<std::string> participants = {"Caveman", "Cavewoman"};
        std::vector<MultiChatTurn> turns = {
            {"Fire good!", {{"Caveman", "Ugh!"}, {"Cavewoman", "Warm!"}}}
        };
        SaveGroupConversation(participants, turns, tmpDir);

        auto loaded = LoadGroupConversation(participants, tmpDir);
        auto groups = ListGroupChats(tmpDir);

        std::string key = GroupChatKey(participants);
        std::string file = tmpDir + "/group_" + key + ".md";
        ::unlink(file.c_str());
        ::rmdir(tmpDir.c_str());

        bool ok = loaded.size() == 1 &&
                  loaded[0].userMessage == "Fire good!" &&
                  loaded[0].responses[0].first == "Caveman" &&
                  groups.size() == 1 &&
                  groups[0].key == key;
        if (!ok) {
            std::cerr << "FAIL [group-chat-save-load]: loaded=" << loaded.size()
                      << " groups=" << groups.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [group-chat-save-load]\n";
        }
    }

    return failures;
}
