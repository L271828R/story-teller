#include "image_panel.h"
#include "image_panel_html.h"
#include "images.h"
#include <wx/filename.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

static std::string jsonField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size() || json[pos] != '"') return "";
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

static std::string lowerExt(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return "";
    std::string e = filename.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e;
}

static std::string mimeForExt(const std::string& ext) {
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png")  return "image/png";
    if (ext == "gif")  return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "svg")  return "image/svg+xml";
    return "image/jpeg";
}

static std::string base64Enc(const std::vector<unsigned char>& d) {
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

static std::string toDataURL(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(f)),
         std::istreambuf_iterator<char>());
    std::string ext = lowerExt(path);
    return "data:" + mimeForExt(ext) + ";base64," + base64Enc(bytes);
}

ImagePanel::ImagePanel(wxWindow* parent, bool darkMode,
                       const std::string& projectDir,
                       const std::string& mdPath,
                       std::function<void()> onChanged)
    : wxFrame(parent, wxID_ANY, "Project Images",
              wxDefaultPosition, wxSize(720, 420),
              wxDEFAULT_FRAME_STYLE)
    , m_projectDir(projectDir)
    , m_mdPath(mdPath)
    , m_onChanged(std::move(onChanged))
{
    // Build image list with thumbnails
    std::vector<std::pair<std::string,std::string>> imgPairs;
    for (const auto& name : ScanProjectImages(projectDir)) {
        std::string dataUrl = toDataURL((fs::path(projectDir) / name).string());
        if (!dataUrl.empty())
            imgPairs.push_back({name, dataUrl});
    }

    // Extract headings from the markdown file
    std::vector<std::string> headings;
    {
        std::ifstream f(mdPath);
        if (f) {
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            headings = ExtractHeadings(content);
        }
    }

    m_webView = wxWebView::New(this, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("image");
    m_webView->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED,
        [this](wxWebViewEvent& evt) { OnScriptMessage(evt); });

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_webView, 1, wxEXPAND);
    SetSizer(sizer);

    std::string html = BuildImagePanelHTML(imgPairs, headings, darkMode);
    m_webView->SetPage(wxString::FromUTF8(html), "");
}

std::string ImagePanel::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    return {(std::istreambuf_iterator<char>(f)),
             std::istreambuf_iterator<char>()};
}

void ImagePanel::OnScriptMessage(wxWebViewEvent& evt) {
    std::string payload = evt.GetString().ToStdString();
    std::string action  = jsonField(payload, "action");

    if (action == "insert") {
        std::string size    = jsonField(payload, "size");
        std::string align   = jsonField(payload, "align");
        std::string heading = jsonField(payload, "heading");
        if (size.empty())  size  = "medium";
        if (align.empty()) align = "center";

        wxFileDialog dlg(this, "Choose image",
                         wxString::FromUTF8(m_projectDir), "",
                         "Image files (*.jpg;*.jpeg;*.png;*.webp;*.gif)"
                         "|*.jpg;*.jpeg;*.png;*.webp;*.gif|All files (*)|*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_CANCEL) return;

        std::string srcPath = dlg.GetPath().ToStdString();
        std::string filename = fs::path(srcPath).filename().string();
        std::string destPath = (fs::path(m_projectDir) / filename).string();

        // Copy image into project folder (no-op if already there)
        if (srcPath != destPath) {
            std::error_code ec;
            fs::copy_file(srcPath, destPath,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                wxMessageBox("Could not copy image: " +
                             wxString::FromUTF8(ec.message()),
                             "StoryTeller", wxOK | wxICON_ERROR, this);
                return;
            }
        }

        // Insert anchor into the markdown file
        std::string md = readFile(m_mdPath);
        md = InsertImageAnchor(md, filename, heading, size, align);
        {
            std::ofstream f(m_mdPath);
            if (!f) {
                wxMessageBox("Could not write markdown file.",
                             "StoryTeller", wxOK | wxICON_ERROR, this);
                return;
            }
            f << md;
        }

        if (m_onChanged) m_onChanged();
        Close();

    } else if (action == "remove") {
        std::string filename = jsonField(payload, "filename");
        if (filename.empty()) return;

        // Remove anchor from markdown
        std::string md = readFile(m_mdPath);
        md = RemoveImageAnchor(md, filename);
        { std::ofstream f(m_mdPath); f << md; }

        // Delete image file
        std::error_code ec;
        fs::remove(fs::path(m_projectDir) / filename, ec);

        if (m_onChanged) m_onChanged();

        // Rebuild and redisplay the panel without the removed image
        std::vector<std::pair<std::string,std::string>> imgPairs;
        for (const auto& name : ScanProjectImages(m_projectDir)) {
            std::string dataUrl = toDataURL((fs::path(m_projectDir) / name).string());
            if (!dataUrl.empty())
                imgPairs.push_back({name, dataUrl});
        }
        std::vector<std::string> headings;
        {
            std::ifstream f(m_mdPath);
            if (f) {
                std::string content((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
                headings = ExtractHeadings(content);
            }
        }
        // Reload panel HTML
        std::string html = BuildImagePanelHTML(imgPairs, headings,
            GetBackgroundColour().GetLuminance() < 0.5);
        m_webView->SetPage(wxString::FromUTF8(html), "");
    }
}
