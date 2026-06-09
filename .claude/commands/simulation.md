# Simulation Workloads

You are helping extend **GPUKit** as a simulation-ready GPU compute framework for scientific and engineering fields. The primary API for simulation code is the `compute` module (`Tensor`, `Kernel`, `Stream`).

## Context

GPUKit is evolving to support GPU-accelerated simulation for:
- **Physics**: rigid body dynamics, soft body / cloth, particle systems
- **Fluid dynamics**: SPH (smoothed-particle hydrodynamics), grid-based CFD
- **Numerical solvers**: FEM (finite element method), FDM (finite difference method)
- **ML / scientific computing**: inference kernels, data-parallel algorithms
- **Engineering**: structural analysis, thermal simulation

## Guiding principles

1. **Thin abstraction**: keep the compute module thin enough that experts can drop to raw gpukit (or raw Vulkan via `include/gpukit/vulkan.h`) when needed
2. **Zero-boilerplate for common patterns**: dispatch → barrier → sync should be one-liners
3. **Headless-first**: all simulation code must work without a window (`gpukit::init({})`)
4. **Explicit sync**: never hide synchronization — surface it so users understand cost

## Adding a new simulation kernel

1. Write a GLSL compute shader in the appropriate location (e.g., `shaders/physics/integrate.comp`)
2. Follow GLSL conventions:
   ```glsl
   #version 450
   layout(local_size_x = 64) in;
   layout(set = 0, binding = 0, std430) buffer Positions { vec4 pos[]; };
   layout(set = 0, binding = 1, std430) buffer Velocities { vec4 vel[]; };
   // Guard against out-of-bounds threads:
   layout(push_constant) uniform PC { uint n; } pc;

   void main() {
       uint i = gl_GlobalInvocationID.x;
       if (i >= pc.n) return;
       // ...
   }
   ```
3. Create a `Kernel` and integrate into a `Stream`:
   ```cpp
   Tensor<float, 4> positions(N), velocities(N);
   Kernel integrate("shaders/physics/integrate.comp");
   Stream sim_stream;

   // Per simulation step:
   sim_stream.dispatch(integrate, positions, velocities);
   sim_stream.sync();
   ```

## Iterative solver pattern

Most simulation algorithms are iterative. Use `Stream` reuse and ping-pong buffers:

```cpp
Tensor<float> state_a(N), state_b(N);
Kernel solver("shaders/solver.comp");
Stream s;

for (int step = 0; step < num_steps; step++) {
    auto& src = (step % 2 == 0) ? state_a : state_b;
    auto& dst = (step % 2 == 0) ? state_b : state_a;
    s.dispatch(solver, src, dst);
    s.sync();
}
state_a.download(result.data(), N);  // or state_b depending on parity
```

## Multi-stage pipelines

Chain dependent stages in a single `Stream` batch — barriers are automatic:

```cpp
Stream sim;
sim.dispatch(force_accumulate_k,  positions, forces);
sim.dispatch(integrate_k,         forces, velocities, positions);  // barrier before this
sim.dispatch(constraint_solve_k,  positions, constraints);          // barrier before this
sim.sync();
```

Use separate `Stream` objects for truly independent sub-systems (no shared buffers) to avoid conservative barrier overhead.

## Data layout recommendations

For simulation data in GLSL:
- **Scalar arrays**: `Tensor<float>` → `layout(std430) buffer { float v[]; }`
- **3D vectors**: `Tensor<float, 4>` (pad to vec4) → `layout(std430) buffer { vec4 v[]; }` — never use `vec3` in std430, it has alignment issues
- **Structs**: define a matching C++ struct and use `Tensor<MyStruct>`

Example for a particle system:
```cpp
struct Particle { float x, y, z, mass; };   // 16 bytes, vec4-aligned
Tensor<Particle> particles(N);
// GLSL: layout(std430) buffer { vec4 data[]; } particles;  (treat as vec4)
```

## GPU performance for simulation

- **Minimize sync points**: batch as many dispatches as possible before `stream.sync()`
- **Prefer DEVICE tensors** for intermediate buffers that never need CPU readback
- **Use HOST tensors** for boundary conditions or parameters that change each step
- **Local size tuning**: start with 64 (default), profile with 128 or 256 for memory-bound kernels; must match `layout(local_size_x = N)` in the shader
- **Shared memory**: for stencil operations (FDM, SPH neighbor search), use `shared` arrays in GLSL — requires careful workgroup sizing

## What to add to the framework

When adding simulation capabilities, consider whether the pattern is general enough to belong in:
- A new `Tensor<T>` method (e.g., `fill()`, `copy_to()`)
- A utility in the `Stream` (e.g., explicit local size override)
- A standalone simulation utility header under `include/gpukit/sim/`
- Or just a well-documented example in `examples/`

Prefer examples over new API surface until a pattern proves itself in multiple simulation domains.
