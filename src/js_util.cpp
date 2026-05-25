#include "js_util.h"

std::string JsStr(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if      (c == '\'') out += "\\'";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else                out += c;
    }
    return out + "'";
}
