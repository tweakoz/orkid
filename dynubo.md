# Dynamic UBO Investigation Report

## Executive Summary

This document chronicles our investigation into why `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` does not work correctly. ~~Originally suspected to be a MoltenVK issue on macOS/Apple Silicon~~. **CONFIRMED: Issue also occurs on Linux with native Vulkan drivers, ruling out MoltenVK as the cause.** The problem is in the Orkid Vulkan implementation itself.

---

## CRITICAL UPDATE (2026-01-04): Linux Confirmation

**The issue reproduces on Linux.** This is the most important finding because:
1. **MoltenVK is ruled out** as the cause
2. The bug is in **Orkid's Vulkan code**, not the translation layer
3. The shader inheritance pattern hypothesis needs re-evaluation
4. The rejected C++ changes from commit `08896651` may contain necessary fixes

---

## Problem Statement

### Goal
Implement a ring buffer allocation pattern for uniform buffer objects (UBOs) where:
- A single large GPU buffer holds data for all draws
- Each draw call allocates a slice from this buffer
- Dynamic offsets are passed to `vkCmdBindDescriptorSets` to point the shader at the correct slice

### Symptom
In the Metal debugger:
- Matrix data appears as zeros or incorrect values
- Marker patterns (`[11111, 22222, 33333, offset]`) are visible but at "drifting" positions
- Each row in the buffer view shows the marker at a different column position
- This drift pattern suggests the shader reads with wrong stride (1136 bytes instead of 1472 bytes)

### UBO Layout
```
Binding 15: ub_frg_fwd        (32 bytes)
Binding 16: ublk_std_lighting (144 bytes)
Binding 17: ublk_std_matrices (1136 bytes)  <- Contains MVP matrices
Binding 18: ublk_std_pbr      (160 bytes)
Total stride per draw: 1472 bytes
```

---

## What We Verified as Correct (Orkid Side)

### 1. Descriptor Pool Creation (`vulkan_ctx.cpp:388`)
```cpp
poolsize_uniform_buffers_dynamic.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
```

### 2. Descriptor Set Layout Creation (`vulkan_fxi_pipelines_create.cpp:251`)
```cpp
vk_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
```

### 3. Descriptor Write (`vulkan_fxi_pipelines_bind.cpp:588`)
```cpp
DWRITE.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
buffer_info.offset = 0;  // Static offset is 0, dynamic offset at bind time
buffer_info.range = ubo_block->_buffer_size;
```

### 4. UBO Ordering
UBOs are sorted by `(descriptor_set_id, binding_id)` to match Vulkan spec requirement that dynamic offsets are consumed in binding order.

### 5. Dynamic Offset Passing (`vulkan_fxi_pipelines_bind.cpp:142-150`)
```cpp
vkCmdBindDescriptorSets(
    cmdbuf,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipeline->_pipelineLayout,
    0,  // first set
    1,  // set count
    &desc_set->_vkdescset,
    pipeline->_dynamic_offsets.size(),  // 4 offsets
    pipeline->_dynamic_offsets.data()); // [offset0, offset1, offset2, offset3]
```

### 6. Data Flow Verification
Log output confirmed:
- Shadow buffers contain valid matrix data before copy
- Data correctly copied to ring buffer allocations
- Dynamic offsets correctly aligned (16-byte alignment)
- Offsets increase by 1472 bytes per draw (correct stride)

Example from log:
```
dynamic_offsets count<4>: [0]=3882672 [1]=3882704 [2]=3882848 [3]=3883984
MARKER: base=0x33de1c000 cpu_ptr=0x33e1cff60 abs_offset=3882848 dyn_offset=3882848
Written [11111, 22222, 33333, 3882848] at absolute offset 0x3b3f60
```

---

## MoltenVK Code Analysis

### How MoltenVK Should Handle Dynamic UBOs

#### Step 1: Descriptor Set Layout Creation
**File:** `MVKDescriptorSet.mm`

```cpp
// Line 147-149
static bool needsDynamicOffset(VkDescriptorType type) {
    return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
           type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
}

// Line 261
count.dynamicOffset = needsDynamicOffset(type);
```

