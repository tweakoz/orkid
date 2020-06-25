////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/scene.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/reflect/properties/AccessorPropertyType.hpp>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "Skybox.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SkyBoxArchetype, "SkyBoxArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SkyBoxControllerInst, "SkyBoxControllerInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SkyBoxControllerData, "SkyBoxControllerData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void SkyBoxArchetype::Describe() {
}

SkyBoxArchetype::SkyBoxArchetype() {
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxArchetype::DoLinkEntity(Simulation* psi, Entity* pent) const {
  struct yo {
    const SkyBoxArchetype* parch;
    Entity* pent;

    static void doit(lev2::RenderContextInstData& RCID) {
      auto renderable = dynamic_cast<const lev2::CallbackRenderable*>(RCID._dagrenderable);
      auto context    = RCID.context();
      auto yo_ptr     = renderable->GetUserData0().Get<const yo*>();

      const SkyBoxArchetype* parch     = yo_ptr->parch;
      const Entity* pent               = yo_ptr->pent;
      const SkyBoxControllerInst* ssci = pent->GetTypedComponent<SkyBoxControllerInst>();
      const SkyBoxControllerData& cd   = ssci->GetCD();
      bool isPickState                 = RCID.GetRenderer()->GetTarget()->FBI()->isPickState();
      float fphase                     = ssci->GetPhase();

      if (cd.GetModel()) {
        ork::lev2::XgmModelInst minst(cd.GetModel());
        ork::lev2::RenderContextInstData MatCtx;
        ork::lev2::RenderContextInstModelData MdlCtx;
        ork::lev2::XgmMaterialStateInst MatInst(minst);
        MatCtx.SetMaterialInst(&MatInst);
        MatCtx.SetRenderer(RCID.GetRenderer());
        MdlCtx.SetSkinned(false);
        ///////////////////////////////////////////////////////////
        // picking support
        ///////////////////////////////////////////////////////////
        fcolor4 ObjColor;
        fcolor4 color   = context->FBI()->isPickState() ? ObjColor : renderable->GetModColor();
        const auto& CPD = RCID._RCFD->topCPD();
        ///////////////////////////////////////////////////////////
        // setup headlight (default lighting)
        ///////////////////////////////////////////////////////////
        float fscale = cd.GetScale();
        fvec3 pos    = CPD.cameraMatrices()->_camdat.GetEye();
        fmtx4 mtxSPIN;
        mtxSPIN.RotateY(fphase);
        fmtx4 mtxSKY;
        mtxSKY.SetScale(fscale);
        mtxSKY.SetTranslation(pos);
        mtxSKY         = mtxSPIN * mtxSKY;
        int inummeshes = cd.GetModel()->numMeshes();
        for (int imesh = 0; imesh < inummeshes; imesh++) {
          const lev2::XgmMesh& mesh = *cd.GetModel()->mesh(imesh);
          int inumclusset           = mesh.numSubMeshes();
          for (int ics = 0; ics < inumclusset; ics++) {
            const lev2::XgmSubMesh& submesh = *mesh.subMesh(ics);
            auto material                   = submesh._material;
            int inumclus                    = submesh._clusters.size();
            MatCtx.SetMaterialIndex(ics);
            for (int ic = 0; ic < inumclus; ic++) {
              MdlCtx.mMesh    = &mesh;
              MdlCtx.mSubMesh = &submesh;
              MdlCtx._cluster = submesh.cluster(ic);
              cd.GetModel()->RenderRigid(color, mtxSKY, context, MatCtx, MdlCtx);
            }
          }
        }
      }
    }
  };

  auto pdrw = std::make_shared<lev2::CallbackDrawable>(pent);
  pent->addDrawableToDefaultLayer(pdrw);
  pdrw->SetRenderCallback(yo::doit);
  pdrw->SetOwner(pent->data());
  pdrw->SetSortKey(0);

  yo* pyo    = new yo;
  pyo->parch = this;
  pyo->pent  = pent;

  lev2::Drawable::var_t ap;
  ap.Set<const yo*>(pyo);
  pdrw->SetUserDataA(ap);
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxArchetype::DoCompose(ork::ent::ArchComposer& composer) {
  composer.Register<SkyBoxControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxControllerData::Describe() {
  reflect::RegisterProperty("SpinRate", &SkyBoxControllerData::mfSpinRate);

  reflect::annotatePropertyForEditor<SkyBoxControllerData>("SpinRate", "editor.range.min", "-6.28");
  reflect::annotatePropertyForEditor<SkyBoxControllerData>("SpinRate", "editor.range.max", "6.28");

  reflect::RegisterProperty("Model", &SkyBoxControllerData::mModelAsset);

  ork::reflect::annotatePropertyForEditor<SkyBoxControllerData>("Model", "editor.class", "ged.factory.assetlist");
  ork::reflect::annotatePropertyForEditor<SkyBoxControllerData>("Model", "editor.assettype", "xgmodel");
  ork::reflect::annotatePropertyForEditor<SkyBoxControllerData>("Model", "editor.assetclass", "xgmodel");

  ork::reflect::RegisterProperty("Scale", &SkyBoxControllerData::mfScale);

  reflect::annotatePropertyForEditor<SkyBoxControllerData>("Scale", "editor.range.min", "-1000.0");
  reflect::annotatePropertyForEditor<SkyBoxControllerData>("Scale", "editor.range.max", "1000.0");
}

///////////////////////////////////////////////////////////////////////////////

SkyBoxControllerData::SkyBoxControllerData()
    : mfSpinRate(0.0f)
    , mModelAsset(0)
    , mfScale(1.0f) {
}

///////////////////////////////////////////////////////////////////////////////

lev2::XgmModel* SkyBoxControllerData::GetModel() const {
  lev2::XgmModel* pmodel = (mModelAsset != 0) ? mModelAsset->GetModel() : 0;
  return pmodel;
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxControllerInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

SkyBoxControllerInst::SkyBoxControllerInst(const SkyBoxControllerData& data, ent::Entity* pent)
    : ork::ent::ComponentInst(&data, pent)
    , mCD(data)
    , mPhase(0.0f) {
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* SkyBoxControllerData::createComponent(ent::Entity* pent) const {
  return OrkNew SkyBoxControllerInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxControllerInst::DoUpdate(ent::Simulation* sinst) {
  mPhase += mCD.GetSpinRate() * sinst->GetDeltaTime();
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
