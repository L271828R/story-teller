#pragma once
#include <string>

// Replace the first occurrence of `from` in `s` with `to`.
inline void htmlSubst(std::string& s, const std::string& from, const std::string& to) {
    auto pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
}