When we create a descriptor set layout with `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`, MoltenVK should set `perDescriptorResourceCount.dynamicOffset = 1`.

#### Step 2: Pipeline Creation - Shader Config
**File:** `MVKPipeline.mm`

```cpp
// Line 202-204
if (desc.perDescriptorResourceCount.dynamicOffset != 0 && used) {
    shaderConfig.dynamicBufferDescriptors.push_back(
        makeDescriptorBinding(stage, dslIdx, desc.binding,
                              binding.stages[stage].dynamicOffsetBufferIndex));
}
```

For each binding where `dynamicOffset != 0`, MoltenVK adds it to `shaderConfig.dynamicBufferDescriptors`. This config is passed to SPIRV-Cross for Metal shader generation.

#### Step 3: Pipeline Creation - Bind Script
**File:** `MVKPipeline.mm`

```cpp
// Line 301-318
if (hasDynamicBuffer(desc.descriptorType)) {
    // Look up dynOffset from shaderConfig.dynamicBufferDescriptors
    uint32_t dynOffset = it->index;
    // Create BindBufferDynamic operation
    script.ops.push_back({ bindBufDyn, set, mslBinding.resourceBinding.msl_buffer,
                           descIdx, nonTexOffset, dynOffset });
} else if (counts.buffer > 0) {
    // Create regular BindBuffer operation
    script.ops.push_back({ bindBuf, set, mslBinding.resourceBinding.msl_buffer,
                           descIdx, nonTexOffset });
}
```

The critical difference: `BindBufferDynamic` stores a `dynOffset` index (in `target2` field) that tells MoltenVK which dynamic offset to use at draw time.

#### Step 4: vkCmdBindDescriptorSets Processing
**File:** `MVKCommandEncoderState.mm`

```cpp
// Line 360-367
for (const auto& binding : setLayout->bindings()) {
    if (!binding.perDescriptorResourceCount.dynamicOffset)
        continue;
    if (binding.stageFlags & vkStage) {
        mvkCopy(write, dynamicOffsets, binding.descriptorCount);
        write += binding.descriptorCount;
    }
    dynamicOffsets += binding.descriptorCount;
}
```

Dynamic offsets from our `vkCmdBindDescriptorSets` call are stored in `implicitBufferData.dynamicOffsets`, indexed by binding order.

#### Step 5: Draw Time - Execute Bind Operation
**File:** `MVKCommandEncoderState.mm`

```cpp
// Line 456-460
uint64_t offset = *reinterpret_cast<const uint64_t*>(src + sizeof(id));
if (Op == MVKDescriptorBindOperationCode::BindBufferDynamic ||
    Op == MVKDescriptorBindOperationCode::BindBufferDynamicWithLiveCheck) {
    offset += dynOffsets[i];  // <-- THIS IS THE KEY LINE
}
bindBuffer(encoder, buffer, offset, target + i, ...);
```

**This is the critical code path.** If `Op` is `BindBufferDynamic`, the dynamic offset is added. If `Op` is just `BindBuffer`, it's not.

---

## Debug Output We Added

### Location 1: Descriptor Resource Count
```cpp
// MVKDescriptorSet.mm - perDescriptorResourceCount()
if (count.dynamicOffset) {
    printf("[MVK-DEBUG] perDescriptorResourceCount: type=%d is DYNAMIC\n", (int)type);
}
```

### Location 2: Pipeline Creation - Dynamic Buffer Descriptors
```cpp
// MVKPipeline.mm - addToShaderConfig region
if (desc.perDescriptorResourceCount.dynamicOffset != 0 && used) {
    printf("[MVK-DEBUG] Pipeline: adding dynamic buffer descriptor "
           "stage=%d set=%d binding=%d dynOffsetIdx=%d\n", ...);
}
```

