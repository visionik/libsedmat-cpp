# libsedmat-cpp

Zero-dependency C++20 library implementing **[SEDMAT](https://sedmat.org)** (*Sed-Expression-Driven Mark(up|down) Annotation & Transformation*, **v3.6 + v3.7 `@` container scopes**).

For full language syntax, flags, and container scope documentation, see **[sedmat.org](https://sedmat.org)** and [`visionik/sedmat`](https://github.com/visionik/sedmat).

## Installation (CMake)

```cmake
include(FetchContent)
FetchContent_Declare(
  libsedmat
  GIT_REPOSITORY https://github.com/visionik/libsedmat-cpp.git
  GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(libsedmat)

target_link_libraries(my_app PRIVATE sedmat)
```

Requires a C++20 compiler (Clang, GCC, or MSVC) and CMake 3.20+. **Zero third-party dependencies.**

## Quick Start

```cpp
#include "sedmat/sedmat.hpp"
#include <iostream>

int main() {
    sedmat::MemoryDocument doc({
        "Draft Proposal",
        "Critical warning in paragraph two",
        "Temporary note to remove",
    });

    const std::string script = R"(
1s/Draft/{h=1}Final/
2s/warning/{b c=red}/
$d
$a/Appendix A/
)";

    sedmat::Executor exec(doc);
    auto preview = exec.execute_string(script, sedmat::ExecuteOptions{true, ""});
    std::cout << "Dry-run matched " << preview.total_matches << " operations\n";

    exec.execute_string(script);
    auto proj = doc.project();
    std::cout << sedmat::format_numbered(sedmat::select_segment(proj));
    return 0;
}
```

## SEDMAT v3.7 Container Scopes (`@`)

Target slides, sheets, tabs, pages, headers/footers, and named regions (`title`, `sub`, `body`, `notes`, `shape=N`):

```cpp
auto prog = sedmat::parse_program(R"(
@1:{z=#0F172A transition=fade}
@slide=2:title:s/.*/{c=navy}New Title/
@2:notes:a/Pause for Q&A./
@"Q4 Budget":s/USD/EUR/g
@$a/{layout=two-col}Next Steps/
@3{move=1}
)");
```

## IntelliSense, Diagnostics & Syntax Tokenizer

`libsedmat-cpp` includes a UI-agnostic, zero-dependency IntelliSense engine for editor command bars, TUIs, CLIs, and LSPs:

```cpp
sedmat::ContextInfo ctx;
ctx.mode = sedmat::Profile::Slides;
ctx.active_container = 2;

// 1. Cursor-aware completions with capability profiles & host extensions
auto res = sedmat::get_completions("s/foo/{c=re", 11, ctx);
// res.suggestions[0].label == "red", swatch == "#FF0000"
// res.ghost_suffix == "d"

// 2. Non-throwing syntax tokenizer
auto tokens = sedmat::tokenize("@2:title:s/foo/{b c=red}bar/gi");

// 3. Structured diagnostics from parser errors
auto diag = sedmat::extract_diagnostic("SEDMAT error at offset 9: Unclosed brace — add '}'");
```

## Implementing a Custom Backend

Subclass `sedmat::Backend` (`project`, `apply_text_plan`, `apply_address_mutations`, `apply_transliterate`, and optional `apply_container_op`) to connect SEDMAT to any rich document engine.

## Guide for AI Agents

- Inspect first via `backend.project()` → `format_structure(seg)` / `format_numbered(seg)` for 1-based indices.
- Validate scripts with `ExecuteOptions{true, ""}` (dry-run) before applying.
- Honor `BraceExpression::no_reset` (`{!0}`) for additive formatting vs. implicit reset default.
- Conformance suite: `testdata/conformance/v3.6.json` (shared across Go, TS, Rust, Python, and C++).

## Examples

All examples have **zero external dependencies** (C++20 stdlib + POSIX sockets for the web demos):

- **[`examples/lsp_server.cpp`](./examples/lsp_server.cpp)** — SEDMAT Language Server (LSP over stdio) with live diagnostics, profile-aware completions + color swatches, and semantic tokens:
  ```bash
  cmake -B build && cmake --build build && ./build/sedmat_lsp_server
  ```
- **[`examples/doc_playground.cpp`](./examples/doc_playground.cpp)** — Launch-a-web Document Playground (`http://127.0.0.1:4173`) with live syntax highlighting, IntelliSense dropdown, color swatches, ghost text, dry-run preview, and numbered document view:
  ```bash
  ./build/sedmat_doc_playground
  ```
- **[`examples/slide_deck.cpp`](./examples/slide_deck.cpp)** — Launch-a-web Slide Deck Studio (`http://127.0.0.1:4174`) demonstrating SEDMAT v3.7 `@` container scopes and Slides profile completions:
  ```bash
  ./build/sedmat_slide_deck
  ```

## Development

```bash
make check   # cmake build + ctest
```

## Related

- **SEDMAT Website & Docs**: [https://sedmat.org](https://sedmat.org)
- **SEDMAT Specification**: [https://github.com/visionik/sedmat](https://github.com/visionik/sedmat)
- **Sibling Libraries**: [`libsedmat-go`](https://github.com/visionik/libsedmat-go) · [`libsedmat-ts`](https://github.com/visionik/libsedmat-ts) · [`libsedmat-rs`](https://github.com/visionik/libsedmat-rs) · [`libsedmat-py`](https://github.com/visionik/libsedmat-py)

## License

MIT — see [LICENSE](./LICENSE).
