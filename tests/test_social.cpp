#include "social.h"
#include <iostream>
#include <string>

#define CHECK(label, cond) \
    do { if (cond) { std::cout << "PASS [" label "]\n"; } \
         else { std::cerr << "FAIL [" label "]\n"; ++failures; } } while(0)

int test_social() {
    int failures = 0;

    // StripSocialNoise — removes tidbits
    {
        std::string md =
            "## Chapter 1\n"
            "Some text.\n"
            ":::tidbit[Confucius]\n"
            "Wisdom here.\n"
            ":::\n"
            "More text.\n";
        std::string r = StripSocialNoise(md);
        CHECK("strip-tidbits-gone",    r.find(":::tidbit") == std::string::npos);
        CHECK("strip-tidbits-content", r.find("Wisdom here") == std::string::npos);
        CHECK("strip-tidbits-keeps-heading", r.find("Chapter 1") != std::string::npos);
        CHECK("strip-tidbits-keeps-more", r.find("More text") != std::string::npos);
    }

    // StripSocialNoise — removes conversations
    {
        std::string md =
            "## Chapter 1\n"
            "Content.\n"
            ":::conversation[Chapter 1]\n"
            "Q: question?\n"
            "A: answer.\n"
            ":::\n"
            "After.\n";
        std::string r = StripSocialNoise(md);
        CHECK("strip-conv-gone",  r.find(":::conversation") == std::string::npos);
        CHECK("strip-conv-qa",    r.find("Q: question") == std::string::npos);
        CHECK("strip-conv-after", r.find("After") != std::string::npos);
    }

    // StripSocialNoise — removes HTML comments
    {
        std::string md = "<!-- ch:1 -->\n## Chapter 1\n<!-- tb:5 -->\nText.\n";
        std::string r = StripSocialNoise(md);
        CHECK("strip-comments-gone",  r.find("<!--") == std::string::npos);
        CHECK("strip-comments-keeps", r.find("Chapter 1") != std::string::npos);
    }

    // StripSocialNoise — leaves plain content untouched
    {
        std::string md = "## Heading\n\nSome paragraph.\n";
        std::string r = StripSocialNoise(md);
        CHECK("strip-plain-heading",   r.find("Heading") != std::string::npos);
        CHECK("strip-plain-paragraph", r.find("Some paragraph") != std::string::npos);
    }

    // BuildHookPrompt — includes document content
    {
        std::string md = "## Darwin\nHe discovered evolution.";
        std::string p = BuildHookPrompt(md, "english", "high");
        bool hasTopic = p.find("Darwin") != std::string::npos
                     || p.find("evolution") != std::string::npos;
        CHECK("hook-prompt-has-content", hasTopic);
    }

    // BuildHookPrompt — language variants
    {
        std::string p_en = BuildHookPrompt("content", "english", "medium");
        std::string p_zh = BuildHookPrompt("content", "chinese", "medium");
        std::string p_bi = BuildHookPrompt("content", "bilingual", "medium");
        CHECK("hook-prompt-english",
            p_en.find("English") != std::string::npos || p_en.find("english") != std::string::npos);
        CHECK("hook-prompt-chinese",
            p_zh.find("Chinese") != std::string::npos || p_zh.find("Mandarin") != std::string::npos);
        CHECK("hook-prompt-bilingual",
            p_bi.find("bilingual") != std::string::npos || p_bi.find("both") != std::string::npos
            || p_bi.find("Both") != std::string::npos);
        std::string p_es = BuildHookPrompt("content", "spanish", "medium");
        std::string p_it = BuildHookPrompt("content", "italian", "medium");
        CHECK("hook-prompt-spanish", p_es.find("Spanish") != std::string::npos);
        CHECK("hook-prompt-italian", p_it.find("Italian") != std::string::npos);
    }

    // BuildHookPrompt — duration hint
    {
        std::string p = BuildHookPrompt("content", "english", "low");
        bool hasDuration = p.find("5") != std::string::npos || p.find("six") != std::string::npos
                        || p.find("hook") != std::string::npos || p.find("Hook") != std::string::npos;
        CHECK("hook-prompt-duration", hasDuration);
    }

    // BuildHookPrompt — nudge text appears in prompt
    {
        std::string p = BuildHookPrompt("content", "english", "medium", "focus on the island isolation angle");
        CHECK("hook-prompt-nudge", p.find("island isolation") != std::string::npos);
        std::string pEmpty = BuildHookPrompt("content", "english", "medium", "");
        CHECK("hook-prompt-no-nudge-clean", pEmpty.find("Additional") == std::string::npos);
    }

    // BuildHookPrompt — previous hooks injected so LLM avoids repeating
    {
        std::vector<std::string> prev = {"Did you know Darwin almost missed the boat?",
                                         "Evolution wasn't Darwin's idea — until it was."};
        std::string p = BuildHookPrompt("content", "english", "medium", "", prev);
        CHECK("hook-prompt-prev-hook0", p.find("Darwin almost missed") != std::string::npos);
        CHECK("hook-prompt-prev-hook1", p.find("Evolution wasn") != std::string::npos);
        // Empty previous list: no "Previously" section
        std::string pClean = BuildHookPrompt("content", "english", "medium", "", {});
        CHECK("hook-prompt-no-prev-clean", pClean.find("Previously") == std::string::npos);
    }

    // BuildPackagingPrompt — includes document content
    {
        std::string md = "## Topic\nSome content about evolution.";
        std::string p = BuildPackagingPrompt(md);
        bool hasTopic = p.find("evolution") != std::string::npos
                     || p.find("Topic") != std::string::npos;
        CHECK("packaging-prompt-has-content", hasTopic);
    }

    // BuildPackagingPrompt — requests all four fields
    {
        std::string p = BuildPackagingPrompt("some content");
        CHECK("packaging-prompt-title",
            p.find("title") != std::string::npos || p.find("Title") != std::string::npos);
        CHECK("packaging-prompt-description",
            p.find("description") != std::string::npos || p.find("Description") != std::string::npos);
        CHECK("packaging-prompt-thumbnail",
            p.find("thumbnail") != std::string::npos || p.find("Thumbnail") != std::string::npos);
        CHECK("packaging-prompt-hashtags",
            p.find("hashtag") != std::string::npos || p.find("Hashtag") != std::string::npos);
    }

    // BuildAtomizationPrompt — includes document content
    {
        std::string md = "## Chapter 1\nStuff.\n## Chapter 2\nMore stuff.";
        std::string p = BuildAtomizationPrompt(md);
        bool hasTopic = p.find("Chapter 1") != std::string::npos
                     || p.find("Chapter 2") != std::string::npos;
        CHECK("atomize-prompt-has-content", hasTopic);
    }

    // BuildAtomizationPrompt — mentions clips
    {
        std::string p = BuildAtomizationPrompt("content");
        bool hasClip = p.find("clip") != std::string::npos || p.find("Clip") != std::string::npos
                    || p.find("cut") != std::string::npos || p.find("segment") != std::string::npos;
        CHECK("atomize-prompt-clips", hasClip);
    }

    return failures;
}
