#include "mdviewer.h"
#include "create_panel.h"
#include "edit_panel.h"
#include "editor.h"
#include "project_panel.h"
#include "markdown.h"
#include "html_template.h"
#include "inspector.h"
#include "chat_panel.h"
#include "creator.h"
#include "config.h"
#include "notes.h"
#include "images.h"
#include "persona.h"
#include "persona_panel.h"
#include <wx/notebook.h>
#include <wx/webview.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/config.h>
#include <wx/tokenzr.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/textdlg.h>
#include <algorithm>
#include <fstream>
#include <unistd.h>
#include <map>
#include <set>
#include <sstream>
#include <vector>

// ---------------------------------------------------------------------------
// Event table
// ---------------------------------------------------------------------------
wxBEGIN_EVENT_TABLE(MDViewerFrame, wxFrame)
    EVT_MENU(wxID_OPEN,      MDViewerFrame::OnOpen)
    EVT_MENU(ID_RELOAD,      MDViewerFrame::OnReload)
    EVT_MENU(wxID_COPY,      MDViewerFrame::OnCopy)
    EVT_MENU(wxID_SELECTALL, MDViewerFrame::OnSelectAll)
    EVT_MENU(wxID_PASTE,     MDViewerFrame::OnPasteView)
    EVT_MENU(wxID_FIND,      MDViewerFrame::OnFindOpen)
    EVT_MENU(ID_FIND_NEXT,   MDViewerFrame::OnFindNext)
    EVT_MENU(ID_FIND_PREV,   MDViewerFrame::OnFindPrev)
    EVT_MENU(ID_FIND_CLOSE,  MDViewerFrame::OnFindClose)
    EVT_MENU(ID_THEME_LIGHT, MDViewerFrame::OnThemeLight)
    EVT_MENU(ID_THEME_DARK,  MDViewerFrame::OnThemeDark)
    EVT_MENU(ID_VIEW_LOGS,   MDViewerFrame::OnViewLogs)
    EVT_MENU(ID_CLEAR_LOGS,  MDViewerFrame::OnClearLogs)
    EVT_MENU(ID_VIEW_DOC,    MDViewerFrame::OnViewDoc)
    EVT_MENU(ID_FONT_INCREASE, MDViewerFrame::OnFontIncrease)
    EVT_MENU(ID_FONT_DECREASE, MDViewerFrame::OnFontDecrease)
    EVT_MENU(ID_FONT_RESET,    MDViewerFrame::OnFontReset)
    EVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED(wxID_ANY, MDViewerFrame::OnScriptMessage)
    EVT_MENU(ID_SAVE_HTML,            MDViewerFrame::OnSaveHTML)
    EVT_MENU(ID_NEW_FROM_CLIPBOARD,   MDViewerFrame::OnNewFromClipboard)
    EVT_MENU(ID_FOCUS_MODE,           MDViewerFrame::OnFocusMode)
    EVT_MENU(ID_MANAGE_PERSONAS,      MDViewerFrame::OnManagePersonas)
    EVT_MENU(ID_MANAGE_IMAGES,        MDViewerFrame::OnManageImages)
    EVT_MENU(ID_HIDE_CHAT_BUBBLES,   MDViewerFrame::OnHideChatBubbles)
    EVT_MENU(ID_DEMO_MODE,           MDViewerFrame::OnDemoMode)
    EVT_MENU(wxID_CLOSE,     MDViewerFrame::OnExit)
    EVT_MENU(wxID_EXIT,      MDViewerFrame::OnExit)
    EVT_CLOSE(               MDViewerFrame::OnClose)
wxEND_EVENT_TABLE()

#include "tab_util.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
static wxString PidTag() {
    return wxString::Format(" [%d]", (int)getpid());
}

