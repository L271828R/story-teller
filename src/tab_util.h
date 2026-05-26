#pragma once
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
