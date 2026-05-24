#pragma once
#include <string>

// Builds the self-contained HTML/CSS/JS page for the Create tab.
// The page communicates with C++ via window.webkit.messageHandlers.create.
std::string BuildCreatePanelHTML(bool darkMode);
