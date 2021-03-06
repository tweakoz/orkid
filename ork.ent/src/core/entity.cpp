////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/AudioComponent.h>

#include <ork/application/application.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.h>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/RTTI.h>
#include <ork/rtti/downcast.h>
#include <pkg/ent/bullet.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>

#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/event/DrawableEvent.h>

#include <ork/kernel/orklut.hpp>
#include <ork/rtti/Class.h>

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/AudioAnalyzer.h>
#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/LightingSystem.h>
#include <pkg/ent/ModelArchetype.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/ParticleControllable.h>
#include <pkg/ent/ScriptComponent.h>
#include <pkg/ent/SimpleAnimatable.h>
#include <pkg/ent/input.h>
#include <pkg/ent/EditorCamera.h>

#include "../camera/ObserverCamera.h"
#include "../camera/SpinnyCamera.h"
#include "../camera/TetherCamera.h"
#include "../character/CharacterLocoComponent.h"
#include "../character/SimpleCharacterArchetype.h"
#include "../core/PerformanceAnalyzer.h"
#include "../misc/GridComponent.h"
#include "../misc/ProcTex.h"
#include "../misc/QuartzComposerTest.h"
#include "../misc/Skybox.h"
#include "../misc/VrSystem.h"

///////////////////////////////////////////////////////////////////////////////

template class ork::reflect::DirectTypedMap<ork::orklut<ork::PoolString, ork::Object*>>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Archetype, "Ent3dArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EntData, "Ent3dEntData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Entity, "Ent3dEntity");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

class SceneDagObjectManipInterface : public ork::lev2::IManipInterface {
  RttiDeclareConcrete(SceneDagObjectManipInterface, ork::lev2::IManipInterface);

public:
  SceneDagObjectManipInterface() {
  }

  const TransformNode& GetTransform(rtti::ICastable* pobj) final {
    SceneDagObject* pdago = rtti::autocast(pobj);
    return pdago->GetDagNode().GetTransformNode();
  }
  void SetTransform(rtti::ICastable* pobj, const TransformNode& node) final {
    SceneDagObject* pdago                  = rtti::autocast(pobj);
    pdago->GetDagNode().GetTransformNode() = node;
  }
};

