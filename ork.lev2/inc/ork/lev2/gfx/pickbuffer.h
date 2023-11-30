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
  PixelFetchContext();

  //////////////////////

  fvec4 encodeVariant(pickvariant_t data);
  pickvariant_t decodeVariant(fvec4 inp);

  //////////////////////

  enum EPixelUsage {
    EPU_FLOAT = 0,
    EPU_PTR64,
    EPU_SVARIANT,
  };

  static const int kmaxitems = 4;

  Context* _gfxContext = nullptr;
  rtgroup_ptr_t _rtgroup;
  int miMrtMask;
  pickvariant_t _pickvalues[kmaxitems];
  EPixelUsage mUsage[kmaxitems];
  std::unordered_map<uint64_t,int> _pickIDlut;
  std::vector<pickvariant_t> _pickIDvec;
  anyp mUserData;
};

/// ////////////////////////////////////////////////////////////////////////////

struct PickBuffer  {

  PickBuffer(ui::Surface* surf, Context* ctx, int iW, int iH);

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
