#include "images.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int test_images() {
    int failures = 0;

    // [extract-headings] — returns level-2 headings stripped of "## "
    {
        std::string md =
            "# Title\n\n"
            "## Chapter 1: The Boy Who Asked Why\n\n"
            "Some text.\n\n"
            "## Chapter 2: From Small Ideas\n\n"
            "More text.\n";
        auto h = ExtractHeadings(md);
        bool ok = h.size() == 2
               && h[0] == "Chapter 1: The Boy Who Asked Why"
               && h[1] == "Chapter 2: From Small Ideas";
        if (!ok) {
            std::cerr << "FAIL [extract-headings]: got " << h.size() << " headings\n";
            for (auto& s : h) std::cerr << "  '" << s << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [extract-headings]\n";
        }
    }

    // [insert-image-anchor-heading] — inserts after the specified heading
    {
        std::string md =
            "## Chapter 1: The Boy Who Asked Why\n\n"
            "Some text here.\n";
        std::string result = InsertImageAnchor(md,
            "young_elon.jpg",
            "Chapter 1: The Boy Who Asked Why",
            "medium", "center");
        bool hasRef    = result.find("![|medium|center](young_elon.jpg)") != std::string::npos;
        bool afterHead = result.find("Chapter 1") < result.find("young_elon.jpg");
        bool beforeTxt = result.find("young_elon.jpg") < result.find("Some text");
        if (!hasRef || !afterHead || !beforeTxt) {
            std::cerr << "FAIL [insert-image-anchor-heading]: hasRef=" << hasRef
                      << " afterHead=" << afterHead << " beforeTxt=" << beforeTxt
                      << "\n  result: '" << result << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [insert-image-anchor-heading]\n";
        }
    }

    // [insert-image-anchor-top] — empty headingText inserts at top of file
    {
        std::string md = "## Chapter 1\n\nSome text.\n";
        std::string result = InsertImageAnchor(md, "cover.jpg", "", "full", "center");
        bool hasRef  = result.find("![|full|center](cover.jpg)") != std::string::npos;
        bool atTop   = result.find("cover.jpg") < result.find("Chapter 1");
        if (!hasRef || !atTop) {
            std::cerr << "FAIL [insert-image-anchor-top]: hasRef=" << hasRef
                      << " atTop=" << atTop << "\n  result: '" << result << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [insert-image-anchor-top]\n";
        }
    }

    // [remove-image-anchor] — removes the image line from the markdown
    {
        std::string md =
            "## Chapter 1\n\n"
            "![|medium|center](young_elon.jpg)\n\n"
            "Some text.\n";
        std::string result = RemoveImageAnchor(md, "young_elon.jpg");
        bool noRef  = result.find("young_elon.jpg") == std::string::npos;
        bool hasText = result.find("Some text") != std::string::npos;
        if (!noRef || !hasText) {
            std::cerr << "FAIL [remove-image-anchor]: noRef=" << noRef
                      << " hasText=" << hasText << "\n  result: '" << result << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [remove-image-anchor]\n";
        }
    }

    // [substitute-local-images] — replaces local img src with base64 data URL
    {
        fs::path tmp = fs::temp_directory_path() / "st_img_test";
        fs::create_directories(tmp);
        // Write a minimal 1x1 white PNG (67 bytes)
        unsigned char png[] = {
            0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
            0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
            0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
            0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
            0xde,0x00,0x00,0x00,0x0c,0x49,0x44,0x41,
            0x54,0x08,0xd7,0x63,0xf8,0xcf,0xc0,0x00,
            0x00,0x00,0x02,0x00,0x01,0xe2,0x21,0xbc,
            0x33,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,
            0x44,0xae,0x42,0x60,0x82
        };
        {
            std::ofstream f(tmp / "test.png", std::ios::binary);
            f.write(reinterpret_cast<char*>(png), sizeof(png));
        }
        std::string html = "<p><img src=\"test.png\" alt=\"|medium|center\" loading=\"lazy\"></p>\n";
        std::string result = SubstituteLocalImages(html, tmp.string());
        bool hasData  = result.find("data:image/png;base64,") != std::string::npos;
        bool noLocal  = result.find("src=\"test.png\"") == std::string::npos;
        bool hasClass = result.find("img-medium") != std::string::npos;
        if (!hasData || !noLocal || !hasClass) {
            std::cerr << "FAIL [substitute-local-images]: hasData=" << hasData
                      << " noLocal=" << noLocal << " hasClass=" << hasClass
                      << "\n  result: '" << result.substr(0,200) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [substitute-local-images]\n";
        }
        fs::remove_all(tmp);
    }

    // [scan-project-images] — lists image files, ignores non-images
    {
        fs::path tmp = fs::temp_directory_path() / "st_scan_test";
        fs::create_directories(tmp);
        { std::ofstream f(tmp / "photo.jpg");  f << "x"; }
        { std::ofstream f(tmp / "chart.png");  f << "x"; }
        { std::ofstream f(tmp / "notes.md");   f << "x"; }
        { std::ofstream f(tmp / "icon.webp");  f << "x"; }
        auto imgs = ScanProjectImages(tmp.string());
        bool hasJpg  = std::find(imgs.begin(), imgs.end(), "photo.jpg")  != imgs.end();
        bool hasPng  = std::find(imgs.begin(), imgs.end(), "chart.png")  != imgs.end();
        bool hasWebp = std::find(imgs.begin(), imgs.end(), "icon.webp")  != imgs.end();
        bool noMd    = std::find(imgs.begin(), imgs.end(), "notes.md")   == imgs.end();
        if (!hasJpg || !hasPng || !hasWebp || !noMd) {
            std::cerr << "FAIL [scan-project-images]: hasJpg=" << hasJpg
                      << " hasPng=" << hasPng << " hasWebp=" << hasWebp
                      << " noMd=" << noMd << "\n";
            ++failures;
        } else {
            std::cout << "PASS [scan-project-images]\n";
        }
        fs::remove_all(tmp);
    }

    // [resize-image-anchor] — updates the size token in an existing anchor
    {
        std::string md =
            "## Chapter One\n\n"
            "![|400px|center](photo.jpg)\n\n"
            "Some text.\n"
            "![|medium|left](other.jpg)\n";
        std::string result = ResizeImageAnchor(md, "photo.jpg", "600px");
        bool updatedPhoto = result.find("![|600px|center](photo.jpg)") != std::string::npos;
        bool keptOther    = result.find("![|medium|left](other.jpg)") != std::string::npos;
        bool removedOld   = result.find("![|400px|center](photo.jpg)") == std::string::npos;
        if (!updatedPhoto || !keptOther || !removedOld) {
            std::cerr << "FAIL [resize-image-anchor]: updatedPhoto=" << updatedPhoto
                      << " keptOther=" << keptOther
                      << " removedOld=" << removedOld << "\n";
            ++failures;
        } else {
            std::cout << "PASS [resize-image-anchor]\n";
        }
    }

    // [image-section-insert-new] — creates :::image block when none exists
    {
        std::string md =
            "## Chapter One\n\n"
            "Some content here.\n\n"
            "## Chapter Two\n\n"
            "Other content.\n";
        std::string result = InsertIntoImageSection(
            md, "photo.jpg", "Chapter One", "400px", "center");
        bool hasBlock   = result.find(":::image") != std::string::npos;
        bool hasClose   = result.find("\n:::") != std::string::npos;
        bool hasAnchor  = result.find("![|400px|center](photo.jpg)") != std::string::npos;
        bool beforeCh2  = result.find(":::image") < result.find("## Chapter Two");
        if (!hasBlock || !hasClose || !hasAnchor || !beforeCh2) {
            std::cerr << "FAIL [image-section-insert-new]: hasBlock=" << hasBlock
                      << " hasClose=" << hasClose
                      << " hasAnchor=" << hasAnchor
                      << " beforeCh2=" << beforeCh2
                      << "\n  result: " << result << "\n";
            ++failures;
        } else {
            std::cout << "PASS [image-section-insert-new]\n";
        }
    }

    // [image-section-insert-existing] — appends to existing :::image block
    {
        std::string md =
            "## Chapter One\n\n"
            "Some content.\n\n"
            ":::image\n"
            "![|200px|left](existing.jpg)\n"
            ":::\n";
        std::string result = InsertIntoImageSection(
            md, "new.png", "Chapter One", "600px", "right");
        bool hasExisting = result.find("existing.jpg") != std::string::npos;
        bool hasNew      = result.find("new.png") != std::string::npos;
        bool oneBlock    = result.find(":::image") == result.rfind(":::image");
        if (!hasExisting || !hasNew || !oneBlock) {
            std::cerr << "FAIL [image-section-insert-existing]: hasExisting=" << hasExisting
                      << " hasNew=" << hasNew
                      << " oneBlock=" << oneBlock
                      << "\n  result: " << result << "\n";
            ++failures;
        } else {
            std::cout << "PASS [image-section-insert-existing]\n";
        }
    }

    // [remove-anchor-cleans-empty-image-section] — removing last image deletes the :::image block
    {
        std::string md =
            "## Chapter One\n\n"
            "Some text.\n\n"
            ":::image\n"
            "![|400px|center](solo.jpg)\n"
            ":::\n\n"
            "## Chapter Two\n\n"
            "Other text.\n";
        std::string result = RemoveImageAnchor(md, "solo.jpg");
        bool noAnchor = result.find("solo.jpg")   == std::string::npos;
        bool noBlock  = result.find(":::image")   == std::string::npos;
        bool noClose  = result.find("\n:::\n")     == std::string::npos;
        bool hasText  = result.find("Some text")  != std::string::npos;
        bool hasCh2   = result.find("Chapter Two") != std::string::npos;
        if (!noAnchor || !noBlock || !noClose || !hasText || !hasCh2) {
            std::cerr << "FAIL [remove-anchor-cleans-empty-image-section]: "
                      << "noAnchor=" << noAnchor << " noBlock=" << noBlock
                      << " noClose=" << noClose << " hasText=" << hasText
                      << " hasCh2=" << hasCh2
                      << "\n  result: '" << result << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [remove-anchor-cleans-empty-image-section]\n";
        }
    }

    // [substitute-preserves-display-none] — pixel-width alt does not add duplicate style attr
    {
        fs::path tmp = fs::temp_directory_path() / "st_style_test";
        fs::create_directories(tmp);
        unsigned char png[] = {
            0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
            0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
            0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
            0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
            0xde,0x00,0x00,0x00,0x0c,0x49,0x44,0x41,
            0x54,0x08,0xd7,0x63,0xf8,0xcf,0xc0,0x00,
            0x00,0x00,0x02,0x00,0x01,0xe2,0x21,0xbc,
            0x33,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,
            0x44,0xae,0x42,0x60,0x82
        };
        { std::ofstream f(tmp / "slide.png", std::ios::binary);
          f.write(reinterpret_cast<char*>(png), sizeof(png)); }
        // Simulate a carousel slide: pixel-width alt + display:none inline style
        std::string html =
            "<img src=\"slide.png\" alt=\"|400px|center\" loading=\"lazy\" style=\"display:none\">";
        std::string result = SubstituteLocalImages(html, tmp.string());
        // Count style= occurrences — must be exactly one
        size_t count = 0, pos = 0;
        while ((pos = result.find("style=", pos)) != std::string::npos) { ++count; ++pos; }
        bool oneStyle    = count == 1;
        bool hasNone     = result.find("display:none") != std::string::npos;
        bool hasData     = result.find("data:image") != std::string::npos;
        if (!oneStyle || !hasNone || !hasData) {
            std::cerr << "FAIL [substitute-preserves-display-none]: "
                      << "styleCount=" << count << " hasNone=" << hasNone
                      << " hasData=" << hasData
                      << "\n  result: '" << result << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [substitute-preserves-display-none]\n";
        }
        fs::remove_all(tmp);
    }

    return failures;
}
