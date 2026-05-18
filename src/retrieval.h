#pragma once
#include "corpus.h"
#include <string>
#include <vector>

// Cosine similarity in [−1, 1]. Returns 0 if either vector is empty or sizes differ.
float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

struct ScoredChunk { float score; std::string text; };

// Pure: rank chunks by cosine similarity to queryEmb; return top topK.
std::vector<ScoredChunk> TopKChunks(const std::vector<CorpusChunk>& chunks,
                                     const std::vector<float>& queryEmb,
                                     int topK);

// Embeds query via Ollama, loads chunks from db, returns top-K texts as a
// single context block ready to prepend to a generation prompt.
// Returns "" if db is null, corpus is empty, or embedding fails.
std::string RetrieveContext(sqlite3* db,
                             const std::string& query,
                             const std::string& ollamaUrl,
                             int topK = 3,
                             const std::string& model = "nomic-embed-text");
