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

  // Dispatch phase management - suspends render pass if active, resumes on end.
  //
  // `label` NAMES the phase for the always-on per-pass GPU timer (gpupassstats.h):
  // a labeled OUTERMOST phase gets a GPU timestamp bracket around its command
  // buffer and a row on the HUD's GPU page. nullptr (the default) = untimed, which
  // is what the offline cook loops must stay: an erosion/relax node opens and closes
  // hundreds of phases per frame and would spend the whole per-frame slice budget on
  // work no frame is waiting for. Label the phases a RENDERED FRAME pays for.
  virtual void beginDispatchPhase(const char* label = nullptr) {}
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

  // Record a compute dispatch DIRECTLY onto the FRAME's primary command buffer (NOT the dedicated
  // compute CB), immediately followed by a memory barrier making its writes visible to a later
  // INDIRECT draw command + mesh/vertex-stage SSBO reads recorded into the same frame CB. The
  // dispatched work thus rides the ONE frame submit — no separate vkQueueSubmit / fence-wait, which
  // on MoltenVK is the whole per-frame cost of a GPU compaction feeding an indirect draw. Bindings
  // are recorded the usual way (bindStorageBuffer / bindStorageBufferOnBlock) BEFORE this call,
  // exactly as for dispatchCompute. Preconditions (fail loud): mid-frame (primary CB recording) and
  // OUTSIDE any render pass — i.e. the compute-legal preRender point, before the draw's render pass.
  virtual void dispatchComputeInline(const FxComputeShader* shader,
                                     uint32_t numgroups_x,
                                     uint32_t numgroups_y,
                                     uint32_t numgroups_z) {}

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
