#include <wx/wx.h>
#include <wx/config.h>
#include "mdviewer.h"
#include "markdown.h"
#include "logger.h"
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>
#include <cstring>

extern char **environ;

static void crash_handler(int sig) {
    void*  frames[64];
    int    n    = backtrace(frames, 64);
    char** syms = backtrace_symbols(frames, n);
    std::string trace = "=== CRASH  signal=" + std::to_string(sig) + " ===\n";
    for (int i = 0; i < n; ++i)
        if (syms) trace += std::string(syms[i]) + "\n";
    free(syms);
    Logger::get().log(trace);
    signal(sig, SIG_DFL);
    raise(sig);
}

class StoryTellerApp : public wxApp {
public:
    bool OnInit() override;
    bool OnExceptionInMainLoop() override;
};

wxIMPLEMENT_APP_NO_MAIN(StoryTellerApp);

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::strcmp(argv[1], "--llm") == 0) {
        std::fputs(GetLLMReadme().c_str(), stdout);
        return 0;
    }

    // When launched from a terminal, re-exec self as a detached process.
    // posix_spawn (not fork) gives the child fresh Mach/XPC state so
    // WKWebView's rendering pipeline initialises correctly.
    if (isatty(STDIN_FILENO)) {
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        posix_spawn_file_actions_addopen(&fa, STDIN_FILENO,  "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        pid_t child;
        posix_spawnp(&child, argv[0], &fa, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&fa);
        return 0;
    }

    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGBUS,  crash_handler);

    char cwdbuf[4096] = {};
    std::string cwd = getcwd(cwdbuf, sizeof(cwdbuf)) ? cwdbuf : "?";
    Logger::get().log("=== startup  pid=" + std::to_string(getpid())
                      + "  arg=" + (argc >= 2 ? argv[1] : "(none)")
                      + "  cwd=" + cwd);

    return wxEntry(argc, argv);
}

bool StoryTellerApp::OnExceptionInMainLoop() {
    try { throw; }
    catch (const std::exception& e) {
        Logger::get().log(std::string("=== EXCEPTION: ") + e.what() + " ===");
    }
    catch (...) {
        Logger::get().log("=== UNKNOWN EXCEPTION in main loop ===");
    }
    return false;
}

bool StoryTellerApp::OnInit() {
    wxString path;

    if (argc >= 2) {
        path = argv[1];
        if (!wxFileExists(path)) {
            wxMessageBox("File not found: " + path, "StoryTeller", wxOK | wxICON_ERROR);
            return false;
        }
    } else {
        wxConfig cfg("MDViewer");
        wxString last;
        if (cfg.Read("lastFile", &last) && wxFileExists(last))
            path = last;
    }

    MDViewerFrame* frame = new MDViewerFrame(path);
    frame->Show(true);
    return true;
}
