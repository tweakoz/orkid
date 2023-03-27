////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/misc_math.h>
//#include <ork/reflect/properties/ObjectProperty.h>

//#include <orktool/ged/ged.h>
//#include <orktool/ged/ged_delegate.h>
//#include <orktool/qtui/qtmainwin.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

using vtx_t = SVtxV16T16C16 ;

GedSkin::GedSkin()
    : miScrollY(0)
    , mpCurrentGedVp(nullptr)
    , mpFONT(nullptr)
    , miCHARW(0)
    , miCHARH(0) {
}

void GedSkin::gpuInit(lev2::Context* ctx) {
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(ctx, "orkshader://ui2");
  _tekpick     = _material->technique("ui_picking");
  _tekvtxcolor = _material->technique("ui_vtxcolor");
  _tekvtxpick  = _material->technique("ui_vtxpicking");
  _tekmodcolor = _material->technique("ui_modcolor");
  _parmvp      = _material->param("mvp");
  _parmodcolor = _material->param("modcolor");
  _parobjid    = _material->param("objid");
  _material->dump();
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
