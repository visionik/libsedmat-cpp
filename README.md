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

## Implementing a Custom Backend

Subclass `sedmat::Backend` (`project`, `apply_text_plan`, `apply_address_mutations`, `apply_transliterate`, and optional `apply_container_op`) to connect SEDMAT to any rich document engine.

## Guide for AI Agents

- Inspect first via `backend.project()` → `format_structure(seg)` / `format_numbered(seg)` for 1-based indices.
- Validate scripts with `ExecuteOptions{true, ""}` (dry-run) before applying.
- Honor `BraceExpression::no_reset` (`{!0}`) for additive formatting vs. implicit reset default.
- Conformance suite: `testdata/conformance/v3.6.json` (shared across Go, TS, Rust, Python, and C++).

## Development

```bash
make check   # cmake build + ctest
```

## License

MIT — see [LICENSE](./LICENSE).
