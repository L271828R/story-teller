#include "corpus_panel.h"
#include "corpus.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/wx.h>

static const char* kOllamaUrl = "http://localhost:11434";

namespace fs = std::filesystem;

enum {
    ID_CP_ADD    = wxID_HIGHEST + 600,
    ID_CP_DELETE,
};

wxBEGIN_EVENT_TABLE(CorpusPanel, wxPanel)
    EVT_BUTTON(ID_CP_ADD,    CorpusPanel::OnAddFile)
    EVT_BUTTON(ID_CP_DELETE, CorpusPanel::OnDeleteDoc)
wxEND_EVENT_TABLE()

CorpusPanel::CorpusPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    auto* top = new wxBoxSizer(wxVERTICAL);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto* bar = new wxBoxSizer(wxHORIZONTAL);
    m_addBtn    = new wxButton(this, ID_CP_ADD,    "Add PDF / Text…");
    m_deleteBtn = new wxButton(this, ID_CP_DELETE, "Delete");
    m_deleteBtn->Enable(false);
    bar->Add(m_addBtn,    0, wxRIGHT, 6);
    bar->Add(m_deleteBtn, 0);
    top->Add(bar, 0, wxALL, 8);

    // ── Document list ─────────────────────────────────────────────────────────
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SIMPLE);
    m_list->InsertColumn(0, "Document",   wxLIST_FORMAT_LEFT, 280);
    m_list->InsertColumn(1, "Chunks",     wxLIST_FORMAT_RIGHT, 70);
    m_list->InsertColumn(2, "Added",      wxLIST_FORMAT_LEFT, 160);
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED,
                 [this](wxListEvent&) { m_deleteBtn->Enable(true); });
    m_list->Bind(wxEVT_LIST_ITEM_DESELECTED,
                 [this](wxListEvent&) { m_deleteBtn->Enable(false); });
    top->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // ── Status ────────────────────────────────────────────────────────────────
    m_statusCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxSize(-1, 60),
                                   wxTE_MULTILINE | wxTE_READONLY | wxBORDER_SIMPLE);
    top->Add(m_statusCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    SetSizer(top);
}

CorpusPanel::~CorpusPanel() {
    CorpusClose(m_db);
}

void CorpusPanel::SetProjectDir(const std::string& projectDir) {
    if (projectDir == m_projectDir) return;
    CorpusClose(m_db);
    m_db = nullptr;
    m_projectDir = projectDir;
    if (projectDir.empty()) { RefreshList(); return; }

    std::string dbPath = (fs::path(projectDir) / "corpus.db").string();
    m_db = CorpusOpen(dbPath);
    if (!m_db)
        SetStatus("Could not open corpus database at: " + dbPath);
    RefreshList();
}

void CorpusPanel::RefreshList() {
    m_list->DeleteAllItems();
    m_deleteBtn->Enable(false);
    if (!m_db) return;
    auto docs = CorpusListDocs(m_db);
    for (int i = 0; i < (int)docs.size(); ++i) {
        long idx = m_list->InsertItem(i, wxString::FromUTF8(docs[i].name));
        m_list->SetItem(idx, 1, wxString::Format("%d", docs[i].chunkCount));
        m_list->SetItem(idx, 2, wxString::FromUTF8(docs[i].addedAt));
        m_list->SetItemData(idx, docs[i].id);
    }
}

void CorpusPanel::SetStatus(const wxString& msg) {
    m_statusCtrl->SetValue(msg);
}

// ---------------------------------------------------------------------------
// Add a PDF or text file
// ---------------------------------------------------------------------------

void CorpusPanel::OnAddFile(wxCommandEvent&) {
    if (!m_db) {
        wxMessageBox("Select a project first — the corpus is stored per project.",
                     "No project", wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxFileDialog dlg(this, "Add document to corpus", wxEmptyString, wxEmptyString,
                     "Documents (*.pdf;*.txt;*.md)|*.pdf;*.txt;*.md|All files|*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    std::string path = dlg.GetPath().ToStdString();
    std::string name = dlg.GetFilename().ToStdString();
    std::string url  = kOllamaUrl;

    SetStatus("Reading " + name + "…");
    m_addBtn->Enable(false);

    std::thread([this, path, name, url]() {
        // Extract text.
        std::string text;
        std::string lpath = path;
        if (lpath.size() >= 4 &&
            lpath.substr(lpath.size() - 4) == ".pdf") {
            text = ExtractPdfText(path);
            if (text.empty()) {
                wxTheApp->CallAfter([this, name]() {
                    SetStatus("Could not extract text from " + name +
                              ".\nEnsure pdftotext is installed (brew install poppler).");
                    m_addBtn->Enable(true);
                });
                return;
            }
        } else {
            std::ifstream f(path);
            if (!f) {
                wxTheApp->CallAfter([this, name]() {
                    SetStatus("Could not read " + name + ".");
                    m_addBtn->Enable(true);
                });
                return;
            }
            text.assign(std::istreambuf_iterator<char>(f), {});
        }

        // Chunk.
        auto chunks = ChunkText(text);
        wxTheApp->CallAfter([this, name, n = (int)chunks.size()]() {
            SetStatus("Embedding " + name + " (" + std::to_string(n) + " chunks)…");
        });

        // Embed each chunk.
        std::vector<std::vector<float>> embeddings;
        embeddings.reserve(chunks.size());
        for (int i = 0; i < (int)chunks.size(); ++i) {
            auto emb = FetchEmbedding(chunks[i], url);
            if (emb.empty()) {
                wxTheApp->CallAfter([this, i]() {
                    SetStatus("Embedding failed at chunk " + std::to_string(i) +
                              ".\nIs Ollama running with nomic-embed-text?");
                    m_addBtn->Enable(true);
                });
                return;
            }
            embeddings.push_back(emb);
            int done = i + 1;
            int total = (int)chunks.size();
            wxTheApp->CallAfter([this, done, total]() {
                SetStatus("Embedding… " + std::to_string(done) + "/" +
                          std::to_string(total));
            });
        }

        // Save to DB.
        wxTheApp->CallAfter([this, name, chunks, embeddings]() {
            int docId = CorpusAddDoc(m_db, name);
            if (docId < 0 || !CorpusAddChunks(m_db, docId, chunks, embeddings)) {
                SetStatus("Failed to save document to corpus database.");
            } else {
                SetStatus("Added "" + name + "" — " +
                          std::to_string(chunks.size()) + " chunks embedded.");
                RefreshList();
            }
            m_addBtn->Enable(true);
        });
    }).detach();
}

// ---------------------------------------------------------------------------
// Delete selected document
// ---------------------------------------------------------------------------

void CorpusPanel::OnDeleteDoc(wxCommandEvent&) {
    long idx = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (idx == wxNOT_FOUND || !m_db) return;

    wxString name = m_list->GetItemText(idx, 0);
    if (wxMessageBox("Delete \"" + name + "\" from the corpus?\nThis cannot be undone.",
                     "Confirm", wxYES_NO | wxICON_WARNING, this) != wxYES) return;

    int docId = (int)m_list->GetItemData(idx);
    if (CorpusDeleteDoc(m_db, docId)) {
        SetStatus("Deleted "" + name + "".");
        RefreshList();
    } else {
        SetStatus("Could not delete document.");
    }
}
