#include "markdown.h"
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

    return failures;
}
