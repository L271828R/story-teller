#pragma once
#include <string>

enum class LLMBackend { ClaudeP, Ollama, API, Clipboard };

struct LLMConfig {
    LLMBackend  backend     = LLMBackend::Clipboard;
    std::string apiKey;
    std::string ollamaModel = "llama3";
    std::string ollamaUrl   = "http://localhost:11434";
};

struct LLMResult {
    bool        ok    = false;
    std::string text;
    std::string error;
};

// Invoke the LLM synchronously. Must be called from a background thread for
// all backends except Clipboard (which must run on the main thread).
// For Clipboard: copies the prompt to clipboard and returns ok=true, text="clipboard".
LLMResult InvokeLLM(const std::string& prompt, const LLMConfig& cfg);
