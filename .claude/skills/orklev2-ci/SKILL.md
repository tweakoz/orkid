---
name: orklev2-ci
description: Answer questions about orkid's ComputeInterface (CI), compute shader dispatch, indirect dispatch, SSBO binding for compute, memory barriers, image/sampler binding, dispatch phases. Use when the user asks about compute shaders, GPU compute, or dispatch.
user-invocable: false
---

# Orkid ComputeInterface (CI) Reference

When answering questions about compute shaders or GPU compute in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| CI Interface | `inc/ork/lev2/gfx/ci.h` (lines 8–44) |
| FxComputeShader | `inc/ork/lev2/gfx/shadman.h` |
| Vulkan CI | `src/gfx/vulkan/headers/vulkan_ctx.h` (lines 584–619) |
| Python Bindings | `pyext/src/pyext_gfx.cpp` |

## CI Interface (ci.h:8–44)

### ImageBindAccess Enum
- `EIBA_READ_ONLY = 0`
- `EIBA_WRITE_ONLY = 1`
- `EIBA_READ_WRITE = 2`

### Dispatch Phase Management
- `beginDispatchPhase()` — suspends active render pass if needed
- `endDispatchPhase()` — resumes render pass

### Compute Dispatch
- `dispatchCompute(shader, numgroups_x, numgroups_y, numgroups_z)` — direct dispatch
- `dispatchComputeIndirect(shader, indirect_ptr)` — indirect dispatch from buffer

### Resource Binding
- `bindStorageBuffer(shader, binding_index, buffer)` — bind SSBO to compute shader
- `bindImage(shader, binding_index, texture, access)` — bind image for load/store
- `bindSampler(shader, binding_index, texture)` — bind texture sampler

### Memory Barriers
- `storageBarrier()` — ensures prior SSBO writes are visible to subsequent dispatches

## Typical Usage

```cpp
auto* CI = ctx->CI();
auto* FXI = ctx->FXI();

// Get compute shader from loaded FxShader
auto* cs = FXI->computeShader(shader, "cs_blur");

CI->beginDispatchPhase();

// Bind resources
CI->bindStorageBuffer(cs, 0, input_ssbo);
CI->bindStorageBuffer(cs, 1, output_ssbo);
CI->bindImage(cs, 2, texture, ComputeInterface::EIBA_WRITE_ONLY);

// Dispatch workgroups
CI->dispatchCompute(cs, groups_x, groups_y, groups_z);

// Barrier between passes
CI->storageBarrier();

// Second dispatch can read first dispatch's output
CI->dispatchCompute(cs2, groups_x, groups_y, groups_z);

CI->endDispatchPhase();
```

## Vulkan Implementation Notes

The Vulkan CI (`VkComputeInterface`) handles:
- Suspending/resuming active render passes for compute dispatch
- Pipeline binding for compute shaders (separate from graphics pipeline)
- Descriptor set binding for compute resources
- `vkCmdDispatch` / `vkCmdDispatchIndirect` calls
- `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` barriers

## How to Answer

1. For CI methods: check `ci.h:8–44`
2. For FxComputeShader: check `shadman.h`
3. For SSBO creation/mapping: consult **orklev2-fxi** skill (FXI manages buffer creation)
4. For Vulkan details: check `vulkan_ctx.h:584–619`
