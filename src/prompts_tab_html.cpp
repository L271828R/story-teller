#include "prompts_tab_html.h"
#include "prompts_tab_html_data.h"
#include "js_util.h"

static std::string escHtml(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;";  break;
            case '<': o += "&lt;";   break;
            case '>': o += "&gt;";   break;
            case '"': o += "&quot;"; break;
            default:  o += c;        break;
        }
    }
    return o;
}

static std::string previewText(const std::string& s) {
    auto t = s.substr(0, 160);
    return escHtml(t);
}

std::string BuildPromptsTabHTML(const std::vector<SavedPrompt>& prompts,
                                 bool darkMode) {
    // Build the prompt cards HTML
    std::string cards;
    if (prompts.empty()) {
        cards = "<div class=\"empty-msg\">No saved prompts yet. Add one below.</div>";
    } else {
        for (const auto& p : prompts) {
            std::string sid = std::to_string(p.id);
            cards +=
                "<div class=\"prompt-card\" id=\"pc-" + sid + "\">\n"
                "  <div class=\"prompt-header\">\n"
                "    <span class=\"prompt-title\">" + escHtml(p.title) + "</span>\n"
                "    <button onclick=\"editPrompt(" + sid + ")\">Edit</button>\n"
                "    <button class=\"danger\" onclick=\"deletePrompt(" + sid + ")\">Delete</button>\n"
                "  </div>\n"
                "  <div class=\"prompt-preview\" onclick=\"editPrompt(" + sid + ")\">"
                    + previewText(p.text) + "</div>\n"
                "  <div class=\"prompt-editor\" style=\"display:none\">\n"
                "    <div class=\"field\">\n"
                "      <label>Name</label>\n"
                "      <input class=\"edit-title\" type=\"text\" value=\"" + escHtml(p.title) + "\">\n"
                "    </div>\n"
                "    <div class=\"field\">\n"
                "      <label>Prompt text</label>\n"
                "      <textarea class=\"edit-text\">" + escHtml(p.text) + "</textarea>\n"
                "    </div>\n"
                "    <div class=\"btn-row\">\n"
                "      <button class=\"primary\" onclick=\"savePrompt(" + sid + ")\">Save</button>\n"
                "      <button onclick=\"cancelEdit(" + sid + ")\">Cancel</button>\n"
                "    </div>\n"
                "  </div>\n"
                "</div>\n";
        }
    }

    std::string html(
        reinterpret_cast<const char*>(prompts_tab_html_data),
        prompts_tab_html_data_len);

    auto replace = [&](const std::string& ph, const std::string& val) {
        size_t pos;
        while ((pos = html.find(ph)) != std::string::npos)
            html.replace(pos, ph.size(), val);
    };

    replace("{{PROMPTS_HTML}}", cards);
    replace("{{BODY_CLASS}}",   darkMode ? "dark" : "");

    return html;
}
