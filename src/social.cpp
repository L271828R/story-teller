#include "social.h"
#include "image_captions.h"
#include <sstream>

// Removes :::fence ... ::: blocks whose opening tag starts with the given keyword.
static std::string stripFenceBlocks(const std::string& md, const std::string& keyword) {
    std::string result;
    result.reserve(md.size());
    size_t i = 0;
    const size_t n = md.size();

    while (i < n) {
        size_t lineEnd = md.find('\n', i);
        if (lineEnd == std::string::npos) lineEnd = n;
        std::string line = md.substr(i, lineEnd - i);

        size_t c = 0;
        while (c < line.size() && line[c] == ':') ++c;
        bool isOpen = c >= 3
            && line.size() > c + keyword.size()
            && line.substr(c, keyword.size()) == keyword;

        if (isOpen) {
            // Skip until closing fence ":::"
            size_t j = lineEnd + 1;
            while (j < n) {
                size_t end = md.find('\n', j);
                if (end == std::string::npos) end = n;
                std::string cl = md.substr(j, end - j);
                size_t cc = 0;
                while (cc < cl.size() && cl[cc] == ':') ++cc;
                bool rest = cc == cl.size() || cl[cc] == '\r';
                if (cc >= 3 && rest) {
                    i = end + 1;
                    break;
                }
                j = end + 1;
            }
            if (j >= n) i = n;
            // Skip following blank line
            while (i < n && md[i] == '\n') ++i;
            if (i < n) result += '\n';
        } else {
            result += line;
            if (lineEnd < n) result += '\n';
            i = lineEnd + 1;
        }
    }
    return result;
}

// Removes <!-- ... --> HTML comments.
static std::string stripHtmlComments(const std::string& s) {
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        if (s.substr(i, 4) == "<!--") {
            size_t end = s.find("-->", i + 4);
            if (end == std::string::npos) break;
            i = end + 3;
            // Eat the newline left behind
            if (i < s.size() && s[i] == '\n') ++i;
        } else {
            out += s[i++];
        }
    }
    return out;
}

std::string StripSocialNoise(const std::string& md) {
    std::string r = stripFenceBlocks(md, "tidbit");
    r = stripFenceBlocks(r, "conversation");
    r = stripHtmlComments(r);
    return r;
}

// Extract image filenames referenced in the markdown (![...](filename) syntax).
static std::vector<std::string> extractImageFilenames(const std::string& md) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < md.size()) {
        // Look for "!["
        auto bang = md.find("![", i);
        if (bang == std::string::npos) break;
        // Skip alt text to find "]("
        auto closeBracket = md.find("](", bang + 2);
        if (closeBracket == std::string::npos) break;
        // Find closing ")"
        auto closeParen = md.find(")", closeBracket + 2);
        if (closeParen == std::string::npos) break;
        std::string url = md.substr(closeBracket + 2, closeParen - closeBracket - 2);
        // Only local filenames (no http/data URI)
        if (url.find("://") == std::string::npos && !url.empty()) {
            // Strip path components — we only want the basename
            auto slash = url.rfind('/');
            std::string name = (slash == std::string::npos) ? url : url.substr(slash + 1);
            if (!name.empty()) out.push_back(name);
        }
        i = closeParen + 1;
    }
    return out;
}

std::string InjectImageCaptions(const std::string& md,
                                  const std::string& projectDir) {
    if (projectDir.empty()) return md;
    auto caps = LoadImageCaptions(projectDir);
    if (caps.empty()) return md;

    auto filenames = extractImageFilenames(md);
    std::string section;
    for (const auto& name : filenames) {
        auto it = caps.find(name);
        if (it == caps.end() || it->second.empty()) continue;
        section += "- " + name + ": " + it->second + "\n";
    }
    if (section.empty()) return md;

    return md + "\n\n[Images in this document]\n" + section;
}

