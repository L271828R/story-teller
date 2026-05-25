#include "quiz_tab_html.h"
#include "quiz_tab_html_data.h"
#include <sstream>

static std::string escJs(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': break;
            default:   out += c;     break;
        }
    }
    return out;
}

std::string BuildQuizTabHTML(const std::vector<QuizTabDoc>& docs,
                              const std::string& selectedDoc,
                              bool darkMode) {
    // Build _docs JS array
    std::ostringstream docsJs;
    docsJs << "[";
    for (size_t i = 0; i < docs.size(); ++i) {
        if (i) docsJs << ",";
        docsJs << "{\"name\":\"" << escJs(docs[i].name) << "\",\"questions\":[";
        for (size_t j = 0; j < docs[i].questions.size(); ++j) {
            if (j) docsJs << ",";
            docsJs << "{\"num\":" << docs[i].questions[j].num
                   << ",\"rawBlock\":\"" << escJs(docs[i].questions[j].rawBlock) << "\"}";
        }
        docsJs << "]}";
    }
    docsJs << "]";

    std::string selJs = "\"" + escJs(selectedDoc) + "\"";

    std::string html(
        reinterpret_cast<const char*>(quiz_tab_html_data),
        quiz_tab_html_data_len);

    // Replace placeholders
    auto replace = [&](const std::string& ph, const std::string& val) {
        size_t pos;
        while ((pos = html.find(ph)) != std::string::npos)
            html.replace(pos, ph.size(), val);
    };

    replace("{{DOCS_JS}}",    docsJs.str());
    replace("{{SEL_DOC_JS}}", selJs);
    replace("{{BODY_CLASS}}", darkMode ? "dark" : "");

    return html;
}
