////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>

#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/LightingSystem.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/kernel/opq.h>

namespace ork {

using namespace lev2;

namespace ent {

///////////////////////////////////////////////////////////////////////////////
static const bool use_tess = true;

class FnBallComponentData : public ComponentData {
  RttiDeclareConcrete(FnBallComponentData, ComponentData);

public:
  ///////////////////////////////////////////////////////
  FnBallComponentData()
      : mpMaterial(nullptr) {
    void_lambda_t lamb = [=]() {
      auto targ = GfxEnv::GetRef().loadingContext();
      if (use_tess)
        mpMaterial = new GfxMaterial3DSolid(targ, "orkshader://fnball", "fb1");
      else
        mpMaterial = new GfxMaterial3DSolid(targ, "orkshader://fnball", "fb_pnt");
      mpMaterial->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
    };
    opq::mainSerialQueue()->enqueue(lamb);
  }
  ~FnBallComponentData() {
  }
  ComponentInst* createComponent(Entity* pent) const override;
  ///////////////////////////////////////////////////////

  GfxMaterial3DSolid* mpMaterial;

private:
};

void FnBallComponentData::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

class FnBallComponentInst : public ComponentInst {
public:
  FnBallComponentInst(const FnBallComponentData& data, Entity* pent);
  ~FnBallComponentInst();

  const FnBallComponentData& GetFbd() const {
    return mFnbData;
  }

private:
  void DoUpdate(Simulation* inst) override {
  }
  bool DoLink(Simulation* psi) override {
    return true;
  }