MDViewerFrame::MDViewerFrame(const wxString& filePath)
    : wxFrame(nullptr, wxID_ANY,
              filePath.empty() ? wxString("StoryTeller") + PidTag()
                               : wxString("StoryTeller — " + wxFileName(filePath).GetFullName()) + PidTag(),
              wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE)
    , m_darkMode(false)
    , m_fontSizePercent(100)
{
    if (!filePath.empty()) {
        wxFileName fn(filePath);
        fn.MakeAbsolute();
        m_filePath = fn.GetFullPath();
    }

    wxConfig cfg("MDViewer");
    m_darkMode        = cfg.ReadBool("darkMode", false);
    m_fontSizePercent = (int)cfg.ReadLong("fontSizePercent", 100);
    m_showChatBubbles = cfg.ReadBool("showChatBubbles", true);
    m_demoMode        = cfg.ReadBool("demoMode", false);

    // ── File menu ────────────────────────────────────────────────────────
    wxMenuBar* bar  = new wxMenuBar();
    wxMenu*    file = new wxMenu();
    file->Append(wxID_OPEN,             "&Open…\tCtrl+O");
    file->Append(ID_NEW_FROM_CLIPBOARD, "New from &Clipboard…\tCtrl+Shift+N");
    file->Append(ID_MANAGE_PERSONAS,    "Manage &Personas…");
    file->Append(ID_MANAGE_IMAGES,      "Manage &Images…");
    file->Append(ID_RELOAD,             "&Reload\tCtrl+R");
    file->Append(ID_SAVE_HTML,          "Save &HTML…\tCtrl+Shift+S");
    file->AppendSeparator();
    file->Append(wxID_CLOSE, "&Close Window\tCtrl+W");
    file->Append(wxID_EXIT,  "E&xit\tCtrl+Q");
    bar->Append(file, "&File");

    // ── Edit menu ────────────────────────────────────────────────────────
    wxMenu* edit = new wxMenu();
    edit->Append(wxID_COPY,      "&Copy\tCtrl+C");
    edit->Append(wxID_SELECTALL, "Select &All\tCtrl+A");
    edit->AppendSeparator();
    edit->Append(wxID_PASTE,     "&Paste && Render\tCtrl+V");
    edit->AppendSeparator();
    edit->Append(wxID_FIND,      "&Find…\tCtrl+F");
    edit->Append(ID_FIND_NEXT,   "Find &Next\tCtrl+G");
    edit->Append(ID_FIND_PREV,   "Find &Previous\tCtrl+Shift+G");
    bar->Append(edit, "&Edit");

    // ── View menu ────────────────────────────────────────────────────────
    wxMenu* view = new wxMenu();
    view->AppendRadioItem(ID_THEME_LIGHT, "&Light Mode\tCtrl+Shift+L");
    view->AppendRadioItem(ID_THEME_DARK,  "&Dark Mode\tCtrl+Shift+D");
    view->Check(m_darkMode ? ID_THEME_DARK : ID_THEME_LIGHT, true);
    view->AppendSeparator();
    view->Append(ID_VIEW_LOGS,   "View &Logs");
    view->Append(ID_CLEAR_LOGS,  "Clear Logs");
    view->Append(ID_VIEW_DOC,  "View &Document\tCtrl+Shift+V");
    view->AppendSeparator();
    view->Append(ID_FONT_INCREASE, "Increase Font Size\tCtrl++");
    view->Append(ID_FONT_DECREASE, "Decrease Font Size\tCtrl+-");
    view->Append(ID_FONT_RESET,    "Reset Font Size\tCtrl+0");
    view->AppendSeparator();
    view->Append(ID_FOCUS_MODE,    "Focus Mode\tCtrl+Shift+H");
    view->AppendCheckItem(ID_HIDE_CHAT_BUBBLES, "Hide Chat &Bubbles");
    view->Check(ID_HIDE_CHAT_BUBBLES, !m_showChatBubbles);
    view->AppendCheckItem(ID_DEMO_MODE, "&Demo Mode");
    bar->Append(view, "&View");

    SetMenuBar(bar);

    CreateStatusBar();
    SetStatusText("Loading…");

    // ── Notebook ─────────────────────────────────────────────────────────
    m_notebook = new wxNotebook(this, wxID_ANY);

    // ── View page: splitter (webview | chat panel) + floating find bar ────
    m_viewPage = new wxPanel(m_notebook, wxID_ANY);

    m_splitter = new wxSplitterWindow(m_viewPage, wxID_ANY,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxSP_LIVE_UPDATE);
    m_splitter->SetSashGravity(0.0);   // webview grows; chat keeps its width
    m_splitter->SetMinimumPaneSize(180);

    m_webViewPanel = new wxPanel(m_splitter, wxID_ANY);
    m_webView = wxWebView::New(m_webViewPanel, wxID_ANY, "about:blank");
    m_webView->AddScriptMessageHandler("fontSizeChange");
    m_webView->AddScriptMessageHandler("clipboardCopy");
    m_webView->AddScriptMessageHandler("chat");
    m_webView->AddScriptMessageHandler("note");
    m_webView->AddScriptMessageHandler("closeChat");
    EnableWebInspector(m_webView);

    auto* wvSizer = new wxBoxSizer(wxVERTICAL);
    wvSizer->Add(m_webView, 1, wxEXPAND);
    m_webViewPanel->SetSizer(wvSizer);

    m_chatPanel = new ChatPanel(
        m_splitter, m_darkMode,
        [this]() {
            CallAfter([this]() {
                if (m_splitter->IsSplit())
                    m_splitter->Unsplit(m_chatPanel);
            });
        },
        [this]() {
            CallAfter([this]() { LoadAndRender(); });
        }
    );
    m_chatPanel->Hide();
    m_splitter->Initialize(m_webViewPanel);   // start unsplit

    auto* viewSizer = new wxBoxSizer(wxVERTICAL);
    viewSizer->Add(m_splitter, 1, wxEXPAND);
    m_viewPage->SetSizer(viewSizer);

    // Find bar floats over the webview panel (right-aligned).
    m_findPanel = new wxPanel(m_webViewPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxTAB_TRAVERSAL | wxBORDER_SIMPLE);
    auto* fs = new wxBoxSizer(wxHORIZONTAL);
    m_findCtrl = new wxTextCtrl(m_findPanel, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(180, -1), wxTE_PROCESS_ENTER);
    auto* prevBtn  = new wxButton(m_findPanel, ID_FIND_PREV, "‹",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    auto* nextBtn  = new wxButton(m_findPanel, ID_FIND_NEXT, "›",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_findStatus   = new wxStaticText(m_findPanel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxSize(72, -1));
    auto* closeBtn = new wxButton(m_findPanel, ID_FIND_CLOSE, "×",
                                  wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    fs->AddSpacer(6);
    fs->Add(m_findCtrl,   0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(4);
    fs->Add(prevBtn,      0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->Add(nextBtn,      0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(6);
    fs->Add(m_findStatus, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(4);
    fs->Add(closeBtn,     0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 5);
    fs->AddSpacer(6);
    m_findPanel->SetSizer(fs);
    m_findPanel->Hide();

    closeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowFindBar(false); });
    nextBtn->Bind(wxEVT_BUTTON,  [this](wxCommandEvent&) { DoFind(true); });
    prevBtn->Bind(wxEVT_BUTTON,  [this](wxCommandEvent&) { DoFind(false); });
    m_findCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        m_findTerm = wxEmptyString;
        m_webView->Find(wxEmptyString);
        DoFind(true);
    });
    m_findCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { DoFind(true); });
    m_findCtrl->Bind(wxEVT_KEY_DOWN,   [this](wxKeyEvent& evt) {
        if (evt.GetKeyCode() == WXK_ESCAPE) ShowFindBar(false);
        else evt.Skip();
    });

    // ── Projects page ─────────────────────────────────────────────────────
    m_projectPage = new ProjectPanel(m_notebook,
        [this](const std::string& path) { LoadFile(path); });
    m_notebook->AddPage(m_projectPage, "Projects");

    // ── Characters/Personas page ──────────────────────────────────────────
    m_characterTab = new CharacterTab(m_notebook, m_darkMode);
    m_characterTab->SetOnRenameCharacter([this](const std::string& oldName,
                                                const std::string& newName) {
        if (m_filePath.empty()) return;
        std::string path = m_filePath.ToStdString();
        std::ifstream in(path);
        if (!in) return;
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        in.close();
        std::string updated = RenameCharacterInDoc(content, oldName, newName);
        if (updated == content) return;
        std::ofstream out(path, std::ios::trunc);
        if (!out) return;
        out << updated;
        out.close();
        LoadAndRender();
    });
    m_notebook->AddPage(m_characterTab, "Personas");

    // ── Create page ───────────────────────────────────────────────────────
    m_createPage = new CreatePanel(m_notebook,
        [this](const std::string& path) {
            LoadFile(path);
            if (m_editPage) m_editPage->RefreshChapters();
        }, m_characterTab, m_darkMode);
    m_notebook->AddPage(m_createPage, "Create");

    // ── Monitor page ──────────────────────────────────────────────────────
    m_monitorPage = new MonitorPanel(m_notebook, m_darkMode);
    m_notebook->AddPage(m_monitorPage, "Monitor");

    // ── Images page ───────────────────────────────────────────────────────
    m_imageTab = new ImageTab(m_notebook, m_darkMode);
    m_notebook->AddPage(m_imageTab, "Images");

    // ── Prompts page ──────────────────────────────────────────────────────
    m_promptsTab = new PromptsTab(m_notebook, m_darkMode);
    m_promptsTab->SetOnChanged([this]{ if (m_createPage) m_createPage->PushPrompts(); });
    m_notebook->AddPage(m_promptsTab, "Prompts");

    // ── Quiz page ─────────────────────────────────────────────────────────
    m_quizTab = new QuizTab(m_notebook, m_darkMode);
    m_notebook->AddPage(m_quizTab, "Quiz");

    // ── Edit page ─────────────────────────────────────────────────────────
    m_editPage = new EditPanel(m_notebook,
        [this](const std::string& path) { LoadFile(path); }, m_darkMode);
    m_notebook->AddPage(m_editPage, "Edit");

    // ── View page (last) ──────────────────────────────────────────────────
    m_notebook->AddPage(m_viewPage, "View");

    // ── Canonical tab list (order matches AddPage calls above) ────────────
    m_tabs = {
        {0, "Projects", m_projectPage},
        {0, "Personas", m_characterTab},
        {0, "Create",   m_createPage},
        {0, "Monitor",  m_monitorPage},
        {0, "Images",   m_imageTab},
        {0, "Prompts",  m_promptsTab},
        {0, "Quiz",     m_quizTab},
        {0, "Edit",     m_editPage},
        {0, "View",     m_viewPage},
    };
    m_notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& evt) {
        evt.Skip();
        wxWindow* page = m_notebook->GetPage(evt.GetSelection());
        if (m_editPage && page == m_editPage)
            m_editPage->RefreshChapters();
        else if (m_projectPage && page == m_projectPage)
            m_projectPage->RefreshProjects();
        else if (m_monitorPage && page == m_monitorPage)
            m_monitorPage->SetProject(CurrentProjectDir());
        else if (m_characterTab && page == m_characterTab)
            m_characterTab->Activate();
        else if (m_imageTab && page == m_imageTab)
            m_imageTab->Reload();
        else if (m_quizTab && page == m_quizTab)
            m_quizTab->Reload();
        else if (m_promptsTab && page == m_promptsTab)
            m_promptsTab->Reload();
    });

    // ── Frame layout ─────────────────────────────────────────────────────
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_notebook, 1, wxEXPAND);
    SetSizer(sizer);

    if (m_demoMode) {
        ApplyDemoMode(true);
        GetMenuBar()->Check(ID_DEMO_MODE, true);
    }

    // Size to fit the display, with a comfortable margin so the title bar
    // is never pushed off the top of the screen.
    wxSize display = wxGetDisplaySize();
    int w = std::min(1280, display.x - 40);
    int h = std::min(860,  display.y - 80);
    SetSize(w, h);
    Centre();

    m_webViewPanel->Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        evt.Skip();
        CallAfter([this]() { PositionFindBar(); });
    });

    CallAfter([this]() {
        // On startup LoadFile isn't called, so the image and quiz tabs never
        // get their project set. Populate them here for the last-used file.
        if (!m_filePath.empty()) {
            std::string basename = wxFileName(m_filePath).GetFullName().ToStdString();
            std::string projDir  = CurrentProjectDir();
            if (m_imageTab) m_imageTab->SetProject(projDir, basename,
                                                   [this]{ LoadAndRender(); });
            if (m_quizTab)  m_quizTab->SetProject(projDir, basename,
                                                  [this]{ LoadAndRender(); });
        }
        LoadAndRender();
    });

    Bind(wxEVT_ACTIVATE, &MDViewerFrame::OnActivate, this);
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
std::string MDViewerFrame::ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Logger::get().log("ReadFile FAILED: " + path);
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();
    Logger::get().log("ReadFile OK: " + path + "  (" + std::to_string(content.size()) + " bytes)");
    return content;
}

