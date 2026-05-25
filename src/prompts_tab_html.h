#pragma once
#include "prompt_store.h"
#include <string>
#include <vector>

std::string BuildPromptsTabHTML(const std::vector<SavedPrompt>& prompts,
                                 bool darkMode);
