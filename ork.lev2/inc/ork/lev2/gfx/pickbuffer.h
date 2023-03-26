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

class PickBuffer //: public ork::lev2::DisplayBuffer
{
public:
  PickBuffer(ui::Surface* surf, Context* ctx, int iW, int iH);

  void Init();
  void resize(int w, int h);
  uint64_t AssignPickId(const ork::Object*);
  ork::Object* GetObjectFromPickId(uint64_t);
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

  std::map<uint64_t, ork::Object*> mPickIds;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
