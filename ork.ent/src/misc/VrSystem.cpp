////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterPropertyX.inl>
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
#include <ork/reflect/properties/DirectTyped.hpp>
#include "VrSystem.h"

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::ent::VrSystemData, "VrSystemData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
using namespace ork::lev2::orkidvr;
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
    , _vrCamera("vrcamera")
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
  _vrCamDat         = psim->cameraData(_vrSystemData.vrCamera());
  bool good2go      = (_vrCamDat != nullptr);
  _vrstate          = int(good2go);
  if (good2go) {

    ////////////////////////////////////////////////////////
    // view matrix generator
    ////////////////////////////////////////////////////////

    _baseCamDat = *_vrCamDat;
    auto cammtx = _baseCamDat.computeMatrices(1);
    fquat r;
    r.fromAxisAngle(fvec4(0, 1, 0, PI));
    fmtx4 offsetmtx   = cammtx._vmatrix * r.ToMatrix();
    vrdev._usermtxgen = [=]() -> fmtx4 {
      return _vrSystemData.useCamView() //
                 ? (offsetmtx * _trackedMatrix.inverse()) * _headingMatrix
                 : _trackedMatrix.inverse() * _headingMatrix;
    };

    ////////////////////////////////////////////////////////
    // turn with thumb buttons
    //  TODO move somewhere more appropriate
    ////////////////////////////////////////////////////////

    _trackingnotif            = std::make_shared<VrTrackingNotificationReceiver>();
    _trackingnotif->_callback = [this](const svar256_t& msg) {
      if (auto as_ctrlr = msg.TryAs<VrTrackingControllerNotificationFrame>()) {
        auto ctrlr = as_ctrlr.value();
        if (ctrlr._left->_buttonThumbGatedDown) {
          fquat q;
          q.fromAxisAngle(fvec4(0, 1, 0, -PI / 12.0));
          _headingMatrix = _headingMatrix * q.ToMatrix();
        } else if (ctrlr._right->_buttonThumbGatedDown) {
          fquat q;
          q.fromAxisAngle(fvec4(0, 1, 0, PI / 12.0));
          _headingMatrix = _headingMatrix * q.ToMatrix();
        }
#if 0
        fmtx4 xlate;
        float xlaterate = 12.0 / 80.0;

        if (LCONTROLLER->_button1Down) {
          xlate.SetTranslation(0, xlaterate, 0);
          auto trans = (xlate * _rotMatrix).GetTranslation();
          printf("trans<%g %g %g>\n", trans.x, trans.y, trans.z);
          xlate.SetTranslation(trans);
          _offsetmatrix = _offsetmatrix * xlate;
        }
        if (LCONTROLLER->_button2Down) {
          xlate.SetTranslation(0, -xlaterate, 0);
          auto trans = (xlate * _rotMatrix).GetTranslation();
          printf("trans<%g %g %g>\n", trans.x, trans.y, trans.z);
          xlate.SetTranslation(trans);
          _offsetmatrix = _offsetmatrix * xlate;
        }
        ///////////////////////////////////////////////////////////
        // fwd back
        ///////////////////////////////////////////////////////////
        if (RCONTROLLER->_button1Down) {
          xlate.SetTranslation(0, 0, xlaterate);
          auto trans = (xlate * _rotMatrix).GetTranslation();
          xlate.SetTranslation(trans);
          _offsetmatrix = _offsetmatrix * xlate;
        }
        if (RCONTROLLER->_button2Down) {
          xlate.SetTranslation(0, 0, -xlaterate);
          auto trans = (xlate * _rotMatrix).GetTranslation();
          xlate.SetTranslation(trans);
          _offsetmatrix = _offsetmatrix * xlate;
        }
#endif
      }
    };
    lev2::orkidvr::addVrTrackingNotificationReceiver(_trackingnotif);
  }

  ////////////////////////////////////////////////////////

  return good2go or (false == enabled());
}

///////////////////////////////////////////////////////////////////////////////

void VrSystem::DoUnLink(Simulation* psim) {
  lev2::orkidvr::device()._usermtxgen = nullptr;
  if (_trackingnotif) {
    removeVrTrackingNotificationReceiver(_trackingnotif);
  }
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
