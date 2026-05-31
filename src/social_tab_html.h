#pragma once
#include <string>
#include <vector>
#include <map>
#include "social_results.h"

std::string BuildSocialTabHTML(const std::vector<std::string>& docs,
                                const std::string& selectedDoc,
                                bool darkMode,
                                const std::map<std::string,SocialDocResults>& allResults = {});
