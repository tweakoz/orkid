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
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/string/string.h>
#include <pkg/ent/EditorCamera.h>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorCamArchetype, "EditorCamArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorCamControllerData, "EditorCamControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorCamControllerInst, "EditorCamControllerInst");
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void EditorCamArchetype::DoCompose(ork::ent::ArchComposer& composer) {
  composer.Register<ork::ent::EditorCamControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamArchetype::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

EditorCamArchetype::EditorCamArchetype() {
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamControllerData::Describe() {
  ork::ent::RegisterFamily<EditorCamControllerData>(ork::AddPooledLiteral("camera"));
  ork::reflect::RegisterProperty("Camera", &EditorCamControllerData::CameraAccessor);
}

///////////////////////////////////////////////////////////////////////////////

EditorCamControllerData::EditorCamControllerData() {
  _camera = new EzUiCam;

  _camera->mfLoc = 1.0f;

  _camera->SetName("persp");
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* EditorCamControllerData::createComponent(ent::Entity* pent) const {
  _camera->SetName(pent->name().c_str());
  return new EditorCamControllerInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamControllerInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

EditorCamControllerInst::EditorCamControllerInst(const EditorCamControllerData& occd, Entity* pent)
    : ComponentInst(&occd, pent)
    , mCD(occd) {
  const UiCamera* pcam = mCD.GetCamera();
}

///////////////////////////////////////////////////////////////////////////////

bool EditorCamControllerInst::DoLink(Simulation* psi) {
  const UiCamera* pcam = mCD.GetCamera();
  auto entity          = GetEntity();
  if (entity) {
    const auto& cammats = pcam->cameraMatrices();
    std::string camname = entity->name().c_str();

    psi->setCameraData(camname, &cammats);
    fmtx4 matrix, imatrix;
    matrix.LookAt(cammats.GetEye(), cammats.GetTarget(), cammats.GetUp());
    imatrix.inverseOf(matrix);
    entity->SetDynMatrix(imatrix);

    auto pdrw = std::make_shared<CallbackDrawable>(entity);
    pdrw->SetOwner(entity->data());
    pdrw->SetSortKey(0);

    entity->addDrawableToDefaultLayer(pdrw);

    Drawable::var_t ap;

    auto rendermethod = [=](RenderContextInstData& RCID) {
      auto ezcam = dynamic_cast<const EzUiCam*>(pcam);
      if (ezcam)
        ezcam->draw(RCID.context());
    };
    // ap.Set<const impl*>(pimpl);
    // pdrw->SetUserDataA(ap);

    pdrw->SetRenderCallback(rendermethod);
    // pdrw->setEnqueueOnLayerCallback(impl::enqueueOnLayerCallback);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool EditorCamControllerInst::DoStart(Simulation* psi, const fmtx4& world) {
  // printf( "STARTING EditorCamControllerInst\n" );
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamControllerInst::DoUpdate(Simulation* psi) {
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