  const FnBallComponentData& mFnbData;
};

FnBallComponentInst::FnBallComponentInst(const FnBallComponentData& data, Entity* pent)
    : ComponentInst(&data, pent)
    , mFnbData(data) {
}

FnBallComponentInst::~FnBallComponentInst() {
}

///////////////////////////////////////////////////////////////////////////////

class FnBallArchetype : public Archetype {
  RttiDeclareConcrete(FnBallArchetype, Archetype);

public:
  FnBallArchetype();
  float mTessLevel;
  float mDisplacement;

private:
  void DoCompose(ArchComposer& composer) override;
  void DoStartEntity(Simulation*, const fmtx4& mtx, Entity* pent) const override {
  }
  void DoLinkEntity(Simulation* psi, Entity* pent) const override;
};

void FnBallArchetype::Describe() {
  reflect::RegisterProperty("TessLevel", &FnBallArchetype::mTessLevel);
  reflect::annotatePropertyForEditor<FnBallArchetype>("TessLevel", "editor.range.min", "0.0");
  reflect::annotatePropertyForEditor<FnBallArchetype>("TessLevel", "editor.range.max", "1.0");

  reflect::RegisterProperty("Displacement", &FnBallArchetype::mDisplacement);
  reflect::annotatePropertyForEditor<FnBallArchetype>("Displacement", "editor.range.min", "-24.0");
  reflect::annotatePropertyForEditor<FnBallArchetype>("Displacement", "editor.range.max", "12.0");
}

FnBallArchetype::FnBallArchetype()
    : mTessLevel(0.0f)
    , mDisplacement(0.0f) {
}

void FnBallArchetype::DoCompose(ArchComposer& composer) {
  composer.Register<FnBallComponentData>();
}

void FnBallArchetype::DoLinkEntity(Simulation* psi, Entity* pent) const {
  typedef SVtxV12N12B12T8C4 vertex_t;
  struct yo {
    const FnBallArchetype* parch;
    Entity* pent;

    static void doit(RenderContextInstData& rcid, Context* targ, const CallbackRenderable* pren) {
      const yo* pyo                   = pren->GetUserData0().Get<const yo*>();
      const FnBallArchetype* parch    = pyo->parch;
      const Entity* pent              = pyo->pent;
      const FnBallComponentInst* fnbi = pent->GetTypedComponent<FnBallComponentInst>();
      const FnBallComponentData& fd   = fnbi->GetFbd();
      bool isPickState                = rcid.GetRenderer()->GetTarget()->FBI()->isPickState();
      const fmtx4& MVP                = targ->MTXI()->RefMVPMatrix();

      if (fd.mpMaterial && false == isPickState) {

        auto& vtxbuf = GfxEnv::GetSharedDynamicVB2();
        VtxWriter<vertex_t> vw;

        const float kdim = 30.0f;

        fvec3 ori(0.0f, 0.0f, 0.0f);
        fvec3 top(0.0f, kdim, 0.0f);
        fvec3 bot(0.0f, -kdim, 0.0f);
        fvec3 lft(-kdim, 0.0f, 0.0f);
        fvec3 rht(+kdim, 0.0f, 0.0f);
        fvec3 near(0.0f, 0.0f, -kdim);
        fvec3 far(0.0f, 0.0f, +kdim);

        fvec2 u_top(0.0f, 0.0f);
        fvec2 u_bot(0.0f, 1.0f);
        fvec2 u_lft(0.0f, 0.5f);
        fvec2 u_rht(0.5f, 0.5f);
        fvec2 u_near(0.25f, 0.5f);
        fvec2 u_far(0.75f, 0.5f);

        auto do_vtx = [&](const fvec3& pos, const fvec2& uv) -> vertex_t {
          return vertex_t(pos, pos.Normal(), ori, uv, 0xffffffff);
        };
        auto do_tri = [&](const vertex_t& v0, const vertex_t& v1, const vertex_t& v2) {
          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v2);
        };

        auto vbot = do_vtx(bot, u_bot);
        auto vtop = do_vtx(top, u_top);
        auto vner = do_vtx(near, u_near);
        auto vfar = do_vtx(far, u_far);
        auto vrht = do_vtx(rht, u_rht);
        auto vlft = do_vtx(lft, u_lft);

        static const int knumv = 24;

        vw.Lock(targ, &vtxbuf, knumv);
        {
          do_tri(vtop, vner, vrht);
          do_tri(vtop, vrht, vfar);
          do_tri(vtop, vner, vlft);
          do_tri(vtop, vlft, vfar);

          do_tri(vbot, vner, vrht);
          do_tri(vbot, vrht, vfar);
          do_tri(vbot, vner, vlft);
          do_tri(vbot, vlft, vfar);
        }
        vw.UnLock(targ);

        ////////////////////////////////////////
        auto mtx    = fmtx4::Identity();
        auto RCFD   = targ->topRenderContextFrameData();
        auto& cdata = RCFD->topCPD().cameraMatrices()->_camdat;
        fmtx4 matrs(mtx);
        matrs.SetTranslation(0.0f, 0.0f, 0.0f);
        matrs.Transpose();
        fvec3 nx = cdata.xNormal().Transform(matrs);
        fvec3 ny = cdata.yNormal().Transform(matrs);

        fvec4 user0 = -(ny + nx);
        fvec4 user1 = (ny - nx);
        auto user3  = fvec4(parch->mTessLevel, parch->mDisplacement, kdim, 0.0f);

        fd.mpMaterial->SetUser0(user0);
        fd.mpMaterial->SetUser1(user1);
        fd.mpMaterial->SetUser3(user3);
        ////////////////////////////////////////

        targ->MTXI()->PushMMatrix(mtx);
        if (use_tess)
          targ->GBI()->DrawPrimitive(fd.mpMaterial, vw, PrimitiveType::PATCHES, knumv);
        else
          targ->GBI()->DrawPrimitive(fd.mpMaterial, vw, PrimitiveType::POINTS, knumv);
        targ->MTXI()->PopMMatrix();
      }
    }
  };

#if 0 // DRAWTHREADS
    CallbackDrawable* pdrw = new CallbackDrawable(pent);
    pent->addDrawableToDefaultLayer(pdrw);
    pdrw->SetCallback( yo::doit );
    pdrw->SetOwner(  & pent->GetEntData() );
    pdrw->SetSortKey(0);

    yo* pyo = new yo;
    pyo->parch = this;
    pyo->pent = pent;

    anyp ap;
    ap.Set<const yo*>( pyo );
    pdrw->SetData( ap );
#endif
}

void FnBallArchetypeTouch() {
  FnBallArchetype::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::ComponentInst* FnBallComponentData::createComponent(ork::ent::Entity* pent) const {
  return new FnBallComponentInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ent
} // namespace ork

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FnBallComponentData, "FnBallComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::FnBallArchetype, "FnBallArchetype");