// ---------------------------------------------------------------------------
// Dispatch: .html → load URL directly; .md → render markdown
// ---------------------------------------------------------------------------
void MDViewerFrame::LoadAndRender() {
    if (m_filePath.empty()) {
        std::string body = RenderMarkdown(
            "# StoryTeller\n\n"
            "Open a file with **File → Open** (Ctrl+O) or paste markdown with **Ctrl+V**.\n");
        std::string html = BuildHTML(body, "StoryTeller", m_darkMode, m_fontSizePercent, {}, m_showChatBubbles);
        m_webView->SetPage(wxString::FromUTF8(html), "");
        SetStatusText("Ready");
        return;
    }

    wxString ext = wxFileName(m_filePath).GetExt().Lower();
    Logger::get().log("LoadAndRender: " + m_filePath.ToStdString() + "  ext=" + ext.ToStdString());

    if (ext == "html" || ext == "htm") {
        wxString url = "file://" + m_filePath;
#ifdef __WXMSW__
        url.Replace("/", "\\");
        url = "file:///" + m_filePath;
#endif
        m_webView->LoadURL(url);
        SetStatusText("Loaded HTML: " + m_filePath);
        return;
    }

    std::string raw = ReadFile(m_filePath.ToStdString());

    // Auto-stamp chapter markers so the chat buttons work on legacy documents.
    // Only runs once: if headings exist but no <!-- ch: --> markers are present.
    if (raw.find("<!-- ch:") == std::string::npos &&
        (raw.find("\n## ") != std::string::npos || raw.rfind("## ", 0) == 0)) {
        auto stamped = StampChapters(raw, 0);
        if (stamped.count > 0) {
            std::ofstream f(m_filePath.ToStdString());
            if (f) {
                f << stamped.text;
                raw = stamped.text;
                Logger::get().log("Auto-stamped " + std::to_string(stamped.count)
                                  + " chapters in: " + m_filePath.ToStdString());
            }
        }
    }

    std::string body = RenderMarkdown(raw);

    // Inject note spans into the rendered HTML body (after rendering, so that
    // ProcessInline has already escaped < > — we match the escaped form).
    {
        std::string projDir = CurrentProjectDir();
        if (!projDir.empty()) {
            auto notes = LoadNotes(projDir);
            if (!notes.empty()) {
                std::map<int,std::string> noteTexts;
                std::map<int,std::string> selectedTexts;
                for (const auto& n : notes) {
                    noteTexts[n.id]    = n.text;
                    selectedTexts[n.id] = n.selectedText;
                }
                body = InjectNoteSpans(body, noteTexts, selectedTexts);
            }
        }
    }

    // Substitute local image files with base64 data URLs (WebView sandbox).
    {
        std::string projDir = CurrentProjectDir();
        if (!projDir.empty())
            body = SubstituteLocalImages(body, projDir);
    }

    std::string title = wxFileName(m_filePath).GetFullName().ToStdString();
    std::string html  = BuildHTML(body, title, m_darkMode, m_fontSizePercent,
                                  ToDataURLs(ScanPersonaImages()), m_showChatBubbles);

    wxString baseURL = "file://" + wxFileName(m_filePath).GetPath(wxPATH_GET_SEPARATOR);
    m_webView->SetPage(wxString::FromUTF8(html), baseURL);
    SetStatusText("Rendered: " + m_filePath);

    wxDateTime mt;
    wxFileName(m_filePath).GetTimes(nullptr, &mt, nullptr);
    if (mt.IsValid()) m_fileMtime = mt;
}

