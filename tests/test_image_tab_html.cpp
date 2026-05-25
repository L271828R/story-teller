#include "../src/image_tab_html.h"
#include <iostream>
#include <string>

int test_image_tab_html() {
    int failures = 0;

    std::vector<std::pair<std::string,std::string>> imgs = {
        {"photo.jpg",  "data:image/jpeg;base64,abc123"},
        {"chart.png",  "data:image/png;base64,xyz789"},
    };
    std::vector<ImageTabDoc> docs = {
        {"chapter1.md", {"The Beginning", "The Middle"}},
        {"chapter2.md", {"Chapter Two",   "The End"}},
    };

    std::string light = BuildImageTabHTML(imgs, docs, "chapter1.md", false);
    std::string dark  = BuildImageTabHTML(imgs, docs, "chapter1.md", true);

    auto check = [&](const std::string& name, bool ok) {
        if (!ok) { std::cerr << "FAIL [" << name << "]\n"; ++failures; }
        else      std::cout << "PASS [" << name << "]\n";
    };

    check("image-tab-doctype",
          light.find("<!DOCTYPE html>") != std::string::npos);

    check("image-tab-dark-mode",
          dark.find("class=\"dark\"")  != std::string::npos &&
          light.find("class=\"dark\"") == std::string::npos);

    // Upload action: separate from placement — user uploads first, places later
    check("image-tab-upload-btn",
          light.find("upload") != std::string::npos);

    // Pixel width input: user sets a specific width in pixels (not just preset pills)
    check("image-tab-pixel-width",
          light.find("width-px")   != std::string::npos ||
          light.find("widthPx")    != std::string::npos ||
          light.find("type=\"number\"") != std::string::npos);

    // Place action: insert the selected image anchor into the document
    check("image-tab-place-action",
          light.find("place") != std::string::npos);

    // Document selector: user chooses which .md file to place into
    check("image-tab-doc-selector",
          light.find("chapter1.md") != std::string::npos &&
          light.find("chapter2.md") != std::string::npos);

    // Heading select: user chooses which heading to insert after
    check("image-tab-heading-select",
          light.find("heading") != std::string::npos);

    // Headings per-doc: all docs' headings are embedded in the JS data
    check("image-tab-headings-per-doc",
          light.find("The Beginning") != std::string::npos &&
          light.find("Chapter Two")   != std::string::npos);

    // Align controls: left / center / right
    check("image-tab-align",
          light.find("left")   != std::string::npos &&
          light.find("center") != std::string::npos &&
          light.find("right")  != std::string::npos);

    // Message handler: all postMessages go through the imagetab handler
    check("image-tab-message-handler",
          light.find("messageHandlers.imagetab") != std::string::npos);

    // Image list passed in: filenames appear in rendered HTML
    check("image-tab-images-injected",
          light.find("photo.jpg") != std::string::npos &&
          light.find("chart.png") != std::string::npos);

    // Add-to-section action: insert into :::image block at end of chapter
    check("image-tab-add-to-section",
          light.find("addtosection") != std::string::npos);

    // Resize action: change the width of an already-placed image anchor
    check("image-tab-resize-action",
          light.find("resize") != std::string::npos);

    // Remove-anchor action: remove anchor from doc but keep file in project
    check("image-tab-remove-anchor-action",
          light.find("removeanchor") != std::string::npos);

    return failures;
}
