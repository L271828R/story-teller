#pragma once
#include <string>
#include <vector>

struct QuizTabQuestion {
    int num;
    std::string rawBlock;
};

struct QuizTabDoc {
    std::string name;
    std::vector<QuizTabQuestion> questions;
};

std::string BuildQuizTabHTML(const std::vector<QuizTabDoc>& docs,
                              const std::string& selectedDoc,
                              bool darkMode);
