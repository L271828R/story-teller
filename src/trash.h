#pragma once
#include <string>

// Moves the file/directory at `path` to the macOS Trash. On success returns
// true; on failure returns false and populates `outError` with a human-readable
// message. Prefer this over std::filesystem::remove_all so a mistaken Delete
// is recoverable.
bool TrashPath(const std::string& path, std::string& outError);
