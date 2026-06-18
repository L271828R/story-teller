#include "markdown.h"
#include "tab_util.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

int test_mdviewer() {
    int failures = 0;

    // GetString() must be used for script message payloads, not GetURL().
    // GetURL() returns the page URL (file:///...), not the postMessage value.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool usesGetString = code.find("evt.GetString()") != std::string::npos;
        bool usesGetURLForClipboard =
            code.find("clipboardCopy") != std::string::npos &&
            code.find("SetData(new wxTextDataObject(evt.GetURL()") != std::string::npos;
        if (!usesGetString || usesGetURLForClipboard) {
            std::cerr << "FAIL [getstring-not-geturl]: script message payload must be "
                         "read via GetString(), not GetURL() (GetURL returns the page URL)\n";
            ++failures;
        } else {
            std::cout << "PASS [getstring-not-geturl]\n";
        }
    }

    // Edit menu wires up copy via m_webView->Copy().
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasCopy   = code.find("m_webView->Copy()") != std::string::npos;
        bool hasCopyId = code.find("wxID_COPY") != std::string::npos;
        if (!hasCopy || !hasCopyId) {
            std::cerr << "FAIL [edit-copy]: expected m_webView->Copy() and wxID_COPY "
                         "in mdviewer.cpp\n";
            ++failures;
        } else {
            std::cout << "PASS [edit-copy]\n";
        }
    }

    // Find in page uses m_webView->Find() — not a JS workaround.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasFind   = code.find("m_webView->Find(") != std::string::npos;
        bool hasFindId = code.find("wxID_FIND") != std::string::npos;
        if (!hasFind || !hasFindId) {
            std::cerr << "FAIL [find-in-page]: expected m_webView->Find( and wxID_FIND "
                         "in mdviewer.cpp\n";
            ++failures;
        } else {
            std::cout << "PASS [find-in-page]\n";
        }
    }

    // Paste to render reads clipboard via wxTheClipboard.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasPaste   = code.find("wxID_PASTE") != std::string::npos;
        bool hasClipGet = code.find("wxTheClipboard->GetData") != std::string::npos;
        if (!hasPaste || !hasClipGet) {
            std::cerr << "FAIL [paste-render]: expected wxID_PASTE and "
                         "wxTheClipboard->GetData in mdviewer.cpp\n";
            ++failures;
        } else {
            std::cout << "PASS [paste-render]\n";
        }
    }

    // Find bar must report the correct match count for a known document.
    // wxWebView::Find() returns -1 on macOS WKWebView (async under the hood),
    // so DoFind must obtain the total count via RunScript instead.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool usesRunScript = code.find("RunScript") != std::string::npos;
        if (!usesRunScript) {
            std::cerr << "FAIL [find-count]: DoFind must count via RunScript/JS, not "
                         "wxWebView::Find() return value (unreliable on macOS WKWebView)\n";
            ++failures;
        } else {
            std::cout << "PASS [find-count]\n";
        }
    }

    // OnViewLogs must switch to the View tab, otherwise the rendered HTML is
    // hidden behind whichever tab the user is currently on.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        // Find the function *definition* (not the event-table entry) and confirm
        // SetSelection appears before the closing brace.
        const std::string sig = "void MDViewerFrame::OnViewLogs(";
        auto pos = code.find(sig);
        bool hasSetSelection = false;
        if (pos != std::string::npos) {
            auto end = code.find("\n}\n", pos);
            if (end != std::string::npos)
                hasSetSelection = code.find("SetSelection", pos) < end;
        }
        if (!hasSetSelection) {
            std::cerr << "FAIL [view-logs-switches-tab]: OnViewLogs must call "
                         "m_notebook->SetSelection to make the View tab visible\n";
            ++failures;
        } else {
            std::cout << "PASS [view-logs-switches-tab]\n";
        }
    }

    // File-change reload: when the app is activated (user switches back from vim),
    // mdviewer must compare the file's mtime and re-render if it changed.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasActivate = code.find("wxEVT_ACTIVATE") != std::string::npos;
        bool hasMtime    = code.find("m_fileMtime")    != std::string::npos;
        if (!hasActivate || !hasMtime) {
            std::cerr << "FAIL [reload-on-file-change]: mdviewer must bind wxEVT_ACTIVATE "
                         "and track m_fileMtime to reload files edited externally\n";
            ++failures;
        } else {
            std::cout << "PASS [reload-on-file-change]\n";
        }
    }

    // Window title must include the process PID so the user can tell two running
    // instances apart.  Both the constructor title and SetTitle calls in LoadFile
    // must embed "[<pid>]".
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool usesGetpid  = code.find("getpid()") != std::string::npos;
        bool hasPidBrace = code.find("[%d]")     != std::string::npos ||
                           code.find("[\" +")    != std::string::npos ||
                           code.find("\"[\"")    != std::string::npos;
        if (!usesGetpid || !hasPidBrace) {
            std::cerr << "FAIL [title-has-pid]: window title must embed getpid() "
                         "in [PID] brackets so multiple instances are distinguishable\n";
            ++failures;
        } else {
            std::cout << "PASS [title-has-pid]\n";
        }
    }

    // OnActivate must suppress wx's internal error dialog when GetTimes fails on
    // a deleted file. Without wxLogNull the error dialog fires, dismissing it
    // re-activates the frame, and the dialog loops forever.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        // Find OnActivate and check wxLogNull appears before GetTimes inside it.
        const std::string sig = "void MDViewerFrame::OnActivate(";
        auto fnPos = code.find(sig);
        bool hasLogNull = false;
        if (fnPos != std::string::npos) {
            auto fnEnd = code.find("\n}\n", fnPos);
            if (fnEnd != std::string::npos) {
                auto logNullPos = code.find("wxLogNull", fnPos);
                auto getTimesPos = code.find("GetTimes", fnPos);
                hasLogNull = logNullPos != std::string::npos
                          && logNullPos < fnEnd
                          && logNullPos < getTimesPos;
            }
        }
        if (!hasLogNull) {
            std::cerr << "FAIL [activate-no-error-loop]: OnActivate must use wxLogNull "
                         "before GetTimes to suppress the wx error dialog on missing files\n";
            ++failures;
        } else {
            std::cout << "PASS [activate-no-error-loop]\n";
        }
    }

    // TabInsertPosition: restoring a hidden tab inserts at the right notebook index.
    {
        bool ok = true;
        // All visible, restoring tab 2: 2 before → insert at 2
        ok &= (TabInsertPosition({true,true,false,true}, 2) == 2);
        // Two hidden before it: restoring tab 1 → insert at 1
        ok &= (TabInsertPosition({true,false,false,true}, 1) == 1);
        // Restoring first tab: always insert at 0
        ok &= (TabInsertPosition({false,false,false,true}, 0) == 0);
        // Restoring last tab: all others visible → insert at 3
        ok &= (TabInsertPosition({true,true,true,false}, 3) == 3);
        // [F,T,F,T] restoring tab 2: 1 visible before → insert at 1
        ok &= (TabInsertPosition({false,true,false,true}, 2) == 1);
        if (!ok) {
            std::cerr << "FAIL [tab-insert-position]\n";
            ++failures;
        } else {
            std::cout << "PASS [tab-insert-position]\n";
        }
    }

    // ApplyDemoMode hides all tabs except View via RemovePage,
    // and restores them via InsertPage when turned off.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        const std::string sig = "void MDViewerFrame::ApplyDemoMode(";
        auto pos = code.find(sig);
        bool hasImpl = pos != std::string::npos;
        bool hasRemove = false, hasInsert = false, skipsView = false;
        if (hasImpl) {
            auto end = code.find("\n}\n", pos);
            if (end != std::string::npos) {
                std::string body = code.substr(pos, end - pos);
                hasRemove  = body.find("RemovePage") != std::string::npos;
                hasInsert  = body.find("InsertPage") != std::string::npos;
                // Must not remove the View tab (label "View" must be excluded).
                skipsView  = body.find("\"View\"") != std::string::npos ||
                             body.find(".label == \"View\"") != std::string::npos ||
                             body.find("label==\"View\"") != std::string::npos;
            }
        }
        if (!hasImpl || !hasRemove || !hasInsert || !skipsView) {
            std::cerr << "FAIL [demo-mode-apply]: ApplyDemoMode must implement "
                         "RemovePage (hide) and InsertPage (restore), "
                         "keeping the View tab visible\n";
            ++failures;
        } else {
            std::cout << "PASS [demo-mode-apply]\n";
        }
    }

    // OnDemoMode toggles m_demoMode and saves to config.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        const std::string sig = "void MDViewerFrame::OnDemoMode(";
        auto pos = code.find(sig);
        bool hasImpl = pos != std::string::npos;
        bool callsApply = false, savesConfig = false;
        if (hasImpl) {
            auto end = code.find("\n}\n", pos);
            if (end != std::string::npos) {
                std::string body = code.substr(pos, end - pos);
                callsApply  = body.find("ApplyDemoMode") != std::string::npos;
                savesConfig = body.find("demoMode") != std::string::npos &&
                              body.find("Write") != std::string::npos;
            }
        }
        if (!hasImpl || !callsApply || !savesConfig) {
            std::cerr << "FAIL [demo-mode-toggle]: OnDemoMode must call ApplyDemoMode "
                         "and write demoMode to config\n";
            ++failures;
        } else {
            std::cout << "PASS [demo-mode-toggle]\n";
        }
    }

    // Demo mode must hide the notebook entirely (tab bar disappears) by
    // calling m_notebook->Hide() and showing the view page directly in the
    // frame sizer — not just leaving View as the sole tab.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        const std::string sig = "void MDViewerFrame::ApplyDemoMode(";
        auto pos = code.find(sig);
        bool hasHide = false, hasShow = false, hasReparent = false;
        if (pos != std::string::npos) {
            auto end = code.find("\n}\n", pos);
            if (end != std::string::npos) {
                std::string body = code.substr(pos, end - pos);
                hasHide     = body.find("m_notebook->Hide()") != std::string::npos;
                hasShow     = body.find("m_notebook->Show()") != std::string::npos;
                hasReparent = body.find("Reparent") != std::string::npos;
            }
        }
        if (!hasHide || !hasShow || !hasReparent) {
            std::cerr << "FAIL [demo-mode-no-tab-bar]: ApplyDemoMode must hide "
                         "m_notebook and reparent m_viewPage into the frame sizer "
                         "so the tab bar is invisible in demo mode\n";
            ++failures;
        } else {
            std::cout << "PASS [demo-mode-no-tab-bar]\n";
        }
    }

    // ProjectPanel must have a SetDarkMode method.
    {
        std::ifstream src("src/project_panel.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasMethod = code.find("SetDarkMode") != std::string::npos;
        if (!hasMethod) {
            std::cerr << "FAIL [project-panel-dark-mode-method]: project_panel.cpp "
                         "must implement SetDarkMode\n";
            ++failures;
        } else {
            std::cout << "PASS [project-panel-dark-mode-method]\n";
        }
    }

    // ProjectPanel::SetDarkMode must apply colours to widgets.
    {
        std::ifstream src("src/project_panel.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool setsColour = code.find("SetBackgroundColour") != std::string::npos ||
                          code.find("SetForegroundColour") != std::string::npos;
        if (!setsColour) {
            std::cerr << "FAIL [project-panel-dark-mode-apply]: SetDarkMode must "
                         "call SetBackgroundColour or SetForegroundColour on widgets\n";
            ++failures;
        } else {
            std::cout << "PASS [project-panel-dark-mode-apply]\n";
        }
    }

    // mdviewer must call m_projectPage->SetDarkMode when toggling theme.
    {
        std::ifstream src("src/mdviewer.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool wired = code.find("m_projectPage->SetDarkMode") != std::string::npos;
        if (!wired) {
            std::cerr << "FAIL [project-panel-dark-wired]: mdviewer.cpp must call "
                         "m_projectPage->SetDarkMode in both theme-toggle handlers\n";
            ++failures;
        } else {
            std::cout << "PASS [project-panel-dark-wired]\n";
        }
    }

    // CMakeLists.txt must configure the app as a macOS bundle.
    {
        std::ifstream src("CMakeLists.txt");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasBundle = code.find("MACOSX_BUNDLE") != std::string::npos;
        if (!hasBundle) {
            std::cerr << "FAIL [app-icon-cmake-bundle]: CMakeLists.txt must set "
                         "MACOSX_BUNDLE to produce a proper .app bundle with an icon\n";
            ++failures;
        } else {
            std::cout << "PASS [app-icon-cmake-bundle]\n";
        }
    }

    // CMakeLists.txt must reference AppIcon so the .icns is copied into the bundle.
    {
        std::ifstream src("CMakeLists.txt");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool hasIcon = code.find("AppIcon") != std::string::npos;
        if (!hasIcon) {
            std::cerr << "FAIL [app-icon-cmake-icon-file]: CMakeLists.txt must "
                         "reference AppIcon for the bundle icon\n";
            ++failures;
        } else {
            std::cout << "PASS [app-icon-cmake-icon-file]\n";
        }
    }

    // CollapseToOneLine: multi-line free-text values must not be displayed
    // verbatim in single-line wxStaticText labels — they reflow the parent
    // layout and squeeze sibling widgets (project tree, buttons) off-screen.
    {
        bool ok = true;
        // Newlines become single spaces.
        ok &= (CollapseToOneLine("a\nb\nc", 100) == "a b c");
        // Runs of whitespace collapse to one space.
        ok &= (CollapseToOneLine("a   \n\n b", 100) == "a b");
        // Tabs and carriage returns are treated as whitespace.
        ok &= (CollapseToOneLine("x\ty\rz", 100) == "x y z");
        // Length cap appends ellipsis (UTF-8: …).
        std::string clipped = CollapseToOneLine("abcdefghij", 5);
        ok &= (clipped == "abcde\xE2\x80\xA6");
        if (!ok) {
            std::cerr << "FAIL [collapse-one-line]: CollapseToOneLine must strip "
                         "all whitespace runs to single spaces and cap length\n";
            ++failures;
        } else {
            std::cout << "PASS [collapse-one-line]\n";
        }
    }

    // The project panel's stats label embeds the last LLM topic, which can be a
    // multi-line prompt. It must be passed through CollapseToOneLine before
    // being appended to the label, otherwise the wxStaticText grows tall and
    // pushes the project tree and right-side buttons out of view.
    {
        std::ifstream src("src/project_panel.cpp");
        std::string code((std::istreambuf_iterator<char>(src)),
                          std::istreambuf_iterator<char>());
        bool collapsesTopic =
            code.find("CollapseToOneLine(last.topic") != std::string::npos;
        if (!collapsesTopic) {
            std::cerr << "FAIL [stats-topic-one-line]: project_panel must pass "
                         "last.topic through CollapseToOneLine before appending "
                         "to the stats label\n";
            ++failures;
        } else {
            std::cout << "PASS [stats-topic-one-line]\n";
        }
    }

    // tools/gen_icon.swift must exist to generate the emoji icon at build time.
    {
        std::ifstream src("tools/gen_icon.swift");
        bool exists = src.good();
        if (!exists) {
            std::cerr << "FAIL [app-icon-gen-script]: tools/gen_icon.swift must "
                         "exist to render the emoji icon\n";
            ++failures;
        } else {
            std::cout << "PASS [app-icon-gen-script]\n";
        }
    }

    return failures;
}
