#include "../src/create_panel_html.h"
#include "../src/edit_panel_html.h"
#include "../src/html_template.h"
#include "../src/monitor_panel_html.h"
#include "../src/persona_panel_html.h"
#include "../src/image_tab_html.h"
#include "../src/quiz_tab_html.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Collect the content of every <script>...</script> block and concatenate.
// Handles multiple blocks (e.g. bundled libraries + our code) without including
// the intermediate </script> tags as JS tokens.
static std::string extract_js(const std::string& html) {
    std::string result;
    size_t pos = 0;
    while (pos < html.size()) {
        auto tag_open = html.find("<script", pos);
        if (tag_open == std::string::npos) break;
        auto tag_close = html.find('>', tag_open);
        if (tag_close == std::string::npos) break;
        // Skip external scripts (<script src="...">)
        if (html.substr(tag_open, tag_close - tag_open).find("src=") != std::string::npos) {
            pos = tag_close + 1;
            continue;
        }
        size_t js_start = tag_close + 1;
        auto js_end = html.find("</script>", js_start);
        if (js_end == std::string::npos) break;
        if (!result.empty()) result += "\n";
        result += html.substr(js_start, js_end - js_start);
        pos = js_end + 9; // skip </script>
    }
    return result;
}

static bool lint_js(const std::string& name, const std::string& html) {
    std::string js = extract_js(html);
    if (js.empty()) return true;

    std::string tmp = "/tmp/st_jslint_" + name + ".js";
    { std::ofstream f(tmp); f << js; }
    int rc = std::system(("node --check " + tmp + " 2>/dev/null").c_str());
    return rc == 0;
}

int test_js_lint() {
    int failures = 0;

    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [js-lint-" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [js-lint-" << name << "]\n";
    };

    check("create-panel",  lint_js("create_panel",  BuildCreatePanelHTML(false)));
    check("edit-panel",    lint_js("edit_panel",    BuildEditPanelHTML(false)));
    check("monitor-panel", lint_js("monitor_panel", BuildMonitorPanelHTML(false)));
    check("persona-panel", lint_js("persona_panel", BuildPersonaPanelHTML({}, {}, false)));
    check("main-viewer",   lint_js("main_viewer",   BuildHTML("<p>test</p>", "t", false, 100)));
    check("image-tab",     lint_js("image_tab",     BuildImageTabHTML({}, {}, "", false)));
    check("quiz-tab",      lint_js("quiz_tab",      BuildQuizTabHTML({}, "", false)));

    return failures;
}
