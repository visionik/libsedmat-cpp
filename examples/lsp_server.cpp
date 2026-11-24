// Zero-dependency SEDMAT Language Server (LSP over stdio, C++20).
//
// Build & Run: cmake -B build && cmake --build build && ./build/sedmat_lsp_server

#include "sedmat/sedmat.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string json_escape(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c >= 0x20) out.push_back(static_cast<char>(c));
                break;
        }
    }
    out += "\"";
    return out;
}

std::optional<std::string> find_json_string(const std::string& body, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    std::size_t idx = body.find(pat);
    if (idx == std::string::npos) return std::nullopt;
    std::size_t colon = body.find(':', idx + pat.size());
    if (colon == std::string::npos) return std::nullopt;
    std::size_t i = colon + 1;
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) ++i;
    if (i >= body.size() || body[i] != '"') return std::nullopt;
    ++i;
    std::string out;
    while (i < body.size()) {
        if (body[i] == '\\' && i + 1 < body.size()) {
            char nxt = body[++i];
            switch (nxt) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                default: out.push_back(nxt); break;
            }
        } else if (body[i] == '"') {
            return out;
        } else {
            out.push_back(body[i]);
        }
        ++i;
    }
    return std::nullopt;
}

std::optional<std::size_t> find_json_number(const std::string& body, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    std::size_t idx = body.find(pat);
    if (idx == std::string::npos) return std::nullopt;
    std::size_t colon = body.find(':', idx + pat.size());
    if (colon == std::string::npos) return std::nullopt;
    std::size_t i = colon + 1;
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) ++i;
    std::size_t start = i;
    while (i < body.size() && std::isdigit(static_cast<unsigned char>(body[i]))) ++i;
    if (i == start) return std::nullopt;
    return static_cast<std::size_t>(std::stoull(body.substr(start, i - start)));
}

void send_frame(const std::string& body) {
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body << std::flush;
}

bool read_frame(std::string& out) {
    std::string line;
    std::size_t content_len = 0;
    bool got_len = false;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        std::string lower = line;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const std::string prefix = "content-length:";
        if (lower.rfind(prefix, 0) == 0) {
            content_len = static_cast<std::size_t>(std::stoull(line.substr(prefix.size())));
            got_len = true;
        }
    }
    if (!got_len) return false;
    out.assign(content_len, '\0');
    std::cin.read(out.data(), static_cast<std::streamsize>(content_len));
    return std::cin.gcount() == static_cast<std::streamsize>(content_len);
}

int kind_to_lsp(sedmat::SuggestionKind k) {
    switch (k) {
        case sedmat::SuggestionKind::Scope: return 9;
        case sedmat::SuggestionKind::Command: return 14;
        case sedmat::SuggestionKind::Flag: return 10;
        case sedmat::SuggestionKind::Value: return 12;
        case sedmat::SuggestionKind::Snippet: return 15;
    }
    return 15;
}

int token_type_index(sedmat::TokenKind k) {
    switch (k) {
        case sedmat::TokenKind::Comment: return 0;
        case sedmat::TokenKind::Scope: return 1;
        case sedmat::TokenKind::Address: return 2;
        case sedmat::TokenKind::Command: return 3;
        case sedmat::TokenKind::Delimiter: return 4;
        case sedmat::TokenKind::Pattern: return 5;
        case sedmat::TokenKind::Brace: return 6;
        case sedmat::TokenKind::Text: return 7;
        case sedmat::TokenKind::Flag: return 8;
        case sedmat::TokenKind::Whitespace: return 9;
    }
    return 7;
}

std::pair<std::size_t, std::size_t> offset_to_line_char(const std::string& text, std::size_t offset) {
    std::size_t safe = std::min(offset, text.size());
    std::size_t line = 0;
    std::size_t last_nl = 0;
    for (std::size_t i = 0; i < safe; ++i) {
        if (text[i] == '\n') {
            ++line;
            last_nl = i + 1;
        }
    }
    return {line, safe - last_nl};
}

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        std::size_t nl = s.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(s.substr(start));
            break;
        }
        lines.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

void publish_diagnostics(const std::string& uri, const std::string& text) {
    auto lines = split_lines(text);
    std::ostringstream items;
    bool first = true;
    for (std::size_t idx = 0; idx < lines.size(); ++idx) {
        std::string line = lines[idx];
        std::size_t s = 0;
        while (s < line.size() && std::isspace(static_cast<unsigned char>(line[s]))) ++s;
        std::string trimmed = line.substr(s);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        try {
            (void)sedmat::parse_expression(trimmed);
        } catch (const std::exception& e) {
            auto d = sedmat::extract_diagnostic(e.what());
            std::size_t col = d.error_offset.value_or(0);
            std::size_t len = std::max<std::size_t>(1, d.error_length);
            std::string msg = d.hint ? (d.summary + " \xE2\x80\x94 " + *d.hint) : d.summary;
            if (!first) items << ",";
            first = false;
            items << "{\"range\":{\"start\":{\"line\":" << idx << ",\"character\":" << col
                  << "},\"end\":{\"line\":" << idx << ",\"character\":" << (col + len)
                  << "}},\"severity\":1,\"source\":\"libsedmat-cpp\",\"message\":" << json_escape(msg) << "}";
        }
    }
    send_frame("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":" +
               json_escape(uri) + ",\"diagnostics\":[" + items.str() + "]}}");
}