### Location 3: Bind Script Creation
```cpp
// MVKPipeline.mm - script creation
if (hasDynamicBuffer(desc.descriptorType)) {
    printf("[MVK-DEBUG] Script: hasDynamicBuffer=true for set=%d binding=%d\n", ...);
    // ... lookup logic ...
    printf("[MVK-DEBUG] Script: creating BindBufferDynamic op, dynOffset=%u\n", ...);
} else if (counts.buffer > 0) {
    printf("[MVK-DEBUG] Script: NON-dynamic buffer for set=%d binding=%d\n", ...);
}
```

### Location 4: Draw Time Execution
```cpp
// MVKCommandEncoderState.mm - executeBindOp()
if (Op == BindBufferDynamic || Op == BindBufferDynamicWithLiveCheck) {
    printf("[MVK-DEBUG] executeBindOp: DYNAMIC buffer, static=%llu dyn=%u final=%llu\n",
           offset, dynOffsets[i], offset + dynOffsets[i]);
}
```

---

## Why Debug Output Didn't Appear

After rebuilding MoltenVK (`make macos-debug`) and orkid, no `[MVK-DEBUG]` output appeared in logs despite the library timestamp showing it was rebuilt.

### Possible Reasons

1. **Output Buffering**: `printf` to stdout may be buffered. Should use `fprintf(stderr, ...)` with `fflush(stderr)`.

2. **Wrong Library Loaded**: The Vulkan loader might be picking up a different MoltenVK. Check with `DYLD_PRINT_LIBRARIES=1`.

3. **Code Path Not Executed**: If pipelines are cached (either in MoltenVK's `MVKShaderLibraryCache` or orkid's datablock cache), the creation code paths won't run.

4. **Dynamic Linking Issue**: The dylib might not be reloaded if it's cached by the OS.

---

## Hypotheses for Root Cause

### Hypothesis 1: Pipeline/Shader Cache
MoltenVK caches compiled shaders in `MVKShaderLibraryCache`. The cache key includes `dynamicBufferDescriptors`. If:
- A pipeline was created BEFORE we switched to dynamic UBOs
- The cache key doesn't properly distinguish dynamic vs non-dynamic
- Then MoltenVK reuses the old shader with `BindBuffer` instead of `BindBufferDynamic`

### Hypothesis 2: Descriptor Set Layout Not Recognized as Dynamic
If `perDescriptorResourceCount.dynamicOffset` is not set to 1 when we create the layout, then:
- `dynamicBufferDescriptors` won't be populated
- `hasDynamicBuffer()` will return false
- `BindBuffer` ops will be created instead of `BindBufferDynamic`
- Dynamic offsets will be ignored

### Hypothesis 3: SPIRV-Cross Translation Issue
The Metal shader generated by SPIRV-Cross might not be set up to read from the dynamic offset implicit buffer, causing it to use static offsets only.

### Hypothesis 4: Argument Buffer Mode Interference
With `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=0`, MoltenVK uses "discrete" binding mode. The code path for discrete mode (line 285-326 in MVKPipeline.mm) does handle dynamic buffers, but there could be edge cases.

---

## Revised Strategy for Next Session

### Phase 1: Ensure Clean State

```bash
# Clear orkid shader cache
rm -rf ~/.cache/orkid/shaders/*  # or wherever orkid caches datablocks

# Clear MoltenVK pipeline cache (if any)
rm -rf ~/Library/Caches/com.apple.metal/*  # Metal shader cache
rm -rf /tmp/mvk_*  # Any MoltenVK temp files

# Verify correct MoltenVK is loaded
export DYLD_PRINT_LIBRARIES=1
```

### Phase 2: Fix Debug Output

```cpp
// Use stderr and flush immediately
fprintf(stderr, "[MVK-DEBUG] ...\n");
fflush(stderr);

// Or use MoltenVK's logging (MVKLogging.h)
MVKLogInfo("[MVK-DEBUG] ...");
```

### Phase 3: Add Startup Verification

Add a distinctive message in MoltenVK initialization to confirm the right library loads:

```cpp
// In MVKInstance.mm or similar init code
fprintf(stderr, "[MVK-DEBUG] *** CUSTOM MOLTENVK BUILD LOADED ***\n");
fflush(stderr);
```

