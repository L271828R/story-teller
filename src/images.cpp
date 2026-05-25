#include "images.h"
#include "thumbnail.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string base64Encode(const std::vector<unsigned char>& data) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned int b = (unsigned int)data[i] << 16;
        if (i + 1 < data.size()) b |= (unsigned int)data[i+1] << 8;
        if (i + 2 < data.size()) b |= (unsigned int)data[i+2];
        out += table[(b >> 18) & 0x3f];
        out += table[(b >> 12) & 0x3f];
        out += (i + 1 < data.size()) ? table[(b >> 6) & 0x3f] : '=';
        out += (i + 2 < data.size()) ? table[b & 0x3f]        : '=';
    }
    return out;
}

static std::string mimeType(const std::string& ext) {
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png")  return "image/png";
    if (ext == "gif")  return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "svg")  return "image/svg+xml";
    return "image/jpeg";
}

static std::string lowerExt(const std::string& filename) {
    auto dot = filename.rfind('.');
    if (dot == std::string::npos) return "";
    std::string e = filename.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e;
}

static bool isImageExt(const std::string& ext) {
    return ext == "jpg" || ext == "jpeg" || ext == "png"
        || ext == "gif" || ext == "webp" || ext == "svg";
}

// ---------------------------------------------------------------------------
// ExtractHeadings
// ---------------------------------------------------------------------------

std::vector<std::string> ExtractHeadings(const std::string& mdContent) {
    std::vector<std::string> result;
    std::istringstream ss(mdContent);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.size() > 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ')
            result.push_back(line.substr(3));
    }
    return result;
}

// ---------------------------------------------------------------------------
// InsertImageAnchor
// ---------------------------------------------------------------------------

std::string InsertImageAnchor(const std::string& mdContent,
                              const std::string& filename,
                              const std::string& headingText,
                              const std::string& size,
                              const std::string& align) {
    std::string ref = "![|" + size + "|" + align + "](" + filename + ")\n";

    if (headingText.empty()) {
        return ref + "\n" + mdContent;
    }

    std::string target = "## " + headingText;
    std::istringstream ss(mdContent);
    std::string result;
    std::string line;
    bool inserted = false;
    while (std::getline(ss, line)) {
        result += line + "\n";
        if (!inserted && line == target) {
            result += "\n" + ref;
            inserted = true;
        }
    }
    return inserted ? result : mdContent;
}

// ---------------------------------------------------------------------------
// RemoveImageAnchor
// ---------------------------------------------------------------------------

std::string RemoveImageAnchor(const std::string& mdContent,
                              const std::string& filename) {
    // First pass: collect lines, dropping the anchor
    std::istringstream ss(mdContent);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find("](" + filename + ")") != std::string::npos
            && line.size() > 2 && line[0] == '!')
            continue;
        lines.push_back(line);
    }

    // Second pass: remove any :::image blocks that are now empty
    std::vector<std::string> out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] != ":::image") {
            out.push_back(lines[i]);
            continue;
        }
        // Peek ahead: find the matching ::: close
        size_t j = i + 1;
        bool hasContent = false;
        while (j < lines.size() && lines[j] != ":::") {
            if (!lines[j].empty()) hasContent = true;
            ++j;
        }
        if (!hasContent && j < lines.size()) {
            // Block is empty — skip open fence, contents, and close fence
            i = j; // loop increment will move past :::
            // Also drop the blank line before the block if present
            while (!out.empty() && out.back().empty()) out.pop_back();
        } else {
            out.push_back(lines[i]);
        }
    }

    std::string result;
    for (auto& l : out) result += l + "\n";
    return result;
}

// ---------------------------------------------------------------------------
// SubstituteLocalImages
// ---------------------------------------------------------------------------

