#pragma once
#include <string>

// Wraps s in single quotes and escapes characters that would break a
// JavaScript string literal: \, ', newlines, carriage returns, and NUL.
std::string JsStr(const std::string& s);
