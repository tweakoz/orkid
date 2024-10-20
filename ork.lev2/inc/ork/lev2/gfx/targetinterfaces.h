////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Graphics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "config.h"
#include <ork/lev2/lev2_types.h>

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>

#include <ork/file/path.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/datablock.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxrasterstate.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/ui/ui.h>
#include <ork/math/TransformNode.h>
#include <ork/object/Object.h>
#include <ork/lev2/gfx/texman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////


struct ViewportRect : public ui::Rect {
  ViewportRect();
  ViewportRect(int x, int y, int w, int h);
};

////////////////////////////////////////////////////////////////////////////////

class IManipInterface : public ork::Object {
  RttiDeclareAbstract(IManipInterface, ork::Object);

public:
  IManipInterface() {
  }

  virtual const TransformNode& GetTransform(rtti::ICastable* pobj)            = 0;
  virtual void SetTransform(rtti::ICastable* pobj, const TransformNode& node) = 0;
  virtual void
  Attach(rtti::ICastable* pobj){}; /// optional - only needed if an object needs to know when it is going to be manipulated
  virtual void
  Detach(rtti::ICastable* pobj){}; /// optional - only needed if an object needs to know when it will stop being manipulated
};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Buffer for capturing data from VRAM to ram
/// primarily useful for GP-GPU Tasks (general purpose computation on the GPU)
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct CaptureBuffer {

  int GetStride() const;
  int CalcDataIndex(int ix, int iy) const;
  size_t length() const {
    return _buffersize;
  }
  void SetWidth(int iw);
  void SetHeight(int ih);
  int width() const;
  int height() const;
  EBufferFormat format() const;
  const void* GetData() const {
    return _data;
  }
  void CopyData(const void* pfrom, int isize);
  ////////////////////////////
  void setFormatAndSize(EBufferFormat fmt, int w, int h);
  ////////////////////////////
  CaptureBuffer();
  ~CaptureBuffer();
  ////////////////////////////
  EBufferFormat meFormat;
  int miW;
  int miH;
  void* _data;
  std::vector<uint8_t> _tempbuffer;
  size_t _buffersize;
  int _captureX = 0;
  int _captureY = 0;
  int _captureW = 0;
  int _captureH = 0;
  ////////////////////////////
};

#include "fxi.h"
#include "imi.h"
#include "mtxi.h"
#include "gbi.h"
#include "fbi.h"
#include "txi.h"
#include "rsi.h"
#include "ci.h"
#include "dwi.h"

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
