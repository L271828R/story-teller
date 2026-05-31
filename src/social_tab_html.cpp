#include "social_tab_html.h"
#include "social_tab_html_data.h"
#include <map>
#include <sstream>

static std::string escJs(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
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

std::string BuildSocialTabHTML(const std::vector<std::string>& docs,
                                const std::string& selectedDoc,
                                bool darkMode,
                                const std::map<std::string,SocialDocResults>& allResults) {
    std::ostringstream docsJs;
    docsJs << "[";
    for (size_t i = 0; i < docs.size(); ++i) {
        if (i) docsJs << ",";
        docsJs << "\"" << escJs(docs[i]) << "\"";
    }
    docsJs << "]";

    std::string selJs = "\"" + escJs(selectedDoc) + "\"";

    // Serialize allResults as nested JS object keyed by doc name
    auto serEntries = [&](const std::vector<SocialEntry>& v) {
        std::ostringstream s;
        s << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) s << ",";
            s << "{\"text\":\"" << escJs(v[i].text)
              << "\",\"meta\":\"" << escJs(v[i].meta) << "\"}";
        }
        s << "]";
        return s.str();
    };

    std::ostringstream savedJs;
    savedJs << "{";
    bool firstDoc = true;
    for (const auto& kv : allResults) {
        if (!firstDoc) savedJs << ",";
        firstDoc = false;
        const auto& r = kv.second;
        savedJs << "\"" << escJs(kv.first) << "\":"
                << "{\"hooks\":"       << serEntries(r.hooks)
                << ",\"packaging\":"   << serEntries(r.packaging)
                << ",\"atomization\":" << serEntries(r.atomization)
                << "}";
    }
    savedJs << "}";

    std::string html(
        reinterpret_cast<const char*>(social_tab_html_data),
        social_tab_html_data_len);

    auto replace = [&](const std::string& ph, const std::string& val) {
        size_t pos;
        while ((pos = html.find(ph)) != std::string::npos)
            html.replace(pos, ph.size(), val);
    };

    replace("{{DOCS_JS}}",    docsJs.str());
    replace("{{SEL_DOC_JS}}", selJs);
    replace("{{SAVED_JS}}",   savedJs.str());
    replace("{{BODY_CLASS}}", darkMode ? "dark" : "");

    return html;
}
