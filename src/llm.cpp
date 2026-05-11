#include "llm.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>

namespace fs = std::filesystem;

static std::string temp_prompt_path() {
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return (fs::temp_directory_path() / ("mdviewer_prompt_" + std::to_string(ns) + ".txt"))
           .string();
}

static LLMResult run_shell(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {false, "", "popen failed: " + cmd};
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    int rc = pclose(pipe);
    if (rc != 0) return {false, out, "command failed (exit " + std::to_string(rc) + ")"};
    return {true, out, ""};
}

// Extract the value of a JSON string field (simple, no full parser needed).
static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    std::string val;
    bool esc = false;
    for (std::size_t i = pos; i < json.size(); ++i) {
        if (esc) {
            switch (json[i]) {
                case 'n': val += '\n'; break;
                case 't': val += '\t'; break;
                case 'r': val += '\r'; break;
                default:  val += json[i]; break;
            }
            esc = false;
        } else if (json[i] == '\\') {
            esc = true;
        } else if (json[i] == '"') {
            break;
        } else {
            val += json[i];
        }
    }
    return val;
}

LLMResult InvokeLLM(const std::string& prompt, const LLMConfig& cfg) {
    if (cfg.backend == LLMBackend::Clipboard) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(prompt)));
            wxTheClipboard->Close();
        }
        return {true, "clipboard", ""};
    }

    std::string tmpFile = temp_prompt_path();
    {
        std::ofstream f(tmpFile);
        f << prompt;
        if (!f.good()) return {false, "", "could not write temp file"};
    }

    LLMResult result;

    if (cfg.backend == LLMBackend::ClaudeP) {
        // Use login shell so ~/.nvm, Homebrew, etc. are on PATH.
        std::string cmd = "bash -l -c 'claude -p < \"" + tmpFile + "\"' 2>&1";
        result = run_shell(cmd);
        if (!result.ok && result.text.find("not found") != std::string::npos)
            result.error = "claude CLI not found — check PATH or use Clipboard mode";
    }
    else if (cfg.backend == LLMBackend::Ollama) {
        // Write JSON body to a second temp file to avoid shell quoting issues.
        std::string jsonFile = tmpFile + ".json";
        {
            std::ofstream jf(jsonFile);
            // Escape the prompt for JSON by re-using the file content via @.
            // We use --data-binary @file to avoid any shell interpolation.
            // Build a minimal JSON object around the raw prompt.
            jf << "{\"model\":\"" << cfg.ollamaModel << "\","
               << "\"stream\":false,"
               << "\"prompt\":\"";
            for (char c : prompt) {
                if      (c == '"')  jf << "\\\"";
                else if (c == '\\') jf << "\\\\";
                else if (c == '\n') jf << "\\n";
                else if (c == '\r') jf << "\\r";
                else if (c == '\t') jf << "\\t";
                else                jf << c;
            }
            jf << "\"}";
        }
        std::string cmd = "curl -s -X POST \"" + cfg.ollamaUrl + "/api/generate\""
                          " -H 'Content-Type: application/json'"
                          " --data-binary @\"" + jsonFile + "\"" " 2>&1";
        auto raw = run_shell(cmd);
        fs::remove(jsonFile);
        if (!raw.ok) return raw;
        std::string text = extract_json_string(raw.text, "response");
        if (text.empty())
            return {false, "", "Ollama returned unexpected JSON: " + raw.text.substr(0, 200)};
        result = {true, text, ""};
    }
    else if (cfg.backend == LLMBackend::API) {
        std::string jsonFile = tmpFile + ".json";
        {
            std::ofstream jf(jsonFile);
            jf << "{\"model\":\"claude-sonnet-4-6\",\"max_tokens\":8096,"
               << "\"messages\":[{\"role\":\"user\",\"content\":\"";
            for (char c : prompt) {
                if      (c == '"')  jf << "\\\"";
                else if (c == '\\') jf << "\\\\";
                else if (c == '\n') jf << "\\n";
                else if (c == '\r') jf << "\\r";
                else if (c == '\t') jf << "\\t";
                else                jf << c;
            }
            jf << "\"}]}";
        }
        std::string cmd =
            "curl -s -X POST https://api.anthropic.com/v1/messages"
            " -H 'x-api-key: " + cfg.apiKey + "'"
            " -H 'anthropic-version: 2023-06-01'"
            " -H 'Content-Type: application/json'"
            " --data-binary @\"" + jsonFile + "\"" " 2>&1";
        auto raw = run_shell(cmd);
        fs::remove(jsonFile);
        if (!raw.ok) return raw;
        std::string text = extract_json_string(raw.text, "text");
        if (text.empty())
            return {false, "", "API returned unexpected JSON: " + raw.text.substr(0, 200)};
        result = {true, text, ""};
    }

    fs::remove(tmpFile);
    return result;
}