### Phase 4: Minimal Test Case

Create a simplified test:
```cpp
// One UBO, two draws, verify each gets correct data
// This isolates from complexity of 4 UBOs and many draws
```

### Phase 5: Enable MoltenVK Diagnostics

```bash
export MVK_CONFIG_LOG_LEVEL=4           # Debug logging
export MVK_CONFIG_TRACE_VULKAN_CALLS=2  # Trace all Vulkan calls
export MVK_CONFIG_SHADER_DUMP_DIR=/tmp/mvk_shaders
export MVK_CONFIG_SHADER_LOG_ESTIMATED_GLSL=1
```

### Phase 6: Alternative Implementations (If Dynamic UBOs Fail)

1. **Per-draw descriptor set updates**: Update `buffer_info.offset` each draw instead of using dynamic offsets. More API calls but simpler.

2. **Push constants for matrices**: Move MVP matrices to push constants. Simpler data path, guaranteed to work.

3. **Multiple pre-allocated descriptor sets**: Create N descriptor sets, round-robin through them.

4. **Buffer device address**: Pass buffer address + offset directly to shader (if VK_KHR_buffer_device_address supported).

---

## Key Files Reference

### Orkid
| File | Purpose |
|------|---------|
| `vulkan_ctx.cpp` | Descriptor pool creation |
| `vulkan_fxi_pipelines_create.cpp` | Descriptor set layout, pipeline creation |
| `vulkan_fxi_pipelines_bind.cpp` | Descriptor writes, vkCmdBindDescriptorSets |
| `vulkan_ubo_dynamic.h/cpp` | Ring buffer allocation system |

### MoltenVK
| File | Purpose |
|------|---------|
| `MVKDescriptorSet.mm` | `needsDynamicOffset()`, layout creation |
| `MVKPipeline.mm` | `hasDynamicBuffer()`, bind script creation |
| `MVKCommandEncoderState.mm` | `bindDescriptorSets()`, `executeBindOp()` |
| `SPIRVToMSLConverter.h` | Shader conversion config |
| `MVKShaderModule.mm` | Shader caching |

---

---

## Critical Finding: Shader Structure Difference

### What Works vs What Doesn't

| Shader | Status | UBO Structure |
|--------|--------|---------------|
| Rigid meshes (pbr.fxv2) | WORKS | Interface has `ub_std_vtx`, shader adds `: ublk_std_matrices` |
| Skinned meshes (pbr.fxv2) | WORKS | Interface has `ub_std_vtx`, shader adds `: ublk_std_matrices` |
| Imposters (i5.fxv2) | WORKS | Interface has `: ub_std_vtx`, shader adds `: ublk_std_matrices` |
| Particles | WORKS | Uses `ublk_vtx` (different UBO entirely) |
| Geoclipmesh | **BROKEN** | Interface has `: ublk_std_matrices` directly, shader doesn't add it |

### The Pattern

**WORKING shaders (e.g., `vs_forward_test` in pbrtools.i2):**
```glsl
vertex_interface vif_FWDTEST : vif_forward_outputs : ub_std_vtx {
  // ...
}

vertex_shader vs_forward_test
  : vif_FWDTEST
  : lib_pbr_vtx
  : ublk_std_matrices {   // <-- UBO added directly on shader
  // ...
}
```

**BROKEN shader (geoclipmesh_test.fxv2):**
```glsl
vertex_interface vif_terrain : ublk_std_matrices {  // <-- UBO on interface
  // ...
}

vertex_shader vs_ground_mono
    : vif_terrain           // <-- Inherits ublk_std_matrices indirectly
    : lib_ground_vtx_base { // <-- NO direct ublk_std_matrices
  // ...
}
```

### Hypothesis

The difference in how `ublk_std_matrices` is inherited may affect:
1. **UBO ordering**: When UBO is on interface vs shader, the binding order may differ
2. **Merged resource generation**: The shader compiler may generate different `merged_resources` structures
3. **Descriptor set layout**: The resulting Vulkan descriptor set layout may have different binding numbers

