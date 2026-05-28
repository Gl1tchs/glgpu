# Contributing to GPUKit

## Development Setup

**Requirements:** CMake ≥ 3.17, C++20 compiler (Clang recommended), Vulkan SDK ≥ 1.3, `glslc`.

```bash
git clone --recursive https://github.com/Gl1tchs/gpukit.git
cd gpukit
cmake --preset debug
cmake --build --preset debug
```

## Build Presets

| Preset | Purpose | Output dir |
|--------|---------|------------|
| `debug` | Development, symbols, no optimization | `build/` |
| `release` | Optimized, no debug info | `build/` |
| `sanitize` | ASan + UBSan (Clang/GCC) or ASan (MSVC) | `build-san/` |
| `coverage` | gcov instrumentation + lcov HTML report | `build-cov/` |

## Running Tests

```bash
# Standard test run
cmake --build --preset debug --target gpukit_tests
ctest --test-dir build --output-on-failure

# Sanitizer run
cmake --preset sanitize
cmake --build --preset sanitize --target gpukit_tests
ctest --test-dir build-san --output-on-failure

# Coverage report (requires lcov + genhtml)
cmake --preset coverage
cmake --build build-cov --target coverage
# Report written to build-cov/coverage/html/index.html
```

Tests use Catch2 and are located in `tests/`. Any new GPU-facing code must have tests; headless Vulkan runs in CI so no window is required.

## Commit Style

```
feat(<module>): short imperative description
fix(<module>): short imperative description
```

- **One logical change per commit** — never bundle unrelated files.
- Stage files individually (`git add <file>`), not with `git add .`.
- Module examples: `renderer`, `shader`, `buffer`, `sync`, `cmake`, `tests`.

## Adding Tests

- Mirror the style of existing tests: `ensure_test_device()` at the top of each
  `TEST_CASE`, `REQUIRE(res.is_ok())` before `.value()`, explicit resource cleanup.
- Shared shader loading helpers live in `test_common.h` — use
  `gpukit::test::load_shader()` and `gpukit::test::load_compute_shader()`.
- Shader assets go in `tests/assets/` (GLSL source only; `.spv` files are git-ignored).
- GPU tests run headlessly — do not assume a swapchain or display surface exists.

## Code Style

Formatting is enforced by `.clang-format`. Run before committing:

```bash
clang-format -i src/**/*.cpp include/**/*.h
```

Static analysis is configured in `.clang-tidy`. Warnings introduced by new code
should be resolved before a PR is opened.

## Pull Request Checklist

- [ ] All existing tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] New functionality has tests
- [ ] Sanitizer run is clean (`ctest --test-dir build-san`)
- [ ] No new clang-tidy warnings on changed files
- [ ] Commit messages follow the `feat/fix(<module>):` format
- [ ] Public API additions are documented in `docs/architecture.md`
