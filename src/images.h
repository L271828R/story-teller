#pragma once
#include <string>
#include <vector>

// Returns all level-2 heading texts from a markdown document, in order.
// e.g. "## Chapter 1: The Boy Who Asked Why" → "Chapter 1: The Boy Who Asked Why"
std::vector<std::string> ExtractHeadings(const std::string& mdContent);

// Inserts  ![|size|align](filename)  into mdContent.
// headingText: heading to insert after (stripped of "## "); empty = top of file.
// size: "small" | "medium" | "large" | "full"
// align: "left" | "center" | "right"
// Returns the modified markdown string.
std::string InsertImageAnchor(const std::string& mdContent,
                              const std::string& filename,
                              const std::string& headingText,
                              const std::string& size  = "medium",
                              const std::string& align = "center");

// Removes the ![...](filename) line from mdContent. Returns modified string.
std::string RemoveImageAnchor(const std::string& mdContent,
                              const std::string& filename);

// Post-processes rendered HTML: replaces  <img src="local.jpg">  with
// base64 data URLs for any image file found in projectDir.
// Also parses  alt="|size|align"  to emit CSS classes.
std::string SubstituteLocalImages(const std::string& html,
                                  const std::string& projectDir);

// Lists image filenames (jpg/jpeg/png/gif/webp/svg) in projectDir.
std::vector<std::string> ScanProjectImages(const std::string& projectDir);
