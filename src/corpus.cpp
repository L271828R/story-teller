#include "corpus.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

std::vector<std::string> ChunkText(const std::string& text,
                                    int chunkWords,
                                    int overlapWords) {
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string w;
    while (iss >> w) words.push_back(w);
    if (words.empty()) return {};

    int step = chunkWords - overlapWords;
    if (step <= 0) step = chunkWords;

    std::vector<std::string> chunks;
    for (int i = 0; i < (int)words.size(); i += step) {
        int end = std::min((int)words.size(), i + chunkWords);
        std::string chunk;
        for (int j = i; j < end; ++j) {
            if (j > i) chunk += ' ';
            chunk += words[j];
        }
        chunks.push_back(chunk);
        if (end == (int)words.size()) break;
    }
    return chunks;
}

std::vector<float> ParseEmbeddingResponse(const std::string& json) {
    auto pos = json.find("\"embedding\"");
    if (pos == std::string::npos) return {};
    auto bracket = json.find('[', pos);
    if (bracket == std::string::npos) return {};
    auto close = json.find(']', bracket);
    if (close == std::string::npos) return {};

    std::string arr = json.substr(bracket + 1, close - bracket - 1);
    std::vector<float> result;
    std::istringstream ss(arr);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try { result.push_back(std::stof(token)); }
        catch (...) {}
    }
    return result;
}

// ---------------------------------------------------------------------------
// Blob serialization
// ---------------------------------------------------------------------------

static std::vector<uint8_t> FloatsToBlob(const std::vector<float>& v) {
    std::vector<uint8_t> blob(v.size() * sizeof(float));
    std::memcpy(blob.data(), v.data(), blob.size());
    return blob;
}

static std::vector<float> BlobToFloats(const void* data, int bytes) {
    std::vector<float> v(bytes / sizeof(float));
    std::memcpy(v.data(), data, bytes);
    return v;
}

// ---------------------------------------------------------------------------
// SQLite DB lifecycle
// ---------------------------------------------------------------------------

static const char* kSchema = R"SQL(
PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS documents (
    id       INTEGER PRIMARY KEY AUTOINCREMENT,
    name     TEXT    NOT NULL,
    added_at TEXT    NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS chunks (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    doc_id      INTEGER NOT NULL REFERENCES documents(id) ON DELETE CASCADE,
    chunk_index INTEGER NOT NULL,
    text        TEXT    NOT NULL,
    embedding   BLOB    NOT NULL
);
)SQL";

sqlite3* CorpusOpen(const std::string& dbPath) {
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        return nullptr;
    }
    char* err = nullptr;
    if (sqlite3_exec(db, kSchema, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        sqlite3_close(db);
        return nullptr;
    }
    // Enable WAL for better concurrent read performance.
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    return db;
}

void CorpusClose(sqlite3* db) {
    if (db) sqlite3_close(db);
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

int CorpusAddDoc(sqlite3* db, const std::string& name) {
    if (!db) return -1;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO documents (name) VALUES (?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    return (int)sqlite3_last_insert_rowid(db);
}

bool CorpusAddChunks(sqlite3* db, int docId,
                     const std::vector<std::string>& texts,
                     const std::vector<std::vector<float>>& embeddings) {
    if (!db || texts.size() != embeddings.size()) return false;
    const char* sql =
        "INSERT INTO chunks (doc_id, chunk_index, text, embedding) VALUES (?,?,?,?);";
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    for (int i = 0; i < (int)texts.size(); ++i) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        auto blob = FloatsToBlob(embeddings[i]);
        sqlite3_bind_int(stmt, 1, docId);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_text(stmt, 3, texts[i].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 4, blob.data(), (int)blob.size(), SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool CorpusDeleteDoc(sqlite3* db, int docId) {
    if (!db) return false;
    // Enable FK for this connection so CASCADE fires.
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM documents WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, docId);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<CorpusDocument> CorpusListDocs(sqlite3* db) {
    std::vector<CorpusDocument> result;
    if (!db) return result;
    const char* sql =
        "SELECT d.id, d.name, d.added_at, COUNT(c.id) "
        "FROM documents d LEFT JOIN chunks c ON c.doc_id = d.id "
        "GROUP BY d.id ORDER BY d.added_at DESC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CorpusDocument doc;
        doc.id         = sqlite3_column_int(stmt, 0);
        doc.name       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        doc.addedAt    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        doc.chunkCount = sqlite3_column_int(stmt, 3);
        result.push_back(doc);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<CorpusChunk> CorpusLoadChunks(sqlite3* db) {
    std::vector<CorpusChunk> result;
    if (!db) return result;
    const char* sql =
        "SELECT id, doc_id, text, embedding FROM chunks ORDER BY doc_id, chunk_index;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CorpusChunk chunk;
        chunk.id    = sqlite3_column_int(stmt, 0);
        chunk.docId = sqlite3_column_int(stmt, 1);
        chunk.text  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const void* blob  = sqlite3_column_blob(stmt, 3);
        int         bytes = sqlite3_column_bytes(stmt, 3);
        chunk.embedding   = BlobToFloats(blob, bytes);
        result.push_back(chunk);
    }
    sqlite3_finalize(stmt);
    return result;
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

std::vector<float> FetchEmbedding(const std::string& text,
                                   const std::string& ollamaUrl,
                                   const std::string& model) {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path tmp = fs::temp_directory_path()
                   / ("corpus_emb_" + std::to_string(ns) + ".json");
    {
        std::ofstream f(tmp);
        // Simple JSON: escape only backslash and double-quote.
        auto esc = [](const std::string& s) {
            std::string out;
            for (char c : s) {
                if (c == '"')  { out += "\\\""; }
                else if (c == '\\') { out += "\\\\"; }
                else if (c == '\n') { out += "\\n"; }
                else           { out += c; }
            }
            return out;
        };
        f << "{\"model\":\"" << esc(model) << "\","
          << "\"prompt\":\"" << esc(text) << "\"}";
    }
    std::string cmd = "curl -s -X POST \"" + ollamaUrl + "/api/embeddings\""
                      " -H 'Content-Type: application/json'"
                      " --data-binary @\"" + tmp.string() + "\" 2>/dev/null";
    std::string raw;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), pipe)) raw += buf;
        pclose(pipe);
    }
    fs::remove(tmp);
    return ParseEmbeddingResponse(raw);
}

// ---------------------------------------------------------------------------
// PDF extraction
// ---------------------------------------------------------------------------

std::string ExtractPdfText(const std::string& pdfPath) {
    std::string cmd = "pdftotext " + std::string("\"") + pdfPath + "\" - 2>/dev/null";
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    return out;
}
