#pragma once
#include <string>
#include <vector>

// Returns the wxNotebook index at which to re-insert a tab being made visible.
// visibility[i] = true means the i-th canonical tab is currently in the notebook.
// origIdx is the position of the tab in the canonical (always-ordered) list.
inline int TabInsertPosition(const std::vector<bool>& visibility, int origIdx) {
    int pos = 0;
    for (int i = 0; i < origIdx; ++i)
        if (visibility[i]) ++pos;
    return pos;
}

// Collapse all whitespace runs (incl. newlines) into single spaces and truncate
// to maxLen characters with an ellipsis. Used on free-text fields embedded in
// single-line labels so a multi-line value can't reflow the parent layout.
inline std::string CollapseToOneLine(const std::string& s, size_t maxLen) {
    std::string out;
    out.reserve(std::min(s.size(), maxLen));
    bool prevSpace = false;
    for (char c : s) {
        char ch = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
        if (ch == ' ') {
            if (prevSpace) continue;
            prevSpace = true;
        } else {
            prevSpace = false;
        }
        out += ch;
        if (out.size() >= maxLen) { out += "\xE2\x80\xA6"; break; }  // …
    }
    return out;
}
