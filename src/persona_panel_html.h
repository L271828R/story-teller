#pragma once
#include <map>
#include <string>
#include <vector>

// Build the full HTML page for the Manage Personas panel.
// categories: top-level category name → sorted list of persona display names
// images:     normalized_name → file:// URL (personas that already have images)
std::string BuildPersonaPanelHTML(
    const std::map<std::string, std::vector<std::string>>& categories,
    const std::map<std::string, std::string>& images,
    bool darkMode);
