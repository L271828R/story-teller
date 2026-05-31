#include "social_tab_html.h"
#include <iostream>
#include <string>
#include <vector>

#define CHECK(label, cond) \
    do { if (cond) { std::cout << "PASS [" label "]\n"; } \
         else { std::cerr << "FAIL [" label "]\n"; ++failures; } } while(0)

int test_social_tab_html() {
    int failures = 0;

    // Dark mode class present
    {
        std::string h = BuildSocialTabHTML({}, "", true);
        CHECK("social-tab-dark-class", h.find("dark") != std::string::npos);
    }

    // No dark class in light mode
    {
        std::string h = BuildSocialTabHTML({}, "", false);
        CHECK("social-tab-no-dark-in-light", h.find("class=\"dark\"") == std::string::npos);
    }

    // Script message handler registered
    {
        std::string h = BuildSocialTabHTML({}, "", false);
        CHECK("social-tab-message-handler", h.find("socialtab") != std::string::npos);
    }

    // Document names appear in rendered HTML
    {
        std::vector<std::string> docs = {"darwin.md", "newton.md"};
        std::string h = BuildSocialTabHTML(docs, "darwin.md", false);
        CHECK("social-tab-doc-darwin", h.find("darwin.md") != std::string::npos);
        CHECK("social-tab-doc-newton", h.find("newton.md") != std::string::npos);
    }

    // Selected doc is reflected in JS
    {
        std::vector<std::string> docs = {"darwin.md", "newton.md"};
        std::string h = BuildSocialTabHTML(docs, "newton.md", false);
        CHECK("social-tab-selected-doc", h.find("newton.md") != std::string::npos);
    }

    // Hook section present
    {
        std::string h = BuildSocialTabHTML({}, "", false);
        bool hasHook = h.find("hook") != std::string::npos || h.find("Hook") != std::string::npos;
        CHECK("social-tab-hook-section", hasHook);
    }

    // Packaging section present
    {
        std::string h = BuildSocialTabHTML({}, "", false);
        bool hasPkg = h.find("Packaging") != std::string::npos
                   || h.find("packaging") != std::string::npos;
        CHECK("social-tab-packaging-section", hasPkg);
    }

    // Atomization section present
    {
        std::string h = BuildSocialTabHTML({}, "", false);
        bool hasAtom = h.find("Atomiz") != std::string::npos
                    || h.find("clip") != std::string::npos
                    || h.find("Clip") != std::string::npos;
        CHECK("social-tab-atomization-section", hasAtom);
    }

    // Copy button routes through send(), not navigator.clipboard
    {
        std::string h = BuildSocialTabHTML({}, "", false);
        CHECK("social-tab-copy-uses-send",    h.find("action:'copy'") != std::string::npos
                                           || h.find("action:\"copy\"") != std::string::npos);
        CHECK("social-tab-no-navigator-clipboard", h.find("navigator.clipboard") == std::string::npos);
    }

    return failures;
}
