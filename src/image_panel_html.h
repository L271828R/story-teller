#pragma once
#include <string>
#include <vector>
#include <utility>

// images: list of (filename, base64-data-url) pairs already in the project.
// headings: level-2 headings from the current document (for the dropdown).
std::string BuildImagePanelHTML(
    const std::vector<std::pair<std::string,std::string>>& images,
    const std::vector<std::string>& headings,
    bool darkMode);