std::string BuildHookPrompt(const std::string& md,
                             const std::string& language,
                             const std::string& energy,
                             const std::string& nudge,
                             const std::vector<std::string>& previousHooks) {
    std::string langInstr;
    if (language == "chinese")
        langInstr = "Write the hook entirely in Mandarin Chinese (simplified characters).";
    else if (language == "bilingual")
        langInstr = "Write the hook in both English and Mandarin Chinese (simplified), "
                    "interleaving the two languages naturally — the way a bilingual creator speaks.";
    else if (language == "spanish")
        langInstr = "Write the hook entirely in Spanish.";
    else if (language == "italian")
        langInstr = "Write the hook entirely in Italian.";
    else
        langInstr = "Write the hook in English.";

    std::string energyInstr;
    if (energy == "high")
        energyInstr = "Use high energy: punchy, fast, dramatic.";
    else if (energy == "low")
        energyInstr = "Use a calm, curious, inviting tone.";
    else
        energyInstr = "Use a warm, engaging, medium-energy tone.";

    std::ostringstream p;
    p << "You are a social media content strategist specializing in educational YouTube and TikTok videos.\n\n"
      << "The following is a document that will be read on camera. "
         "Your job is to write a SHORT HOOK — a 5 to 6 second spoken opener "
         "(approximately 15–20 words) that grabs attention and makes the viewer desperate to keep watching.\n\n"
      << langInstr << "\n"
      << energyInstr << "\n\n"
      << "Rules:\n"
      << "- No more than 20 words total.\n"
      << "- Must create immediate curiosity or surprise — a \"pattern interruption\".\n"
      << "- Do NOT start with \"In this video\" or \"Today we will\".\n"
      << "- The hook should hint at the most surprising or counter-intuitive fact in the document.\n\n"
      << "Output ONLY the hook text, nothing else. No labels, no explanation.\n\n";
    if (!previousHooks.empty()) {
        p << "Previously generated hooks for this document (write something meaningfully different — "
             "different angle, different opening word, different surprise):\n";
        for (const auto& h : previousHooks)
            p << "- " << h << "\n";
        p << "\n";
    }
    if (!nudge.empty())
        p << "Additional direction from the creator: " << nudge << "\n\n";
    p << "Document:\n---\n" << md << "\n---\n";
    return p.str();
}

std::string BuildPackagingPrompt(const std::string& md) {
    std::ostringstream p;
    p << "You are a YouTube content strategist. "
         "The following document will be presented as an educational video by a bilingual creator "
         "(English and Mandarin Chinese).\n\n"
      << "Produce the following content packaging elements. "
         "Use this exact format so it can be parsed:\n\n"
      << "TITLE: <a compelling YouTube title, under 70 characters, SEO-optimized>\n"
      << "DESCRIPTION: <a 3–5 sentence YouTube description with keywords and a bilingual call-to-action. "
         "Include relevant English and Chinese search terms.>\n"
      << "THUMBNAIL: <describe the ideal thumbnail concept: what text overlay, what visual, what emotion — "
         "be specific enough for a designer to execute it in 2 sentences>\n"
      << "HASHTAGS: <10–15 hashtags, mix of English and Chinese, comma-separated>\n\n"
      << "Document:\n---\n" << md << "\n---\n";
    return p.str();
}

std::string BuildAtomizationPrompt(const std::string& md) {
    std::ostringstream p;
    p << "You are a video editor and content strategist. "
         "The following is an educational document that will be read on camera as a long-form video.\n\n"
      << "Your task: identify the best CLIP MOMENTS — natural pause points or topic shifts "
         "that could each stand alone as a 60–120 second mini-video for YouTube Shorts or TikTok.\n\n"
      << "For each clip moment, output in this exact format:\n"
      << "CLIP N: <section title or heading>\n"
      << "MINI-TITLE: <a punchy short-form video title for this clip, under 50 characters>\n"
      << "HOOK: <a one-sentence hook (15 words max) to open this clip>\n"
      << "DURATION: <estimated duration, e.g. ~60 sec, ~90 sec, ~2 min>\n"
      << "PAUSE-CUE: <describe a natural visual or verbal pause point where the cut should happen>\n\n"
      << "List every clip moment you find. Aim for 3–8 clips depending on document length.\n\n"
      << "Document:\n---\n" << md << "\n---\n";
    return p.str();
}