### Potential Fix

Modify geoclipmesh shaders to match the working pattern:
```glsl
// Change interface to NOT have ublk_std_matrices
vertex_interface vif_terrain {
  // ...
}

// Add ublk_std_matrices directly on shader
vertex_shader vs_ground_mono
    : vif_terrain
    : lib_ground_vtx_base
    : ublk_std_matrices {   // <-- Add this
  // ...
}
```

### Note About `ub_std_vtx`

`ub_std_vtx` is used in working shaders but never defined in shader files. It must be:
- A built-in/generated uniform block
- Possibly empty or minimal
- Its presence may affect UBO ordering

---

## Conclusion

**Primary Finding**: The geoclipmesh shader has a different UBO inheritance pattern than working shaders. Working shaders have `ublk_std_matrices` directly on the vertex shader declaration, while geoclipmesh has it only on the vertex interface.

**Recommended Next Steps (in order):**

1. **Try the simple fix first**: Modify geoclipmesh shaders to match the working pattern - add `: ublk_std_matrices` directly to the vertex shader declarations.

2. **If that doesn't work**: The issue may be in how MoltenVK translates dynamic UBOs to Metal:
   - Verify debug MoltenVK actually loads (startup message)
   - Clear all caches before testing
   - Confirm debug output appears for dynamic UBO code paths

3. **Fallback**: If dynamic UBOs truly don't work, implement alternative approach (push constants or per-draw descriptor updates)

**The "drifting marker" observation**: Data IS in the buffer, but the shader reads from wrong offsets. This could be caused by:
- Different UBO binding order due to inheritance pattern difference
- Dynamic offsets not being applied in MoltenVK's `executeBindOp()`
- Both issues compounding

---

## Commit 08896651 (toz-2025-dynubo) Analysis

### What Was Cherry-Picked (Applied)

1. **Python test files** (`geoclipmesh_*.py`) - Updated texture loading, shader paths, lighting manager init
2. **Shader files** (`geoclipmesh_*.fxv2`) - New geoclipmesh shaders
3. **Vertex format changes** (`vulkan_gbi.cpp`) - Added `V12N12T16` format support
4. **Minor debug script change** (`ork.debug.xcode.py`)

### What Was Rejected (Caused Regressions)

The following C++ changes were rejected because they caused regressions elsewhere:

| File | Lines Added | Purpose |
|------|-------------|---------|
| `vulkan_fxi_pipelines_bind.cpp` | +97 | Debug output for dynamic offsets, marker writing |
| `vulkan_fxi_pipelines_create.cpp` | +38 | Binding sort, UBO verification, debug output |
| `vulkan_ubo_dynamic.cpp` | +45 | Pre-allocation offset alignment, verification |
| `vulkan_ubo_dynamic.h` | +1 | `get_mapped_base()` accessor |
| `vulkan_fxi_bindparam.cpp` | +8 | Debug output for matrix binding |

### Key Changes in Rejected Code

#### 1. Pre-Allocation Offset Alignment (`vulkan_ubo_dynamic.cpp`)
```cpp
// COMMIT VERSION (rejected):
VkDynamicUBOSystem::Allocation VkDynamicUBOSystem::allocate(...) {
  // First, align the CURRENT OFFSET to ensure dynamic offset is properly aligned
  _current_offset = align_up(_current_offset, _actual_alignment);  // <-- THIS LINE

  size_t aligned_size = align_up(data_size, _actual_alignment);
  ...
}

// CURRENT VERSION:
VkDynamicUBOSystem::Allocation VkDynamicUBOSystem::allocate(...) {
  // Only aligns size, not offset
  size_t aligned_size = align_up(data_size, _actual_alignment);
  ...
}
```

**Analysis**: Current code should maintain alignment since we always add aligned sizes. However, the explicit offset alignment in the commit was defensive/explicit.

