#include "corpus.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int test_corpus() {
    int failures = 0;

    // ChunkText splits a long text into overlapping windows.
    {
        // 10 words, chunkWords=4, overlapWords=1 → step=3
        // chunk 0: words 0-3, chunk 1: words 3-6, chunk 2: words 6-9
        std::string text = "one two three four five six seven eight nine ten";
        auto chunks = ChunkText(text, 4, 1);
        bool sizeOk  = chunks.size() == 3;
        bool first   = !chunks.empty() && chunks[0] == "one two three four";
        bool overlap = chunks.size() >= 2 && chunks[1].rfind("four", 0) == 0;
        if (!sizeOk || !first || !overlap) {
            std::cerr << "FAIL [chunk-text-basic]: size=" << chunks.size()
                      << " first='" << (chunks.empty() ? "" : chunks[0])
                      << "' second='" << (chunks.size() < 2 ? "" : chunks[1]) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [chunk-text-basic]\n";
        }
    }

    // Short text (fewer words than chunkWords) returns a single chunk.
    {
        auto chunks = ChunkText("hello world", 10, 2);
        if (chunks.size() != 1 || chunks[0] != "hello world") {
            std::cerr << "FAIL [chunk-text-short]: size=" << chunks.size()
                      << " chunk='" << (chunks.empty() ? "" : chunks[0]) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [chunk-text-short]\n";
        }
    }

    // Empty text returns no chunks.
    {
        auto chunks = ChunkText("", 10, 2);
        if (!chunks.empty()) {
            std::cerr << "FAIL [chunk-text-empty]: got " << chunks.size() << " chunks\n";
            ++failures;
        } else {
            std::cout << "PASS [chunk-text-empty]\n";
        }
    }

    // ParseEmbeddingResponse extracts floats from Ollama JSON.
    {
        std::string json = R"({"embedding":[0.5,-0.25,1.0]})";
        auto emb = ParseEmbeddingResponse(json);
        bool sizeOk = emb.size() == 3;
        bool valOk  = sizeOk && emb[0] == 0.5f && emb[1] == -0.25f && emb[2] == 1.0f;
        if (!sizeOk || !valOk) {
            std::cerr << "FAIL [parse-embedding-response]: size=" << emb.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-embedding-response]\n";
        }
    }

    // ParseEmbeddingResponse returns empty on bad input.
    {
        auto emb = ParseEmbeddingResponse("{}");
        if (!emb.empty()) {
            std::cerr << "FAIL [parse-embedding-empty]: got " << emb.size() << " values\n";
            ++failures;
        } else {
            std::cout << "PASS [parse-embedding-empty]\n";
        }
    }

    // CorpusOpen with ":memory:" succeeds and CorpusClose does not crash.
    {
        sqlite3* db = CorpusOpen(":memory:");
        if (!db) {
            std::cerr << "FAIL [corpus-open-memory]: got null handle\n";
            ++failures;
        } else {
            std::cout << "PASS [corpus-open-memory]\n";
            CorpusClose(db);
        }
    }

    // CorpusAddDoc inserts a row; CorpusListDocs returns it.
    {
        sqlite3* db = CorpusOpen(":memory:");
        int id = CorpusAddDoc(db, "My Novel Research");
        auto docs = CorpusListDocs(db);
        bool foundId   = (id > 0);
        bool foundName = !docs.empty() && docs[0].name == "My Novel Research";
        if (!foundId || !foundName) {
            std::cerr << "FAIL [corpus-add-list-doc]: id=" << id
                      << " docs=" << docs.size()
                      << " name='" << (docs.empty() ? "" : docs[0].name) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [corpus-add-list-doc]\n";
        }
        CorpusClose(db);
    }

    // CorpusAddChunks stores chunks; CorpusLoadChunks retrieves them with embeddings.
    {
        sqlite3* db  = CorpusOpen(":memory:");
        int docId    = CorpusAddDoc(db, "test-doc");
        std::vector<std::string>        texts = {"chunk one", "chunk two"};
        std::vector<std::vector<float>> embs  = {{1.0f, 0.0f}, {0.0f, 1.0f}};
        bool added   = CorpusAddChunks(db, docId, texts, embs);
        auto chunks  = CorpusLoadChunks(db);
        bool sizeOk  = chunks.size() == 2;
        bool textOk  = sizeOk && chunks[0].text == "chunk one";
        bool embOk   = sizeOk && chunks[0].embedding.size() == 2
                               && chunks[0].embedding[0] == 1.0f;
        if (!added || !sizeOk || !textOk || !embOk) {
            std::cerr << "FAIL [corpus-add-chunks]: added=" << added
                      << " size=" << chunks.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [corpus-add-chunks]\n";
        }
        CorpusClose(db);
    }

    // CorpusDeleteDoc removes the document and its chunks (CASCADE).
    {
        sqlite3* db = CorpusOpen(":memory:");
        int id = CorpusAddDoc(db, "to-delete");
        CorpusAddChunks(db, id, {"text"}, {{0.1f}});
        bool deleted = CorpusDeleteDoc(db, id);
        auto docs    = CorpusListDocs(db);
        auto chunks  = CorpusLoadChunks(db);
        if (!deleted || !docs.empty() || !chunks.empty()) {
            std::cerr << "FAIL [corpus-delete-cascades]: deleted=" << deleted
                      << " docs=" << docs.size() << " chunks=" << chunks.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [corpus-delete-cascades]\n";
        }
        CorpusClose(db);
    }

    // CorpusListDocs reports chunkCount per document.
    {
        sqlite3* db = CorpusOpen(":memory:");
        int id = CorpusAddDoc(db, "counted");
        CorpusAddChunks(db, id, {"a", "b", "c"}, {{1.f}, {1.f}, {1.f}});
        auto docs = CorpusListDocs(db);
        bool ok = !docs.empty() && docs[0].chunkCount == 3;
        if (!ok) {
            std::cerr << "FAIL [corpus-chunk-count]: count="
                      << (docs.empty() ? -1 : docs[0].chunkCount) << "\n";
            ++failures;
        } else {
            std::cout << "PASS [corpus-chunk-count]\n";
        }
        CorpusClose(db);
    }

    return failures;
}
