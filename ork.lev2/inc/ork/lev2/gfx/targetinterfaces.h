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
#include <ork/lev2/gfx/rasterstate.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/ui/ui.h>
#include <ork/math/TransformNode.h>
#include <ork/object/Object.h>
#include <ork/lev2/gfx/texman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
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
  ////////////////////////////
  void setFormatAndSize(EBufferFormat fmt, int w, int h);
  ////////////////////////////
  CaptureBuffer();
  ~CaptureBuffer();
  ////////////////////////////
  EBufferFormat meFormat;
  int miW;
  int miH;
  image_ptr_t _image;
  image_ptr_t _raw_image; // temporary working image for conversions
  size_t _buffersize;
  int _captureX = 0;
  int _captureY = 0;
  int _captureW = 0;
  int _captureH = 0;
  svarshp_t _impl;
  ////////////////////////////
};

using capturebuffer_ptr_t = std::shared_ptr<CaptureBuffer>;
using capturebuffer_wkptr_t = std::weak_ptr<CaptureBuffer>;
using capturebuffer_list_t = std::vector<capturebuffer_ptr_t>;
using capturebuffer_list_ptr_t = std::shared_ptr<capturebuffer_list_t>;

///////////////////////////////////////////////////////////////////////////////

struct CaptureAsync {
  CaptureAsync();
  ~CaptureAsync();
  
  // Wait for capture to complete and get the result
  bool wait(CaptureBuffer* out_buffer);
  
  // Check if capture is ready (non-blocking)
  bool isReady() const;
  
  // Get progress (0.0 to 1.0)
  float progress() const;
  
  // Implementation-specific data (staging buffer, fence, etc)
  svarshp_t _impl;
  
  // Capture parameters
  int _width = 0;
  int _height = 0;
  EBufferFormat _format = EBufferFormat::NONE;
  
  // Output destinations
  capturebuffer_ptr_t _captureBuffer;  // For capture to buffer
  texture_ptr_t _captureTexture;       // For capture to texture  
  file::Path _capturePath;             // For capture to file
  
  // Completion callback
  void_lambda_t _on_capture_complete;
  
  // Pixel fetch context (for pixel picking operations)
  pixelfetchctx_ptr_t _pixelFetchContext;
  
  // Status
  bool _completed = false;
  bool _failed = false;
};

using captureasync_ptr_t = std::shared_ptr<CaptureAsync>;
using captureasync_wkptr_t = std::weak_ptr<CaptureAsync>;
using captureasync_list_t = std::vector<captureasync_ptr_t>;
using captureasync_list_ptr_t = std::shared_ptr<captureasync_list_t>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

#include "fxi.h"
#include "imi.h"
#include "mtxi.h"
#include "gbi.h"
#include "fbi.h"
#include "txi.h"
#include "ci.h"
#include "dwi.h"
