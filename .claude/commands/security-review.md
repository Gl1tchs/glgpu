# Security Review — GPUKit / Vulkan Code

You are performing a security review of GPU compute code in **GPUKit**, a Vulkan abstraction library targeting scientific simulation workloads.

## Scope

Perform a targeted security review of the changed or specified files. Focus on the threat model relevant to a GPU compute library:

### 1. Memory Safety (highest priority)
- **Buffer overreads/overwrites** in staging copy paths (`buffer_upload`, `image_upload`, `tensor upload/download`)
- **Integer overflow** in size calculations: `count * sizeof(T)` must not overflow `uint64_t` / `size_t`
- **Use-after-free** on handles: check that moved-from handles are set to `GL_NULL_HANDLE` before destructor runs (Tensor, Kernel, Stream move constructors/operators)
- **Double-free** if a Vulkan object destructor is called twice
- **Unvalidated `value()` calls**: ensure every `Res<T>` is checked with `is_ok()` before `.value()` — unchecked `.value()` on an error result is undefined behavior

### 2. GPU Resource Exhaustion / Denial
- **Unbounded descriptor pool growth**: check `uniform_set_create_bindless` `max_count` is validated against device limits
- **Uncapped staging allocations**: loops calling `upload()` without `sync()` can exhaust VRAM
- **Missing `device_wait()` before `shutdown()`**: can crash the driver if GPU still uses freed resources

### 3. Synchronization Hazards
- **Missing pipeline barriers**: RAW (read-after-write) or WAW (write-after-write) hazards on Buffers/Images between dispatches
- **Timeline semaphore counter regression**: signaling a value ≤ current value causes undefined behavior
- **Fence reuse without reset**: submitting to a queue with a fence that was never reset after its last use

### 4. Shader / SPIR-V Input Validation
- **Unchecked shader file paths**: `Kernel(const char* path)` and `shader_create("path")` should not allow path traversal if paths come from user-controlled input
- **Unvalidated specialization constants**: `SpecializationConstant` values that exceed the declared constant size
- **Reflection data trust**: SPIRV-Reflect output is trusted data from compiled bytecode — verify it's not blindly used to index arrays without bounds checks

### 5. Host-Side Validation
- **Null handle dereference**: any public API function accepting a handle should reject `GL_NULL_HANDLE` with `Error::INVALID_HANDLE` rather than crashing
- **Zero-size allocations**: `buffer_create(0, ...)` or `Tensor<float>(0)` — what happens? Should be an explicit error.
- **Mismatched set/binding indices** in `uniform_set_update_*` vs declared shader layout

## Output Format

For each finding:
1. **File + line number**
2. **Severity**: Critical / High / Medium / Low
3. **What**: one-sentence description of the vulnerability
4. **Why it matters**: impact in a simulation/scientific computing context
5. **Fix**: concrete code change or validation to add

Skip informational/style issues — only report actual security or correctness risks.