std::string SubstituteLocalImages(const std::string& html,
                                  const std::string& projectDir) {
    std::string result;
    result.reserve(html.size());
    size_t pos = 0;

    while (pos < html.size()) {
        size_t imgStart = html.find("<img ", pos);
        if (imgStart == std::string::npos) {
            result += html.substr(pos);
            break;
        }
        result += html.substr(pos, imgStart - pos);

        size_t imgEnd = html.find('>', imgStart);
        if (imgEnd == std::string::npos) {
            result += html.substr(imgStart);
            break;
        }
        std::string tag = html.substr(imgStart, imgEnd - imgStart + 1);
        pos = imgEnd + 1;

        // Extract src value
        size_t srcPos = tag.find("src=\"");
        if (srcPos == std::string::npos) { result += tag; continue; }
        size_t srcStart = srcPos + 5;
        size_t srcEnd   = tag.find('"', srcStart);
        if (srcEnd == std::string::npos) { result += tag; continue; }
        std::string src = tag.substr(srcStart, srcEnd - srcStart);

        // Only substitute local filenames (not http/data/absolute paths)
        if (src.empty() || src.find("://") != std::string::npos
            || src[0] == '/' || src[0] == '.') {
            result += tag; continue;
        }

        std::string ext = lowerExt(src);
        if (!isImageExt(ext)) { result += tag; continue; }

        fs::path imgPath  = fs::path(projectDir) / src;
        std::string thumb = EnsureThumb(imgPath.string());
        std::ifstream f(thumb, std::ios::binary);
        if (!f) { result += tag; continue; }
        std::vector<unsigned char> bytes(
            (std::istreambuf_iterator<char>(f)),
             std::istreambuf_iterator<char>());

        std::string thumbExt = lowerExt(thumb);
        std::string dataUrl  = "data:" + mimeType(thumbExt) + ";base64," + base64Encode(bytes);

        // Parse alt text for size/align: "|medium|center" or "|400px|center"
        std::string cssClass;
        std::string styleAttr;
        size_t altPos = tag.find("alt=\"");
        if (altPos != std::string::npos) {
            size_t altStart = altPos + 5;
            size_t altEnd   = tag.find('"', altStart);
            if (altEnd != std::string::npos) {
                std::string alt = tag.substr(altStart, altEnd - altStart);
                if (!alt.empty() && alt[0] == '|') {
                    std::istringstream as(alt.substr(1));
                    std::string sizeVal, alignVal;
                    std::getline(as, sizeVal,  '|');
                    std::getline(as, alignVal, '|');
                    bool isPx = sizeVal.size() > 2
                             && sizeVal.substr(sizeVal.size()-2) == "px";
                    if (isPx) {
                        styleAttr = "width:" + sizeVal + ";max-width:100%;height:auto";
                    } else if (!sizeVal.empty()) {
                        cssClass += " img-" + sizeVal;
                    }
                    if (!alignVal.empty()) cssClass += " img-" + alignVal;
                }
                // Clear the encoded alt so it doesn't show as broken text
                tag.replace(altStart, altEnd - altStart, "");
            }
        }

        // Replace src with data URL.
        tag.replace(srcStart, srcEnd - srcStart, dataUrl);

        // Merge styleAttr into existing style= if present, or prepend a new one.
        if (!styleAttr.empty()) {
            size_t spos = tag.find("style=\"");
            if (spos != std::string::npos) {
                tag.insert(spos + 7, styleAttr + ";");
            } else {
                tag.insert(5, "style=\"" + styleAttr + "\" ");
            }
        }
        if (!cssClass.empty())
            tag.insert(5, "class=\"" + cssClass.substr(1) + "\" ");
        result += tag;
    }
    return result;
}

// ---------------------------------------------------------------------------
// ResizeImageAnchor
// ---------------------------------------------------------------------------

std::string ResizeImageAnchor(const std::string& mdContent,
                               const std::string& filename,
                               const std::string& newSize) {
    std::istringstream ss(mdContent);
    std::string result;
    std::string line;
    std::string anchor = "](" + filename + ")";
    while (std::getline(ss, line)) {
        if (line.find(anchor) != std::string::npos
            && line.size() > 3
            && line[0] == '!' && line[1] == '[' && line[2] == '|') {
            // Format: ![|size|align](filename)
            auto pipe1 = line.find('|', 2);          // between "![" and size
            auto pipe2 = line.find('|', pipe1 + 1);  // between size and align
            if (pipe1 != std::string::npos && pipe2 != std::string::npos) {
                line = line.substr(0, pipe1 + 1) + newSize + line.substr(pipe2);
            }
        }
        result += line + "\n";
    }
    return result;
}

// ---------------------------------------------------------------------------
// InsertIntoImageSection
// ---------------------------------------------------------------------------

std::string InsertIntoImageSection(const std::string& mdContent,
                                   const std::string& filename,
                                   const std::string& chapterHeading,
                                   const std::string& size,
                                   const std::string& align) {
    std::string anchor = "![|" + size + "|" + align + "](" + filename + ")";
    std::string target = chapterHeading.empty() ? "" : "## " + chapterHeading;

    std::istringstream ss(mdContent);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }

    // Find the chapter section: start line (inclusive) and end line (exclusive)
    size_t secStart = 0;   // first line of the chapter section
    size_t secEnd   = lines.size(); // exclusive end

    if (!target.empty()) {
        bool found = false;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i] == target) {
                secStart = i;
                found = true;
                // End is the next ##-level heading or EOF
                for (size_t j = i + 1; j < lines.size(); ++j) {
                    if (lines[j].size() > 3
                        && lines[j][0] == '#' && lines[j][1] == '#'
                        && lines[j][2] == ' ') {
                        secEnd = j;
                        break;
                    }
                }
                break;
            }
        }
        if (!found) return mdContent; // heading not found — no-op
    }

    // Search for an existing :::image block within the section
    for (size_t i = secStart; i < secEnd; ++i) {
        if (lines[i] == ":::image") {
            // Find the closing :::
            for (size_t j = i + 1; j < secEnd; ++j) {
                if (lines[j] == ":::") {
                    lines.insert(lines.begin() + (std::ptrdiff_t)j, anchor);
                    std::string result;
                    for (const auto& ln : lines) result += ln + "\n";
                    return result;
                }
            }
        }
    }

    // No :::image block found — insert one before secEnd, after trailing blank lines
    size_t insertBefore = secEnd;
    while (insertBefore > secStart && lines[insertBefore - 1].empty())
        --insertBefore;

    std::vector<std::string> block = {"", ":::image", anchor, ":::", ""};
    lines.insert(lines.begin() + (std::ptrdiff_t)insertBefore,
                 block.begin(), block.end());

    std::string result;
    for (const auto& ln : lines) result += ln + "\n";
    return result;
}

// ---------------------------------------------------------------------------
// ScanProjectImages
// ---------------------------------------------------------------------------

std::vector<std::string> ScanProjectImages(const std::string& projectDir) {
    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(projectDir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (isImageExt(lowerExt(name)))
            result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}