std::string encode_semantic_tokens(const std::string& text) {
    auto tokens = sedmat::tokenize(text);
    std::ostringstream oss;
    bool first = true;
    std::size_t prev_line = 0, prev_char = 0;
    for (const auto& tok : tokens) {
        if (tok.kind == sedmat::TokenKind::Whitespace || tok.text.find('\n') != std::string::npos) continue;
        auto [line, ch] = offset_to_line_char(text, tok.start);
        std::size_t delta_line = line - prev_line;
        std::size_t delta_start = (delta_line == 0) ? (ch - prev_char) : ch;
        if (!first) oss << ",";
        first = false;
        oss << delta_line << "," << delta_start << "," << (tok.end - tok.start) << ","
            << token_type_index(tok.kind) << ",0";
        prev_line = line;
        prev_char = ch;
    }
    return oss.str();
}

} // namespace

int main() {
    std::unordered_map<std::string, std::string> docs;
    bool shutdown_requested = false;
    std::string body;

    while (read_frame(body)) {
        std::string method = find_json_string(body, "method").value_or("");
        auto id = find_json_number(body, "id");

        if (method == "initialize") {
            std::size_t id_num = id.value_or(1);
            send_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id_num) +
                ",\"result\":{\"capabilities\":{\"textDocumentSync\":1,"
                "\"completionProvider\":{\"triggerCharacters\":[\"{\",\"@\",\"/\",\"=\",\":\",\" \"]},"
                "\"signatureHelpProvider\":{\"triggerCharacters\":[\"/\",\"{\",\"=\"]},"
                "\"semanticTokensProvider\":{\"legend\":{\"tokenTypes\":[\"comment\",\"namespace\",\"number\",\"keyword\",\"operator\",\"regexp\",\"decorator\",\"string\",\"modifier\",\"variable\"],\"tokenModifiers\":[]},\"full\":true}},"
                "\"serverInfo\":{\"name\":\"libsedmat-cpp-lsp\",\"version\":\"0.1.0\"}}}");
        } else if (method == "initialized") {
            // no-op
        } else if (method == "shutdown") {
            shutdown_requested = true;
            send_frame("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id.value_or(0)) + ",\"result\":null}");
        } else if (method == "exit") {
            return shutdown_requested ? 0 : 1;
        } else if (method == "textDocument/didOpen" || method == "textDocument/didChange") {
            auto uri = find_json_string(body, "uri");
            auto text = find_json_string(body, "text");
            if (uri && text) {
                docs[*uri] = *text;
                publish_diagnostics(*uri, *text);
            }
        } else if (method == "textDocument/completion") {
            std::size_t id_num = id.value_or(0);
            std::string uri = find_json_string(body, "uri").value_or("");
            std::size_t line = find_json_number(body, "line").value_or(0);
            std::size_t ch = find_json_number(body, "character").value_or(0);
            auto lines = split_lines(docs[uri]);
            std::string line_text = (line < lines.size()) ? lines[line] : "";
            sedmat::ContextInfo ctx;
            ctx.mode = sedmat::Profile::All;
            auto res = sedmat::get_completions(line_text, ch, ctx);
            std::ostringstream items;
            for (std::size_t i = 0; i < res.suggestions.size(); ++i) {
                const auto& s = res.suggestions[i];
                if (i > 0) items << ",";
                std::string detail = s.swatch ? (s.detail + " (" + *s.swatch + ")") : s.detail;
                items << "{\"label\":" << json_escape(s.label)
                      << ",\"kind\":" << kind_to_lsp(s.kind)
                      << ",\"detail\":" << json_escape(detail)
                      << ",\"textEdit\":{\"range\":{\"start\":{\"line\":" << line
                      << ",\"character\":" << s.replace_from
                      << "},\"end\":{\"line\":" << line
                      << ",\"character\":" << s.replace_to
                      << "}},\"newText\":" << json_escape(s.insert_text) << "}}";
            }
            send_frame("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id_num) +
                       ",\"result\":{\"isIncomplete\":false,\"items\":[" + items.str() + "]}}");
        } else if (method == "textDocument/semanticTokens/full") {
            std::size_t id_num = id.value_or(0);
            std::string uri = find_json_string(body, "uri").value_or("");
            send_frame("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id_num) +
                       ",\"result\":{\"data\":[" + encode_semantic_tokens(docs[uri]) + "]}}");
        } else if (id.has_value()) {
            send_frame("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(*id) +
                       ",\"error\":{\"code\":-32601,\"message\":" + json_escape("Method not found: " + method) + "}}");
        }
    }
    return 0;
}
