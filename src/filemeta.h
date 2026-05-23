#pragma once
#include <string>

// Reads the language from a "<!-- language: X -->" comment in the first few
// lines of filePath. Returns "" if the comment is absent or the file can't be read.
std::string ReadLanguage(const std::string& filePath);

// Writes (or updates) a "<!-- language: X -->" comment at the top of filePath.
// If the comment already exists it is replaced in-place; otherwise it is prepended.
// Returns false on I/O error.
bool WriteLanguage(const std::string& filePath, const std::string& language);

// Returns the file's last-modification time as "YYYY-MM-DD HH:MM", or "" on error.
std::string FileModifiedTime(const std::string& filePath);

// Returns the file's creation (birth) time as "YYYY-MM-DD HH:MM", or "" on error.
// Falls back to modification time on platforms that don't expose birth time.
std::string FileCreatedTime(const std::string& filePath);
