////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/math/misc_math.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

std::set<void*>& GedSkin::GetObjSet() {
  static std::set<void*> gObjSet;
  return gObjSet;
}
void GedSkin::ClearObjSet() {
  GetObjSet().clear();
}
void GedSkin::AddToObjSet(void* pobj) {
  GetObjSet().insert(pobj);
}
bool GedSkin::IsObjInSet(void* pobj) {
  bool rval = false;
  rval      = (GetObjSet().find(pobj) != GetObjSet().end());
  return rval;
}

void GedSkin::pushCustomColor(fcolor3 color){
  _colorStack.push(color);
}
void GedSkin::popCustomColor(){
  _colorStack.pop();
}

////////////////////////////////////////////////////////////////

GedSkin::GedSkin()
    : _scrollY(0)
    , _gedVP(nullptr)
    , _font(nullptr)
    , _char_w(0)
    , _char_h(0) {
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


//'ork::tool::ged::GedSkin::PrimContainer *' to
//'ork::tool::ged::GedSkin::PrimContainer *'

void GedSkin::AddPrim(const GedPrim& cb) {
  int isort                   = calcsort(cb.miSortKey);
  PrimContainers::iterator it = mPrimContainers.find(isort);
  if (it == mPrimContainers.end()) {
    PrimContainer* pcontainer = mPrimContainerPool.allocate();
    OrkAssert(pcontainer != 0);
    it = mPrimContainers.AddSorted(isort, pcontainer);
  }

  PrimContainer* pctr = it->second;
  if (cb.mDrawCB) {
    GedPrim* pooledprim = pctr->mPrimPool.allocate();
    *pooledprim         = cb;
    pctr->mCustomPrims.push_back(pooledprim);
  } else
    switch (cb.meType) {
      case PrimitiveType::LINES: {
        GedPrim* pooledprim = pctr->mPrimPool.allocate();
        *pooledprim         = cb;
        pctr->mLinePrims.push_back(pooledprim);
        break;
      }
      case PrimitiveType::QUADS: {
        GedPrim* pooledprim = pctr->mPrimPool.allocate();
        *pooledprim         = cb;
        pctr->mQuadPrims.push_back(pooledprim);
        break;
      }
    }
}

void GedSkin::PrimContainer::clear() {
  mPrimPool.clear();
  mLinePrims.clear();
  mQuadPrims.clear();
  mCustomPrims.clear();
}

void GedSkin::clear() {
  for (int i = 0; i < int(mPrimContainerPool.capacity()); i++) {
    mPrimContainerPool.direct_access(i).clear();
  }
  mPrimContainerPool.clear();
  mPrimContainers.clear();
}
  
////////////////////////////////////////////////////////////////

orkvector<GedSkin*> instantiateSkins(ork::lev2::Context* ctx) {
  orkvector<GedSkin*> skins;
  auto skin0 = new GedSkin0(ctx);
  auto skin1 = new GedSkin1(ctx);
  skins.push_back(skin0);
  skins.push_back(skin1);
    return skins;
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////
