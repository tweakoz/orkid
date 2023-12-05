////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>

/////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
/////////////////////////////////////////////////////////////////////////

IndexBufferBase::IndexBufferBase()
    : miNumIndices(0)
    , mhIndexBuf(0)
    , mpIndices(0) {
}

IndexBufferBase::~IndexBufferBase() {
  auto target = lev2::contextForCurrentThread();
  // target->GBI()->ReleaseIB( *this );
  mpIndices = 0;
}

/////////////////////////////////////////////////////////////////////////
template <typename T> IdxBuffer<T>::~IdxBuffer() {
}
/////////////////////////////////////////////////////////////////////////

template <typename T>
StaticIndexBuffer<T>::StaticIndexBuffer(int inumidx, const T* src)
    : IdxBuffer<T>() {
  if (inumidx) {
    this->miNumIndices = inumidx;

    if (src) {
      this->mpIndices = const_cast<T*>(src);
    }
  }
}

template <typename T> StaticIndexBuffer<T>::~StaticIndexBuffer() {
  this->mpIndices = 0;
}

/////////////////////////////////////////////////////////////////////////

template <typename T>
DynamicIndexBuffer<T>::DynamicIndexBuffer(int inumidx)
    : IdxBuffer<T>() {
  this->miNumIndices = inumidx;
}

template <typename T> DynamicIndexBuffer<T>::~DynamicIndexBuffer() {
  this->mpIndices = 0;
}

/////////////////////////////////////////////////////////////////////////

template class StaticIndexBuffer<U16>;
template class DynamicIndexBuffer<U16>;
template class StaticIndexBuffer<U32>;
template class DynamicIndexBuffer<U32>;

/////////////////////////////////////////////////////////////////////////
template <typename T> vtxbufferbase_ptr_t _createvb(int _numverts, bool _static) {
  vtxbufferbase_ptr_t rval;

  using static_t  = StaticVertexBuffer<T>;
  using dynamic_t = DynamicVertexBuffer<T>;

  if (_static) {
    rval = std::static_pointer_cast<VertexBufferBase> //
        (std::make_shared<static_t>(_numverts, 0));
  } else {
    rval = std::static_pointer_cast<VertexBufferBase> //
        (std::make_shared<dynamic_t>(_numverts, 0));
  }
  return rval;
}
/////////////////////////////////////////////////////////////////////////

vtxbufferbase_ptr_t VertexBufferBase::CreateVertexBuffer(EVtxStreamFormat eformat, int inumverts, bool bstatic) {
  vtxbufferbase_ptr_t pvb;

  switch (eformat) {
    case EVtxStreamFormat::V12:
      pvb = _createvb<VtxV12>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12C4T16:
      pvb = _createvb<SVtxV12C4T16>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N6C2T4:
      pvb = _createvb<SVtxV12N6C2T4>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N6I1T4:
      pvb = _createvb<SVtxV12N6I1T4>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12I4N12T8:
      pvb = _createvb<SVtxV12I4N12T8>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N12T8I4W4:
      pvb = _createvb<SVtxV12N12T8I4W4>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N12B12T8I4W4:
      pvb = _createvb<SVtxV12N12B12T8I4W4>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N12B12T8C4:
      pvb = _createvb<SVtxV12N12B12T8C4>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N12B12T16:
      pvb = _createvb<SVtxV12N12B12T16>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N12T16C4:
      pvb = _createvb<SVtxV12N12T16C4>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::MODELERRIGID:
      pvb = _createvb<SVtxMODELERRIGID>(inumverts, bstatic);
      break;
    case EVtxStreamFormat::V12N12T8DF12C4:
      pvb = _createvb<SVtxV12N12T8DF12C4>(inumverts, bstatic);
      break;
    default:
      OrkAssert(false);
  }
  return pvb;
}

/////////////////////////////////////////////////////////////////////////

VertexBufferBase::VertexBufferBase(int iMax, int iFlush, int iSize, /*PrimitiveType eType,*/ EVtxStreamFormat eFmt)
    : miNumVerts(0)
    , miMaxVerts(iMax)
    , miVtxSize(iSize)
    , miLockWriteIndex(0)
    , miFlushSize(iFlush)
    //, mePrimType(eType)
    , meStreamFormat(eFmt)
    , mbLocked(false)
    , mbRingLock(false) {
}

VertexBufferBase::~VertexBufferBase() {
}

/////////////////////////////////////////////////////////////////////////
void VtxWriterBase::Lock(Context* pT, VertexBufferBase* pVB, int icount) {
  Lock(pT->GBI(), pVB, icount);
}

void VtxWriterBase::Lock(GeometryBufferInterface* GBI, VertexBufferBase* pVB, int icount) {
  OrkAssert(pVB != 0);
  bool bringlock = pVB->GetRingLock();
  int ivbase     = pVB->GetNumVertices();
  int imax       = pVB->GetMax();
  ////////////////////////////////////////////
  OrkAssert(icount != 0);
  ///////////////////////////////////////////
  // ringbuffer lock ?
  ///////////////////////////////////////////
  if (bringlock) {
    int inewbase = pVB->_ring_lock_index + icount;


    if (inewbase > imax) {
      ivbase   = 0;
      pVB->_ring_lock_index = 0;
      if(0)printf( "ringcyc vb<%p> rli<%d> inewbase<%d> icount<%d> imax<%d> \n",  //
              pVB, //
              pVB->_ring_lock_index, //
              inewbase, //
              icount, //
              imax );
    }
    else{
      ivbase = pVB->_ring_lock_index;
    }
    pVB->_ring_lock_index += icount;
    miWriteBase    = ivbase;
    miWriteCounter = 0;
    miWriteMax     = icount;
    mpVB           = pVB;
  }
  ///////////////////////////////////////////
  // standard lock
  ///////////////////////////////////////////
  else {
    int inewbase = ivbase + icount;
    pVB->SetNumVertices(inewbase);
    miWriteBase    = ivbase;
    miWriteCounter = 0;
    miWriteMax     = icount;
    mpVB           = pVB;
    OrkAssert((ivbase + icount) <= imax);
  }
  ////////////////////////////////////////////
  void* pdata = GBI->LockVB(*pVB, ivbase, icount);
  OrkAssert(pdata != 0);
  ////////////////////////////////////////////
  mpBase         = (char*)pdata;
}
void VtxWriterBase::UnLock(Context* pT, u32 ulflgs) {
  UnLock(pT->GBI(), ulflgs);
}
void VtxWriterBase::UnLock(GeometryBufferInterface* GBI, u32 ulflgs) {
  OrkAssert(mpVB != 0);
  GBI->UnLockVB(*mpVB);

  if ((ulflgs & EULFLG_ASSIGNVBLEN) != 0) {
    mpVB->SetNumVertices(miWriteCounter);
  }
}

}} // namespace ork::lev2
