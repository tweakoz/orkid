////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/renderer/renderer.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
///////////////////////////////////////////////////////////////////////////////
#include "GridComponent.h"
#include <ork/lev2/gfx/dbgfontman.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::GridArchetype, "GridArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::GridControllerInst, "GridControllerInst");
ImplementReflectionX(ork::ent::GridControllerData, "GridControllerData");
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
using namespace ork::opq;
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void GridArchetype::Describe() {
}

GridArchetype::GridArchetype() {
}

///////////////////////////////////////////////////////////////////////////////

struct impl {
  impl()
      : _material(nullptr)
      , _entity(nullptr) {
  }
  ~impl() {
    if (_material)
      delete _material;
  }
  const GridArchetype* _archetype;
  Entity* _entity;
  PBRMaterial* _material;
  Texture* _colortexture;
  bool _initted = false;

  void gpuInit(lev2::Context* ctx) {
    auto material       = new PBRMaterial();
    material->_texColor = _colortexture;
    //_material->_enablePick         = true;
    material->Init(ctx);
    material->_metallicFactor  = 0.0f;
    material->_roughnessFactor = 1.0f;
    material->_baseColor       = fvec3(1, 1, 1);
    _material                  = material;
    _initted                   = true;
  }

  static void RenderCallback(RenderContextInstData& rcid, Context* targ, const CallbackRenderable* pren) {
    const impl* pimpl = pren->GetUserData0().Get<const impl*>();
    if (pimpl->_initted == false)
      return;

    const GridArchetype* _archetype = pimpl->_archetype;
    const Entity* pent              = pimpl->_entity;
    const GridControllerInst* ssci  = pent->GetTypedComponent<GridControllerInst>();
    const GridControllerData& data  = ssci->GetCD();

    bool isPickState = targ->FBI()->isPickState();
    float fphase     = ssci->GetPhase();

    auto RCFD        = targ->topRenderContextFrameData();
    const auto& CPD  = RCFD->topCPD();
    auto cammatrices = CPD.cameraMatrices();
    const auto& FRUS = cammatrices->GetFrustum();
    bool stereo1pass = CPD.isStereoOnePass();

    float extent = data.extent();
    fvec3 topl(-extent, 0, -extent);
    fvec3 topr(+extent, 0, -extent);
    fvec3 botr(+extent, 0, +extent);
    fvec3 botl(-extent, 0, +extent);

    float uvextent = extent / data.tileDim();

    auto uv_topl  = fvec2(-uvextent, -uvextent);
    auto uv_topr  = fvec2(+uvextent, -uvextent);
    auto uv_botr  = fvec2(+uvextent, +uvextent);
    auto uv_botl  = fvec2(-uvextent, +uvextent);
    auto normal   = fvec3(0, 1, 0);
    auto binormal = fvec3(1, 0, 0);

    auto v0 = SVtxV12N12B12T8C4(topl, normal, binormal, uv_topl, 0xffffffff);
    auto v1 = SVtxV12N12B12T8C4(topr, normal, binormal, uv_topr, 0xffffffff);
    auto v2 = SVtxV12N12B12T8C4(botr, normal, binormal, uv_botr, 0xffffffff);
    auto v3 = SVtxV12N12B12T8C4(botl, normal, binormal, uv_botl, 0xffffffff);

    auto& VB = GfxEnv::GetSharedDynamicVB2();
    VtxWriter<SVtxV12N12B12T8C4> vw;
    vw.Lock(targ, &VB, 6);

    vw.AddVertex(v0);
    vw.AddVertex(v1);
    vw.AddVertex(v2);

    vw.AddVertex(v0);
    vw.AddVertex(v2);
    vw.AddVertex(v3);

    vw.UnLock(targ);

    auto mtxi = targ->MTXI();
    auto gbi  = targ->GBI();
    mtxi->PushMMatrix(pent->GetEffectiveMatrix());
    fvec4 modcolor = fcolor4::Green();
    if (isPickState) {
      auto pickBuf = targ->FBI()->currentPickBuffer();
      uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)pent) : 0;
      modcolor.SetRGBAU64(pid);
    }
    targ->PushModColor(modcolor);
    targ->PushMaterial(pimpl->_material);
    gbi->DrawPrimitive(vw, EPrimitiveType::TRIANGLES, 6);
    targ->PopModColor();
    mtxi->PopMMatrix();
  }
  static void enqueueOnLayerCallback(DrawableBufItem& cdb) {
    // AssertOnOpQ2( updateSerialQueue() );
  }
}; // namespace ent