// ---------------------------------------------------------------------------
// Menu handlers
// ---------------------------------------------------------------------------
void MDViewerFrame::OnViewLogs(wxCommandEvent&) {
    const std::string logPath = Logger::get().filePath();
    std::string html = BuildLogsHTML(ReadFile(logPath), logPath, m_darkMode);
    m_webView->SetPage(wxString::FromUTF8(html), "");
    // Switch to the View tab so the rendered HTML is actually visible.
    if (m_notebook->GetPageCount() > 0)
        m_notebook->SetSelection(m_notebook->GetPageCount() - 1);  // View is always last
    SetStatusText("Viewing logs — use View > View Document to return");
}

void MDViewerFrame::OnClearLogs(wxCommandEvent&) {
    const std::string logPath = Logger::get().filePath();
    std::ofstream f(logPath, std::ios::trunc);
    SetStatusText("Logs cleared");
}

void MDViewerFrame::LoadFile(const std::string& path) {
    m_filePath = wxString::FromUTF8(path);
    SetTitle("StoryTeller — " + wxFileName(m_filePath).GetFullName() + PidTag());
    if (m_editPage) m_editPage->RefreshChapters();
    if (m_createPage) m_createPage->SyncProject(path);
    if (m_imageTab) {
        std::string basename = wxFileName(m_filePath).GetFullName().ToStdString();
        m_imageTab->SetProject(CurrentProjectDir(), basename,
                               [this]{ LoadAndRender(); });
    }
    if (m_quizTab) {
        std::string basename = wxFileName(m_filePath).GetFullName().ToStdString();
        m_quizTab->SetProject(CurrentProjectDir(), basename,
                              [this]{ LoadAndRender(); });
    }
    wxConfig("MDViewer").Write("lastFile", m_filePath);
    if (m_notebook->GetPageCount() > 0)
        m_notebook->SetSelection(m_notebook->GetPageCount() - 1);  // View is always last
    LoadAndRender();
}

