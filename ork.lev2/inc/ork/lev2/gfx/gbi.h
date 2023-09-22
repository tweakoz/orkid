////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2{

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
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase           = 0,
      int ivcount          = 0);

  void DrawIndexedPrimitive(
      GfxMaterial* mtl,
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase           = 0,
      int ivcount          = 0);

  ///////////////////////////////////////////////////////

  virtual void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase           = 0,
      int ivcount          = 0) = 0;

#if defined(ENABLE_COMPUTE_SHADERS)
  virtual void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase           = 0,
      int ivcount          = 0) = 0;
#endif      

  virtual void DrawIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase           = 0,
      int ivcount          = 0) = 0;

  virtual void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count) = 0;

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

protected:
  int miTrianglesRendered;
  Context& _context;

private:
  virtual void _doBeginFrame() {
  }
  virtual void _doEndFrame() {
  }
};

} //namespace ork::lev2{
