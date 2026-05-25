#pragma once
#include <string>
#include <vector>
#include <utility>

struct ImageTabDoc {
    std::string              name;     // filename, e.g. "chapter1.md"
    std::vector<std::string> headings; // level-2 headings from that file
};

// images:      list of (filename, base64-data-url) pairs already in the project.
// docs:        all .md files in the project with their headings.
// selectedDoc: filename of the doc pre-selected in the dropdown.
std::string BuildImageTabHTML(
    const std::vector<std::pair<std::string,std::string>>& images,
    const std::vector<ImageTabDoc>& docs,
    const std::string& selectedDoc,
    bool darkMode);