void MDViewerFrame::OnViewDoc(wxCommandEvent&)  { LoadAndRender(); }

void MDViewerFrame::OnThemeLight(wxCommandEvent&) {
    if (m_darkMode) {
        m_darkMode = false;
        wxConfig cfg("MDViewer");
        cfg.Write("darkMode", false);
        if (m_chatPanel)     m_chatPanel->SetDarkMode(false);
        if (m_createPage)    m_createPage->SetDarkMode(false);
        if (m_editPage)      m_editPage->SetDarkMode(false);
        if (m_monitorPage)   m_monitorPage->SetDarkMode(false);
        if (m_characterTab)  m_characterTab->SetDarkMode(false);
        if (m_imageTab)      m_imageTab->SetDarkMode(false);
        if (m_quizTab)       m_quizTab->SetDarkMode(false);
        if (m_promptsTab)    m_promptsTab->SetDarkMode(false);
        LoadAndRender();
    }
}

void MDViewerFrame::OnThemeDark(wxCommandEvent&) {
    if (!m_darkMode) {
        m_darkMode = true;
        wxConfig cfg("MDViewer");
        cfg.Write("darkMode", true);
        if (m_chatPanel)     m_chatPanel->SetDarkMode(true);
        if (m_createPage)    m_createPage->SetDarkMode(true);
        if (m_editPage)      m_editPage->SetDarkMode(true);
        if (m_monitorPage)   m_monitorPage->SetDarkMode(true);
        if (m_characterTab)  m_characterTab->SetDarkMode(true);
        if (m_imageTab)      m_imageTab->SetDarkMode(true);
        if (m_quizTab)       m_quizTab->SetDarkMode(true);
        if (m_promptsTab)    m_promptsTab->SetDarkMode(true);
        LoadAndRender();
    }
}

