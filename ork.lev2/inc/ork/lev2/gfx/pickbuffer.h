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

class PickBuffer //: public ork::lev2::OffscreenBuffer
{
public:
  PickBuffer(ui::Surface* surf, Context* ctx, int iW, int iH);

  void Init();

  uint64_t AssignPickId(ork::Object*);
  ork::Object* GetObjectFromPickId(uint64_t);
  Context* context() {
    return _context;
  }

  virtual void Draw(lev2::PixelFetchContext& ctx);

  ///////////////////////

  int _width;
  int _height;
  bool _inittex;
  GfxMaterialUITextured* _uimaterial;
  ork::lev2::RtGroup* _rtgroup;
  Context* _context;
  ui::Surface* _surface;

  std::map<uint64_t, ork::Object*> mPickIds;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