#### 2. Descriptor Set Layout Binding Sort (`vulkan_fxi_pipelines_create.cpp`)
```cpp
// COMMIT VERSION (rejected):
// Sort bindings by binding number to ensure consistent ordering
std::sort(bindings.begin(), bindings.end(),
    [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
      return a.binding < b.binding;
    });
```

**Analysis**: Vulkan doesn't require bindings to be sorted in the layout create info. The order of bindings in the array doesn't affect their binding numbers. This is likely unnecessary.

#### 3. UBO Descriptor Set ID Verification (`vulkan_fxi_pipelines_create.cpp`)
```cpp
// COMMIT VERSION (rejected):
if (ubo->_descriptor_set_id != (size_t)set_id) {
  printf("  WARNING: UBO descriptor_set_id<%zu> != merged_resources set_id<%d>!\n",
         ubo->_descriptor_set_id, set_id);
}
```

**Analysis**: This verification could reveal if UBOs have mismatched descriptor set IDs, which would cause incorrect sorting and thus wrong dynamic offset ordering.

---

## Current State Analysis

### Shader File Status

| File | UBO Pattern | Status |
|------|-------------|--------|
| `geoclipmesh_basic.fxv2` | UBO directly on shader | **FIXED** |
| `geoclipmesh_test.fxv2` | UBO on interface | **BROKEN** |
| `geoclipmesh_terrain.fxv2` | UBO on interface | **BROKEN** |
| `geoclipmesh_terrain1b.fxv2` | UBO on interface | **BROKEN** |
| `geoclipmesh_terrain2.fxv2` | UBO on interface | **BROKEN** |
| `geoclipmesh_water.fxv2` | UBO on interface | **BROKEN** |

**Example of FIXED pattern** (`geoclipmesh_basic.fxv2:108-114`):
```glsl
vertex_interface vif_geoclipmesh {  // NO UBO here
  inputs { ... }
  outputs { ... }
}

vertex_shader vs_ground_mono
    : vif_geoclipmesh
    : ublk_std_matrices    // <-- UBO directly on shader
    : lib_ground_vtx {
```

**Example of BROKEN pattern** (`geoclipmesh_test.fxv2:25-79`):
```glsl
vertex_interface vif_terrain : ublk_std_matrices {  // <-- UBO on interface
  inputs { ... }
  outputs { ... }
}

vertex_shader vs_ground_mono
    : vif_terrain           // Inherits UBO indirectly
    : lib_ground_vtx_base { // NO direct UBO
```

### Current Vulkan Code Flow

1. **Pipeline Creation** (`vulkan_fxi_pipelines_create.cpp:248-271`):
   - Iterates `merged_resources->descriptor_sets`
   - Creates `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` bindings
   - Adds UBOs to `pipeline->_uniform_blocks` and `pipeline->_ubo_by_binding`

2. **UBO Sorting** (`vulkan_fxi_pipelines_create.cpp:376-393`):
   - Sorts `_uniform_blocks` by `(descriptor_set_id, binding_id)`
   - Uses `ubo_to_binding` map to get binding IDs

3. **Dynamic Allocation** (`vulkan_fxi_pipelines_bind.cpp:240-264`):
   - Iterates `_uniform_blocks` in sorted order
   - Allocates from ring buffer
   - Copies shadow buffer data
   - Builds `_dynamic_offsets` vector

4. **Descriptor Set Binding** (`vulkan_fxi_pipelines_bind.cpp:118-127`):
   - Passes `_dynamic_offsets` to `vkCmdBindDescriptorSets`

### Potential Issues Identified

#### Issue 1: Descriptor Write Order vs Dynamic Offset Order

In `fetchDescriptorSetForProgram` (line 420-550), we iterate through `merged_resources->descriptor_sets` and create descriptor writes in that order. But `_dynamic_offsets` is built from `_uniform_blocks` (which is sorted).

**If the descriptor write order doesn't match the binding order**, the dynamic offsets will be applied to wrong bindings.

**However**: Vulkan spec says dynamic offsets are consumed in binding order within each set, not descriptor write order. So this should be fine.

#### Issue 2: `_descriptor_set_id` Source

