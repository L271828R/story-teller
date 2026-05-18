#include "retrieval.h"
#include <cmath>
#include <iostream>

int test_retrieval() {
    int failures = 0;

    // A vector dotted with itself has cosine similarity 1.0.
    {
        std::vector<float> v = {1.0f, 2.0f, 3.0f};
        float s = CosineSimilarity(v, v);
        if (std::abs(s - 1.0f) > 1e-5f) {
            std::cerr << "FAIL [cosine-same]: got " << s << "\n";
            ++failures;
        } else {
            std::cout << "PASS [cosine-same]\n";
        }
    }

    // Orthogonal vectors have cosine similarity 0.
    {
        float s = CosineSimilarity({1.0f, 0.0f}, {0.0f, 1.0f});
        if (std::abs(s) > 1e-5f) {
            std::cerr << "FAIL [cosine-orthogonal]: got " << s << "\n";
            ++failures;
        } else {
            std::cout << "PASS [cosine-orthogonal]\n";
        }
    }

    // Opposite vectors have cosine similarity −1.
    {
        float s = CosineSimilarity({1.0f, 0.0f}, {-1.0f, 0.0f});
        if (std::abs(s - (-1.0f)) > 1e-5f) {
            std::cerr << "FAIL [cosine-opposite]: got " << s << "\n";
            ++failures;
        } else {
            std::cout << "PASS [cosine-opposite]\n";
        }
    }

    // Empty vectors return 0 (guard against divide-by-zero).
    {
        float s = CosineSimilarity({}, {});
        if (s != 0.0f) {
            std::cerr << "FAIL [cosine-empty]: got " << s << "\n";
            ++failures;
        } else {
            std::cout << "PASS [cosine-empty]\n";
        }
    }

    // TopKChunks returns the highest-scoring chunks in descending order.
    {
        std::vector<CorpusChunk> chunks = {
            {1, 1, "low",  {0.0f, 1.0f}},
            {2, 1, "high", {1.0f, 0.0f}},
            {3, 1, "mid",  {0.7f, 0.7f}},
        };
        std::vector<float> query = {1.0f, 0.0f};  // aligned with "high"
        auto top = TopKChunks(chunks, query, 2);
        bool sizeOk  = top.size() == 2;
        bool firstOk = sizeOk && top[0].text == "high";
        bool ordered = sizeOk && top[0].score >= top[1].score;
        if (!sizeOk || !firstOk || !ordered) {
            std::cerr << "FAIL [topk-basic]: size=" << top.size()
                      << " first='" << (top.empty() ? "" : top[0].text) << "'\n";
            ++failures;
        } else {
            std::cout << "PASS [topk-basic]\n";
        }
    }

    // TopKChunks clamps to available chunks when topK > chunks.size().
    {
        std::vector<CorpusChunk> chunks = {{1, 1, "only", {1.0f}}};
        auto top = TopKChunks(chunks, {1.0f}, 5);
        if (top.size() != 1) {
            std::cerr << "FAIL [topk-clamp]: size=" << top.size() << "\n";
            ++failures;
        } else {
            std::cout << "PASS [topk-clamp]\n";
        }
    }

    return failures;
}
