#include "../src/js_util.h"
#include <iostream>
#include <string>

int test_js_util() {
    int failures = 0;
    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [" << name << "]\n";
    };

    // Basic wrapping
    check("jsstr-basic",       JsStr("hello")         == "'hello'");
    check("jsstr-empty",       JsStr("")               == "''");

    // Single-quote must be escaped so it doesn't close the literal
    check("jsstr-single-quote", JsStr("it's") == "'it\\'s'");

    // Backslash must be escaped
    check("jsstr-backslash",   JsStr("a\\b")           == "'a\\\\b'");

    // Newline in topic caused "Unexpected EOF" in WKWebView/JavaScriptCore
    check("jsstr-newline",     JsStr("line1\nline2")   == "'line1\\nline2'");
    check("jsstr-cr",          JsStr("a\rb")           == "'a\\rb'");
    check("jsstr-crlf",        JsStr("a\r\nb")         == "'a\\r\\nb'");

    return failures;
}
