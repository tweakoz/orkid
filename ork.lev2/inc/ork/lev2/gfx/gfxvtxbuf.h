////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/util/endian.h>

namespace ork::lev2 {

class GeometryBufferInterface;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class IndexBufferBase {

public:

  IndexBufferBase();
  virtual ~IndexBufferBase();

  int GetNumIndices() const;
  void SetNumIndices(int inum);
  void* GetHandle(void) const;
  void SetHandle(void* ph) const;
  virtual int GetIndexSize() const = 0;
  virtual bool IsStatic() const    = 0;

protected:
  int miNumIndices;
  mutable void* mhIndexBuf;
  void* mpIndices;
  bool mbLocked;

  void Release(void);
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> //
class IdxBuffer //
  : public IndexBufferBase { //
public:
  IdxBuffer();
  ~IdxBuffer();
  int GetIndexSize() const final;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class StaticIndexBuffer : public IdxBuffer<T> {
public:
  StaticIndexBuffer(int inumidx = 0, const T* src = 0);
  ~StaticIndexBuffer();
  bool IsStatic() const final;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class DynamicIndexBuffer : public IdxBuffer<T> {
public:
  DynamicIndexBuffer(int inumidx = 0);
  ~DynamicIndexBuffer();
  bool IsStatic() const final;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class VertexBufferBase {
public:
  VertexBufferBase(int iMax, int iFlush, int iSize, EVtxStreamFormat eFmt);
  virtual ~VertexBufferBase();

  ///////////////////////////////////////////////////////////////

  virtual void EndianSwap() = 0;

  int GetMax(void) const;
  int GetNumVertices(void) const;
  int GetVtxSize(void) const;
  void SetHandle(void* hVB);
  void* GetHandle(void) const;
  void Reset(void);
  void SetNumVertices(int inum);
  EVtxStreamFormat GetStreamFormat(void) const;
  bool IsLocked(void) const;
  void Lock() const;
  void Unlock() const;
  void SetRingLock(bool v);
  bool GetRingLock() const;

  int _ring_lock_index  = 0;

  ///////////////////////////////////////////////////////////////

  static vtxbufferbase_ptr_t CreateVertexBuffer(EVtxStreamFormat eformat, int inumverts, bool bstatic);

  virtual bool IsStatic() const = 0;

protected:
  void* _vertices = nullptr;
  void* _IMPL     = nullptr;

  int miNumVerts;
  int miMaxVerts;
  int miVtxSize;
  mutable int miLockWriteIndex;
  int miFlushSize;
  EVtxStreamFormat meStreamFormat;
  mutable bool mbLocked;
  bool mbInited;
  bool mbRingLock;

private:
  void SetLock(bool bLock) const {
    mbLocked = bLock;
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class CVtxBuffer : public VertexBufferBase {
public:
  typedef T vertex_t;

  CVtxBuffer(int iMax, int iFlush);
  void EndianSwap() final; 
};

///////////////////////////////////////////////////////////////////////////////

enum EVtxWriteUnlockFlags {
  EULFLG_NONE        = 0,
  EULFLG_ASSIGNVBLEN = 1,
  EULFLG_END,
};

struct VtxWriterBase {
  int miWriteBase;
  int miWriteCounter;
  int miWriteMax;
  char* mpBase;
  VertexBufferBase* mpVB;

  VtxWriterBase();
  void Lock(Context* pT, VertexBufferBase* mpVB, int icount = 0);
  void UnLock(Context* pT, u32 UnLockFlags = EULFLG_NONE);
  void Lock(GeometryBufferInterface* GBI, VertexBufferBase* mpVB, int icount = 0);
  void UnLock(GeometryBufferInterface* GBI, u32 UnLockFlags = EULFLG_NONE);
};

template <typename T> struct VtxWriter : public VtxWriterBase {
  T& RefAndIncrement();
  int AddVertex(T const& rT);
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct StaticVertexBuffer : public CVtxBuffer<T> {
  StaticVertexBuffer(int iMax, int iFlush);
  bool IsStatic() const final;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct DynamicVertexBuffer : public CVtxBuffer<T> {
  DynamicVertexBuffer(int iMax, int iFlush);
  bool IsStatic() const final;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

#include <ork/lev2/gfx/gfxvtxbuf_structs.h>
