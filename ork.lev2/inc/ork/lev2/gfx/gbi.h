#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Geometry Buffer Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class GeometryBufferInterface {

public:
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

  virtual void DrawPrimitive(const VertexBufferBase& VBuf, EPrimitiveType eType = EPRIM_NONE, int ivbase = 0, int ivcount = 0) = 0;
  virtual void DrawIndexedPrimitive(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      EPrimitiveType eType = EPRIM_NONE,
      int ivbase           = 0,
      int ivcount          = 0) = 0;
  virtual void
  DrawPrimitiveEML(const VertexBufferBase& VBuf, EPrimitiveType eType = EPRIM_NONE, int ivbase = 0, int ivcount = 0) = 0;
  virtual void DrawIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      EPrimitiveType eType = EPRIM_NONE,
      int ivbase           = 0,
      int ivcount          = 0) = 0;

  virtual void* LockIB(IndexBufferBase& VBuf, int ibase = 0, int icount = 0) = 0;
  virtual void UnLockIB(IndexBufferBase& VBuf)                               = 0;

  virtual const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0) = 0;
  virtual void UnLockIB(const IndexBufferBase& VBuf)                                     = 0;

  virtual void ReleaseIB(IndexBufferBase& VBuf) = 0;

  void DrawPrimitive(const VtxWriterBase& VW, EPrimitiveType eType, int icount = 0);
  void DrawPrimitiveEML(const VtxWriterBase& VW, EPrimitiveType eType, int icount = 0);

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

  GeometryBufferInterface();
  ~GeometryBufferInterface();

protected:
  int miTrianglesRendered;

private:
  virtual void _doBeginFrame() {
  }
  virtual void _doEndFrame() {
  }
};
