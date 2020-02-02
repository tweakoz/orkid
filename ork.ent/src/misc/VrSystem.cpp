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
  c->memberProperty("VisualOffset", &VrSystemData::_visualoffset);
  c->memberProperty("UseCameraView", &VrSystemData::_useCamView);
  // todo - property annotation which pops up a choicelist with the current set of entities
}

///////////////////////////////////////////////////////////////////////////////

VrSystemData::VrSystemData()
    : _vrTrackedObject(AddPooledString("vrtrackedobject"))
    , _vrCamera(AddPooledString("vrcamera"))
    , _useCamView(false) {
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

///////////////////////////////////////////////////////////////////////////////

bool VrSystem::enabled() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

VrSystem::~VrSystem() {
}

///////////////////////////////////////////////////////////////////////////////

void VrSystem::DoUpdate(Simulation* psim) {
}

///////////////////////////////////////////////////////////////////////////////

void VrSystem::enqueueDrawables(lev2::DrawableBuffer& buffer) {
  if (_vrstate != 0) {
    //////////////////////////////////////////////////
    // copy vr matrix from updthread to renderthread
    //////////////////////////////////////////////////
    fmtx4 mtx = _trackedObject ? _trackedObject->GetEffectiveMatrix() : fmtx4();
    fvec3 pos;
    fquat rot;
    float scal;
    mtx.decompose(pos, rot, scal);
    _trackedMatrix.compose(pos + _vrSystemData.visualOffset(), fquat(), 1.0f);
    buffer.setPreRenderCallback(0, [=](lev2::RenderContextFrameData& RCFD) {
      //
      RCFD.setUserProperty("vrcam"_crc, this->_vrCamDat);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

bool VrSystem::DoLink(Simulation* psim) {
  auto& vrdev       = lev2::orkidvr::device();
  vrdev._calibstate = 0;
  _trackedObject    = psim->FindEntity(AddPooledString(_vrSystemData.vrTrackedObject()));
  _vrCamDat         = psim->cameraData(AddPooledString(_vrSystemData.vrCamera()));
  bool good2go      = (_vrCamDat != nullptr);
  _vrstate          = int(good2go);
  if (good2go) {
    _baseCamDat = *_vrCamDat;
    auto cammtx = _baseCamDat.computeMatrices(1);
    fquat r;
    r.FromAxisAngle(fvec4(0, 1, 0, PI));
    fmtx4 offsetmtx   = cammtx._vmatrix * r.ToMatrix();
    vrdev._usermtxgen = [=]() -> fmtx4 {
      return _vrSystemData.useCamView() //
                 ? (offsetmtx * _trackedMatrix.inverse())
                 : _trackedMatrix.inverse();
    };
  }
  return good2go or (false == enabled());
}

///////////////////////////////////////////////////////////////////////////////

void VrSystem::DoUnLink(Simulation* psim) {
  lev2::orkidvr::device()._usermtxgen = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
