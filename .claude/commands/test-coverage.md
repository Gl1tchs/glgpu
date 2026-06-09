# Test Coverage

You are helping improve test coverage for **GPUKit**, a Vulkan GPU compute library.

## Coverage workflow

```bash
# Build with coverage instrumentation
cmake --preset coverage
cmake --build build-cov --target gpukit_tests

# Run tests and generate HTML report
cmake --build build-cov --target coverage
# Report: build-cov/coverage/html/index.html
```

The `coverage` preset sets `GPUKIT_ENABLE_COVERAGE=ON`, which adds `--coverage` to compile and link flags and wires up an `lcov`/`genhtml` target. Requires `lcov` and `genhtml` installed.

## Test structure

- All tests live in `tests/` using **Catch2 v3** (`TEST_CASE` / `SECTION` / `REQUIRE`)
- Every `TEST_CASE` that touches the GPU starts with `ensure_test_device()` from `test_common.h`
- Tests are **headless** — no swapchain, no window, just a Vulkan compute/graphics device
- Shared helpers: `gpukit::test::load_shader()`, `gpukit::test::load_compute_shader()` in `test_common.h`
- Shader source goes in `tests/assets/` (GLSL only, `.spv` is gitignored)

## Adding a new test file

Mirror this pattern exactly:

```cpp
#include "test_common.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("feature does X", "[module]") {
    ensure_test_device();

    auto res = gpukit::some_create_function(...);
    REQUIRE(res.is_ok());
    auto handle = res.value();

    // ... test logic ...

    gpukit::some_free_function(handle);
}
```

Register the file in `tests/CMakeLists.txt` by adding it to the `TEST_SOURCES` glob (it already uses `GLOB_RECURSE *.cpp` so a new `test_*.cpp` file is picked up automatically).

## Coverage gaps to watch for

Review the current test files and identify which areas lack coverage:
1. Scan `tests/` to see which modules already have tests
2. Compare against the public API surface in `include/gpukit/types.h` and `include/gpukit/device.h`
3. Check the compute module — `include/gpukit/compute/tensor.h`, `kernel.h`, `stream.h`
4. Look at error paths: are `Error::*` variants exercised, not just the happy path?
5. Check barrier correctness in `Stream` (write-after-write, read-after-write scenarios)

Report findings as a prioritized list: which uncovered paths matter most for a simulation-ready library.