void SceneDagObjectManipInterface::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EntData::Describe() {
  ArrayString<64> arrstr;
  MutableString mutstr(arrstr);
  mutstr.format("EntData");
  GetClassStatic()->SetPreferredName(arrstr);

  reflect::annotateClassForEditor<EntData>("editor.3dpickable", true);
  reflect::annotateClassForEditor<EntData>("editor.3dxfable", true);
  reflect::annotateClassForEditor<EntData>("editor.3dxfinterface", ConstString("SceneDagObjectManipInterface"));

  reflect::RegisterProperty("Archetype", &EntData::ArchetypeGetter, &EntData::ArchetypeSetter);
  reflect::annotatePropertyForEditor<EntData>("Archetype", "editor.choicelist", "archetype");
  reflect::annotatePropertyForEditor<EntData>("Archetype", "editor.factorylistbase", "Ent3dArchetype");

  reflect::RegisterFunctor("SlotArchetypeDeleted", &EntData::SlotArchetypeDeleted);

  reflect::annotateClassForEditor<EntData>(
      "editor.object.ops", ConstString("ArchDeRef:EntArchDeRef ArchReRef:EntArchReRef ArchSplit:EntArchSplit"));

  ork::reflect::RegisterMapProperty("UserProperties", &EntData::mUserProperties);
}
///////////////////////////////////////////////////////////////////////////////
ConstString EntData::GetUserProperty(const ConstString& key) const {
  ConstString rval("");
  orklut<ConstString, ConstString>::const_iterator it = mUserProperties.find(key);
  if (it != mUserProperties.end())
    rval = (*it).second;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool EntData::postDeserialize(reflect::serdes::IDeserializer&) {
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void EntData::SetArchetype(const Archetype* parch) {
  if (mArchetype != parch)
    mArchetype = parch;
}
///////////////////////////////////////////////////////////////////////////////

void EntData::SlotArchetypeDeleted(const ork::ent::Archetype* parch) {
  if (GetArchetype() == parch) {
    SetArchetype(0);
  }
}

///////////////////////////////////////////////////////////////////////////////
EntData::EntData()
    : mArchetype(0) {
}
///////////////////////////////////////////////////////////////////////////////
EntData::~EntData() {
}
///////////////////////////////////////////////////////////////////////////////
void EntData::ArchetypeGetter(ork::rtti::ICastable*& val) const {
  Archetype* nonconst = const_cast<Archetype*>(mArchetype);
  val                 = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void EntData::ArchetypeSetter(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  SetArchetype((ptr == 0) ? 0 : rtti::safe_downcast<Archetype*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::DeleteComponents() {
  for (ComponentDataTable::LutType::const_iterator it = mComponentDatas.begin(); it != mComponentDatas.end(); it++) {
    ComponentData* pdata = it->second;
    delete pdata;
  }

  mComponentDataTable.Clear();
  mComponentDatas.clear();
}
///////////////////////////////////////////////////////////////////////////////
void Entity::Describe() {
  reflect::RegisterProperty("EntData", &Entity::EntDataGetter, static_cast<void (Entity::*)(ork::rtti::ICastable* const& val)>(0));
  reflect::RegisterFunctor("Self", &Entity::Self);
  reflect::RegisterFunctor("Position", &Entity::GetEntityPosition);
  reflect::RegisterFunctor("PrintName", &Entity::PrintName);
  reflect::RegisterFunctor("GetComponentByClassName", &Entity::GetComponentByClassName);
}
///////////////////////////////////////////////////////////////////////////////
PoolString Entity::name() const {
  static const PoolString noname = ork::AddPooledString("noname");
  auto ed                        = data();
  PoolString ename               = ed ? ed->GetName() : noname;
  return ename;
}
///////////////////////////////////////////////////////////////////////////////
ComponentInst* Entity::GetComponentByClass(rtti::Class* clazz) {
  const ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    ComponentInst* cinst = (*it).second;
    if (cinst->GetClass()->IsSubclassOf(clazz))
      return cinst;
  }
  return NULL;
}
///////////////////////////////////////////////////////////////////////////////
ComponentInst* Entity::GetComponentByClassName(ork::PoolString classname) {
  if (rtti::Class* clazz = rtti::Class::FindClass(classname))
    return GetComponentByClass(clazz);
  return NULL;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::EntDataGetter(ork::rtti::ICastable*& ptr) const {
  EntData* pdata = const_cast<EntData*>(_entdata);
  ptr            = static_cast<ork::rtti::ICastable*>(pdata);
}
///////////////////////////////////////////////////////////////////////////////
Entity::Entity(const EntData* edata, Simulation* inst)
    : _components(EKEYPOLICY_MULTILUT)
    , _entdata(edata)
    , mDagNode(edata->GetDagNode())
    , mComponentTable(_components)
    , mSimulation(inst) {
  OrkAssert(edata != nullptr);
}
///////////////////////////////////////////////////////////////////////////////
fmtx4 Entity::GetEffectiveMatrix() const {
  fmtx4 rval;
  switch (mSimulation->GetSimulationMode()) {
    case ESCENEMODE_RUN:
    case ESCENEMODE_SINGLESTEP:
    case ESCENEMODE_PAUSE: {
      const DagNode& dagn = this->GetDagNode();
      const auto& xf      = dagn.GetTransformNode().GetTransform();
      rval                = xf.GetMatrix();
      break;
    }
    default: {
      const DagNode& dagn = this->data()->GetDagNode();
      const auto& xf      = dagn.GetTransformNode().GetTransform();
      rval                = xf.GetMatrix();
      break;
    }
  }
  return rval;
}

void Entity::SetDynMatrix(const fmtx4& mtx) {
  this->GetDagNode().SetTransformMatrix(mtx);
}

///////////////////////////////////////////////////////////////////////////////

void Entity::setRotAxisAngle(fvec4 axisang) {

  fvec3 pos;
  fquat rot;
  float sca     = 0.0f;
  DagNode& dagn = this->GetDagNode();
  auto xf       = dagn.GetTransformNode().GetTransform().GetMatrix();
  xf.decompose(pos, rot, sca);
  rot.fromAxisAngle(axisang);
  xf.compose(pos, rot, sca);
  dagn.SetTransformMatrix(xf);
}

///////////////////////////////////////////////////////////////////////////////

void Entity::setPos(fvec3 newpos) {
  fvec3 tmppos;
  fquat rot;
  float sca     = 0.0f;
  DagNode& dagn = this->GetDagNode();
  auto xf       = dagn.GetTransformNode().GetTransform().GetMatrix();
  xf.decompose(tmppos, rot, sca);
  xf.compose(newpos, rot, sca);
  dagn.SetTransformMatrix(xf);
}

///////////////////////////////////////////////////////////////////////////////

Entity::~Entity() {
  // printf( "Delete Entity<%p>\n", this );
  for (ComponentTable::LutType::const_iterator it = _components.begin(); it != _components.end(); it++) {
    ComponentInst* pinst = it->second;
    delete pinst;
  }
}
///////////////////////////////////////////////////////////////////////////////
fvec3 Entity::GetEntityPosition() const {
  return GetDagNode().GetTransformNode().GetTransform().GetPosition();
}
///////////////////////////////////////////////////////////////////////////////
void Entity::PrintName() {
  orkprintf("EntityName:%s: \n", name().c_str());
}
///////////////////////////////////////////////////////////////////////////////
bool Entity::doNotify(const ork::event::Event* event) {
  bool result                  = false;
  ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    ComponentInst* inst = (*it).second;
    result              = static_cast<ork::Object*>(inst)->Notify(event) || result;
  }
  return result;
}
///////////////////////////////////////////////////////////////////////////////
ComponentTable& Entity::GetComponents() {
  return mComponentTable;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::addDrawableToDefaultLayer(lev2::drawable_ptr_t pdrw) {
  std::string layername = "Default";
  if (auto ED = data()) {
    ConstString layer = ED->GetUserProperty("DrawLayer");
    if (strlen(layer.c_str()) != 0) {
      layername = layer.c_str();
    }
  }
  printf("layername<%s>\n", layername.c_str());
  _addDrawable(layername, pdrw);
}
///////////////////////////////////////////////////////////////////////////////
void Entity::addDrawableToLayer(lev2::drawable_ptr_t pdrw, const std::string& layername) {
  _addDrawable(layername, pdrw);
}
///////////////////////////////////////////////////////////////////////////////
const ComponentTable& Entity::GetComponents() const {
  return mComponentTable;
}
///////////////////////////////////////////////////////////////////////////////
void ArchComposer::Register(ork::ent::ComponentData* pdata) {
  if (pdata) {
    ork::object::ObjectClass* pclass = pdata->GetClass();
    _components.AddSorted(pclass, pdata);
  }
}
///////////////////////////////////////////////////////////////////////////////
ArchComposer::ArchComposer(ork::ent::Archetype* parch, SceneComposer& scene_composer)
    : mpArchetype(parch)
    , _components(ork::EKEYPOLICY_MULTILUT)
    , mSceneComposer(scene_composer) {
}
///////////////////////////////////////////////////////////////////////////////
ArchComposer::~ArchComposer() {
  mpArchetype->GetComponentDataTable().Clear();
  for (ork::orklut<ork::object::ObjectClass*, ork::Object*>::const_iterator it = _components.begin(); it != _components.end();
       it++) {
    ork::object::ObjectClass* pclass = it->first;
    ork::ent::ComponentData* pdata   = ork::rtti::autocast(it->second);
    if (0 == pdata) {
      OrkAssert(pclass->IsSubclassOf(ork::ent::ComponentData::GetClassStatic()));
      pdata = ork::rtti::autocast(pclass->CreateObject());
    }
    mpArchetype->GetComponentDataTable().AddComponent(pdata);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::Describe() {
  ArrayString<64> arrstr;
  MutableString mutstr(arrstr);
  mutstr.format("/arch/Archetype");
  GetClassStatic()->SetPreferredName(arrstr);

  reflect::RegisterMapProperty("Components", &Archetype::mComponentDatas);
  reflect::annotatePropertyForEditor<Archetype>("Components", "editor.map.policy.const", "true");
}
///////////////////////////////////////////////////////////////////////////////
Archetype::Archetype()
    : mComponentDatas(EKEYPOLICY_MULTILUT)
    , mComponentDataTable(mComponentDatas)
    , mpSceneData(0) {
}
///////////////////////////////////////////////////////////////////////////////
bool Archetype::postDeserialize(reflect::serdes::IDeserializer&) {
  // Compose();
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::ComposeEntity(Entity* pent) const {
  if (pent)
    DoComposeEntity(pent);
}

void Archetype::DoComposeEntity(Entity* pent) const {
  printf("Archetype::DoComposeEntity pent<%p>\n", pent);
  const ent::ComponentDataTable::LutType& clut = GetComponentDataTable().GetComponents();
  for (ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it != clut.end(); it++) {
    ent::ComponentData* pcompdata = it->second;
    if (pcompdata) {
      ent::ComponentInst* pinst = pcompdata->createComponent(pent);
      if (pinst) {
        pent->GetComponents().AddComponent(pinst);
      }
    }
  }
}

void Archetype::LinkEntity(ork::ent::Simulation* psi, ork::ent::Entity* pent) const {
  if (GetClass() != ReferenceArchetype::GetClassStatic()) {
    const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
    for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
      ComponentInst* inst = (*it).second;
      inst->Link(psi);
    }
  }

  DoLinkEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::UnLinkEntity(ork::ent::Simulation* psi, ork::ent::Entity* pent) const {
  if (GetClass() != ReferenceArchetype::GetClassStatic()) {
    const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
    for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
      ComponentInst* inst = (*it).second;
      inst->UnLink(psi);
    }
  }

  DoUnLinkEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::DoLinkEntity(Simulation* psi, Entity* pent) const {
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::DoUnLinkEntity(Simulation* psi, Entity* pent) const {
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::DoDeComposeEntity(Entity* pent) const {
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::StartEntity(Simulation* psi, const fmtx4& world, Entity* pent) const {
  // printf( "Archetype<%p>::StartEntity<%p>\n", this, pent );

  StopEntity(psi, pent);

  pent->GetDagNode().GetTransformNode().GetTransform().SetMatrix(world);

  // printf( "yo0\n" );
  if (GetClass() != ReferenceArchetype::GetClassStatic()) {
    const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
    for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
      ComponentInst* inst = (*it).second;
      inst->Start(psi, world);
    }
  }
  // printf( "yo1\n" );
  DoStartEntity(psi, world, pent);
  // printf( "yo2\n" );
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::StopEntity(Simulation* psi, Entity* pent) const {
  // printf( "Archetype<%p>::StopEntity<%p:%s>::0\n", this, pent, pent->GetEntData().GetName().c_str() );

  if (GetClass() != ReferenceArchetype::GetClassStatic()) {
    // printf( "Archetype<%p>::StopEntity<%p>::1\n", this, pent );

    const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
    for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
      ComponentInst* inst = (*it).second;
      inst->Stop(psi);
    }
    // printf( "Archetype<%p>::StopEntity<%p>::2\n", this, pent );
  }
  DoStopEntity(psi, pent);
  // printf( "Archetype<%p>::StopEntity<%p:%s>::3\n", this, pent, pent->GetEntData().GetName().c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::Compose(SceneComposer& scene_composer) {
  mpSceneData = scene_composer.GetSceneData();

  ork::ent::ArchComposer arch_composer(this, scene_composer);
  DoCompose(arch_composer);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::DeCompose() {
  DeleteComponents();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FnBallArchetypeTouch();

void ClassInit() {
  Archetype::GetClassStatic();
  ModelArchetype::GetClassStatic();
  SkyBoxArchetype::GetClassStatic();
  ProcTexArchetype::GetClassStatic();
  ObserverCamArchetype::GetClassStatic();
  SequenceCamArchetype::GetClassStatic();
  BulletObjectArchetype::GetClassStatic();
  auto bwcd = BulletSystemData::GetClassStatic();
  // printf("BWCD<%p>\n", bwcd);
  PerformanceAnalyzerArchetype::GetClassStatic();
  SimpleCharacterArchetype::GetClassStatic();
  EntData::GetClassStatic();
  SceneObject::GetClassStatic();
  SceneData::GetClassStatic();
  Simulation::GetClassStatic();
  GridControllerData::GetClassStatic();

  InputComponentData::GetClassStatic();

  // heightfield_rt_inst::GetClassStatic();

  ScriptComponentData::GetClassStatic();
  ScriptComponentInst::GetClassStatic();

  CompositingSystemData::GetClassStatic();

  ork::psys::ParticleControllableData::GetClassStatic();
  ork::psys::ParticleControllableInst::GetClassStatic();

  ork::psys::ModularSystem::GetClassStatic();
  ork::psys::NovaParticleSystem::GetClassStatic();

  ork::psys::ModParticleItem::GetClassStatic();
  ork::psys::NovaParticleItemBase::GetClassStatic();

  SceneDagObjectManipInterface::GetClassStatic();

  EditorCamArchetype::GetClassStatic();
  EditorCamControllerData::GetClassStatic();
  EditorCamControllerInst::GetClassStatic();

  RegisterClassX(DagNode);
  RegisterClassX(VrSystemData);

#if defined(ORK_OSXX)
  AudioAnalysisSystemData::GetClassStatic();
  AudioAnalysisSystem::GetClassStatic();
  AudioAnalysisComponentData::GetClassStatic();
  AudioAnalysisComponentInst::GetClassStatic();
  AudioAnalysisArchetype::GetClassStatic();
  QuartzComposerArchetype::GetClassStatic();
#endif

  FnBallArchetypeTouch();

  // TODO - auto register ?

  RegisterFamily<EditorPropMapData>(ork::AddPooledLiteral("")); // no update

  RegisterFamily<SimpleAnimatableData>(ork::AddPooledLiteral("animate"));
  RegisterFamily<ProcTexControllerData>(ork::AddPooledLiteral("animate"));
  RegisterFamily<DataflowRecieverComponentData>(ork::AddPooledLiteral("animate"));

  RegisterFamily<AudioEffectComponentData>(ork::AddPooledLiteral("audio"));
  RegisterFamily<AudioStreamComponentData>(ork::AddPooledLiteral("audio"));

  RegisterFamily<BulletObjectControllerData>(ork::AddPooledLiteral("bullet"));

  RegisterFamily<ObserverCamControllerData>(ork::AddPooledLiteral("camera"));
  RegisterFamily<SequenceCamControllerData>(ork::AddPooledLiteral("camera"));
  RegisterFamily<TetherCamControllerData>(ork::AddPooledLiteral("camera"));

  RegisterFamily<CompositingSystemData>(AddPooledLiteral("prerender"));

  RegisterFamily<SimpleCharControllerData>(AddPooledLiteral("control"));
  RegisterFamily<ScriptComponentData>(ork::AddPooledLiteral("control"));
  RegisterFamily<CompositingSystemData>(ork::AddPooledLiteral("control"));
  RegisterFamily<CharacterLocoData>(ork::AddPooledLiteral("control"));
  RegisterFamily<GridControllerData>(ork::AddPooledLiteral("control"));
  RegisterFamily<SkyBoxControllerData>(ork::AddPooledLiteral("control"));
  RegisterFamily<PerfAnalyzerControllerData>(ork::AddPooledLiteral("control"));
  RegisterFamily<ModelComponentData>(ork::AddPooledLiteral("control"));
  // RegisterFamily<SectorTrackerData>(ork::AddPooledLiteral("control"));
  // RegisterFamily<RacingLineData>(ork::AddPooledLiteral("control"));

  RegisterFamily<InputComponentData>(ork::AddPooledLiteral("input"));

  RegisterFamily<LightingComponentData>(ork::AddPooledLiteral("lighting"));

  RegisterFamily<psys::ParticleControllableData>(ork::AddPooledLiteral("particle"));

  RegisterFamily<BulletSystemData>(ork::AddPooledLiteral("physics"));
}

void Init2() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneDagObjectManipInterface, "SceneDagObjectManipInterface");
