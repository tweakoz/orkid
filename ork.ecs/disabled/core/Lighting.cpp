////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/ecs/entity.h>
#include <ork/ecs/entity.hpp>
#include <ork/ecs/scene.h>
#include <ork/ecs/scene.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>

#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/ecs/LightingSystem.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingComponentData, "LightingComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingComponentInst, "LightingComponentInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightArchetype, "LightArchetype");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingSystemData, "LightingSystemSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingComponentData::Describe() {
  ork::reflect::RegisterProperty("LightData", &LightingComponentData::LdGetter, &LightingComponentData::LdSetter);
  ork::reflect::RegisterProperty("Dynamic", &LightingComponentData::_dynamic);
  ork::reflect::annotatePropertyForEditor<LightingComponentData>("LightData", "editor.factorylistbase", "LightData");
}

///////////////////////////////////////////////////////////////////////////////

LightingComponentData::LightingComponentData()
    : _lightdata(0)
    , _dynamic(false) {
}

LightingComponentData::~LightingComponentData() {
  if (_lightdata)
    delete _lightdata;
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::ComponentInst* LightingComponentData::createComponent(ork::ent::Entity* pent) const {
  return OrkNew LightingComponentInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingComponentInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

LightingComponentInst::LightingComponentInst(const LightingComponentData& data, ork::ent::Entity* pent)
    : ork::ent::ComponentInst(&data, pent)
    , _light(nullptr)
    , _lcdata(data) {
  ork::lev2::LightData* lightdata = data.GetLightData();

  ork::lev2::DirectionalLightData* dirlight_data = ork::rtti::autocast(lightdata);
  ork::lev2::PointLightData* pntlight_data       = ork::rtti::autocast(lightdata);
  ork::lev2::SpotLightData* sptlight_data        = ork::rtti::autocast(lightdata);
  ork::lev2::AmbientLightData* amblight_data     = ork::rtti::autocast(lightdata);

  auto xfgen = [this]() -> fmtx4 { return this->_entity->GetEffectiveMatrix(); };

  if (amblight_data) {
    _light = new ork::lev2::AmbientLight(xfgen, amblight_data);
  }
  if (dirlight_data) {
    _light = new ork::lev2::DirectionalLight(xfgen, dirlight_data);
  }
  if (pntlight_data) {
    _light = new ork::lev2::PointLight(xfgen, pntlight_data);
  }
  if (sptlight_data) {
    _light = new ork::lev2::SpotLight(xfgen, sptlight_data);
  }

  struct yo {
    //
    ork::lev2::XgmModelAsset* mpModel;
    ork::ent::Entity* _entity;
    ork::lev2::Light* _light;

    static void draw_tricircle(
        ork::lev2::RenderContextInstData& rcid,
        ork::lev2::Context* targ,
        const ork::lev2::CallbackRenderable* pren,
        ork::lev2::PointLight* ppntlight) {
#if defined(INTOOL)
      const yo* pyo = pren->GetUserData0().Get<const yo*>();

      const ork::lev2::Renderer* prenderer = rcid.GetRenderer();

      const ork::TransformNode& xf = &pyo->_entity->GetEntData().GetDagNode().GetTransformNode();
      ork::fmtx4 mtxw              = xf.GetTransform()->GetMatrix();

      if (false == targ->FBI()->isPickState()) {
        ork::lev2::SphereRenderable sphrend;
        sphrend.SetColor(ork::fcolor4::White());
        sphrend.SetPosition(mtxw.GetTranslation());
        sphrend.SetRadius(ppntlight->GetRadius());
        prenderer->RenderSphere(sphrend);
      }

      ork::fcolor4 ModColor = pren->GetModColor();
      ork::fcolor4 ObjColor;
      ObjColor.SetRGBAU32(reinterpret_cast<U32>(pren->GetObject()));

      ork::fcolor4 color = targ->FBI()->isPickState() ? ObjColor : ModColor * ppntlight->GetColor();

      mtxw.SetScale(0.25f);
      targ->PushModColor(color);
      targ->MTXI()->PushMMatrix(mtxw);
      ork::lev2::GfxPrimitives::GetRef().RenderDiamond(targ);
      targ->MTXI()->PopMMatrix();
      targ->PopModColor();
#endif
    }

    static void
    draw_dirlight(ork::lev2::RenderContextInstData& rcid, ork::lev2::Context* targ, const ork::lev2::CallbackRenderable* pren) {
#if defined(INTOOL)
      const yo* pyo               = pren->GetUserData0().Get<const yo*>();
      ork::lev2::XgmModel* pmodel = (pyo->mpModel == 0) ? 0 : pyo->mpModel->GetModel();
      if (pmodel) {
        const ork::TransformNode3D& xf = &pyo->_entity->GetEntData().GetDagNode().GetTransformNode();
        // const ork::lev2::RenderContextFrameData* fdata = renderer->GetTarget()->topRenderContextFrameData();
        int inummeshes = pmodel->numMeshes();
        for (int imesh = 0; imesh < inummeshes; imesh++) {
          const ork::lev2::XgmMesh& mesh = *pmodel->mesh(imesh);
          int inumclusset                = mesh.numSubMeshes();
          for (int ics = 0; ics < inumclusset; ics++) {
            const ork::lev2::XgmSubMesh& submesh   = *mesh.subMesh(ics);
            const ork::lev2::GfxMaterial* material = submesh.mpMaterial;
            int inumclus                           = submesh.miNumClusters;
            for (int ic = 0; ic < inumclus; ic++) {
              bool btest                           = true;
              const ork::lev2::XgmCluster& cluster = submesh.cluster(ic);

              ork::fcolor4 ModColor = pren->GetModColor();

              if (pyo->mpLight) {
                ModColor *= pyo->mpLight->GetColor();
              }
              ork::fcolor4 ObjColor;
              ObjColor.SetRGBAU32(reinterpret_cast<U32>(pren->GetObject()));

              ork::fcolor4 color = targ->FBI()->isPickState() ? ObjColor : ModColor;

              ork::fmtx4 mtxw = xf.GetTransform()->GetMatrix();
              ork::lev2::RenderContextInstModelData MdlCtx;

              MdlCtx.mMesh    = &mesh;
              MdlCtx.mSubMesh = &submesh;
              MdlCtx.mCluster = &cluster;

              MdlCtx.SetSkinned(false);

              ork::lev2::RenderContextInstData ovrcid = rcid;
              ovrcid.SetLightingGroup(0);
              ovrcid.SetMaterialIndex(0);

              pmodel->RenderRigid(color, mtxw, targ, ovrcid, MdlCtx);
            }
          }
        }
      }
#endif
    }
    static void doit(ork::lev2::RenderContextInstData& rcid, ork::lev2::Context* targ, const ork::lev2::CallbackRenderable* pren) {
      const yo* pyo = pren->GetUserData0().Get<const yo*>();

      if (pyo->_light) {
        auto ppntlight = dynamic_cast<ork::lev2::PointLight*>(pyo->_light);
        if (ppntlight) {
          draw_tricircle(rcid, targ, pren, ppntlight);
        } else {
          draw_dirlight(rcid, targ, pren);
        }
      } else {
        draw_dirlight(rcid, targ, pren);
      }
    }
    yo() {
      auto model_asset = ork::asset::AssetManager<ork::lev2::XgmModelAsset>::load("data://editor/dirlight");
      mpModel          = model_asset.get();
    }
  };

#if 0 // DRAWTHREADS
	yo* pyo = new yo;
	pyo->_entity = pent;
	pyo->_light = GetLight();

	ork::ent::CallbackDrawable* pdrw = new ork::ent::CallbackDrawable(pent);
	pent->addDrawableToDefaultLayer(pdrw);
	pdrw->SetCallback( yo::doit );
	pdrw->SetOwner( & pent->GetEntData() );
	pdrw->SetData( (const yo*) pyo );
#endif

  /////////////////////////////////////////////////

  /////////////////////////////////////////////////
}

bool LightingComponentInst::DoLink(ork::ent::Simulation* psi) {

  auto lmi = psi->findSystem<ent::LightingSystem>();
  if (nullptr == lmi)
    return false;

  ork::lev2::LightManager& lightmanager = lmi->GetLightManager();
  bool bisdyn                           = _lcdata.isDynamic();

  auto light_instance = GetLight();
  if (light_instance) {
    light_instance->_dynamic = bisdyn;
    switch (light_instance->LightType()) {
      case ork::lev2::ELIGHTTYPE_SPOT:
      case ork::lev2::ELIGHTTYPE_POINT:
      case ork::lev2::ELIGHTTYPE_DIRECTIONAL:
      case ork::lev2::ELIGHTTYPE_AMBIENT: {
        if (bisdyn)
          lightmanager.mGlobalMovingLights.AddLight(light_instance);
        else
          lightmanager.mGlobalStationaryLights.AddLight(light_instance);
      } break;
    }
  }
  return true;
}

LightingComponentInst::~LightingComponentInst() {
  if (auto lmi = GetEntity()->simulation()->findSystem<ent::LightingSystem>()) {

    ork::lev2::LightManager& lightmanager = lmi->GetLightManager();

    ork::lev2::LightContainer& global_container = lightmanager.mGlobalMovingLights;

    if (_light) {
      global_container.RemoveLight(_light);
    }
  }
  if (_light)
    delete _light;
}

void LightingComponentInst::DoUpdate(ork::ent::Simulation* inst) {
  if (_lcdata.isDynamic()) {
    if (_light) {
      //_light->_worldMatrix = _entity->GetEffectiveMatrix();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingSystemData::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

LightingSystemData::LightingSystemData() {
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::System* LightingSystemData::createSystem(ork::ent::Simulation* pinst) const {
  return new LightingSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

LightingSystem::LightingSystem(const LightingSystemData& data, ork::ent::Simulation* pinst)
    : ork::ent::System(&data, pinst)
    , mLightManager(data.Lmd()) {
}

///////////////////////////////////////////////////////////////////////////////

void LightArchetype::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

LightArchetype::LightArchetype() {
}

///////////////////////////////////////////////////////////////////////////////

void LightArchetype::DoCompose(ork::ent::ArchComposer& composer) {
  composer.Register<ork::ent::LightingComponentData>();
}
void LightingComponentData::DoRegisterWithScene(ork::ent::SceneComposer& sc) {
  sc.Register<ork::ent::LightingSystemData>();
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
template const ork::ent::LightingComponentData* ork::ent::EntData::GetTypedComponent<ork::ent::LightingComponentData>() const;
template ork::ent::LightingComponentInst* ork::ent::Entity::GetTypedComponent<ork::ent::LightingComponentInst>(bool);
