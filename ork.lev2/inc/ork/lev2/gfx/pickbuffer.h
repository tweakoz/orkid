////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/event/Event.h>
#include <ork/object/AutoConnector.h>
#include <ork/util/scrambler.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Pixel Getter Context
///  this can grab pixels from buffers, including multiple pixels from MRT's
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct PixelFetchContext {
  ork::rtti::ICastable* GetObject(PickBuffer* pb, int ichan) const;
  void* GetPointer(int ichan) const;
  PixelFetchContext(size_t size=0);
  void resize(size_t s);
  //////////////////////

  void beginPickRender();
  void endPickRender();

  uint32_t encodeVariant(pickvariant_t data);
  pickvariant_t decodePixel(fvec4 fv4_pixel);
  pickvariant_t decodePixel(u32vec4 u32v4_pixel);

  //////////////////////

  enum EPixelUsage {
    EPU_FLOAT = 0,
    EPU_FVEC4,
    EPU_PTR64,
    EPU_SVARIANT,
  };

  Context* _gfxContext = nullptr;
  rtgroup_ptr_t _rtgroup;
  int miMrtMask;
  std::vector<pickvariant_t> _pickvalues;
  std::vector<EPixelUsage> _usage;
  std::unordered_map<uint64_t,int> _pickIDlut;
  std::vector<pickvariant_t> _pickIDvec;
  anyp mUserData;
  int _pickindex = -1;
  uint64_t _offset = 0;
  static indexscrambler65k_ptr_t _gscrambler;
  static std::atomic<int> _gpickcounter;
};

/// ////////////////////////////////////////////////////////////////////////////

struct PickBuffer  {

  PickBuffer(ui::Surface* surf, Context* ctx, int iW, int iH);
  virtual ~PickBuffer() {}
  void Init();
  void resize(int w, int h);
  uint64_t AssignPickId(const void*);
  void* GetObjectFromPickId(uint64_t);
  Context* context() {
    return _context;
  }

  virtual void Draw(lev2::PixelFetchContext& ctx);

  ///////////////////////

  ork::lev2::rtgroup_ptr_t _rtgroup;

  GfxMaterialUITextured* _uimaterial = nullptr;
  Context* _context                  = nullptr;
  ui::Surface* _surface              = nullptr;

  int _width    = 0;
  int _height   = 0;
  bool _inittex = true;

  std::map<uint64_t, void*> mPickIds;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
