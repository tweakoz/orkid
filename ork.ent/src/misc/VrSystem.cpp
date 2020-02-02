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
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/vr/vr.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include "VrSystem.h"

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::ent::VrSystemData, "VrSystemData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void VrSystemData::describeX(class_t* c) {
  c->memberProperty("TrackingObjectEntity", &VrSystemData::_vrTrackedObject);
  c->memberProperty("VrCameraEntity", &VrSystemData::_vrCamera);
  // todo - property annotation which pops up a choicelist with the current set of entities
}

///////////////////////////////////////////////////////////////////////////////

VrSystemData::VrSystemData()
    : _vrTrackedObject(AddPooledString("vrtrackedobject"))
    , _vrCamera(AddPooledString("vrcamera")) {
}

void VrSystemData::defaultSetup() {
}
///////////////////////////////////////////////////////////////////////////////

ork::ent::System* VrSystemData::createSystem(ork::ent::Simulation* pinst) const {
  return new VrSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VrSystem::VrSystem(const VrSystemData& data, Simulation* psim)
    : ork::ent::System(&data, psim)
    , _vrSystemData(data)
    , _vrCamDat(nullptr) {
  _vrstate = 0;
}

bool VrSystem::enabled() const {
  return true;
}

VrSystem::~VrSystem() {
}

void VrSystem::DoUpdate(Simulation* psim) {
}

void VrSystem::enqueueDrawables(lev2::DrawableBuffer& buffer) {
  if (_vrstate != 0) {
    //////////////////////////////////////////////////
    // copy vr matrix from updthread to renderthread
    //////////////////////////////////////////////////
    fmtx4 vrmtx; // = this->_trackedObject->GetEffectiveMatrix();
    buffer.setPreRenderCallback(0, [=](lev2::RenderContextFrameData& RCFD) {
      RCFD.setUserProperty("vrroot"_crc, vrmtx);
      RCFD.setUserProperty("vrcam"_crc, this->_vrCamDat);
    });
  }
}

bool VrSystem::DoLink(Simulation* psim) {

  lev2::orkidvr::device()._calibstate = 0;

  _trackedObject = psim->FindEntity(AddPooledString(_vrSystemData.vrTrackedObject()));
  _vrCamDat      = psim->cameraData(AddPooledString(_vrSystemData.vrCamera()));
  bool good2go   = (_trackedObject != nullptr) and (_vrCamDat != nullptr);
  _vrstate       = int(good2go);
  if (good2go) {
    _baseCamDat = *_vrCamDat;
    auto& uoff  = lev2::orkidvr::device()._userOffsetMatrix;
    auto cammtx = _baseCamDat.computeMatrices(1);

    fquat r;
    r.FromAxisAngle(fvec4(0, 1, 0, PI));

    uoff = cammtx._vmatrix * r.ToMatrix();
  }
  return good2go or (false == enabled());
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
