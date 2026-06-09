# Synchronization Primitives

You are working on **GPU synchronization** in GPUKit. This covers fences, binary semaphores, timeline semaphores, pipeline barriers, and image layout transitions.

## Key types (from `include/gpukit/types.h`)

| Type | What it does |
|---|---|
| `Fence` | CPU/GPU sync — CPU waits for GPU to finish |
| `Semaphore` | GPU/GPU sync — signal one queue, wait on another |
| `SemaphoreSubmitInfo` | Timeline-aware submit: carries `value` for timeline semaphores |

## Fence — CPU/GPU sync

```cpp
Fence fence = fence_create(false);    // unsignaled
queue_submit(queue, cmd, fence);
fence_wait(fence);                    // CPU blocks until GPU signals
fence_reset(fence);                   // rearm before reuse

// Pre-signaled: useful to avoid blocking on the first frame
Fence first_frame_fence = fence_create(true);
```

Rules:
- Always `fence_reset` before reusing a fence that was previously waited on
- Never submit with a fence that is already in the signaled state

## Binary semaphore — GPU/GPU sync

```cpp
Semaphore signal_sem = semaphore_create();

// Queue A signals when done
queue_submit(queueA, cmd1, /*fence=*/GL_NULL_HANDLE, /*wait=*/GL_NULL_HANDLE, signal_sem);

// Queue B waits before starting
queue_submit(queueB, cmd2, fence, signal_sem, /*signal=*/GL_NULL_HANDLE);
```

## Timeline semaphore — ordered CPU/GPU sync

Timeline semaphores carry a monotonically increasing counter. Essential for multi-frame overlap and simulation step ordering.

```cpp
Semaphore tl = timeline_semaphore_create(0).value();  // starts at 0

// GPU signals value N on completion
queue_submit(queue, cmd,
    SemaphoreSubmitInfo{},                                         // no wait
    SemaphoreSubmitInfo{ .semaphore = tl, .value = step });        // signal step N

// CPU waits for GPU to reach step N
semaphore_wait(tl, step);

// CPU can also advance the counter (e.g. for producer/consumer patterns)
semaphore_signal(tl, step + 1);
```

Critical invariants:
- The signaled value must **strictly increase** — never signal a value ≤ current
- Multiple queues can wait on the same timeline at different values
- A queue can wait on a timeline value it will also signal (as long as the wait value < signal value)

## Pipeline barriers

Manual barriers (when not using `Stream`):

```cpp
// Buffer barrier: compute write → compute read
BufferBarrier barrier = {};
barrier.buffer     = buf;
barrier.src_stage  = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
barrier.dst_stage  = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
barrier.src_access = ACCESS_SHADER_WRITE_BIT;
barrier.dst_access = ACCESS_SHADER_READ_BIT;

command_pipeline_barrier(cmd, barrier);
```

Common patterns:
- **Write→Read (compute)**: `src=COMPUTE|WRITE`, `dst=COMPUTE|READ`
- **Compute→Transfer**: `src=COMPUTE|WRITE`, `dst=TRANSFER|READ`
- **Transfer→Compute**: `src=TRANSFER|WRITE`, `dst=COMPUTE|READ`

## Image layout transitions

```cpp
command_transition_image(cmd, image,
    ImageLayout::UNDEFINED,
    ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
```

Common transitions:
| From | To | Use case |
|---|---|---|
| `UNDEFINED` | `COLOR_ATTACHMENT_OPTIMAL` | Start of render pass |
| `COLOR_ATTACHMENT_OPTIMAL` | `PRESENT_SRC` | Before present |
| `UNDEFINED` | `TRANSFER_DST_OPTIMAL` | Before image upload |
| `TRANSFER_DST_OPTIMAL` | `SHADER_READ_ONLY_OPTIMAL` | After upload, ready for sampling |
| `SHADER_READ_ONLY_OPTIMAL` | `GENERAL` | Compute read/write |

## Stream auto-barriers

`Stream` inserts a `COMPUTE_SHADER_BIT` barrier covering all tracked buffers between consecutive dispatches. This is conservative — it covers all buffers, not just overlapping ones. For fully independent pipelines use separate `Stream` objects.

## Simulation-specific patterns

**Ping-pong buffers** (double buffering for iterative solvers):
```cpp
Tensor<float> a(N), b(N);
for (int step = 0; step < steps; step++) {
    stream.dispatch(solver_kernel, (step % 2 == 0 ? a : b), (step % 2 == 0 ? b : a));
    stream.sync();
}
```

**Multi-stage pipeline** (e.g., integrate → collision → resolve):
```cpp
stream.dispatch(integrate_k,  positions, velocities);
stream.dispatch(collision_k,  positions, collisions);  // barrier auto-inserted
stream.dispatch(resolve_k,    collisions, positions);  // barrier auto-inserted
stream.sync();
```
