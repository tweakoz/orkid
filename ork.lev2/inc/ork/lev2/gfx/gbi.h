////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2 {

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Geometry Buffer Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class GeometryBufferInterface {

public:
  GeometryBufferInterface(Context& ctx);
  virtual ~GeometryBufferInterface();

  void BeginFrame();
  void EndFrame();

  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface

  virtual void copyTensorIntoVertexBuffer(VertexBufferBase& vbuf, torchtensor_ptr_t tensor) { }

  void FlushVB(VertexBufferBase& VBuf);

  //////////////////////////////////
  virtual void* LockVB(VertexBufferBase& VBuf, int ivbase = 0, int icount = 0) = 0;
  virtual void UnLockVB(VertexBufferBase& VBuf)                                = 0;

  virtual const void* LockVB(const VertexBufferBase& VBuf, int ivbase = 0, int icount = 0) = 0;
  virtual void UnLockVB(const VertexBufferBase& VBuf)                                      = 0;

  virtual void ReleaseVB(VertexBufferBase& VBuf) = 0; // e release memory

  ///////////////////////////////////////////////////////

  void DrawPrimitive(
      GfxMaterial* mtl,
      const VtxWriterBase& VW, //
      PrimitiveType eType,
      int icount = 0);

  void DrawPrimitive(
      GfxMaterial* mtl,
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase           = 0,
      int ivcount          = 0);

  void DrawIndexedPrimitive(
      GfxMaterial* mtl,
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType);

  ///////////////////////////////////////////////////////

  virtual void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase           = 0,
      int ivcount          = 0) = 0;

  virtual void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType,
      int ivbase           = 0,
      int ivcount          = 0) = 0;

  virtual void DrawIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType) = 0;

  virtual void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count) = 0;

  virtual void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count,
      size_t first_instance) = 0;

  //////////////////////////////////////////////
  // GPU-driven indirect draws: the draw count (instance/vertex count) is NOT supplied by the
  // CPU — it is read at draw time from `indirect_args`, a storage buffer a compute shader wrote
  // (e.g. a cull pass that compacted survivors, or a generator that amplified geometry). No CPU
  // readback / stall. `indirect_args` must be created with INDIRECT usage (createStorageBuffer
  // grants it). args_offset is the byte offset of the command within that buffer.
  //////////////////////////////////////////////

  // (1) fixed-mesh instanced, INDEXED, indirect. Geometry from bound vertex+index buffers
  // (vertex-cache friendly, reused across instances); instanceCount comes from the GPU buffer.
  // args = VkDrawIndexedIndirectCommand. The per-instance data (matrices) is the usual instance
  // SSBO the material binds; the VS indexes it by gl_InstanceIndex.
  virtual void DrawInstancedIndexedPrimitiveIndirectEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0) = 0;

  // (2) SSBO vertex-pull, NON-indexed, indirect. No vertex/index buffers — the VS reads vertices
  // from a storage block via gl_VertexIndex (cf. the DrawPrimitiveEML(SSBO,...) overload).
  // args = VkDrawIndirectCommand (vertexCount/instanceCount/firstVertex/firstInstance).
  virtual void DrawIndirectEML(
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0) = 0;

  // (3) SSBO vertex-pull, INDEXED, indirect. Vertices pulled from a storage block (gl_VertexIndex);
  // indices come from a compute-written storage buffer bound as the index buffer (createStorageBuffer
  // grants INDEX usage). For compute-generated geometry (L-systems) that wants vertex reuse.
  // args = VkDrawIndexedIndirectCommand. index_size: 2=uint16, 4=uint32 (compute-gen default).
  virtual void DrawIndexedIndirectEML(
      const FxShaderStorageBuffer* index_buffer,
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0,
      int index_size = 4) = 0;

  virtual void* LockIB(IndexBufferBase& VBuf, int ibase = 0, int icount = 0) = 0;
  virtual void UnLockIB(IndexBufferBase& VBuf)                               = 0;

  virtual const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0) = 0;
  virtual void UnLockIB(const IndexBufferBase& VBuf)                                     = 0;

  virtual void ReleaseIB(IndexBufferBase& VBuf) = 0;

  void DrawPrimitiveEML(const VtxWriterBase& VW, PrimitiveType eType, int icount = 0);
  
  void render2dQuadEML(fvec4 quadrect=fvec4(-1, -1, 2, 2), //
                       fvec4 uvrecta=fvec4(0, 0, 1, 1), //
                       fvec4 uvrectb=fvec4(0, 0, 1, 1), //
                       float depth = 0.0f );

  void render2dQuadEMLCCL(fvec4 quadrect=fvec4(-1, -1, 2, 2), //
                       fvec4 uvrecta=fvec4(0, 0, 1, 1), //
                       fvec4 uvrectb=fvec4(0, 0, 1, 1), //
                       float depth = 0.0f );

  //////////////////////////////////////////////
  // nvidia mesh shaders
  //////////////////////////////////////////////

#if !defined(__APPLE__)
  virtual void DrawMeshTasksNV(uint32_t first, uint32_t count) {
  }

  virtual void DrawMeshTasksIndirectNV(int32_t* indirect) {
  }

  virtual void MultiDrawMeshTasksIndirectNV(int32_t* indirect, uint32_t drawcount, uint32_t stride) {
  }

  virtual void MultiDrawMeshTasksIndirectCountNV(int32_t* indirect, int32_t* drawcount, uint32_t maxdrawcount, uint32_t stride) {
  }
#endif

  //////////////////////////////////////////////

  int GetNumTrianglesRendered(void) {
    return miTrianglesRendered;
  }

  bool _debugNextPrimitive = false;
  
protected:
  int miTrianglesRendered;
  Context& _context;

private:
  virtual void _doBeginFrame() {
  }
  virtual void _doEndFrame() {
  }
};

} // namespace ork::lev2