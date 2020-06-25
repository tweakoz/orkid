////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/kernel/orklut.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/properties/AccessorPropertyType.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
///////////////////////////////////////////////////////////////////////////////
#include "SpinnyCamera.h"
#include <ork/kernel/string/string.h>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SequenceCamArchetype, "SequenceCamArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SequenceCamControllerData, "SequenceCamControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SequenceCamControllerInst, "SequenceCamControllerInst");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SeqCamItemDataBase, "SeqCamItemDataBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SpinnyCamControllerData, "SpinnyCamControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CurvyCamControllerData, "CurvyCamControllerData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void SequenceCamArchetype::DoCompose(ork::ent::ArchComposer& composer) {
  composer.Register<ork::ent::SequenceCamControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void SequenceCamArchetype::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

SequenceCamArchetype::SequenceCamArchetype() {
}

///////////////////////////////////////////////////////////////////////////////

void SequenceCamControllerData::Describe() {
  ork::reflect::RegisterMapProperty("CamItems", &SequenceCamControllerData::mItemDatas);
  ork::reflect::annotatePropertyForEditor<SequenceCamControllerData>("CamItems", "editor.factorylistbase", "SeqCamItemDataBase");
  ork::reflect::annotatePropertyForEditor<SequenceCamControllerData>("CamItems", "editor.map.policy.impexp", "true");

  ork::reflect::RegisterProperty("CurrentItem", &SequenceCamControllerData::mCurrentItem);
}