void MDViewerFrame::OnOpen(wxCommandEvent&) {
    wxFileDialog dlg(this, "Open file", "", "",
                     "Markdown and HTML files (*.md;*.html;*.htm)|*.md;*.html;*.htm"
                     "|Markdown files (*.md)|*.md"
                     "|HTML files (*.html;*.htm)|*.html;*.htm"
                     "|All files (*)|*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_CANCEL) return;
    m_filePath = dlg.GetPath();
    SetTitle("MDViewer — " + wxFileName(m_filePath).GetFullName() + PidTag());
    wxConfig("MDViewer").Write("lastFile", m_filePath);
    LoadAndRender();
}

void MDViewerFrame::OnNewFromClipboard(wxCommandEvent&) {
    if (!wxTheClipboard->Open()) return;
    wxTextDataObject data;
    bool ok = wxTheClipboard->IsSupported(wxDF_TEXT) && wxTheClipboard->GetData(data);
    wxTheClipboard->Close();
    if (!ok || data.GetText().empty()) {
        SetStatusText("Clipboard is empty or contains no text.");
        return;
    }

    wxFileDialog dlg(this, "Save clipboard as…", "", "untitled.md",
                     "Markdown files (*.md)|*.md|All files (*)|*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    std::string path = dlg.GetPath().ToStdString();
    {
        std::ofstream f(path);
        if (!f) {
            wxMessageBox("Could not write: " + dlg.GetPath(), "StoryTeller",
                         wxOK | wxICON_ERROR);
            return;
        }
        f << data.GetText().ToStdString();
    }
    LoadFile(path);
}

void MDViewerFrame::OnManagePersonas(wxCommandEvent&) {
    if (m_characterTab && m_notebook) {
        int idx = m_notebook->FindPage(m_characterTab);
        if (idx != wxNOT_FOUND)
            m_notebook->SetSelection(idx);
    }
}

void MDViewerFrame::ApplyDemoMode(bool demo) {
    if (demo) {
        // Remove implementation tabs from notebook (reverse to keep indices stable).
        for (int i = (int)m_tabs.size() - 1; i >= 0; --i) {
            if (m_tabs[i].label == "View") continue;
            if (!m_tabs[i].visible) continue;
            int idx = m_notebook->FindPage(m_tabs[i].page);
            if (idx != wxNOT_FOUND) m_notebook->RemovePage(idx);
            m_tabs[i].visible = false;
        }
        // Pull View out of the notebook and promote it directly into the frame
        // sizer — the tab bar disappears entirely.
        int viewIdx = m_notebook->FindPage(m_viewPage);
        if (viewIdx != wxNOT_FOUND) m_notebook->RemovePage(viewIdx);
        m_viewPage->Reparent(this);
        GetSizer()->Replace(m_notebook, m_viewPage);
        m_notebook->Hide();
        m_viewPage->Show();
        Layout();
    } else {
        // Return View to the notebook and show the notebook again.
        GetSizer()->Replace(m_viewPage, m_notebook);
        m_viewPage->Reparent(m_notebook);
        m_notebook->AddPage(m_viewPage, "View");
        m_notebook->Show();
        // Restore hidden implementation tabs in canonical order.
        for (int i = 0; i < (int)m_tabs.size(); ++i) {
            if (m_tabs[i].visible) continue;
            std::vector<bool> vis;
            for (auto& t : m_tabs) vis.push_back(t.visible);
            vis[i] = true;
            int insertAt = TabInsertPosition(vis, i);
            m_notebook->InsertPage(insertAt, m_tabs[i].page, m_tabs[i].label, false);
            m_tabs[i].visible = true;
        }
        Layout();
    }
}

void MDViewerFrame::OnDemoMode(wxCommandEvent& evt) {
    m_demoMode = evt.IsChecked();
    ApplyDemoMode(m_demoMode);
    wxConfig cfg("MDViewer");
    cfg.Write("demoMode", m_demoMode);
}

void MDViewerFrame::OnHideChatBubbles(wxCommandEvent& evt) {
    m_showChatBubbles = !evt.IsChecked();
    wxConfig cfg("MDViewer");
    cfg.Write("showChatBubbles", m_showChatBubbles);
    LoadAndRender();
}

void MDViewerFrame::OnManageImages(wxCommandEvent&) {
    if (m_imageTab) {
        int idx = m_notebook->FindPage(m_imageTab);
        if (idx != wxNOT_FOUND)
            m_notebook->SetSelection(idx);
    }
}

void MDViewerFrame::OnReload(wxCommandEvent&) { LoadAndRender(); }

void MDViewerFrame::OnActivate(wxActivateEvent& evt) {
    evt.Skip();
    if (!evt.GetActive() || m_filePath.empty()) return;
    wxDateTime mt;
    { wxLogNull noLog; // suppress wx error dialog if file was deleted
      if (!wxFileName(m_filePath).GetTimes(nullptr, &mt, nullptr) || !mt.IsValid()) return; }
    if (m_fileMtime.IsValid() && mt != m_fileMtime) {
        m_fileMtime = mt;
        LoadAndRender();
    }
}

void MDViewerFrame::OnSaveHTML(wxCommandEvent&) {
    if (m_filePath.empty()) {
        SetStatusText("No file loaded — open a .md file first.");
        return;
    }

    wxString ext = wxFileName(m_filePath).GetExt().Lower();
    std::string html;
    if (ext == "html" || ext == "htm") {
        html = ReadFile(m_filePath.ToStdString());
    } else {
        std::string raw   = ReadFile(m_filePath.ToStdString());
        std::string body  = RenderMarkdown(raw);
        std::string title = wxFileName(m_filePath).GetFullName().ToStdString();
        html = BuildHTML(body, title, m_darkMode, m_fontSizePercent);
    }

    wxString defaultName = wxFileName(m_filePath).GetName() + ".html";
    wxFileDialog dlg(this, "Save HTML", wxFileName(m_filePath).GetPath(),
                     defaultName,
                     "HTML files (*.html)|*.html|All files (*)|*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_CANCEL) return;

    std::ofstream f(dlg.GetPath().ToStdString());
    f << html;
    SetStatusText("Saved HTML: " + dlg.GetPath());
}

void MDViewerFrame::OpenChat(int chId, const std::string& chTitle) {
    if (m_filePath.empty()) return;

    AppState st = LoadAppState();
    LLMConfig cfg;
    cfg.backend     = BackendFromLabel(st.backend);
    cfg.apiKey      = st.apiKey;
    cfg.ollamaModel = st.ollamaModel;

    m_chatPanel->Open(m_filePath.ToStdString(), chId, chTitle, cfg);

    if (!m_splitter->IsSplit()) {
        int w = m_splitter->GetClientSize().GetWidth();
        m_splitter->SplitVertically(m_webViewPanel, m_chatPanel, w * 6 / 10);
    }
}

void MDViewerFrame::OnExit(wxCommandEvent&)   { Close(true); }

void MDViewerFrame::OnClose(wxCloseEvent& evt) {
    Destroy();
    evt.Skip();
}

void MDViewerFrame::OnFontIncrease(wxCommandEvent&) {
    m_fontSizePercent = std::min(200, m_fontSizePercent + 10);
    wxConfig cfg("MDViewer");
    cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    LoadAndRender();
}

void MDViewerFrame::OnFontDecrease(wxCommandEvent&) {
    m_fontSizePercent = std::max(50, m_fontSizePercent - 10);
    wxConfig cfg("MDViewer");
    cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    LoadAndRender();
}

void MDViewerFrame::OnFocusMode(wxCommandEvent&) {
    m_webView->RunScript("toggleFocusMode()");
}

void MDViewerFrame::OnFontReset(wxCommandEvent&) {
    m_fontSizePercent = 100;
    wxConfig cfg("MDViewer");
    cfg.Write("fontSizePercent", 100L);
    LoadAndRender();
}

// ---------------------------------------------------------------------------
// Edit: copy, select-all, paste-to-render
// ---------------------------------------------------------------------------
void MDViewerFrame::OnCopy(wxCommandEvent&)      { m_webView->Copy(); }
void MDViewerFrame::OnSelectAll(wxCommandEvent&) { m_webView->SelectAll(); }

void MDViewerFrame::OnPasteView(wxCommandEvent&) {
    if (!wxTheClipboard->Open()) return;
    wxTextDataObject data;
    bool ok = wxTheClipboard->IsSupported(wxDF_TEXT) && wxTheClipboard->GetData(data);
    wxTheClipboard->Close();
    if (!ok) return;

    std::string md   = data.GetText().ToStdString();
    std::string body = RenderMarkdown(md);
    std::string html = BuildHTML(body, "Clipboard", m_darkMode, m_fontSizePercent);
    m_webView->SetPage(wxString::FromUTF8(html), wxEmptyString);
    wxString status = "Rendering clipboard markdown";
    if (!m_filePath.empty()) status += "  (Ctrl+R to return to file)";
    SetStatusText(status);
}

// ---------------------------------------------------------------------------
// Find bar
// ---------------------------------------------------------------------------
void MDViewerFrame::OnFindOpen(wxCommandEvent&) {
    if (m_findPanel->IsShown()) {
        m_findCtrl->SetFocus();
        m_findCtrl->SelectAll();
    } else {
        ShowFindBar(true);
    }
}

void MDViewerFrame::OnFindNext(wxCommandEvent&)  { DoFind(true); }
void MDViewerFrame::OnFindPrev(wxCommandEvent&)  { DoFind(false); }
void MDViewerFrame::OnFindClose(wxCommandEvent&) { ShowFindBar(false); }

void MDViewerFrame::ShowFindBar(bool show) {
    if (show) {
        m_findPanel->Show();
        PositionFindBar();
        m_findCtrl->SetFocus();
        m_findCtrl->SelectAll();
    } else {
        m_findPanel->Hide();
        m_webView->Find(wxEmptyString);
        m_findStatus->SetLabel(wxEmptyString);
        m_findTotal = 0; m_findCurrent = 0; m_findTerm.clear();
        m_webView->SetFocus();
    }
}

void MDViewerFrame::PositionFindBar() {
    if (!m_findPanel || !m_findPanel->IsShown()) return;
    wxSize panel  = m_findPanel->GetBestSize();
    wxSize client = m_webViewPanel ? m_webViewPanel->GetClientSize() : GetClientSize();
    m_findPanel->SetSize(client.x - panel.x - 12, 8, panel.x, panel.y);
    m_findPanel->Raise();
}

void MDViewerFrame::DoFind(bool forward) {
    wxString term = m_findCtrl->GetValue();
    if (term.empty()) {
        m_webView->Find(wxEmptyString);
        m_findStatus->SetLabel(wxEmptyString);
        m_findTotal = 0; m_findCurrent = 0; m_findTerm.clear();
        return;
    }

    bool newSearch = (term != m_findTerm);
    m_findTerm = term;

    // wxWebView::Find() returns -1 on macOS WKWebView (async under the hood),
    // so count via JS instead — only when the term changes.
    if (newSearch) {
        wxString escaped = term.Lower();
        escaped.Replace("\\", "\\\\");
        escaped.Replace("\"", "\\\"");
        wxString js = wxString::Format(
            "(document.body.innerText.toLowerCase().split(\"%s\").length - 1)",
            escaped);
        wxString result;
        long n = 0;
        m_findTotal = (m_webView->RunScript(js, &result) && result.ToLong(&n))
                      ? (int)std::max(0L, n) : 0;
    }

    int flags = wxWEBVIEW_FIND_HIGHLIGHT_RESULT | wxWEBVIEW_FIND_WRAP;
    if (!forward) flags |= wxWEBVIEW_FIND_BACKWARDS;
    m_webView->Find(term, static_cast<wxWebViewFindFlags>(flags));

    if (m_findTotal == 0) {
        m_findCurrent = 0;
        m_findStatus->SetLabel("No results");
    } else {
        if (newSearch) {
            m_findCurrent = 1;
        } else if (forward) {
            m_findCurrent = m_findCurrent % m_findTotal + 1;
        } else {
            m_findCurrent = (m_findCurrent - 2 + m_findTotal) % m_findTotal + 1;
        }
        m_findStatus->SetLabel(wxString::Format("%d of %d", m_findCurrent, m_findTotal));
    }
}

// ---------------------------------------------------------------------------
// Simple JSON field extractor: finds "key":"value" or "key":number in json.
// For string values, returns content between the first pair of quotes after ':'.
// For number values (when expectString=false), returns digits.
static std::string ExtractJsonField(const std::string& json,
                                    const std::string& key,
                                    bool expectString = true) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size()) return "";
    if (expectString) {
        if (json[pos] != '"') return "";
        ++pos;
        std::string val;
        while (pos < json.size()) {
            char c = json[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < json.size()) {
                char esc = json[pos++];
                switch (esc) {
                    case '"':  val += '"';  break;
                    case '\\': val += '\\'; break;
                    case 'n':  val += '\n'; break;
                    case 'r':  val += '\r'; break;
                    case 't':  val += '\t'; break;
                    default:   val += esc;  break;
                }
            } else {
                val += c;
            }
        }
        return val;
    } else {
        size_t numStart = pos;
        while (pos < json.size() && (std::isdigit((unsigned char)json[pos]) || json[pos] == '-'))
            ++pos;
        return json.substr(numStart, pos - numStart);
    }
}

void MDViewerFrame::OnScriptMessage(wxWebViewEvent& evt) {
    if (evt.GetMessageHandler() == "chat") {
        wxString payload = evt.GetString();
        int sep = payload.Find('|');
        if (sep != wxNOT_FOUND) {
            long chId = 0;
            payload.Left(sep).ToLong(&chId);
            std::string chTitle = payload.Mid(sep + 1).ToStdString();
            if (chTitle.empty()) chTitle = "Document";
            OpenChat((int)chId, chTitle);
        }
        return;
    }
    if (evt.GetMessageHandler() == "closeChat") {
        if (m_splitter && m_splitter->IsSplit())
            m_splitter->Unsplit(m_chatPanel);
        return;
    }
    if (evt.GetMessageHandler() == "clipboardCopy") {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(evt.GetString()));
            wxTheClipboard->Close();
        }
        return;
    }
    if (evt.GetMessageHandler() == "note") {
        std::string payload = evt.GetString().ToStdString();
        std::string action  = ExtractJsonField(payload, "action");
        if (action == "add") {
            std::string selText = ExtractJsonField(payload, "selectedText");
            std::string context = ExtractJsonField(payload, "context");
            OnNoteAdd(selText, context);
        } else if (action == "edit") {
            std::string idStr = ExtractJsonField(payload, "id", false);
            if (!idStr.empty()) {
                try { OnNoteEdit(std::stoi(idStr)); } catch (...) {}
            }
        } else if (action == "delete") {
            std::string idStr = ExtractJsonField(payload, "id", false);
            if (!idStr.empty()) {
                try { OnNoteDelete(std::stoi(idStr)); } catch (...) {}
            }
        }
        return;
    }
    long val;
    if (evt.GetString().ToLong(&val)) {
        m_fontSizePercent = (int)std::max(50L, std::min(200L, val));
        wxConfig cfg("MDViewer");
        cfg.Write("fontSizePercent", (long)m_fontSizePercent);
    }
}

