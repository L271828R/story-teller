#include "image_tab.h"
#include "image_tab_html.h"
#include "images.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string jfField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        ++pos;
        std::string val;
        while (pos < json.size()) {
            char c = json[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < json.size()) {
                char e = json[pos++];
                switch (e) {
                    case '"':  val += '"';  break;
                    case '\\': val += '\\'; break;
                    case 'n':  val += '\n'; break;
                    default:   val += e;    break;
                }
            } else val += c;
        }
        return val;
    }
    std::string val;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}')
        val += json[pos++];
    return val;
}

static std::string lowerExtIt(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return "";
    std::string e = filename.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e;
}

static std::string mimeForExtIt(const std::string& ext) {
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png")  return "image/png";
    if (ext == "gif")  return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "svg")  return "image/svg+xml";
    return "image/jpeg";
}

static std::string base64EncIt(const std::vector<unsigned char>& d) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((d.size() + 2) / 3 * 4);
    for (size_t i = 0; i < d.size(); i += 3) {
        unsigned b = (unsigned)d[i] << 16;
        if (i+1 < d.size()) b |= (unsigned)d[i+1] << 8;
        if (i+2 < d.size()) b |= (unsigned)d[i+2];
        out += T[(b>>18)&63]; out += T[(b>>12)&63];
        out += (i+1 < d.size()) ? T[(b>>6)&63] : '=';
        out += (i+2 < d.size()) ? T[b&63]      : '=';
    }
    return out;
}

static std::string toDataURLIt(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>());
    std::string ext = lowerExtIt(path);
    return "data:" + mimeForExtIt(ext) + ";base64," + base64EncIt(bytes);
}

static std::string sizeToken(const std::string& widthStr) {
    int px = widthStr.empty() ? 0 : std::stoi(widthStr);
    return px > 0 ? std::to_string(px) + "px" : "full";
}

// ---------------------------------------------------------------------------

ImageTab::ImageTab(wxWindow* parent, bool darkMode)
    : wxPanel(parent, wxID_ANY)
    , m_darkMode(darkMode)
{
    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("imagetab");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) { OnScriptMessage(evt); });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    Reload();
}

void ImageTab::SetProject(const std::string& projDir,
                          const std::string& selectedDoc,
                          std::function<void()> onChanged)
{
    m_projectDir  = projDir;
    m_selectedDoc = selectedDoc;
    m_onChanged   = std::move(onChanged);
    Reload();
}

void ImageTab::SetDarkMode(bool dark) {
    m_darkMode = dark;
    Reload();
}

void ImageTab::Reload() {
    // Thumbnails for all images in the project
    std::vector<std::pair<std::string,std::string>> imgPairs;
    if (!m_projectDir.empty()) {
        for (const auto& name : ScanProjectImages(m_projectDir)) {
            std::string dataUrl = toDataURLIt(
                (fs::path(m_projectDir) / name).string());
            if (!dataUrl.empty())
                imgPairs.push_back({name, dataUrl});
        }
    }

    // All .md files in the project with their headings
    std::vector<ImageTabDoc> docs;
    if (!m_projectDir.empty()) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(m_projectDir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            if (lowerExtIt(name) != "md") continue;
            ImageTabDoc doc;
            doc.name = name;
            std::ifstream f(entry.path());
            if (f) {
                std::string content((std::istreambuf_iterator<char>(f)), {});
                doc.headings = ExtractHeadings(content);
            }
            docs.push_back(doc);
        }
        std::sort(docs.begin(), docs.end(),
                  [](const ImageTabDoc& a, const ImageTabDoc& b) {
                      return a.name < b.name; });
    }

    std::string html = BuildImageTabHTML(imgPairs, docs, m_selectedDoc, m_darkMode);
    m_webView->SetPage(wxString::FromUTF8(html), "");
}

std::string ImageTab::readFile(const std::string& path) const {
    std::ifstream f(path);
    if (!f) return "";
    return {(std::istreambuf_iterator<char>(f)),
             std::istreambuf_iterator<char>()};
}

void ImageTab::writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) {
        wxMessageBox("Could not write: " + wxString::FromUTF8(path),
                     "StoryTeller", wxOK | wxICON_ERROR, this);
        return;
    }
    f << content;
}

