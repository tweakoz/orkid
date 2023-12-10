////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxvtxbuf.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T>
IdxBuffer<T>::IdxBuffer()
    : IndexBufferBase() {
}

template <typename T> IdxBuffer<T>::~IdxBuffer() {
}
template <typename T> int IdxBuffer<T>::GetIndexSize() const { // final
  return sizeof(T);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> StaticIndexBuffer<T>::~StaticIndexBuffer() {
  this->mpIndices = 0;
}

template <typename T> bool StaticIndexBuffer<T>::IsStatic() const { // final
  return true;
}

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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> bool DynamicIndexBuffer<T>::IsStatic() const { // final
  return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T>
DynamicIndexBuffer<T>::DynamicIndexBuffer(int inumidx)
    : IdxBuffer<T>() {
  this->miNumIndices = inumidx;
}

template <typename T> DynamicIndexBuffer<T>::~DynamicIndexBuffer() {
  this->mpIndices = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T>
CVtxBuffer<T>::CVtxBuffer(int iMax, int iFlush)
    : VertexBufferBase(iMax, iFlush, sizeof(T), T::meFormat) {
}

template <typename T> void CVtxBuffer<T>::EndianSwap() { // final
  T* tbase = (T*)_vertices;

  for (int i = 0; i < miNumVerts; i++) {
    tbase[i].EndianSwap();
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> T& VtxWriter<T>::RefAndIncrement() {
  OrkAssert(mpBase != 0);
  OrkAssert(miWriteCounter < miWriteMax);
  T* pVtx = reinterpret_cast<T*>(mpBase);
  return pVtx[miWriteCounter++];
}
template <typename T> int VtxWriter<T>::AddVertex(T const& rT) {
  OrkAssert(mpBase != 0);
  OrkAssert(miWriteCounter < miWriteMax);
  T* pVtx    = reinterpret_cast<T*>(mpBase);
  int rval   = miWriteCounter++;
  pVtx[rval] = rT;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> bool StaticVertexBuffer<T>::StaticVertexBuffer::IsStatic() const { // final
  return true;
}

template <typename T>
StaticVertexBuffer<T>::StaticVertexBuffer(int iMax, int iFlush)
    : CVtxBuffer<T>(iMax, iFlush) {
  // printf("StaticVertexBuffer max<%d> len<%zu>\n", iMax, iMax * sizeof(T));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename T> bool DynamicVertexBuffer<T>::IsStatic() const { // final
  return false;
}

template <typename T>
DynamicVertexBuffer<T>::DynamicVertexBuffer(int iMax, int iFlush)
    : CVtxBuffer<T>(iMax, iFlush) {
  // printf("DynamicVertexBuffer max<%d> len<%zu>\n", iMax, iMax * sizeof(T));
}

} // namespace ork::lev2