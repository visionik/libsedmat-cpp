// Launch-a-Web SEDMAT Document Playground (zero external dependencies, POSIX sockets + C++20).
//
// Build & Run: cmake -B build && cmake --build build && ./build/sedmat_doc_playground

#include "sedmat/sedmat.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace {

const char* HTML = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>libsedmat-cpp — Document Playground</title>
  <style>
    :root { color-scheme: dark; font-family: ui-sans-serif, system-ui, sans-serif; }
    body { margin: 0; background: #0b1120; color: #e2e8f0; display: grid; place-items: center; min-height: 100vh; }
    .app { width: min(920px, 94vw); display: grid; gap: 16px; padding: 24px 0; }
    .card { background: #111827; border: 1px solid #1f2937; border-radius: 12px; padding: 16px; }
    .row { display: flex; gap: 8px; }
    input { flex: 1; background: #0f172a; color: #f8fafc; border: 1px solid #334155; border-radius: 8px; padding: 10px 12px; font-family: ui-monospace, monospace; font-size: 14px; }
    button { background: #2563eb; color: #fff; border: 0; border-radius: 8px; padding: 10px 16px; font-weight: 600; cursor: pointer; }
    button.sec { background: #334155; }
    .tokens { font-family: ui-monospace, monospace; font-size: 13px; margin-top: 8px; min-height: 20px; }
    .tok-scope { color: #c084fc; } .tok-address { color: #fbbf24; } .tok-command { color: #38bdf8; font-weight: 700; }
    .tok-pattern { color: #f472b6; } .tok-brace { color: #4ade80; } .tok-flag { color: #fb923c; } .tok-delimiter { color: #64748b; }
    .ok { color: #4ade80; } .err { color: #f87171; }
    .sugs { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 10px; }
    .sug { background: #1e293b; border: 1px solid #334155; color: #e2e8f0; border-radius: 6px; padding: 4px 8px; font-size: 12px; cursor: pointer; display: inline-flex; align-items: center; gap: 6px; }
    .swatch { width: 10px; height: 10px; border-radius: 2px; border: 1px solid #475569; }
    pre { margin: 0; white-space: pre-wrap; font-family: ui-monospace, monospace; font-size: 14px; line-height: 1.6; }
  </style>
</head>
<body>
  <div class="app">
    <h1 style="margin:0;font-size:20px;">libsedmat-cpp — Document Playground (Zero-Dep C++20)</h1>
    <div class="card">
      <div class="row">
        <input id="cmd" value="2s/TODO/{b c=red}/g" />
        <button id="apply">Apply</button>
        <button id="reset" class="sec">Reset</button>
      </div>
      <div id="tokens" class="tokens"></div>
      <div id="status" style="font-size:13px;margin-top:8px;"></div>
      <div id="sugs" class="sugs"></div>
    </div>
    <div class="card">
      <h3 style="margin:0 0 10px;font-size:13px;color:#94a3b8;text-transform:uppercase;">Numbered Document (cat -N)</h3>
      <pre id="doc"></pre>
    </div>
  </div>
  <script>
    const cmd = document.getElementById("cmd");
    async function refresh() {
      const r = await fetch("/api/state").then(x => x.json());
      document.getElementById("doc").textContent = r.numbered;
    }
    async function inspect() {
      const input = cmd.value;
      const r = await fetch("/api/inspect", { method: "POST", body: input }).then(x => x.json());
      document.getElementById("tokens").innerHTML = r.tokens.map(t => '<span class="tok-'+t.kind+'">'+t.text.replace(/[<>&]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;'}[c]))+'</span>').join("") + (r.ghost ? '<span style="opacity:.4">'+r.ghost+'</span>' : "");
      const st = document.getElementById("status");
      st.className = r.ok ? "ok" : "err";
      st.textContent = (r.ok ? "✓ " : "✗ ") + r.summary;
      const sugs = document.getElementById("sugs");
      sugs.innerHTML = "";
      for (const s of r.suggestions.slice(0, 8)) {
        const b = document.createElement("button");
        b.className = "sug";
        b.innerHTML = (s.swatch ? '<span class="swatch" style="background:'+s.swatch+'"></span>' : '') + '<strong>'+s.label+'</strong> <span style="opacity:.7">'+s.detail+'</span>';
        b.onclick = () => { cmd.value = input.slice(0, s.replaceFrom) + s.insertText + input.slice(s.replaceTo); cmd.focus(); inspect(); };
        sugs.appendChild(b);
      }
    }
    document.getElementById("apply").onclick = async () => {
      await fetch("/api/apply", { method: "POST", body: cmd.value });
      await refresh(); await inspect();
    };
    document.getElementById("reset").onclick = async () => { await fetch("/api/reset", { method: "POST" }); await refresh(); };
    cmd.addEventListener("input", inspect);
    refresh(); inspect();
  </script>
</body>
</html>)HTML";

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

const char* kind_name(sedmat::TokenKind k) {
    switch (k) {
        case sedmat::TokenKind::Comment: return "comment";
        case sedmat::TokenKind::Scope: return "scope";
        case sedmat::TokenKind::Address: return "address";
        case sedmat::TokenKind::Command: return "command";
        case sedmat::TokenKind::Delimiter: return "delimiter";
        case sedmat::TokenKind::Pattern: return "pattern";
        case sedmat::TokenKind::Brace: return "brace";
        case sedmat::TokenKind::Text: return "text";
        case sedmat::TokenKind::Flag: return "flag";
        case sedmat::TokenKind::Whitespace: return "whitespace";
    }
    return "text";
}

sedmat::MemoryDocument initial_doc() {
    return sedmat::MemoryDocument({
        "SEDMAT v3.7 Interactive Document",
        "This paragraph contains a TODO item and a critical warning.",
        "DRAFT: delete this scratch note with 3d or d/DRAFT/",
        "Closing summary ready for formatting and appends.",
    });
}

void send_http(int fd, const std::string& status, const std::string& ct, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\nContent-Type: " << ct
        << "\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
    std::string resp = oss.str();
    (void)write(fd, resp.data(), resp.size());
}

} // namespace

int main() {
    int port = 4173;
    if (const char* p = std::getenv("PORT")) port = std::atoi(p);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || listen(server_fd, 16) < 0) {
        std::cerr << "Failed to bind port " << port << "\n";
        return 1;
    }

    std::cout << "SEDMAT C++ Document Playground running at http://127.0.0.1:" << port << "\n";
    auto doc = initial_doc();

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;
        char buf[16384];
        ssize_t n = read(client, buf, sizeof(buf) - 1);
        if (n <= 0) {
            close(client);
            continue;
        }
        buf[n] = '\0';
        std::string req(buf, static_cast<std::size_t>(n));
        std::istringstream iss(req);
        std::string method, path;
        iss >> method >> path;
        std::size_t body_pos = req.find("\r\n\r\n");
        std::string body = (body_pos != std::string::npos) ? req.substr(body_pos + 4) : "";

        if (method == "GET" && path == "/") {
            send_http(client, "200 OK", "text/html; charset=utf-8", HTML);
        } else if (method == "GET" && path == "/api/state") {
            auto proj = doc.project();
            std::string numbered = sedmat::format_numbered(sedmat::select_segment(proj));
            send_http(client, "200 OK", "application/json", "{\"numbered\":" + json_escape(numbered) + "}");
        } else if (method == "POST" && path == "/api/reset") {
            doc = initial_doc();
            send_http(client, "200 OK", "application/json", "{\"ok\":true}");
        } else if (method == "POST" && path == "/api/inspect") {
            auto res = sedmat::get_completions(body, body.size());
            auto toks = sedmat::tokenize(body);
            std::ostringstream t_json, s_json;
            for (std::size_t i = 0; i < toks.size(); ++i) {
                if (i > 0) t_json << ",";
                t_json << "{\"kind\":\"" << kind_name(toks[i].kind) << "\",\"text\":" << json_escape(toks[i].text) << "}";
            }
            for (std::size_t i = 0; i < res.suggestions.size(); ++i) {
                const auto& s = res.suggestions[i];
                if (i > 0) s_json << ",";
                s_json << "{\"label\":" << json_escape(s.label)
                       << ",\"insertText\":" << json_escape(s.insert_text)
                       << ",\"replaceFrom\":" << s.replace_from
                       << ",\"replaceTo\":" << s.replace_to
                       << ",\"detail\":" << json_escape(s.detail)
                       << ",\"swatch\":" << (s.swatch ? json_escape(*s.swatch) : "null") << "}";
            }
            bool ok = true;
            std::string summary = "Ready";
            if (!body.empty()) {
                try {
                    sedmat::Executor ex(doc);
                    auto rep = ex.execute_string(body, sedmat::ExecuteOptions{true, ""});
                    summary = "Dry-run matched " + std::to_string(rep.total_matches) + " operation(s)";
                } catch (const std::exception& e) {
                    ok = false;
                    summary = sedmat::extract_diagnostic(e.what()).summary;
                }
            }
            std::string ghost = res.ghost_suffix ? json_escape(*res.ghost_suffix) : "null";
            send_http(client, "200 OK", "application/json",
                      "{\"ok\":" + std::string(ok ? "true" : "false") +
                          ",\"summary\":" + json_escape(summary) +
                          ",\"ghost\":" + ghost +
                          ",\"tokens\":[" + t_json.str() +
                          "],\"suggestions\":[" + s_json.str() + "]}");
        } else if (method == "POST" && path == "/api/apply") {
            try {
                sedmat::Executor ex(doc);
                ex.execute_string(body);
                send_http(client, "200 OK", "application/json", "{\"ok\":true}");
            } catch (const std::exception& e) {
                send_http(client, "400 Bad Request", "application/json",
                          "{\"ok\":false,\"error\":" + json_escape(e.what()) + "}");
            }
        } else {
            send_http(client, "404 Not Found", "text/plain", "Not Found");
        }
        close(client);
    }
    return 0;
}
