# Changelog

All notable changes to `libsedmat-cpp` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **UI-Agnostic IntelliSense, Diagnostics & Tokenizer (`src/intellisense.cpp`)**:
  - `sedmat::get_completions` with capability profiles (`Document`/`Word`, `Slides`, `Spreadsheet`, `Vector`, `Timeline`, `All`), semicolon-chained command awareness, live grammar signatures, inline `ghost_suffix`, canonical `sedmat.org` color swatches, and host extension hooks (`IntelliSenseExtensions`).
  - `sedmat::extract_diagnostic` returning structured error offset, span length, summary, and actionable hint.
  - `sedmat::tokenize` non-throwing lossless syntax tokenizer.

## [0.1.0] - 2026-11-24

### Added
- Zero-dependency C++20 implementation of SEDMAT v3.6 + v3.7 (`@` container scopes and slide/shape brace flags).
- Full parser, semantic enricher, canonical `{flags}` brace DSL (`{0}` reset and `{!0}` additive no-reset), planners, `format_structure` / `format_numbered`, `Backend` interface, `Executor` (with `dry_run`), and `MemoryDocument`.
- CMake build and CTest conformance suite verifying `testdata/conformance/v3.6.json`, ported `v7`–`v12` `.sed` fixtures, and v3.7 container scopes.
