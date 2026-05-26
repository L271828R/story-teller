#include "html_template.h"
#include "markdown.h"
#include <iostream>
#include <string>

int test_html_template() {
    int failures = 0;

    // DOCTYPE must be well-formed — a stray ')' puts the browser in quirks
    // mode, breaking CSS custom-property inheritance and dark-mode colours.
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find("<!DOCTYPE html>") == std::string::npos) {
            std::cerr << "FAIL [doctype]: expected '<!DOCTYPE html>' — got: "
                      << html.substr(0, html.find('\n')) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [doctype]\n";
        }
    }

    // Dark mode: <html class="dark"> must be present so .dark CSS rules fire.
    {
        std::string html = BuildHTML("", "test", true, 100);
        if (html.find("<html lang=\"en\" class=\"dark\">") == std::string::npos) {
            std::cerr << "FAIL [dark-class]: '<html lang=\"en\" class=\"dark\">' not found\n";
            ++failures;
        } else {
            std::cout << "PASS [dark-class]\n";
        }
    }

    // Light mode must NOT carry the dark class.
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find("class=\"dark\"") != std::string::npos) {
            std::cerr << "FAIL [light-no-dark-class]: class=\"dark\" found in light-mode HTML\n";
            ++failures;
        } else {
            std::cout << "PASS [light-no-dark-class]\n";
        }
    }

    // Font size is injected into the CSS.
    {
        std::string html = BuildHTML("", "test", false, 120);
        if (html.find("19.2px") == std::string::npos) {
            std::cerr << "FAIL [font-size]: expected '19.2px' for 120% not found\n";
            ++failures;
        } else {
            std::cout << "PASS [font-size]\n";
        }
    }

    // GetLLMReadme() covers the tidbit extension and core syntax.
    {
        std::string readme = GetLLMReadme();
        bool hasTidbit  = readme.find(":::tidbit") != std::string::npos;
        bool hasMermaid = readme.find("mermaid")   != std::string::npos;
        bool hasHeading = readme.find("# ")        != std::string::npos;
        if (!hasTidbit || !hasMermaid || !hasHeading) {
            std::cerr << "FAIL [llm-readme]: missing tidbit=" << hasTidbit
                      << " mermaid=" << hasMermaid << " heading=" << hasHeading << "\n";
            ++failures;
        } else {
            std::cout << "PASS [llm-readme]\n";
        }
    }

    // Copy button CSS is emitted (.copy-btn class present).
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find(".copy-btn") == std::string::npos) {
            std::cerr << "FAIL [copy-btn-css]: '.copy-btn' not found in HTML output\n";
            ++failures;
        } else {
            std::cout << "PASS [copy-btn-css]\n";
        }
    }

    // Copy button JS sends via the clipboardCopy message handler.
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find("clipboardCopy") == std::string::npos) {
            std::cerr << "FAIL [copy-btn-js]: 'clipboardCopy' handler not found in HTML output\n";
            ++failures;
        } else {
            std::cout << "PASS [copy-btn-js]\n";
        }
    }

    // GetLLMReadme() documents the copy button and text-vs-code fence rule.
    {
        std::string readme = GetLLMReadme();
        bool hasCopy     = readme.find("Copy") != std::string::npos;
        bool hasTextRule = readme.find("no code") != std::string::npos;
        bool hasLangTable= readme.find("bash") != std::string::npos;
        if (!hasCopy || !hasTextRule || !hasLangTable) {
            std::cerr << "FAIL [llm-copy]: --llm output missing copy docs, "
                         "text-fence rule, or language table\n";
            ++failures;
        } else {
            std::cout << "PASS [llm-copy]\n";
        }
    }

    // Copy button must NOT be appended inside <pre> — doing so causes WebKit
    // to re-parse the element and strip hljs highlight spans.
    {
        std::string html = BuildHTML("", "test", false, 100);
        if (html.find("block.parentElement.appendChild") != std::string::npos) {
            std::cerr << "FAIL [copy-btn-outside-pre]: copy button is appended inside <pre>; "
                         "use a wrapper div to avoid breaking hljs highlighting\n";
            ++failures;
        } else {
            std::cout << "PASS [copy-btn-outside-pre]\n";
        }
    }

    // hljs actually highlights Python code — run the bundled highlight.min.js
    // via Node and verify keyword/string spans appear on a hello-world block.
    {
        int rc = std::system("node tests/test_hljs.js highlight.min.js 2>&1");
        if (rc != 0) {
            std::cerr << "FAIL [hljs-python]: syntax highlighting test failed "
                         "(see output above)\n";
            ++failures;
        } else {
            std::cout << "PASS [hljs-python]\n";
        }
    }

    // BuildLogsHTML wraps log lines in a themed HTML table.
    {
        std::string raw =
            "2026-05-10 12:00:00  ReadFile OK: /tmp/test.md  (42 bytes)\n"
            "2026-05-10 12:00:01  ReadFile FAILED: /tmp/missing.md\n";
        std::string html = BuildLogsHTML(raw, "/tmp/mdviewer.log", false);
        bool hasDoctype = html.find("<!DOCTYPE html>") != std::string::npos;
        bool hasTable   = html.find("<table>")         != std::string::npos;
        bool hasOK      = html.find("ReadFile OK")     != std::string::npos;
        bool hasFailed  = html.find("ReadFile FAILED") != std::string::npos;
        if (!hasDoctype || !hasTable || !hasOK || !hasFailed) {
            std::cerr << "FAIL [logs-html]: BuildLogsHTML missing expected content: "
                      << "doctype=" << hasDoctype << " table=" << hasTable
                      << " ok-line=" << hasOK << " failed-line=" << hasFailed << "\n";
            ++failures;
        } else {
            std::cout << "PASS [logs-html]\n";
        }
    }

    // Long CJK/no-space text should wrap inside the document column.
    {
        std::string html = BuildHTML("<p>很长很长很长很长很长很长很长很长很长很长</p>", "wrap", false, 100);
        bool bodyWrap = html.find("overflow-wrap:anywhere") != std::string::npos;
        bool wordBreak = html.find("word-break:break-word") != std::string::npos;
        if (!bodyWrap || !wordBreak) {
            std::cerr << "FAIL [cjk-wrap-css]: wrap CSS missing\n";
            ++failures;
        } else {
            std::cout << "PASS [cjk-wrap-css]\n";
        }
    }

    // The note toolbar is position:fixed, so its left/top must be set from
    // getBoundingClientRect() coords directly — adding scrollX/scrollY converts
    // to document coordinates and pushes the toolbar off-screen for scrolled pages.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool badLeft = html.find("rect.left + window.scrollX") != std::string::npos;
        bool badTop  = html.find("rect.top + window.scrollY")  != std::string::npos;
        if (badLeft || badTop) {
            std::cerr << "FAIL [toolbar-fixed-positioning]: toolbar uses scroll offsets "
                         "with position:fixed — remove scrollX/scrollY from positioning\n";
            ++failures;
        } else {
            std::cout << "PASS [toolbar-fixed-positioning]\n";
        }
    }

    // Focus mode wrapAll must process atoms at the block level, not per-text-node.
    // If it walks individual text nodes, a note-marker span splits the paragraph
    // into micro-chunks of 1 word each. The fix: iterate direct children of block
    // elements (p, h2, li, …) so that text nodes and inline spans in the same
    // paragraph share one 10-word budget.
    {
        std::string html = BuildHTML("", "test", false, 100);
        // The old per-text-node approach used:
        //   createTreeWalker(document.body, NodeFilter.SHOW_TEXT, ...)
        //   nodes.forEach(function(textNode) { ... makeChunks(textNode.textContent ...) ... })
        // The block-level approach instead iterates block element children.
        // We check that wrapAll does NOT do per-text-node makeChunks:
        // it must NOT call makeChunks(textNode.textContent inside a forEach over text nodes.
        // Instead it should collect atoms from block direct children.
        bool usesBlockAtoms = html.find("firstChild") != std::string::npos &&
                              html.find("nextSibling") != std::string::npos;
        // Must not process each text node's textContent individually
        // (old code had: makeChunks(textNode.textContent, CHUNK_SIZE))
        bool noPerTextNodeChunk =
            html.find("makeChunks(textNode.textContent") == std::string::npos;
        // Must use a CJK-aware token counter (not /\S+/g) for inline atoms
        // so Chinese text (no spaces) is counted character-by-character.
        bool usesCjkCounter = html.find("TOKRE") != std::string::npos &&
                              html.find("tokenCount") != std::string::npos;
        // Must detach original block children before appending chunk spans —
        // otherwise the original text nodes remain in the DOM alongside the new
        // chunk spans and every word appears twice, breaking focus navigation.
        bool detachesFirst =
            html.find("while (block.firstChild)") != std::string::npos &&
            html.find("temp.appendChild(block.firstChild)") != std::string::npos;
        // Inline atoms (note-markers) must be appended to cur FIRST, then the
        // chunk is committed if full — not pre-committed before appending.
        // Pre-commit isolates the marker in its own 2-3 word chunk.
        bool postCommitInline =
            html.find("cur.appendChild(atom); // post-commit") != std::string::npos;
        if (!usesBlockAtoms || !noPerTextNodeChunk || !usesCjkCounter ||
            !detachesFirst || !postCommitInline) {
            std::cerr << "FAIL [focus-mode-block-atoms]: wrapAll must iterate block "
                         "element children (firstChild/nextSibling), use CJK-aware "
                         "tokenCount (TOKRE), detach children via temp fragment, and "
                         "post-commit inline atoms (not pre-commit)\n";
            ++failures;
        } else {
            std::cout << "PASS [focus-mode-block-atoms]\n";
        }
    }

    // Focus mode: CSS class and JS toggle function must be present.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasCss = html.find(".focus-line") != std::string::npos;
        bool hasJs  = html.find("toggleFocusMode") != std::string::npos;
        if (!hasCss || !hasJs) {
            std::cerr << "FAIL [focus-mode]: missing focus-line CSS=" << hasCss
                      << " toggleFocusMode JS=" << hasJs << "\n";
            ++failures;
        } else {
            std::cout << "PASS [focus-mode]\n";
        }
    }

    // Note markers must use a dotted underline (not an icon) so annotated
    // text is visually distinct without adding inline clutter.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasUnderline = html.find("text-decoration") != std::string::npos &&
                            html.find("underline")       != std::string::npos;
        bool noIcon       = html.find("\xf0\x9f\x93\x9d") == std::string::npos; // no 📝 in CSS/JS
        if (!hasUnderline || !noIcon) {
            std::cerr << "FAIL [note-marker-underline]: note-marker CSS must use underline "
                         "decoration, not a 📝 icon (hasUnderline=" << hasUnderline
                      << " noIcon=" << noIcon << ")\n";
            ++failures;
        } else {
            std::cout << "PASS [note-marker-underline]\n";
        }
    }

    // Note popover must expose a delete action so the user can remove notes
    // from the view without opening the edit dialog.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasDeleteAction = html.find("action:'delete'") != std::string::npos ||
                               html.find("action:\"delete\"") != std::string::npos;
        if (!hasDeleteAction) {
            std::cerr << "FAIL [note-delete-action]: no delete action found in note JS\n";
            ++failures;
        } else {
            std::cout << "PASS [note-delete-action]\n";
        }
    }

    // Hovering a note marker must show a tooltip with the note text.
    // Check for the tooltip CSS class and that getAttribute('data-note-text')
    // is used inside a mouseover handler (not just present on elements).
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasTooltipClass   = html.find("note-tooltip") != std::string::npos;
        bool hasGetNoteText    = html.find("getAttribute('data-note-text')") != std::string::npos ||
                                 html.find("getAttribute(\"data-note-text\")") != std::string::npos;
        if (!hasTooltipClass || !hasGetNoteText) {
            std::cerr << "FAIL [note-hover-tooltip]: hasTooltipClass=" << hasTooltipClass
                      << " hasGetNoteText=" << hasGetNoteText << "\n";
            ++failures;
        } else {
            std::cout << "PASS [note-hover-tooltip]\n";
        }
    }

    // Tidbit carousel must have both a prev (‹) and a next (›) arrow so the
    // user can navigate in both directions.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasPrev = html.find("&#x2039;") != std::string::npos ||
                       html.find("\xe2\x80\xb9")  != std::string::npos ||  // ‹ UTF-8
                       html.find("'\\u2039'")     != std::string::npos ||
                       html.find("'‹'")           != std::string::npos;
        bool hasNext = html.find("&#x203a;") != std::string::npos ||
                       html.find("\xe2\x80\xba")  != std::string::npos ||  // › UTF-8
                       html.find("'\\u203a'")     != std::string::npos ||
                       html.find("'›'")           != std::string::npos;
        if (!hasPrev || !hasNext) {
            std::cerr << "FAIL [carousel-both-arrows]: carousel must have both ‹ prev and › next arrows"
                      << " (hasPrev=" << hasPrev << " hasNext=" << hasNext << ")\n";
            ++failures;
        } else {
            std::cout << "PASS [carousel-both-arrows]\n";
        }
    }

    // Quiz interactive: CSS classes for option buttons must be present.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasOptClass     = html.find("quiz-opt")     != std::string::npos;
        bool hasCorrectClass = html.find("quiz-correct") != std::string::npos;
        bool hasWrongClass   = html.find("quiz-wrong")   != std::string::npos;
        if (!hasOptClass || !hasCorrectClass || !hasWrongClass) {
            std::cerr << "FAIL [quiz-interactive-css]: quiz CSS missing:"
                      << " quiz-opt="     << hasOptClass
                      << " quiz-correct=" << hasCorrectClass
                      << " quiz-wrong="   << hasWrongClass << "\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-interactive-css]\n";
        }
    }

    // Quiz interactive: initQuiz JS function must be present and hide answers.
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasInitQuiz  = html.find("initQuiz")  != std::string::npos;
        bool hidesAnswer  = html.find("Answer:")   != std::string::npos ||
                            html.find("visibility") != std::string::npos;
        if (!hasInitQuiz || !hidesAnswer) {
            std::cerr << "FAIL [quiz-interactive-js]: initQuiz JS function missing or"
                      << " answer hiding logic absent (hasInitQuiz=" << hasInitQuiz
                      << " hidesAnswer=" << hidesAnswer << ")\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-interactive-js]\n";
        }
    }

    // Quiz: options use 2-column grid so A/B and C/D are always row-aligned
    {
        std::string html = BuildHTML("", "test", false, 100);
        bool hasGrid  = html.find("grid-template-columns") != std::string::npos;
        bool hasBadge = html.find("quiz-opt-badge") != std::string::npos;
        if (!hasGrid || !hasBadge) {
            std::cerr << "FAIL [quiz-grid-layout]: hasGrid=" << hasGrid
                      << " hasBadge=" << hasBadge << "\n";
            ++failures;
        } else {
            std::cout << "PASS [quiz-grid-layout]\n";
        }
    }

    // Image section carousel: CSS class and JS function present in template
    {
        std::string html = BuildHTML("<p>x</p>", "t", false, 100);
        bool hasCss = html.find("img-carousel") != std::string::npos;
        bool hasJs  = html.find("imgCarMove")   != std::string::npos;
        if (!hasCss || !hasJs) {
            std::cerr << "FAIL [img-carousel-template]: hasCss=" << hasCss
                      << " hasJs=" << hasJs << "\n";
            ++failures;
        } else {
            std::cout << "PASS [img-carousel-template]\n";
        }
    }

    // When showChatBubbles=false, a CSS rule hides all chat buttons
    {
        std::string withBubbles    = BuildHTML("<p>x</p>", "t", false, 100, {}, true);
        std::string withoutBubbles = BuildHTML("<p>x</p>", "t", false, 100, {}, false);
        bool bubblesCssPresent = withBubbles.find(".chat-btn") != std::string::npos;
        bool bubblesHidden     = withoutBubbles.find("#doc-chat-btn{display:none}") != std::string::npos;
        if (!bubblesCssPresent || !bubblesHidden) {
            std::cerr << "FAIL [hide-chat-bubbles]: bubblesCssPresent=" << bubblesCssPresent
                      << " bubblesHidden=" << bubblesHidden << "\n";
            ++failures;
        } else {
            std::cout << "PASS [hide-chat-bubbles]\n";
        }
    }

    return failures;
}
