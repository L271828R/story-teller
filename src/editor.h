#pragma once
#include <string>

// Returns the :::tidbit...:::  block that follows the <!-- tb:N --> marker
// in fileContent. Returns "" if the marker is not found.
std::string ExtractTidbit(const std::string& fileContent, int tidbitId);

// Returns a copy of fileContent with the tidbit block following <!-- tb:N -->
// replaced by newBlock. The <!-- tb:N --> marker itself is preserved.
// Returns fileContent unchanged if the marker is not found.
std::string PatchTidbit(const std::string& fileContent,
                        int tidbitId,
                        const std::string& newBlock);

// Reads the file at filepath, applies PatchTidbit, and writes the result back.
// Returns true on success.
bool ApplyTidbitPatch(const std::string& filepath,
                      int tidbitId,
                      const std::string& newBlock);

// Overwrites the file at filepath with newContent.
// Returns true on success.
bool ReplaceChapter(const std::string& filepath, const std::string& newContent);
