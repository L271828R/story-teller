#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>

struct CorpusDocument {
    int         id;
    std::string name;
    std::string addedAt;
    int         chunkCount;
};

struct CorpusChunk {
    int                id;
    int                docId;
    std::string        text;
    std::vector<float> embedding;
};

// Pure: split text into overlapping word-based windows.
std::vector<std::string> ChunkText(const std::string& text,
                                    int chunkWords   = 300,
                                    int overlapWords = 50);

// Pure: parse Ollama /api/embeddings JSON response into a float vector.
std::vector<float> ParseEmbeddingResponse(const std::string& json);

// Open (or create) corpus SQLite DB. Applies schema. Returns null on failure.
sqlite3* CorpusOpen(const std::string& dbPath);
void     CorpusClose(sqlite3* db);

// Insert a new document row and return its id (-1 on failure).
int  CorpusAddDoc(sqlite3* db, const std::string& name);

// Insert pre-computed chunks for docId. texts and embeddings must be same length.
bool CorpusAddChunks(sqlite3* db, int docId,
                     const std::vector<std::string>& texts,
                     const std::vector<std::vector<float>>& embeddings);

// Delete document and its chunks (CASCADE).
bool CorpusDeleteDoc(sqlite3* db, int docId);

std::vector<CorpusDocument> CorpusListDocs(sqlite3* db);
std::vector<CorpusChunk>    CorpusLoadChunks(sqlite3* db);

// Network: POST to Ollama /api/embeddings. Returns empty vector on failure.
std::vector<float> FetchEmbedding(const std::string& text,
                                   const std::string& ollamaUrl,
                                   const std::string& model = "nomic-embed-text");

// Shell out to pdftotext. Returns extracted text, or "" if unavailable.
std::string ExtractPdfText(const std::string& pdfPath);
