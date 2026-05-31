#include "image_captions.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

#define CHECK(label, cond) \
    do { if (cond) { std::cout << "PASS [" label "]\n"; } \
         else { std::cerr << "FAIL [" label "]\n"; ++failures; } } while(0)

// Minimal temp-dir RAII helper.
struct TmpDir {
    std::string path;
    TmpDir() {
        path = fs::temp_directory_path() / ("icap_test_" + std::to_string(getpid()));
        fs::create_directories(path);
    }
    ~TmpDir() { fs::remove_all(path); }
};

int test_image_captions() {
    int failures = 0;

    // Empty project → empty map
    {
        TmpDir d;
        auto caps = LoadImageCaptions(d.path);
        CHECK("captions-empty-on-new-project", caps.empty());
    }

    // Save one caption, load it back
    {
        TmpDir d;
        SaveImageCaption(d.path, "darwin.jpg", "Charles Darwin as a young man");
        auto caps = LoadImageCaptions(d.path);
        CHECK("captions-save-and-load",
            caps.count("darwin.jpg") && caps.at("darwin.jpg") == "Charles Darwin as a young man");
    }

    // GetImageCaption helper
    {
        TmpDir d;
        SaveImageCaption(d.path, "finches.png", "Galápagos finch beak variation");
        CHECK("captions-get-existing",
            GetImageCaption(d.path, "finches.png") == "Galápagos finch beak variation");
        CHECK("captions-get-missing",
            GetImageCaption(d.path, "missing.jpg").empty());
    }

    // Saving multiple captions preserves all of them
    {
        TmpDir d;
        SaveImageCaption(d.path, "img1.jpg", "Caption one");
        SaveImageCaption(d.path, "img2.png", "Caption two");
        SaveImageCaption(d.path, "img3.jpg", "Caption three");
        auto caps = LoadImageCaptions(d.path);
        CHECK("captions-multiple-preserved",
            caps.size() == 3
            && caps.at("img1.jpg") == "Caption one"
            && caps.at("img2.png") == "Caption two"
            && caps.at("img3.jpg") == "Caption three");
    }

    // Updating a caption overwrites the old value
    {
        TmpDir d;
        SaveImageCaption(d.path, "photo.jpg", "First draft");
        SaveImageCaption(d.path, "photo.jpg", "Revised caption");
        CHECK("captions-update",
            GetImageCaption(d.path, "photo.jpg") == "Revised caption");
    }

    // Empty caption is stored (and can clear a previous one)
    {
        TmpDir d;
        SaveImageCaption(d.path, "photo.jpg", "Something");
        SaveImageCaption(d.path, "photo.jpg", "");
        CHECK("captions-clear", GetImageCaption(d.path, "photo.jpg").empty());
    }

    return failures;
}
