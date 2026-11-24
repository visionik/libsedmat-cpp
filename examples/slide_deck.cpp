// Launch-a-Web SEDMAT v3.7 Slide Deck Studio (zero external dependencies, POSIX sockets + C++20).
//
// Build & Run: cmake -B build && cmake --build build && ./build/sedmat_slide_deck

#include "sedmat/sedmat.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

const char* HTML = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>libsedmat-cpp — Slide Deck Studio</title>
  <style>
    :root { color-scheme: dark; font-family: ui-sans-serif, system-ui, sans-serif; }
    body { margin: 0; background: #020617; color: #f8fafc; padding: 24px; display: grid; gap: 16px; max-width: 960px; margin-inline: auto; }
    .stage { aspect-ratio: 16 / 9; border-radius: 16px; padding: 40px; display: flex; flex-direction: column; justify-content: center; border: 1px solid #334155; }
    .stage h2 { margin: 0 0 16px; font-size: 36px; }
    .stage li { font-size: 20px; margin: 8px 0; }
    .notes { background: #0f172a; padding: 12px 16px; border-radius: 8px; font-size: 14px; color: #94a3b8; }
    .bar { display: flex; gap: 8px; }
    input { flex: 1; background: #0f172a; color: #f8fafc; border: 1px solid #334155; border-radius: 8px; padding: 10px 12px; font-family: ui-monospace, monospace; }
    button { background: #2563eb; color: #fff; border: 0; border-radius: 8px; padding: 10px 16px; cursor: pointer; font-weight: 600; }
    .sugs { display: flex; flex-wrap: wrap; gap: 6px; }
    .sug { background: #1e293b; border: 1px solid #334155; color: #e2e8f0; border-radius: 6px; padding: 4px 8px; font-size: 12px; cursor: pointer; }
  </style>
</head>
<body>
  <h1 style="margin:0;font-size:20px;">libsedmat-cpp — Slide Deck Studio (v3.7 Container Scopes)</h1>
  <div id="stage" class="stage"></div>
  <div id="notes" class="notes"></div>
  <div class="bar">
    <input id="cmd" value="@2:s/145ms/95ms/g" />
    <button id="run">Run</button>
    <button id="prev" style="background:#334155">Prev</button>
    <button id="next" style="background:#334155">Next</button>
  </div>
  <div id="sugs" class="sugs"></div>
  <script>
    let idx = 0, deck = [];
    const cmd = document.getElementById("cmd");
    async function render() {
      deck = await fetch("/api/slides").then(r => r.json());
      const s = deck[idx] || deck[0];
      const stage = document.getElementById("stage");
      stage.style.background = s.background || "#0F172A";
      stage.innerHTML = "<h2>" + s.title + "</h2><ul>" + s.paragraphs.map(p => "<li>" + p + "</li>").join("") + "</ul>";
      document.getElementById("notes").textContent = "Speaker notes (" + (idx+1) + "/" + deck.length + "): " + (s.notes || "(none)");
    }
    async function complete() {
      const r = await fetch("/api/complete", { method: "POST", body: cmd.value }).then(x => x.json());
      const sugs = document.getElementById("sugs");
      sugs.innerHTML = "";
      for (const s of r.suggestions.slice(0, 6)) {
        const b = document.createElement("button");
        b.className = "sug";
        b.textContent = s.label + " - " + s.detail;
        b.onclick = () => { cmd.value = cmd.value.slice(0, s.replaceFrom) + s.insertText + cmd.value.slice(s.replaceTo); complete(); };
        sugs.appendChild(b);
      }
    }
    document.getElementById("run").onclick = async () => {
      await fetch("/api/apply", { method: "POST", body: cmd.value });
      await render();
    };
    document.getElementById("prev").onclick = () => { idx = Math.max(0, idx - 1); render(); };
    document.getElementById("next").onclick = () => { idx = Math.min(deck.length - 1, idx + 1); render(); };
    cmd.addEventListener("input", complete);
    render(); complete();
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

sedmat::MemoryDocument initial_deck() {
    sedmat::MemoryDocument doc({"Container-scoped editing with @1:, @2:, and @*:"});
    doc.tabs[0].title = "SEDMAT v3.7 Slide Studio";
    doc.tabs[0].background = "#0F172A";
    doc.tabs[0].notes = "Welcome to the C++ Slide Deck Studio.";
    doc.add_tab("slide-2", "Architecture Benchmarks", {"Latency is 145ms across regions", "Zero external runtime dependencies"});
    doc.tabs[1].background = "#1E293B";
    doc.tabs[1].notes = "Highlight benchmark methodology.";
    return doc;
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
    int port = 4174;
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

    std::cout << "SEDMAT C++ Slide Deck Studio running at http://127.0.0.1:" << port << "\n";
    auto deck = initial_deck();

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
        } else if (method == "GET" && path == "/api/slides") {
            std::ostringstream oss;
            oss << "[";
            for (std::size_t i = 0; i < deck.tabs.size(); ++i) {
                const auto& t = deck.tabs[i];
                if (i > 0) oss << ",";
                oss << "{\"id\":" << json_escape(t.id)
                    << ",\"title\":" << json_escape(t.title)
                    << ",\"paragraphs\":[";
                for (std::size_t j = 0; j < t.paragraphs.size(); ++j) {
                    if (j > 0) oss << ",";
                    oss << json_escape(t.paragraphs[j].text);
                }
                oss << "],\"notes\":" << json_escape(t.notes.value_or(""))
                    << ",\"background\":" << json_escape(t.background.value_or("#0F172A")) << "}";
            }
            oss << "]";
            send_http(client, "200 OK", "application/json", oss.str());
        } else if (method == "POST" && path == "/api/complete") {
            sedmat::ContextInfo ctx;
            ctx.mode = sedmat::Profile::Slides;
            ctx.active_slide = 1;
            ctx.total_slides = deck.tabs.size();
            auto res = sedmat::get_completions(body, body.size(), ctx);
            std::ostringstream oss;
            oss << "{\"suggestions\":[";
            for (std::size_t i = 0; i < res.suggestions.size(); ++i) {
                const auto& s = res.suggestions[i];
                if (i > 0) oss << ",";
                oss << "{\"label\":" << json_escape(s.label)
                    << ",\"insertText\":" << json_escape(s.insert_text)
                    << ",\"replaceFrom\":" << s.replace_from
                    << ",\"replaceTo\":" << s.replace_to
                    << ",\"detail\":" << json_escape(s.detail) << "}";
            }
            oss << "]}";
            send_http(client, "200 OK", "application/json", oss.str());
        } else if (method == "POST" && path == "/api/apply") {
            try {
                sedmat::Executor ex(deck);
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
