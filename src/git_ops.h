#pragma once
#include <string>
#include <vector>

struct GitCommit {
    std::string hash;      // full 40-char hash
    std::string shortHash; // 7-char abbreviation
    std::string subject;   // first line of commit message
    std::string date;      // YYYY-MM-DD
};

// Initialise a git repo in dir. Safe to call on an existing repo.
bool GitInit(const std::string& dir);

// Stage relPath and commit it. relPath is relative to projectDir.
bool GitCommitFile(const std::string& projectDir, const std::string& relPath,
                   const std::string& message);

// Return git log for relPath (newest first).
std::vector<GitCommit> GitLogFile(const std::string& projectDir,
                                  const std::string& relPath);

// Return the content of relPath at the given commit hash.
std::string GitShowFile(const std::string& projectDir, const std::string& hash,
                        const std::string& relPath);

// Overwrite the working copy of relPath with its state at hash.
bool GitRestoreFile(const std::string& projectDir, const std::string& hash,
                    const std::string& relPath);

// Return an HTML page showing the diff of relPath between hash1 and hash2.
// Pass "" for hash2 to diff against the current working file.
std::string GitDiffHTML(const std::string& projectDir,
                        const std::string& hash1, const std::string& hash2,
                        const std::string& relPath);
