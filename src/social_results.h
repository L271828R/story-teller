#pragma once
#include <string>
#include <vector>
#include <map>

struct SocialEntry {
    std::string text;
    std::string meta; // e.g. "english/high" for hooks, "" for others
};

struct SocialDocResults {
    std::vector<SocialEntry> hooks;
    std::vector<SocialEntry> packaging;
    std::vector<SocialEntry> atomization;
};

// Load saved results for all docs in projectDir.
// Keys are doc filenames (e.g. "darwin.md").
std::map<std::string,SocialDocResults> LoadAllSocialResults(const std::string& projectDir);

// Append one entry to the given section ("hooks", "packaging", "atomization").
void AppendSocialResult(const std::string& projectDir,
                        const std::string& docName,
                        const std::string& section,
                        const SocialEntry& entry);

// Delete all saved results for section in docName.
void ClearSocialSection(const std::string& projectDir,
                        const std::string& docName,
                        const std::string& section);