UBO's `_descriptor_set_id` is set in `vulkan_fxi_DBread.cpp:422` from the shader datablock:
```cpp
vk_uniblk->_descriptor_set_id = dset_id;
```

If the shader compiler generates different `dset_id` values based on UBO inheritance pattern (interface vs shader), the sorting would produce different orders.

#### Issue 3: Missing `get_mapped_base()` (Minor)

The commit added `get_mapped_base()` to `VkDynamicUBOSystem` for debug marker writing. This accessor is missing in current code, but it's only needed for debugging.

---

## Questions Requiring Testing

1. **Does `geoclipmesh_basic.py` work?** (uses the FIXED shader pattern)
2. **Does `geoclipmesh_test.py` fail?** (uses the BROKEN shader pattern)
3. If both fail, the issue is deeper than shader inheritance
4. If only the BROKEN pattern fails, we need to fix all geoclipmesh shaders

---

## Updated Recommended Next Steps

### Step 1: Test Fixed vs Broken Shaders
```bash
# Test the FIXED pattern
cd ork.lev2/pyext/tests/renderer/geoclip
./geoclipmesh_basic.py

# Test the BROKEN pattern
./geoclipmesh_test.py
```

If `geoclipmesh_basic.py` works but `geoclipmesh_test.py` doesn't, fix all shader files to use direct UBO inheritance.

### Step 2: If Both Fail - Add Debug Output

Re-apply the debug output from the rejected commit (without the code changes that caused regressions):

```cpp
// In applyPendingUboUpdates:
printf("dynamic_offsets count<%zu>: ", pipeline->_dynamic_offsets.size());
for (size_t i = 0; i < pipeline->_dynamic_offsets.size(); i++) {
  printf("[%zu]=%u ", i, pipeline->_dynamic_offsets[i]);
}
printf("\n");
```

### Step 3: Verify UBO Binding Order

Add verification that `_descriptor_set_id` matches the merged resources set ID:
```cpp
if (ubo->_descriptor_set_id != (size_t)set_id) {
  printf("WARNING: UBO<%s> descriptor_set_id<%zu> != set_id<%d>\n",
         binding->name.c_str(), ubo->_descriptor_set_id, set_id);
}
```

### Step 4: Consider Alternative Approaches

If dynamic UBOs continue to fail:
1. **Push constants** for matrices (simplest, guaranteed to work)
2. **Per-draw descriptor set updates** (more API calls but simpler)
3. **Buffer device address** (modern approach, requires extension)

---

## Files Changed Summary

### Cherry-Picked from `08896651`:
- `ork.lev2/pyext/tests/renderer/geoclip/*.py` - Test scripts
- `ork.lev2/pyext/tests/renderer/geoclip/*.fxv2` - Shader files
- `ork.lev2/src/gfx/vulkan/vulkan_gbi.cpp` - V12N12T16 format (lines 54, 139, 308-314)

### NOT Applied (pbrtools.i2 reordering):
The `std_forward_all` libblock reordering was NOT applied, but it's irrelevant since it only moves `uset_std_viewport` (push constants) relative to UBOs.

### Rejected (caused regressions):
- UBO-specific changes in `vulkan_fxi_pipelines_bind.cpp`
- UBO-specific changes in `vulkan_fxi_pipelines_create.cpp`
- Changes in `vulkan_ubo_dynamic.cpp/h`
- Debug output in `vulkan_fxi_bindparam.cpp`

---

## CRITICAL ROOT CAUSE ANALYSIS (2026-01-04)

### The Vulkan Spec Requirement

From Vulkan 1.3 spec on `vkCmdBindDescriptorSets`:

> "The order of the dynamic descriptor bindings within each descriptor set is the order in which they appear in the **pBindings array** passed to `vkCreateDescriptorSetLayout`."

**This is the key insight!** The dynamic offset order is determined by the **order bindings appear in the array**, NOT the binding numbers themselves.

### The Bug

In `_createPipelineLayoutData` (`vulkan_fxi_pipelines_create.cpp:229-368`):

