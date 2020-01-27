////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/scene.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/kernel/string/deco.inl>
///////////////////////////////////////////////////////////////////////////////
//#include "ModelArchetype.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ModelComponentData, "ModelComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ModelComponentInst, "ModelComponentInst");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ModelComponentData::Describe() {
  ork::reflect::RegisterProperty("Offset", &ModelComponentData::mOffset);
  ork::reflect::RegisterProperty("Rotate", &ModelComponentData::mRotate);

  reflect::RegisterProperty("Model", &ModelComponentData::mModel);
  reflect::RegisterProperty("ShowBoundingSphere", &ModelComponentData::mbShowBoundingSphere);
  reflect::RegisterProperty("EditorDagDebug", &ModelComponentData::mbCopyDag);

  ork::reflect::annotatePropertyForEditor<ModelComponentData>("Model", "editor.class", "ged.factory.assetlist");
  ork::reflect::annotatePropertyForEditor<ModelComponentData>("Model", "editor.assettype", "xgmodel");
  ork::reflect::annotatePropertyForEditor<ModelComponentData>("Model", "editor.assetclass", "xgmodel");

  ork::reflect::RegisterMapProperty("MaterialOverrides", &ModelComponentData::_materialOverrides);
  // ork::reflect::annotatePropertyForEditor<ModelComponentData>("MaterialOverrides", "editor.assettype", "FxShader");
  // ork::reflect::annotatePropertyForEditor<ModelComponentData>("MaterialOverrides", "editor.assetclass", "FxShader");

  ork::reflect::RegisterProperty("AlwaysVisible", &ModelComponentData::mAlwaysVisible);
  ork::reflect::RegisterProperty("Scale", &ModelComponentData::mfScale);
  ork::reflect::RegisterProperty("BlenderZup", &ModelComponentData::mBlenderZup);

  reflect::annotatePropertyForEditor<ModelComponentData>("Scale", "editor.range.min", "-1000.0");
  reflect::annotatePropertyForEditor<ModelComponentData>("Scale", "editor.range.max", "1000.0");
  // reflect::annotatePropertyForEditor<ModelComponentData>( "Scale", "editor.range.log", "true" );
}

///////////////////////////////////////////////////////////////////////////////

lev2::XgmModel* ModelComponentData::GetModel() const {
  return (mModel == 0) ? 0 : mModel->GetModel();
}
void ModelComponentData::SetModel(lev2::XgmModelAsset* passet) {
  mModel = passet;
}

///////////////////////////////////////////////////////////////////////////////

ModelComponentData::ModelComponentData()
    : mModel(0)
    , mAlwaysVisible(false)
    , mfScale(1.0f)
    , mRotate(0.0f, 0.0f, 0.0f)
    , mOffset(0.0f, 0.0f, 0.0f)
    , mbShowBoundingSphere(false)
    , mbCopyDag(false)
    , mBlenderZup(false) {
}

///////////////////////////////////////////////////////////////////////////////

