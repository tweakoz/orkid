////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/math/TransformNode.h>
#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <pkg/ent/entity.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
class XgmModel;
class GfxMaterial3DSolid;
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class EditorCamArchetype : public Archetype {
  RttiDeclareConcrete(EditorCamArchetype, Archetype);

  void DoStartEntity(Simulation* psi, const fmtx4& world, Entity* pent) const final {}
  void DoCompose(ork::ent::ArchComposer& composer) final;

public:
  EditorCamArchetype();
};

///////////////////////////////////////////////////////////////////////////////

class EditorCamControllerData : public ent::ComponentData {
  RttiDeclareConcrete(EditorCamControllerData, ent::ComponentData);
  ent::ComponentInst* createComponent(ent::Entity* pent) const final;

public:
  lev2::UiCamera* _camera;

  EditorCamControllerData();
  const lev2::UiCamera* GetCamera() const { return _camera; }
  ork::Object* CameraAccessor() { return _camera; }
};

///////////////////////////////////////////////////////////////////////////////

class EditorCamControllerInst : public ent::ComponentInst {
  RttiDeclareAbstract(EditorCamControllerInst, ent::ComponentInst);

  const EditorCamControllerData& mCD;

  void DoUpdate(ent::Simulation* sinst) final;
  bool DoLink(Simulation* psi) final;
  bool DoStart(Simulation* psi, const fmtx4& world) final;

public:
  const EditorCamControllerData& GetCD() const { return mCD; }

  EditorCamControllerInst(const EditorCamControllerData& cd, ork::ent::Entity* pent);
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