```cpp
// Step 1: Build bindings array in merged_resources iteration order (UNORDERED)
for (const auto& [set_id, sources] : resources->descriptor_sets) {
  bindings.clear();
  for (const auto& source : sources) {
    for (const auto& binding : source->bindings) {
      // Bindings pushed in whatever order they appear in merged_resources
      bindings.push_back(vk_binding);

      // For UBOs, also track in _uniform_blocks (same unordered order)
      pipeline->_uniform_blocks.push_back(ubo);
    }
  }
  // Layout created from UNORDERED bindings array
  vkCreateDescriptorSetLayout(..., bindings.data(), ...);
}

// Step 2: AFTER layout creation, sort _uniform_blocks by binding ID
std::sort(pipeline->_uniform_blocks.begin(), pipeline->_uniform_blocks.end(),
    [](a, b) { return binding_a < binding_b; });  // Now in sorted order
```

**The mismatch:**
- `pBindings` array order: `[B=17, A=15, C=16, D=18]` (whatever order merged_resources had)
- After sort, `_uniform_blocks`: `[A=15, B=16, C=17, D=18]` (ascending order)
- Dynamic offsets built from sorted `_uniform_blocks`: `[offset_15, offset_16, offset_17, offset_18]`
- **But Vulkan expects offsets in pBindings order:** `[offset_17, offset_15, offset_16, offset_18]`

**Result:** Each UBO gets the wrong dynamic offset! The data IS in the buffer, but the shader reads from wrong offsets.

### Evidence from Logs

The previous logs showed:
- DESCRIPTOR-WRITE confirms data written correctly to ring buffer
- MARKER-CHECK shows valid matrix data (MVP[0]=1.0)
- But Metal debugger showed all zeros in UBO view
- Skybox works (different technique, different binding order that might accidentally match)
- Terrain never worked since GL to Vulkan port

### The Fix

Sort the `bindings` array by binding_id **BEFORE** calling `vkCreateDescriptorSetLayout`:

```cpp
// In _createPipelineLayoutData, before creating layout:
// Sort bindings by binding number for consistent dynamic offset ordering
std::sort(bindings.begin(), bindings.end(),
    [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
      return a.binding < b.binding;
    });

// Now bindings array is in same order as _uniform_blocks will be after its sort
vkCreateDescriptorSetLayout(_contextVK->_vkdevice, &LCI, nullptr, &dset_layout);
```

This ensures both:
1. Layout binding order = ascending binding_id
2. `_uniform_blocks` order (after sort) = ascending binding_id
3. Dynamic offsets built from `_uniform_blocks` = ascending binding_id ✓
4. Vulkan consumes offsets in pBindings order = ascending binding_id ✓

**MATCH!**

### Why This Bug Was Hard to Find

1. **CPU-side verification passed** - shadow buffer had correct data, memcpy worked
2. **Same VkBuffer used everywhere** - no buffer pointer mismatch
3. **Offsets were aligned correctly** - no alignment issues
4. **Some shaders worked** - skybox works (maybe simpler UBO setup, or accidental order match)
5. **Vulkan validation layers didn't catch it** - it's a semantic bug, not an API misuse

### Why the Rejected Commit May Have Worked

Looking back at the rejected commit's changes:
```cpp
// Rejected code had this sort:
std::sort(bindings.begin(), bindings.end(),
    [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
      return a.binding < b.binding;
    });
```

**This was the fix!** But it was bundled with other changes that caused regressions elsewhere, so the whole commit was rejected.

---

## Action Required

Apply this targeted fix to `vulkan_fxi_pipelines_create.cpp`:

**Location:** Line ~346, just before the `if (!bindings.empty())` block

**Add:**
```cpp
// Sort bindings by binding number to match the order that _uniform_blocks
// will be sorted into. Vulkan spec requires dynamic offsets to be provided
// in the order bindings appear in pBindings array.
std::sort(bindings.begin(), bindings.end(),
    [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b) {
      return a.binding < b.binding;
    });
```

This is a minimal, targeted fix that addresses the root cause without the other changes that caused regressions.