// ---------------------------------------------------------------------------
// Note helpers
// ---------------------------------------------------------------------------
std::string MDViewerFrame::CurrentProjectDir() const {
    if (m_filePath.empty()) return "";
    return wxFileName(m_filePath).GetPath().ToStdString();
}

void MDViewerFrame::OnNoteAdd(const std::string& selectedText,
                              const std::string& context) {
    if (m_filePath.empty() || selectedText.empty()) return;
    std::string projDir = CurrentProjectDir();
    auto notes = LoadNotes(projDir);
    wxTextEntryDialog dlg(this, "Enter your note:", "Add Note");
    if (dlg.ShowModal() != wxID_OK) return;
    std::string noteText = dlg.GetValue().ToStdString();
    if (noteText.empty()) return;

    int id = NextNoteId(notes);
    Note n;
    n.id           = id;
    n.anchor       = "note:" + std::to_string(id);
    n.selectedText = selectedText;
    n.text         = noteText;
    n.file         = wxFileName(m_filePath).GetFullName().ToStdString();

    // Read, patch, write the .md file.
    std::string content;
    {
        std::ifstream f(m_filePath.ToStdString());
        content.assign(std::istreambuf_iterator<char>(f), {});
    }
    content = InsertNoteAnchor(content, selectedText, context, id);
    {
        std::ofstream f(m_filePath.ToStdString(), std::ios::trunc);
        f << content;
    }
    notes.push_back(n);
    SaveNotes(projDir, notes);
    LoadAndRender();
}

