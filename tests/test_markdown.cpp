#include "markdown.h"
#include <iostream>
#include <string>

int test_markdown() {
    int failures = 0;

    // :::tidbit[Speaker] block renders as a <details> widget.
    {
        std::string md =
            ":::tidbit[Bjarne Stroustrup]\n"
            "vtables are a feature, not a bug.\n"
            ":::\n";
        std::string html = RenderMarkdown(md);
        bool hasDetails = html.find("<details class=\"tidbit\">") != std::string::npos;
        bool hasSpeaker = html.find("Bjarne Stroustrup") != std::string::npos;
        bool hasContent = html.find("vtables are a feature") != std::string::npos;
        if (!hasDetails || !hasSpeaker || !hasContent) {
            std::cerr << "FAIL [tidbit]: expected <details class=\"tidbit\"> with speaker and content\n"
                      << "  got: " << html << "\n";
            ++failures;
        } else {
            std::cout << "PASS [tidbit]\n";
        }
    }

    // Find-count scenario: fruit list renders with exactly 3 visible "apple" words.
    // (Part A of the find-count test — verifies the rendered text; Part B is in
    //  test_mdviewer.cpp which checks the DoFind implementation.)
    {
        std::string md =
            "# Fruits\n\n"
            "- apple\n"
            "- banana\n"
            "- apple\n"
            "- cherry\n"
            "- apple\n";
        std::string body = RenderMarkdown(md);

        std::string visible;
        bool inTag = false;
        for (char c : body) {
            if      (c == '<') { inTag = true;  continue; }
            else if (c == '>') { inTag = false; continue; }
            else if (!inTag)   visible += c;
        }
        int count = 0;
        for (size_t p = 0; (p = visible.find("apple", p)) != std::string::npos; ++p)
            ++count;

        if (count != 3) {
            std::cerr << "FAIL [fruit-render]: expected 3 visible 'apple' occurrences, got "
                      << count << "\n";
            ++failures;
        } else {
            std::cout << "PASS [fruit-render]\n";
        }
    }

    // Trailing two spaces on paragraph lines (LLM artifact) must NOT produce
    // visible "<br>" text. ProcessInline escapes '<' to '&lt;', so the buggy
    // flushParagraph produces the literal text "&lt;br>" that the user sees
    // between sentences. Both that and a real <br> element are wrong here.
    {
        std::string md =
            "First sentence.  \n"
            "Second sentence.  \n"
            "Third sentence.  \n";
        std::string html = RenderMarkdown(md);
        // ProcessInline escapes both < and >, so the bug produces "&lt;br&gt;"
        // as visible text. Also guard against a raw <br> element just in case.
        bool hasVisibleBr = html.find("&lt;br&gt;") != std::string::npos;
        bool hasHtmlBr    = html.find("<br>")       != std::string::npos;
        if (hasVisibleBr || hasHtmlBr) {
            std::cerr << "FAIL [no-br-trailing-spaces]: trailing spaces on paragraph lines "
                         "must not produce visible <br> text or a <br> element\n"
                      << "  got: " << html << "\n";
            ++failures;
        } else {
            std::cout << "PASS [no-br-trailing-spaces]\n";
        }
    }

    // :::image block renders as a collapsible <details class="image-section">
    {
        std::string md =
            "## Chapter One\n\n"
            "Some text.\n\n"
            ":::image\n"
            "![|400px|center](photo.jpg)\n"
            ":::\n";
        std::string html = RenderMarkdown(md);
        bool hasDetails  = html.find("image-section") != std::string::npos;
        bool hasSummary  = html.find("<summary>") != std::string::npos;
        bool hasImg      = html.find("<img ") != std::string::npos;
        bool hasPhotoRef = html.find("photo.jpg") != std::string::npos;
        if (!hasDetails || !hasSummary || !hasImg || !hasPhotoRef) {
            std::cerr << "FAIL [image-section-renders]: hasDetails=" << hasDetails
                      << " hasSummary=" << hasSummary
                      << " hasImg=" << hasImg
                      << " hasPhotoRef=" << hasPhotoRef
                      << "\n  got: " << html.substr(0, 300) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [image-section-renders]\n";
        }
    }

    // Single image in :::image block — no carousel, just bare img
    {
        std::string md = ":::image\n![|400px|center](solo.jpg)\n:::\n";
        std::string html = RenderMarkdown(md);
        bool hasImg      = html.find("solo.jpg") != std::string::npos;
        bool noCarousel  = html.find("img-carousel") == std::string::npos;
        if (!hasImg || !noCarousel) {
            std::cerr << "FAIL [image-section-single]: hasImg=" << hasImg
                      << " noCarousel=" << noCarousel
                      << "\n  got: " << html.substr(0, 300) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [image-section-single]\n";
        }
    }

    // Two images → carousel with arrows and counter
    {
        std::string md =
            ":::image\n"
            "![|400px|center](first.jpg)\n"
            "![|300px|left](second.png)\n"
            ":::\n";
        std::string html = RenderMarkdown(md);
        bool hasCarousel = html.find("img-carousel") != std::string::npos;
        bool hasFirst    = html.find("first.jpg")    != std::string::npos;
        bool hasSecond   = html.find("second.png")   != std::string::npos;
        bool hasArrows   = html.find("imgCarMove")   != std::string::npos;
        bool hasCounter  = html.find("1 / 2")        != std::string::npos;
        if (!hasCarousel || !hasFirst || !hasSecond || !hasArrows || !hasCounter) {
            std::cerr << "FAIL [image-section-carousel]: hasCarousel=" << hasCarousel
                      << " hasFirst=" << hasFirst << " hasSecond=" << hasSecond
                      << " hasArrows=" << hasArrows << " hasCounter=" << hasCounter
                      << "\n  got: " << html.substr(0, 500) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [image-section-carousel]\n";
        }
    }

    // language comment is stripped — never visible as text
    {
        std::string md =
            "<!-- language: (not specified) -->\n\n"
            "# Hello\n";
        std::string html = RenderMarkdown(md);
        bool noComment = html.find("language:") == std::string::npos &&
                         html.find("not specified") == std::string::npos;
        if (!noComment) {
            std::cerr << "FAIL [language-comment-hidden]: comment leaked into output\n"
                      << "  got: " << html << "\n";
            ++failures;
        } else {
            std::cout << "PASS [language-comment-hidden]\n";
        }
    }

    return failures;
}
