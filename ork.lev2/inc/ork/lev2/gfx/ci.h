#pragma once

namespace ork::lev2 {

class VertexBufferBase;

enum ImageBindAccess {
  EIBA_READ_ONLY = 0,
  EIBA_WRITE_ONLY = 1,
  EIBA_READ_WRITE = 2
};

struct ComputeInterface {

  ComputeInterface() {}
  virtual ~ComputeInterface() {}

  // Dispatch phase management - suspends render pass if active, resumes on end
  virtual void beginDispatchPhase() {}
  virtual void endDispatchPhase() {}

  virtual void dispatchCompute( const FxComputeShader* shader,
                                uint32_t numgroups_x,
                                uint32_t numgroups_y,
                                uint32_t numgroups_z ) {}


  // GPU-driven dispatch: group counts come from a VkDispatchIndirectCommand (3 uint32: x,y,z)
  // at `args_offset` within a GPU-written SSBO (DEFAULT-usage storage buffers already carry
  // INDIRECT usage) — the compute analogue of DrawIndexedIndirectEML. Lets a pipeline size its
  // next pass off GPU-side counts (scan totals, live edge/face counts) with NO readback.
  // `args_offset` must be 4-byte aligned.
  virtual void dispatchComputeIndirect(const FxComputeShader* shader, FxShaderStorageBuffer* args, size_t args_offset = 0) {}

  virtual void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) {}

  // Auto-resolving bind: look up `block`'s reflected SPIR-V binding within `shader` (by name)
  // and bind there — so callers never hardcode an index that must match the merged binding id
  // (which is non-obvious when the shader shares storage with a graphics technique). Distinct
  // name (not an overload of the index form) so a literal-0 index can't bind ambiguously.
  virtual void bindStorageBufferOnBlock(const FxComputeShader* shader, FxShaderStorageBuffer* buffer, const FxShaderStorageBlock* block) {}

  // monotonically accumulating seconds the HOST spent blocked on GPU completion (the fence waits
  // in endDispatchPhase / syncPendingDispatch). Perf instrumentation samples deltas around a
  // workload to split host-wall into CPU vs GPU-wait (see hypermesh HmPerf).
  double _gpuWaitAccum = 0.0;

  // Insert a shader storage (SSBO) memory barrier within an active dispatch phase.
  // Ensures all SSBO writes from prior dispatches are visible to subsequent dispatches
  // in the same command buffer. Must be called between dependent compute passes.
  virtual void storageBarrier() {}

  // GPU buffer-to-buffer copy within a dispatch phase.
  // Copies `size` bytes from src SSBO at `src_offset` to dst SSBO at `dst_offset`.
  virtual void copyBufferRegion(
      FxShaderStorageBuffer* src, size_t src_offset,
      FxShaderStorageBuffer* dst, size_t dst_offset,
      size_t size) {}

  // GPU copy from SSBO to vertex buffer within a dispatch phase.
  virtual void copySSBOToVertexBuffer(
      FxShaderStorageBuffer* src, size_t src_offset,
      VertexBufferBase* dst_vb, size_t dst_offset,
      size_t size) {}


  virtual void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) {}

  virtual void bindSampler(const FxComputeShader* shader, uint32_t binding_index, Texture* tex) {}

};

} //namespace ork::lev2 {