///////////////////////////////////////////////////////////////////////////////

void GridArchetype::DoLinkEntity(Simulation* psi, Entity* pent) const {

  ////////////////////////////////////////////////////////////////
  // pull data out of ECS
  ////////////////////////////////////////////////////////////////

  const GridControllerInst* ssci = pent->GetTypedComponent<GridControllerInst>();
  const GridControllerData& cd   = ssci->GetCD();
  auto texture                   = cd.GetTexture();

  ////////////////////////////////////////////////////////////////
  impl* pimpl          = new impl;
  pimpl->_archetype    = this;
  pimpl->_entity       = pent;
  pimpl->_colortexture = texture;
  auto pdrw            = new lev2::CallbackDrawable(pent);
  pdrw->SetOwner(pent->data());
  pdrw->SetSortKey(0);

  pent->addDrawableToDefaultLayer(pdrw);

  lev2::Drawable::var_t ap;
  ap.Set<const impl*>(pimpl);
  pdrw->SetUserDataA(ap);

  pdrw->SetRenderCallback(impl::RenderCallback);
  pdrw->SetenqueueOnLayerCallback(impl::enqueueOnLayerCallback);

  mainSerialQueue().enqueue([pimpl]() {
    auto ctx = lev2::GfxEnv::GetRef().loadingContext();
    pimpl->gpuInit(ctx);
    updateSerialQueue().enqueue([pimpl]() {
      // todo - we need a method to put above impl replated initialization
      //   code into this opq op -
      //   while enforcing that it gets executed before any scene state changes occur
      //   (so that relevant entities cannot be deleted before the deferred execution)
    });
  });
}

///////////////////////////////////////////////////////////////////////////////

void GridArchetype::DoCompose(ork::ent::ArchComposer& composer) {
  composer.Register<GridControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void GridControllerData::describeX(class_t* c) {
  c->floatProperty("SpinRate", float_range{-6.28, 6.28}, &GridControllerData::_spinrate);
  c->floatProperty("Extent", float_range{-1000.0, 1000.0}, &GridControllerData::_extent);
  c->floatProperty("TileDim", float_range{0.0, 1000.0}, &GridControllerData::_tiledim);

  c->memberProperty("ColorTexture", &GridControllerData::_colorTexture)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");
}

///////////////////////////////////////////////////////////////////////////////

GridControllerData::GridControllerData()
    : _spinrate(0.0f)
    , _colorTexture(nullptr)
    , _extent(1.0f)
    , _tiledim(1.0f) {
}

///////////////////////////////////////////////////////////////////////////////

lev2::Texture* GridControllerData::GetTexture() const {
  lev2::Texture* ptx = (_colorTexture != 0) ? _colorTexture->GetTexture() : 0;
  return ptx;
}

///////////////////////////////////////////////////////////////////////////////

void GridControllerInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

GridControllerInst::GridControllerInst(const GridControllerData& data, ent::Entity* pent)
    : ork::ent::ComponentInst(&data, pent)
    , mCD(data)
    , mPhase(0.0f) {
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* GridControllerData::createComponent(ent::Entity* pent) const {
  return OrkNew GridControllerInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void GridControllerInst::DoUpdate(ent::Simulation* sinst) {
  mPhase += mCD.GetSpinRate() * sinst->GetDeltaTime();
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
