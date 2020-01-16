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
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/texman.h>
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

void GridArchetype::DoLinkEntity(Simulation* psi, Entity* pent) const {
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

    void gpuInit(lev2::Context* ctx) {
      auto material       = new PBRMaterial();
      material->_texColor = _colortexture;
      //_material->_enablePick         = true;
      material->Init(ctx);
      material->_metallicFactor  = 0.0f;
      material->_roughnessFactor = 1.0f;
      material->_baseColor       = fvec3(1, 1, 1);
      _material                  = material;
    }

    static void RenderCallback(RenderContextInstData& rcid, Context* targ, const CallbackRenderable* pren) {
      const impl* pimpl = pren->GetUserData0().Get<const impl*>();

      const GridArchetype* _archetype = pimpl->_archetype;
      const Entity* pent              = pimpl->_entity;
      const GridControllerInst* ssci  = pent->GetTypedComponent<GridControllerInst>();
      const GridControllerData& cd    = ssci->GetCD();

      bool isPickState = targ->FBI()->isPickState();
      float fphase     = ssci->GetPhase();

      auto RCFD        = targ->topRenderContextFrameData();
      const auto& CPD  = RCFD->topCPD();
      auto cammatrices = CPD.cameraMatrices();
      const auto& FRUS = cammatrices->GetFrustum();

      fvec2 topl(-1, -1), topr(+1, -1);
      fvec2 botl(-1, +1), botr(+1, +1);
      fray3 ray_topl, ray_topr, ray_botl, ray_botr;

      cammatrices->projectDepthRay(topl, ray_topl);
      cammatrices->projectDepthRay(topr, ray_topr);
      cammatrices->projectDepthRay(botr, ray_botr);
      cammatrices->projectDepthRay(botl, ray_botl);
      fplane3 groundplane(0, 1, 0, 0);

      float dtl, dtr, dbl, dbr;
      bool does_topl_isect = groundplane.Intersect(ray_topl, dtl);
      bool does_topr_isect = groundplane.Intersect(ray_topr, dtr);
      bool does_botr_isect = groundplane.Intersect(ray_botr, dbr);
      bool does_botl_isect = groundplane.Intersect(ray_botl, dbl);

      if (true) // does_topl_isect&&does_topr_isect&&does_botl_isect&&does_botr_isect)
      {
        fvec3 topl(-10000.0f, 0, -10000.0f); // ray_topl.mOrigin + ray_topl.mDirection*dtl;
        fvec3 topr(+10000.0f, 0, -10000.0f); // = ray_topr.mOrigin + ray_topr.mDirection*dtr;
        fvec3 botr(+10000.0f, 0, +10000.0f); // = ray_botr.mOrigin + ray_botr.mDirection*dbr;
        fvec3 botl(-10000.0f, 0, +10000.0f); // = ray_botl.mOrigin + ray_botl.mDirection*dbl;

        // printf("topl<%g %g %g>\n", topl.x, topl.y, topl.z );
        // printf("topr<%g %g %g>\n", topr.x, topr.y, topr.z );
        // printf("botr<%g %g %g>\n", botr.x, botr.y, botr.z );
        // printf("botl<%g %g %g>\n", botl.x, botl.y, botl.z );

        auto uv0      = fvec2(topl.x, topl.z) * 0.05;
        auto uv1      = fvec2(topr.x, topr.z) * 0.05;
        auto uv2      = fvec2(botr.x, botr.z) * 0.05;
        auto uv3      = fvec2(botl.x, botl.z) * 0.05;
        auto normal   = fvec3(0, 1, 0);
        auto binormal = fvec3(1, 0, 0);

        auto v0 = SVtxV12N12B12T8C4(topl, normal, binormal, uv0, 0xffffffff);
        auto v1 = SVtxV12N12B12T8C4(topr, normal, binormal, uv1, 0xffffffff);
        auto v2 = SVtxV12N12B12T8C4(botr, normal, binormal, uv2, 0xffffffff);
        auto v3 = SVtxV12N12B12T8C4(botl, normal, binormal, uv3, 0xffffffff);

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

        const fmtx4& PMTX = cammatrices->_pmatrix;
        const fmtx4& VMTX = cammatrices->_vmatrix;

        auto mtxi = targ->MTXI();
        auto gbi  = targ->GBI();
        mtxi->PushMMatrix(fmtx4());
        mtxi->PushVMatrix(VMTX);
        mtxi->PushPMatrix(PMTX);
        targ->PushModColor(fcolor4::Green());
        targ->PushMaterial(pimpl->_material);
        gbi->DrawPrimitive(vw, EPRIM_TRIANGLES, 6);
        targ->PopModColor();
        mtxi->PopPMatrix();
        mtxi->PopVMatrix();
        mtxi->PopMMatrix();

      } else {
        printf(
            "itl<%d> itr<%d> ibl<%d> ibr<%d>\n",
            int(does_topl_isect),
            int(does_topr_isect),
            int(does_botl_isect),
            int(does_botr_isect));
      }
    }
    static void enqueueOnLayerCallback(DrawableBufItem& cdb) {
      // AssertOnOpQ2( updateSerialQueue() );
    }
  };

  ////////////////////////////////////////////////////////////////
  // pull data out of ECS
  ////////////////////////////////////////////////////////////////

  const GridControllerInst* ssci = pent->GetTypedComponent<GridControllerInst>();
  const GridControllerData& cd   = ssci->GetCD();
  auto texture                   = cd.GetTexture();

  ////////////////////////////////////////////////////////////////

  mainSerialQueue().enqueue([this, pent, texture]() {
    impl* pimpl          = new impl;
    pimpl->_archetype    = this;
    pimpl->_entity       = pent;
    pimpl->_colortexture = texture;
    ////////////////////////////////////////
    // first initialize rednerthread data
    ////////////////////////////////////////
    auto ctx = lev2::GfxEnv::GetRef().loadingContext();
    pimpl->gpuInit(ctx);
    ////////////////////////////////////////
    // now updatethread data
    ////////////////////////////////////////
    updateSerialQueue().enqueue([pent, pimpl]() {
      auto pdrw = new lev2::CallbackDrawable(pent);
      lev2::Drawable::var_t ap;
      ap.Set<const impl*>(pimpl);
      pdrw->SetUserDataA(ap);
      pent->addDrawableToDefaultLayer(pdrw);
      pdrw->SetRenderCallback(impl::RenderCallback);
      pdrw->SetenqueueOnLayerCallback(impl::enqueueOnLayerCallback);
      pdrw->SetOwner(&pent->GetEntData());
      pdrw->SetSortKey(0);
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
  c->floatProperty("Scale", float_range{-1000.0, 1000.0}, &GridControllerData::_scale);
  c->floatProperty("UvScale", float_range{-1000.0, 1000.0}, &GridControllerData::_uvscale);

  c->memberProperty("ColorTexture", &GridControllerData::_colorTexture)
      ->annotate("editor.class", "ged.factory.assetlist")
      ->annotate("editor.assettype", "lev2tex")
      ->annotate("editor.assetclass", "lev2tex");
}

///////////////////////////////////////////////////////////////////////////////

GridControllerData::GridControllerData()
    : _spinrate(0.0f)
    , _colorTexture(nullptr)
    , _scale(1.0f)
    , _uvscale(1.0f) {
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