void MDViewerFrame::OnNoteEdit(int noteId) {
    if (m_filePath.empty()) return;
    std::string projDir = CurrentProjectDir();
    auto notes = LoadNotes(projDir);
    auto it = std::find_if(notes.begin(), notes.end(),
                           [noteId](const Note& n){ return n.id == noteId; });
    if (it == notes.end()) return;
    wxTextEntryDialog dlg(this, "Edit your note:", "Edit Note",
                          wxString::FromUTF8(it->text));
    if (dlg.ShowModal() != wxID_OK) return;
    it->text = dlg.GetValue().ToStdString();
    SaveNotes(projDir, notes);
    LoadAndRender();
}

void MDViewerFrame::OnNoteDelete(int noteId) {
    if (m_filePath.empty()) return;
    std::string projDir = CurrentProjectDir();
    auto notes = LoadNotes(projDir);
    // Remove anchor from file.
    std::string content;
    {
        std::ifstream f(m_filePath.ToStdString());
        content.assign(std::istreambuf_iterator<char>(f), {});
    }
    content = RemoveNoteAnchor(content, noteId);
    {
        std::ofstream f(m_filePath.ToStdString(), std::ios::trunc);
        f << content;
    }
    notes.erase(std::remove_if(notes.begin(), notes.end(),
                               [noteId](const Note& n){ return n.id == noteId; }),
                notes.end());
    SaveNotes(projDir, notes);
    LoadAndRender();
}
