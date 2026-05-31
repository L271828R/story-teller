#pragma once
#include <string>
#include <map>

// Reads .image_captions.json from projectDir.
// Returns a map of filename → caption text.
std::map<std::string,std::string> LoadImageCaptions(const std::string& projectDir);

// Saves a single caption for filename into .image_captions.json.
// Creates or updates the file; other entries are preserved.
void SaveImageCaption(const std::string& projectDir,
                      const std::string& filename,
                      const std::string& caption);

// Returns the caption for filename, or "" if none is stored.
std::string GetImageCaption(const std::string& projectDir,
                             const std::string& filename);
