#include "retrieval.h"
#include <algorithm>
#include <cmath>
#include <sstream>

float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0f;
    float dot = 0.0f, magA = 0.0f, magB = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot  += a[i] * b[i];
        magA += a[i] * a[i];
        magB += b[i] * b[i];
    }
    if (magA == 0.0f || magB == 0.0f) return 0.0f;
    return dot / (std::sqrt(magA) * std::sqrt(magB));
}

std::vector<ScoredChunk> TopKChunks(const std::vector<CorpusChunk>& chunks,
                                     const std::vector<float>& queryEmb,
                                     int topK) {
    std::vector<ScoredChunk> scored;
    scored.reserve(chunks.size());
    for (const auto& c : chunks)
        scored.push_back({CosineSimilarity(c.embedding, queryEmb), c.text});

    std::partial_sort(scored.begin(),
                      scored.begin() + std::min(topK, (int)scored.size()),
                      scored.end(),
                      [](const ScoredChunk& a, const ScoredChunk& b) {
                          return a.score > b.score;
                      });

    if ((int)scored.size() > topK) scored.resize(topK);
    return scored;
}

std::string RetrieveContext(sqlite3* db,
                             const std::string& query,
                             const std::string& ollamaUrl,
                             int topK,
                             const std::string& model) {
    if (!db) return "";
    auto chunks = CorpusLoadChunks(db);
    if (chunks.empty()) return "";
    auto queryEmb = FetchEmbedding(query, ollamaUrl, model);
    if (queryEmb.empty()) return "";
    auto top = TopKChunks(chunks, queryEmb, topK);
    if (top.empty()) return "";

    std::ostringstream out;
    out << "## Relevant context from your corpus\n\n";
    for (int i = 0; i < (int)top.size(); ++i) {
        out << "> " << top[i].text << "\n\n";
    }
    return out.str();
}
