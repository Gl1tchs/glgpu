# Code Quality

You are reviewing code quality for **GPUKit**, a C++20 Vulkan compute library.

## Automated tools

Run these before reviewing manually:

```bash
# Format check (non-destructive diff)
clang-format --dry-run -Werror src/**/*.cpp include/**/*.h compute/src/**/*.cpp

# Apply formatting
clang-format -i src/**/*.cpp include/**/*.h compute/src/**/*.cpp

# Static analysis on changed files (example)
clang-tidy src/device.cpp -- -std=c++20 -I include -I external/SPIRV-Reflect

# Sanitizer run (ASan + UBSan)
cmake --preset sanitize
cmake --build build-san --target gpukit_tests
ctest --test-dir build-san --output-on-failure
```

Config files: `.clang-format` and `.clang-tidy` at repo root.

## Manual review checklist

### API correctness
- Every fallible function returns `Res<T>`, not raw pointers or `bool`
- No naked `assert()` in public API paths — use `Res<>` with `Error::INVALID_ARGUMENT`
- Handle parameters validated: reject `GL_NULL_HANDLE` at API boundary, return `Error::INVALID_HANDLE`
- Const-correctness: read-only handle parameters should be `const`-qualified where the type allows

### RAII and lifetime
- Non-dispatchable handles freed in the correct order (child before parent)
- Move constructors set source to `GL_NULL_HANDLE` so the destructor is a no-op
- No resource leaks on error paths: if a function creates A then fails creating B, A must be freed before returning the error

### C++20 idioms
- Prefer `std::span` over raw pointer+size pairs in new public API
- `[[nodiscard]]` on `Res<T>`-returning functions so callers can't silently discard errors
- Avoid `reinterpret_cast` except in Vulkan interop; flag any new uses for review

### Compute module specifics
- `Tensor<T>` upload/download: sizes calculated as `count * sizeof(element_type)` — verify no overflow for large simulation grids
- `Stream::dispatch` positional binding: verify the tensor count matches the shader's declared binding count
- `Kernel::from_source()`: inline GLSL should be treated as untrusted if it ever comes from user data

### Performance-sensitive paths
- No heap allocations inside the hot `Stream::dispatch` loop (before the actual GPU submit)
- Descriptor set creation per-dispatch is expected; flag if sets are being created more than once per dispatch call
- Staging buffer reuse: `upload()` on a `DEVICE` tensor allocates a temporary staging buffer per call — acceptable for one-shot loads, wrong for per-frame streaming

## Output format

Group findings by file. For each: file:line, category (API / RAII / Idiom / Perf), and a one-line description + suggested fix. No style nitpicks — only things that affect correctness, safety, or measurable performance.