void ImageTab::OnScriptMessage(wxWebViewEvent& evt) {
    std::string payload = evt.GetString().ToStdString();
    std::string action  = jfField(payload, "action");

    if (action == "upload") {
        if (m_projectDir.empty()) {
            wxMessageBox("Open a project file first.",
                         "StoryTeller", wxOK | wxICON_INFORMATION, this);
            return;
        }
        wxFileDialog dlg(this, "Choose image",
                         wxString::FromUTF8(m_projectDir), "",
                         "Image files (*.jpg;*.jpeg;*.png;*.webp;*.gif)"
                         "|*.jpg;*.jpeg;*.png;*.webp;*.gif|All files (*)|*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (dlg.ShowModal() == wxID_CANCEL) return;

        wxArrayString paths;
        dlg.GetPaths(paths);
        for (const auto& wx_src : paths) {
            std::string srcPath  = wx_src.ToStdString();
            std::string filename = fs::path(srcPath).filename().string();
            std::string destPath = (fs::path(m_projectDir) / filename).string();
            if (srcPath != destPath) {
                std::error_code ec;
                fs::copy_file(srcPath, destPath,
                              fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    wxMessageBox("Could not copy " +
                                 wxString::FromUTF8(filename) + ": " +
                                 wxString::FromUTF8(ec.message()),
                                 "StoryTeller", wxOK | wxICON_ERROR, this);
                }
            }
        }
        Reload();

    } else if (action == "place") {
        std::string filename = jfField(payload, "filename");
        std::string docName  = jfField(payload, "doc");
        std::string align    = jfField(payload, "align");
        std::string heading  = jfField(payload, "heading");
        std::string widthStr = jfField(payload, "width");
        if (filename.empty() || docName.empty() || m_projectDir.empty()) return;
        if (align.empty()) align = "center";

        std::string docPath = (fs::path(m_projectDir) / docName).string();
        std::string md = readFile(docPath);
        md = InsertImageAnchor(md, filename, heading, sizeToken(widthStr), align);
        writeFile(docPath, md);
        if (m_onChanged) m_onChanged();

    } else if (action == "addtosection") {
        std::string filename = jfField(payload, "filename");
        std::string docName  = jfField(payload, "doc");
        std::string align    = jfField(payload, "align");
        std::string heading  = jfField(payload, "heading");
        std::string widthStr = jfField(payload, "width");
        if (filename.empty() || docName.empty() || m_projectDir.empty()) return;
        if (align.empty()) align = "center";

        std::string docPath = (fs::path(m_projectDir) / docName).string();
        std::string md = readFile(docPath);
        md = InsertIntoImageSection(md, filename, heading, sizeToken(widthStr), align);
        writeFile(docPath, md);
        if (m_onChanged) m_onChanged();

    } else if (action == "resize") {
        std::string filename = jfField(payload, "filename");
        std::string docName  = jfField(payload, "doc");
        std::string widthStr = jfField(payload, "width");
        if (filename.empty() || docName.empty() || m_projectDir.empty()) return;

        std::string docPath = (fs::path(m_projectDir) / docName).string();
        std::string md = readFile(docPath);
        md = ResizeImageAnchor(md, filename, sizeToken(widthStr));
        writeFile(docPath, md);
        if (m_onChanged) m_onChanged();

    } else if (action == "removeanchor") {
        std::string filename = jfField(payload, "filename");
        std::string docName  = jfField(payload, "doc");
        if (filename.empty() || docName.empty() || m_projectDir.empty()) return;

        std::string docPath = (fs::path(m_projectDir) / docName).string();
        std::string md = readFile(docPath);
        md = RemoveImageAnchor(md, filename);
        writeFile(docPath, md);
        if (m_onChanged) m_onChanged();

    } else if (action == "remove") {
        // Remove the image file from disk and remove its anchor from all docs
        std::string filename = jfField(payload, "filename");
        if (filename.empty() || m_projectDir.empty()) return;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(m_projectDir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (lowerExtIt(entry.path().filename().string()) != "md") continue;
            std::string md = readFile(entry.path().string());
            writeFile(entry.path().string(), RemoveImageAnchor(md, filename));
        }
        fs::remove(fs::path(m_projectDir) / filename, ec);

        if (m_onChanged) m_onChanged();
        Reload();
    }
}
