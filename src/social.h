#pragma once
#include <string>
#include <vector>

struct SocialClip {
    int         num;
    std::string title;   // chapter/section heading
    std::string hook;    // suggested mini-clip hook line
    std::string timing;  // e.g. "~90 sec"
};

struct SocialPlan {
    std::string hookScript;        // 5–6 second spoken hook
    std::string youtubeTitle;
    std::string description;
    std::string thumbnailConcept;
    std::string hashtags;
    std::vector<SocialClip> clips;
};

// Strip :::conversation ... ::: and :::tidbit ... ::: blocks and HTML comments.
std::string StripSocialNoise(const std::string& md);

// Appends an [Images in this document] section to content using captions from
// projectDir's .image_captions.json. Image filenames are extracted from the
// markdown; only those that have a caption are included.
// Returns content unchanged if projectDir is empty or no captioned images are found.
std::string InjectImageCaptions(const std::string& md,
                                 const std::string& projectDir);

// Builds a prompt asking the LLM to write a 5–6 second hook.
// language: "english", "chinese", or "bilingual"
// energy:   "low", "medium", or "high"
std::string BuildHookPrompt(const std::string& md,
                             const std::string& language,
                             const std::string& energy,
                             const std::string& nudge = "",
                             const std::vector<std::string>& previousHooks = {});

// Builds a prompt for YouTube content packaging:
// title, description, thumbnail concept, hashtags.
std::string BuildPackagingPrompt(const std::string& md);

// Builds a prompt that identifies natural clip/cut moments in the document.
std::string BuildAtomizationPrompt(const std::string& md);