SequenceCamControllerData::SequenceCamControllerData() {
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* SequenceCamControllerData::createComponent(ent::Entity* pent) const {
  return new SequenceCamControllerInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void SequenceCamControllerInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

SequenceCamControllerInst::SequenceCamControllerInst(const SequenceCamControllerData& occd, Entity* pent)
    : ComponentInst(&occd, pent)
    , mCD(occd)
    , mpActiveItem(0) {
}

SequenceCamControllerInst::~SequenceCamControllerInst() {
}

///////////////////////////////////////////////////////////////////////////////

bool SequenceCamControllerInst::DoLink(Simulation* psi) {
  // printf( "LINKING SpinnyCamControllerInst\n" );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool SequenceCamControllerInst::DoStart(Simulation* psi, const fmtx4& world) {
  if (GetEntity() and _cameraData) {
    std::string camname = GetEntity()->name().c_str();
    psi->setCameraData(camname, _cameraData);

    for (orklut<PoolString, ork::Object*>::const_iterator it = GetCD().GetItemDatas().begin(); it != GetCD().GetItemDatas().end();
         it++) {
      if (it->second) {
        SeqCamItemDataBase* pdata = rtti::autocast(it->second);
        SeqCamItemInstBase* item  = pdata->CreateInst(GetEntity());

        mItemInsts.AddSorted(it->first, item);
        mpActiveItem = item;
      }
    }
  }

  // printf( "STARTING SpinnyCamControllerInst\n" );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void SequenceCamControllerInst::DoUpdate(Simulation* psi) {
  const PoolString& ps                                       = mCD.GetCurrentItem();
  orklut<PoolString, SeqCamItemInstBase*>::const_iterator it = mItemInsts.find(ps);

  if (it != mItemInsts.end()) {
    mpActiveItem = it->second;
  }

  if (mpActiveItem) {
    // printf( "SequenceCamControllerInst<%p> ActiveItem<%s:%p>\n", this, ps.c_str(), mpActiveItem );
    mpActiveItem->DoUpdate(psi);
    _cameraData = mpActiveItem->cameraData();
  }
}

///////////////////////////////////////////////////////////////////////////////

bool SequenceCamControllerInst::DoNotify(const ork::event::Event* event) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void SeqCamItemDataBase::Describe() {
}

SeqCamItemDataBase::SeqCamItemDataBase() {
}

///////////////////////////////////////////////////////////////////////////////

SeqCamItemInstBase::SeqCamItemInstBase(const SeqCamItemDataBase& cd)
    : mCD(cd) {
}
SeqCamItemInstBase::~SeqCamItemInstBase() {
}
///////////////////////////////////////////////////////////////////////////////

void SpinnyCamControllerData::Describe() {
  ork::reflect::RegisterProperty("Elevation", &SpinnyCamControllerData::mfElevation);
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Elevation", "editor.range.min", "-100.0f");
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Elevation", "editor.range.max", "100.0f");

  ork::reflect::RegisterProperty("Radius", &SpinnyCamControllerData::mfRadius);
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Radius", "editor.range.min", "-1000.0f");
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Radius", "editor.range.max", "1000.0f");

  ork::reflect::RegisterProperty("SpinRate", &SpinnyCamControllerData::mfSpinRate);
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("SpinRate", "editor.range.min", "-100.0f");
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("SpinRate", "editor.range.max", "100.0f");

  ork::reflect::RegisterProperty("Near", &SpinnyCamControllerData::mfNear);
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Near", "editor.range.min", "0.1f");
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Near", "editor.range.max", "1000.0f");

  ork::reflect::RegisterProperty("Far", &SpinnyCamControllerData::mfFar);
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Far", "editor.range.min", "1.0f");
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Far", "editor.range.max", "10000.0f");

  ork::reflect::RegisterProperty("Aper", &SpinnyCamControllerData::mfAper);
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Aper", "editor.range.min", "15.0f");
  ork::reflect::annotatePropertyForEditor<SpinnyCamControllerData>("Aper", "editor.range.max", "160.0f");
}

SpinnyCamControllerData::SpinnyCamControllerData()
    : mfSpinRate(1.0f)
    , mfElevation(0.0f)
    , mfRadius(1.0f)
    , mfNear(1.0f)
    , mfFar(100.0f)
    , mfAper(45.0f) {
}

SeqCamItemInstBase* SpinnyCamControllerData::CreateInst(ork::ent::Entity* pent) const {
  SpinnyCamControllerInst* pret = new SpinnyCamControllerInst(*this, pent);
  return pret;
}

///////////////////////////////////////////////////////////////////////////////

SpinnyCamControllerInst::SpinnyCamControllerInst(const SpinnyCamControllerData& cd, ork::ent::Entity* pent)
    : SeqCamItemInstBase(cd)
    , mSCCD(cd)
    , mfPhase(0.0f) {
}
SpinnyCamControllerInst::~SpinnyCamControllerInst() {
}
void SpinnyCamControllerInst::DoUpdate(ent::Simulation* psi) {
  mfPhase += mSCCD.GetSpinRate() * psi->GetDeltaTime();

  _cameraData->Persp(mSCCD.GetNear(), mSCCD.GetFar(), mSCCD.GetAper());

  float famp = mSCCD.GetRadius();
  float fx   = sinf(mfPhase) * famp;
  float fy   = -cosf(mfPhase) * famp;

  fvec3 eye(fx, mSCCD.GetElevation(), fy);
  fvec3 tgt(0.0f, 0.0f, 0.0f);
  fvec3 up(0.0f, 1.0f, 0.0f);
  _cameraData->Lookat(eye, tgt, up);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CurvyCamControllerData::Describe() {
  ork::reflect::RegisterProperty("Elevation", &CurvyCamControllerData::mfElevation);
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Elevation", "editor.range.min", "-100.0f");
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Elevation", "editor.range.max", "100.0f");

  ork::reflect::RegisterProperty("Radius", &CurvyCamControllerData::mfRadius);
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Radius", "editor.range.min", "-1000.0f");
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Radius", "editor.range.max", "1000.0f");

  ork::reflect::RegisterProperty("Angle", &CurvyCamControllerData::mfAngle);
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Angle", "editor.range.min", "-100.0f");
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Angle", "editor.range.max", "100.0f");

  ork::reflect::RegisterProperty("Near", &CurvyCamControllerData::mfNear);
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Near", "editor.range.min", "0.1f");
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Near", "editor.range.max", "1000.0f");

  ork::reflect::RegisterProperty("Far", &CurvyCamControllerData::mfFar);
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Far", "editor.range.min", "1.0f");
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Far", "editor.range.max", "10000.0f");

  ork::reflect::RegisterProperty("Aper", &CurvyCamControllerData::mfAper);
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Aper", "editor.range.min", "15.0f");
  ork::reflect::annotatePropertyForEditor<CurvyCamControllerData>("Aper", "editor.range.max", "160.0f");

  ork::reflect::RegisterProperty("RadiusCurve", &CurvyCamControllerData::RadiusCurveAccessor);
}

CurvyCamControllerData::CurvyCamControllerData()
    : mfAngle(0.0f)
    , mfElevation(0.0f)
    , mfRadius(1.0f)
    , mfNear(1.0f)
    , mfFar(100.0f)
    , mfAper(45.0f) {
}

SeqCamItemInstBase* CurvyCamControllerData::CreateInst(ork::ent::Entity* pent) const {
  CurvyCamControllerInst* pret = new CurvyCamControllerInst(*this, pent);
  return pret;
}

///////////////////////////////////////////////////////////////////////////////

CurvyCamControllerInst::CurvyCamControllerInst(const CurvyCamControllerData& cd, ork::ent::Entity* pent)
    : SeqCamItemInstBase(cd)
    , mCCCD(cd)
    , mfPhase(0.0f) {
}
CurvyCamControllerInst::~CurvyCamControllerInst() {
}

void CurvyCamControllerInst::DoUpdate(ent::Simulation* psi) {
  mfPhase += mCCCD.GetAngle() * psi->GetDeltaTime();

  _cameraData->Persp(mCCCD.GetNear(), mCCCD.GetFar(), mCCCD.GetAper());

  float famp = mCCCD.GetRadius();
  float fx   = sinf(mfPhase) * famp;
  float fy   = -cosf(mfPhase) * famp;

  fvec3 eye(fx, mCCCD.GetElevation(), fy);
  fvec3 tgt(0.0f, 0.0f, 0.0f);
  fvec3 up(0.0f, 1.0f, 0.0f);
  _cameraData->Lookat(eye, tgt, up);
}

}} // namespace ork::ent