ComponentInst* ModelComponentData::createComponent(Entity* pent) const {
  ComponentInst* pinst = OrkNew ModelComponentInst(*this, pent);
  return pinst;
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentData::SetModelAccessor(ork::rtti::ICastable* const& model) {
  mModel = model ? ork::rtti::safe_downcast<ork::lev2::XgmModelAsset*>(model) : 0;
}
void ModelComponentData::GetModelAccessor(ork::rtti::ICastable*& model) const {
  model = mModel;
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ModelComponentInst::~ModelComponentInst() {
  if (mXgmModelInst)
    delete mXgmModelInst;
}
ModelComponentInst::ModelComponentInst(const ModelComponentData& data, Entity* pent)
    : ComponentInst(&data, pent)
    , mData(data)
    , mXgmModelInst(0) {

  mModelDrawable        = new lev2::ModelDrawable(pent); // deleted when entity deleted
  lev2::XgmModel* model = data.GetModel();

  if (model) {

    fvec3 whi(1, 1, 1);
    fvec3 red(1, 0, 0);
    fvec3 mag(1, 0, 1);
    fvec3 cyn(0, 1, 1);

    mXgmModelInst = new lev2::XgmModelInst(model);

    mModelDrawable->SetModelInst(mXgmModelInst);
    mModelDrawable->SetScale(mData.GetScale());

    pent->addDrawableToDefaultLayer(mModelDrawable);
    mModelDrawable->SetOwner(pent);

    mXgmModelInst->SetBlenderZup(mData.IsBlenderZup());
    auto& localpose = mXgmModelInst->RefLocalPose();
    localpose.BindPose();

    deco::printf(whi, "ModelComponentInst<%p> constructor:\n", this);
    deco::printe(red, "init localpose (bind)", true);
    deco::prints(localpose.dumpc(red), true);
    localpose.BuildPose();
    deco::printe(mag, "init localpose (pre-concat)", true);
    deco::prints(localpose.dumpc(mag), true);
    localpose.Concatenate();
    deco::printe(cyn, "init localpose (post-concat)", true);
    deco::prints(localpose.dumpc(cyn), true);

    auto& ovmap = mData.MaterialOverrideMap();

    for (auto it : ovmap) {
      std::string mtlvaluename = it.second.c_str();
      if (0 == strcmp(it.first.c_str(), "all")) {
        auto overridemtl = new lev2::PBRMaterial();
        overridemtl->setTextureBaseName(mtlvaluename);
        mXgmModelInst->_overrideMaterial = overridemtl;
      }
      // lev2::FxShaderAsset* passet = it.second;

      // if (passet && passet->IsLoaded()) {
      // lev2::FxShader* pfxshader = passet->GetFxShader();

      // if (pfxshader) {
      // lev2::GfxMaterialFx* pfxmaterial = new lev2::GfxMaterialFx();
      // pfxmaterial->SetEffect(pfxshader);
      //}
      //}
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentInst::DoUpdate(ork::ent::Simulation* psi) {
  if (mData.IsCopyDag()) {
    // mEntity->GetEntData().GetDagNode()
    GetEntity()->GetDagNode() = GetEntity()->data()->GetDagNode();
  }

  mModelDrawable->SetScale(mData.GetScale());
  mModelDrawable->SetRotate(mData.GetRotate());
  mModelDrawable->SetOffset(mData.GetOffset());
  mModelDrawable->ShowBoundingSphere(mData.ShowBoundingSphere());
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentInst::DoStop(ork::ent::Simulation* psi) {
  auto& dw = modelDrawable();
  dw.Disable();
}

void ModelComponentInst::doNotify(const ComponentEvent& e) {
  if (e._eventID == "yo") {
    _yo                             = true;
    auto& dw                        = modelDrawable();
    GetEntity()->_renderMtxProvider = [this]() -> fmtx4 {
      auto m = GetEntity()->GetEffectiveMatrix();
      auto t = m.GetTranslation() + fvec3(0, 2, 0);
      fmtx4 outmtx;
      outmtx.SetTranslation(t);
      return outmtx;
    };
  }
}

bool ModelComponentInst::DoNotify(const ork::event::Event* event) {
  if (const event::MeshEnableEvent* meshenaev = ork::rtti::autocast(event)) {
    if (modelDrawable().GetModelInst()) {
      if (meshenaev->IsEnable())
        modelDrawable().GetModelInst()->EnableMesh(meshenaev->GetName());
      else
        modelDrawable().GetModelInst()->DisableMesh(meshenaev->GetName());
      return true;
    }
  } else if (const event::MeshLayerFxEvent* lfxev = ork::rtti::autocast(event)) {
    if (modelDrawable().GetModelInst()) {
      /*lev2::GfxMaterialFx* pmaterial = 0;
      if (lfxev->IsEnable()) {
        orklut<PoolString, lev2::GfxMaterialFx*>::const_iterator it = mFxMaterials.find(lfxev->GetName());
        if (it != mFxMaterials.end()) {
          lev2::GfxMaterialFx* pmaterial = it->second;
        }
      }
      modelDrawable().GetModelInst()->_overrideMaterial = pmaterial;*/
      return true;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
