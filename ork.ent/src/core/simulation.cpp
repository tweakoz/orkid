////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/asset/AssetManager.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiobank.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/input/inputdevice.h>
#include <ork/pch.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/stream/StringInputStream.h>

#include <ork/kernel/debug.h>
#include <ork/kernel/orklut.hpp>
//
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/math/basicfilters.h>
#include <pkg/ent/scene.hpp>

///////////////////////////////////////////////////////////////////////////////

namespace ork {
void EnterRunMode();
void LeaveRunMode();
}; // namespace ork

///////////////////////////////////////////////////////////////////////////////

#define VERBOSE 1
#define PRINT_CONDITION (ork::PieceString(pent->GetEntData().GetName()).find("missile") != ork::PieceString::npos)

#if VERBOSE
#define DEBUG_PRINT orkprintf
#else
#define DEBUG_PRINT(...)
#endif

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Simulation, "Ent3dSimulation");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SimulationEvent, "SimulationEvent");

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<const ork::object::ObjectClass*, ork::ent::System*>;

namespace ork::ent {

static ork::PoolString sSimulationEvChanName;
static ork::PoolString sAudioFamily;
static ork::PoolString sCameraFamily;
static ork::PoolString sControlFamily;
static ork::PoolString sMotionFamily;
static ork::PoolString sPhysicsFamily;
static ork::PoolString sPositionFamily;
static ork::PoolString sFrustumFamily;
static ork::PoolString sAnimateFamily;
static ork::PoolString sParticleFamily;
static ork::PoolString sLightFamily;
static ork::PoolString sInputFamily;
static ork::PoolString sPreRenderFamily;

void SimulationEvent::Describe() { sSimulationEvChanName = ork::AddPooledLiteral("SimulationEvChannel"); }
const ork::PoolString& Simulation::EventChannel() { return sSimulationEvChanName; }

///////////////////////////////////////////////////////////////////////////////
void Simulation::Describe() {
  reflect::RegisterFunctor("SlotSceneTopoChanged", &Simulation::SlotSceneTopoChanged);

  sInputFamily     = ork::AddPooledLiteral("input");
  sAudioFamily     = ork::AddPooledLiteral("audio");
  sCameraFamily    = ork::AddPooledLiteral("camera");
  sControlFamily   = ork::AddPooledLiteral("control");
  sPhysicsFamily   = ork::AddPooledLiteral("physics");
  sFrustumFamily   = ork::AddPooledLiteral("frustum");
  sAnimateFamily   = ork::AddPooledLiteral("animate");
  sParticleFamily  = ork::AddPooledLiteral("particle");
  sLightFamily     = ork::AddPooledLiteral("lighting");
  sPreRenderFamily = ork::AddPooledLiteral("PreRender");
}
///////////////////////////////////////////////////////////////////////////////
Simulation::Simulation(const SceneData* sdata, Application* application)
    : mSceneData(sdata)
    , meSimulationMode(ESCENEMODE_ATTACHED)
    , mApplication(application)
    , mUpTime(0.0f)
    , mDeltaTime(0.0f)
    , mPrevDeltaTime(0.0f)
    , mLastGameTime(0.0f)
    , mStartTime(0.0f)
    , mUpDeltaTime(0.0f)
    , mGameTime(0.0f)
    , mDeltaTimeAccum(0.0f)
    , mfAvgDtAcc(0.0f)
    , mfAvgDtCtr(0.0f)
    , mEntityUpdateCount(0) {
  printf("new simulation <%p>\n", this);

  AssertOnOpQ2(UpdateSerialOpQ());
  OrkAssertI(mApplication, "Simulation must be constructed with a non-NULL Application!");

  auto player = new lev2::Layer;
  AddLayer(AddPooledLiteral("Default"), player);

  ////////////////////////////
  // create one token
  ////////////////////////////
  lev2::RenderSyncToken rentok;
  while (lev2::DrawableBuffer::mOfflineRenderSynchro.try_pop(rentok)) {
  }
  while (lev2::DrawableBuffer::mOfflineUpdateSynchro.try_pop(rentok)) {
  }
  rentok.mFrameIndex = 0;
  lev2::DrawableBuffer::mOfflineUpdateSynchro.push(rentok); // push 1 token
}
///////////////////////////////////////////////////////////////////////////////
Simulation::~Simulation() {
  printf("deleting simulation <%p>\n", this);
  AssertOnOpQ2(UpdateSerialOpQ());
  lev2::DrawableBuffer::BeginClearAndSyncReaders();
  for (auto it : mEntities) {
    Entity* pent = it.second;
    if (pent) {
      delete pent;
    }
  }
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* sys = it.second;
      if (sys) {
        printf("deleting System <%p>\n", sys);
        delete sys;
      }
    }
  });
  lev2::DrawableBuffer::EndClearAndSyncReaders();

  ////////////////////////////
  // steal all RenderSyncToken's
  ////////////////////////////
  lev2::RenderSyncToken rentok;
  while (lev2::DrawableBuffer::mOfflineRenderSynchro.try_pop(rentok)) {
  }
  while (lev2::DrawableBuffer::mOfflineUpdateSynchro.try_pop(rentok)) {
  }
  ////////////////////////////
}
///////////////////////////////////////////////////////////////////////////

CompositingSystem* Simulation::compositingSystem() {
    return findSystem<CompositingSystem>();
}
const CompositingSystem* Simulation::compositingSystem() const {
  return findSystem<CompositingSystem>();
}

///////////////////////////////////////////////////////////////////////////////

float Simulation::random(float mmin, float mmax) {
  float r = float(rand() & 0xffff) / 65536.0f;
  return mmin + (r * (mmax - mmin));
}

///////////////////////////////////////////////////////////////////////////////
float Simulation::ComputeDeltaTime() {
  auto compsys     = compositingSystem();
  float frame_rate = 0.0f;

  AssertOnOpQ2(UpdateSerialOpQ());
  float systime = float(OldSchool::GetRef().GetLoResTime());
  float fdelta  = (frame_rate != 0.0f) ? (1.0f / frame_rate) : (systime - mUpTime);

  static float fbasetime = systime;

  if (fdelta == 0.0f)
    return 0.0f;

  mUpTime      = systime;
  mUpDeltaTime = fdelta;

  ////////////////////////////////////////////
  // allowed FPS range is 1000hz to .5 hz
  ////////////////////////////////////////////
  if (fdelta < 0.00001f) {
    // orkprintf( "FPS is over 10000HZ!!!! you need to reset valid fps range\n"
    // ); fdelta=0.001f; ork::msleep(1);
    systime      = float(OldSchool::GetRef().GetLoResTime());
    fdelta       = 0.00001f;
    mUpTime      = systime;
    mUpDeltaTime = fdelta;
  } else if (fdelta > 0.1f) {
    // orkprintf( "FPS is less than 10HZ!!!! you need to reset valid fps
    // range\n" );
    fdelta = 0.1f;
  }

  ////////////////////////////////////////////

  mUpDeltaTime = fdelta;

  switch (this->GetSimulationMode()) {
    case ork::ent::ESCENEMODE_ATTACHED:
    case ork::ent::ESCENEMODE_EDIT:
    case ork::ent::ESCENEMODE_SINGLESTEP:
      break;
    case ork::ent::ESCENEMODE_PAUSE: {
      mDeltaTime = 0.0f;
      //			mDeltaTimeAccum = 1.0f/240.0f;
      break;
    }
    case ork::ent::ESCENEMODE_RUN: {
      ///////////////////////////////
      // update clock
      ///////////////////////////////

      mDeltaTime     = (mPrevDeltaTime + fdelta) / 2;
      mPrevDeltaTime = fdelta;
      mLastGameTime  = mGameTime;
      mGameTime += mDeltaTime;
    }
  }

  //	printf( "mGameTime<%f>\n", mGameTime );

  return fdelta;
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::setCameraData(const PoolString& name, const CameraData* camdat) {
  CameraLut::iterator it = _cameraDataLUT.find(name);

  if (it == _cameraDataLUT.end()) {
    if (camdat != 0) {
      _cameraDataLUT.AddSorted(name, camdat);
    }
  } else {
    if (camdat == 0) {
      _cameraDataLUT.erase(it);
    } else {
      it->second = camdat;
    }
  }

  lev2::Camera* pcam = (camdat != 0) ? camdat->getEditorCamera() : 0;

  // orkprintf( "Simulation::setCameraData() name<%s> camdat<%p> l2cam<%p>\n",
  // name.c_str(), camdat, pcam );
}

///////////////////////////////////////////////////////////////////////////////
const CameraData* Simulation::cameraData(const PoolString& name) const {
  CameraLut::const_iterator it = _cameraDataLUT.find(name);
  return (it == _cameraDataLUT.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::SlotSceneTopoChanged() {
  auto topo_op = [=]() {
    msgrouter::channel("Simulation")->post(std::string("SimulationInvalidated"));
    this->GetData().AutoLoadAssets();
    this->EnterEditState();
  };
  UpdateSerialOpQ().push(Op(topo_op));
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::UpdateEntityComponents(const Simulation::ComponentList& components) {
  AssertOnOpQ2(UpdateSerialOpQ());
  for (Simulation::ComponentList::const_iterator it = components.begin(); it != components.end(); ++it) {
    ComponentInst* pci = (*it);
    OrkAssert(pci != 0);
    pci->Update(this);
  }
}
///////////////////////////////////////////////////////////////////////////
ent::Entity* Simulation::GetEntity(const ent::EntData* pdata) const {
  const PoolString& name = pdata->GetName();
  ent::Entity* pent      = FindEntity(name);
  return pent;
}
///////////////////////////////////////////////////////////////////////////
void Simulation::SetEntity(const ent::EntData* pentdata, ent::Entity* pent) {
  AssertOnOpQ2(UpdateSerialOpQ());
  assert(pent != nullptr);
  mEntities[pentdata->GetName()] = pent;
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_compose() {
  ComposeEntities();
  composeSystems();
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_decompose() {
  DecomposeEntities();
  decomposeSystems();
  mActiveEntityComponents.clear();
  mActiveEntities.clear();
  mEntityDeactivateQueue.clear();
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_link() {
  LinkEntities();
  LinkSystems();
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_unlink() {
  UnLinkEntities();
  UnLinkSystems();
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_stage() {
  StartEntities();
  mStartTime      = float(OldSchool::GetRef().GetLoResTime());
  mGameTime       = 0.0f;
  mUpDeltaTime    = 0.0f;
  mPrevDeltaTime  = 1.0f / 30.0f;
  mDeltaTime      = 1.0f / 30.0f;
  mDeltaTimeAccum = 0.0f;
  mUpTime         = mStartTime;
  mLastGameTime   = 0.0f;

  ServiceDeactivateQueue();

  msgrouter::channel("Simulation")->postType<SimulationEvent>(this, SimulationEvent::ESIEV_START);
  lev2::DrawableBuffer::EndClearAndSyncReaders();

  lev2::RenderSyncToken rentok;
  while (lev2::DrawableBuffer::mOfflineRenderSynchro.try_pop(rentok)) {
  }
  while (lev2::DrawableBuffer::mOfflineUpdateSynchro.try_pop(rentok)) {
  }
  rentok.mFrameIndex = 0;
  lev2::DrawableBuffer::mOfflineUpdateSynchro.push(rentok);
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_unstage() { StopEntities(); }

///////////////////////////////////////////////////////////////////////////

void Simulation::_activate() {
  for (const auto& item : mEntities) {
    auto pent = item.second;
    ActivateEntity(pent);
  }
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_deactivate() {}

///////////////////////////////////////////////////////////////////////////
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_RESET "\x1b[30m"
#define ANSI_COLOR_GREEN "\x1b[32m"

void Simulation::EnterEditState() {
  printf("%s", ANSI_COLOR_RED);
  printf("////////////////////////\n");
  printf("Simulation<%p> EnterEditState\n", this);
  printf("////////////////////////\n");
  printf("%s", ANSI_COLOR_GREEN);
  lev2::DrawableBuffer::BeginClearAndSyncReaders();
  AssertOnOpQ2(UpdateSerialOpQ());

  msgrouter::channel("Simulation")->postType<SimulationEvent>(this, SimulationEvent::ESIEV_BIND);

  LeaveRunMode();
  ork::lev2::AudioDevice::GetDevice()->StopAllVoices();

  ///////////////////////////////////
  // tear down
  ///////////////////////////////////

  _deactivate();
  _unstage();
  _unlink();
  _decompose();

  ///////////////////////////////////
  // bring up
  ///////////////////////////////////

  _compose();
  _link();
  _stage();
}
///////////////////////////////////////////////////////////////////////////
void Simulation::EnterPauseState() { ork::lev2::AudioDevice::GetDevice()->StopAllVoices(); }
///////////////////////////////////////////////////////////////////////////////
void Simulation::EnterRunState() {
  printf("%s", ANSI_COLOR_RED);
  printf("////////////////////////\n");
  printf("Simulation<%p> EnterRunState\n", this);
  printf("////////////////////////\n");
  printf("%s", ANSI_COLOR_GREEN);

  lev2::DrawableBuffer::BeginClearAndSyncReaders();
  AssertOnOpQ2(UpdateSerialOpQ());
  EnterRunMode();

  AllocationLabel label("Simulation::EnterRunState::255");

  msgrouter::channel("Simulation")->postType<SimulationEvent>(this, SimulationEvent::ESIEV_BIND);

  ork::lev2::AudioDevice::GetDevice()->StopAllVoices();

  ///////////////////////////////////
  // tear down
  ///////////////////////////////////

  _deactivate();
  _unstage();
  _unlink();
  _decompose();

  ///////////////////////////////////
  // bring up
  ///////////////////////////////////

  _compose();
  _link();
  _stage();
  _activate();

  ///////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::OnSimulationMode(ESimulationMode emode) {
  AssertOnOpQ2(UpdateSerialOpQ());

  switch (meSimulationMode) {
    case ork::ent::ESCENEMODE_ATTACHED:
    case ork::ent::ESCENEMODE_EDIT:
    case ork::ent::ESCENEMODE_SINGLESTEP:
    case ork::ent::ESCENEMODE_PAUSE:
      break;
    case ESCENEMODE_RUN: // leaving runstate
      // setCameraData( ork::AddPooledLiteral("game1"), 0 );
      break;
  }

  switch (emode) {
    case ESCENEMODE_EDIT:
      assert(meSimulationMode != ESCENEMODE_RUN);
      if (meSimulationMode != ESCENEMODE_EDIT) {
        EnterEditState();
      }
      break;
    case ESCENEMODE_RUN:
      assert(meSimulationMode != ESCENEMODE_RUN);
      EnterRunState();
      break;
    case ESCENEMODE_SINGLESTEP:
      break;
    case ESCENEMODE_PAUSE:
      EnterPauseState();
      break;
    case ork::ent::ESCENEMODE_ATTACHED:
      break;
  }
  this->meSimulationMode = emode;
}

///////////////////////////////////////////////////////////////////////////
void Simulation::DecomposeEntities() {
  // printf( "Simulation<%p> BEGIN DecomposeEntities()\n", this );
  // printf( "/////////////////////////////////////\n");
  // std::string bt = get_backtrace();
  // printf( "%s", bt.c_str() );
  AssertOnOpQ2(UpdateSerialOpQ());
  for (auto item : mEntities) {
    const ork::PoolString& name = item.first;
    ork::ent::Entity* pent      = item.second;

    // ork::ent::Archetype* arch = pent->GetArchetype();

    // printf( "deleting ent<%p:%s>\n", pent, name.c_str() );
    delete pent;
  }
  mEntities.clear();

  // printf( "/////////////////////////////////////\n");
  // printf( "Simulation<%p> END DecomposeEntities()\n", this );
}
///////////////////////////////////////////////////////////////////////////
void Simulation::ComposeEntities() {
  AssertOnOpQ2(UpdateSerialOpQ());
  ///////////////////////////////////
  // clear runtime containers
  ///////////////////////////////////

  mEntities.clear();
  _cameraDataLUT.clear();

  ///////////////////////////////////
  // Compose Entities
  ///////////////////////////////////

  // orkprintf( "beg si<%p> Compose Entities..\n", this );

  for (orkmap<ork::PoolString, ork::ent::SceneObject*>::const_iterator it = mSceneData->GetSceneObjects().begin();
       it != mSceneData->GetSceneObjects().end();
       it++) {
    ork::ent::SceneObject* sobj = (*it).second;
    if (ork::ent::EntData* pentdata = ork::rtti::autocast(sobj)) {
      const ork::ent::Archetype* arch = pentdata->GetArchetype();

      ork::ent::Entity* pent = new ork::ent::Entity(*pentdata, this);

      PoolString actualLayerName = AddPooledLiteral("Default");

      ConstString layer_name = pentdata->GetUserProperty("DrawLayer");
      if (strlen(layer_name.c_str()) != 0) {
        actualLayerName = AddPooledString(layer_name.c_str());
      }

      lev2::Layer* player = GetLayer(actualLayerName);
      if (0 == player) {
        player = new lev2::Layer;
        AddLayer(actualLayerName, player);
      }
      ////////////////////////////////////////////////////////////////

      // printf( "Compose Entity<%p> arch<%p> layer<%s>\n", pent, arch,
      // layer_name.c_str() );
      if (arch) {
        arch->ComposeEntity(pent);
      }
      assert(pent != nullptr);
      mEntities[pentdata->GetName()] = pent;
    }
  }

  GetData().AutoLoadAssets();

  // orkprintf( "end si<%p> Compose Entities..\n", this );
}
///////////////////////////////////////////////////////////////////////////
void Simulation::LinkEntities() {
  // orkprintf( "beg si<%p> Link Entities..\n", this );
  AssertOnOpQ2(UpdateSerialOpQ());
  ///////////////////////////////////
  // Link Entities
  ///////////////////////////////////

  // orkprintf( "Link Entities..\n" );
  for (auto item : mEntities) {
    ork::ent::Entity* pent         = item.second;
    const ork::ent::EntData& edata = pent->GetEntData();

    OrkAssert(pent);

    if (edata.GetArchetype()) {
      edata.GetArchetype()->LinkEntity(this, pent);
    }
  }

  // orkprintf( "end si<%p> Link Entities..\n", this );
}

///////////////////////////////////////////////////////////////////////////

void Simulation::UnLinkEntities() {
  // orkprintf( "beg si<%p> Link Entities..\n", this );
  AssertOnOpQ2(UpdateSerialOpQ());

  ///////////////////////////////////
  // Link Entities
  ///////////////////////////////////

  // orkprintf( "Link Entities..\n" );
  for (auto item : mEntities) {
    ork::ent::Entity* pent         = item.second;
    const ork::ent::EntData& edata = pent->GetEntData();

    OrkAssert(pent);

    if (edata.GetArchetype()) {
      edata.GetArchetype()->UnLinkEntity(this, pent);
    }
  }
  // orkprintf( "end si<%p> Link Entities..\n", this );
}

///////////////////////////////////////////////////////////////////////////

void Simulation::composeSystems() {
  AssertOnOpQ2(UpdateSerialOpQ());
  ///////////////////////////////////
  // Systems
  ///////////////////////////////////

  for (auto it : mSceneData->getSystemDatas()) {
    const SystemData* pscd = it.second;
    if (pscd != nullptr) {
      auto sys = pscd->createSystem(this);
      addSystem(sys->systemTypeDynamic(), sys);
    }
  }
}

///////////////////////////////////////////////////////////////////////////

void Simulation::decomposeSystems() {
  AssertOnOpQ2(UpdateSerialOpQ());

  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](SystemLut& syslut) {
    for (auto item : syslut) {
      auto comp = item.second;
      delete comp;
    }
    syslut.clear();
  });
}
///////////////////////////////////////////////////////////////////////////

void Simulation::LinkSystems() {
  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->Link(this);
    }
  });
}

///////////////////////////////////////////////////////////////////////////

void Simulation::UnLinkSystems() {
  AssertOnOpQ2(UpdateSerialOpQ());

  ///////////////////////////////////

  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->UnLink(this);
    }
  });
}

///////////////////////////////////////////////////////////////////////////

void Simulation::StartSystems() {
  AssertOnOpQ2(UpdateSerialOpQ());
  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->Start(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::StopSystems() {
  AssertOnOpQ2(UpdateSerialOpQ());
  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->Stop(this);
    }
    syslut.clear();
  });
}

///////////////////////////////////////////////////////////////////////////
void Simulation::StartEntities() {
  AssertOnOpQ2(UpdateSerialOpQ());

  ///////////////////////////////////
  // Start Entities
  ///////////////////////////////////

  // orkprintf( "Start Entities..\n" );
  for (orkmap<ork::PoolString, ork::ent::Entity*>::const_iterator it = mEntities.begin(); it != mEntities.end(); it++) {
    ork::ent::Entity* pent         = it->second;
    const ork::ent::EntData& edata = pent->GetEntData();

    OrkAssert(pent);

    if (edata.GetArchetype()) {
      fmtx4 world = pent->GetDagNode().GetTransformNode().GetTransform().GetMatrix();
      edata.GetArchetype()->StartEntity(this, world, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::StopEntities() {
  AssertOnOpQ2(UpdateSerialOpQ());
  ///////////////////////////////////
  // Start Entities
  ///////////////////////////////////

  for (orkmap<ork::PoolString, ork::ent::Entity*>::const_iterator it = mEntities.begin(); it != mEntities.end(); it++) {
    ork::ent::Entity* pent         = it->second;
    const ork::ent::EntData& edata = pent->GetEntData();

    OrkAssert(pent);

    if (edata.GetArchetype()) {
      edata.GetArchetype()->StopEntity(this, pent);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
void Simulation::QueueActivateEntity(const EntityActivationQueueItem& item) {
  // DEBUG_PRINT( "QueueActivateEntity<%p:%s>\n",  item.mpEntity,
  // item.mpEntity->GetEntData().GetName().c_str() );
  mEntityActivateQueue.push_back(item);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::QueueDeactivateEntity(Entity* pent) {
  // printf( "QueueDeActivateEntity<%p:%s>\n",  pent,
  // pent->GetEntData().GetName().c_str() );
  mEntityDeactivateQueue.push_back(pent);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::ActivateEntity(ent::Entity* pent) {
  AssertOnOpQ2(UpdateSerialOpQ());
  // DEBUG_PRINT( "ActivateEntity<%p:%s>\n",  pent,
  // pent->GetEntData().GetName().c_str()  );
  EntitySet::iterator it = mActiveEntities.find(pent);
  if (it == mActiveEntities.end()) {
    mActiveEntities.insert(pent);

    /////////////////////////////////////////
    // activate components
    /////////////////////////////////////////

    ent::ComponentTable::LutType& EntComps = pent->GetComponents().GetComponents();
    for (ent::ComponentTable::LutType::const_iterator itc = EntComps.begin(); itc != EntComps.end(); itc++) {
      ent::ComponentInst* cinst = (*itc).second;

      cinst->activate(this);

      PoolString fam = cinst->GetFamily();

      // Don't add components that don't do anything to the active components
      // list

      if (fam.empty())
        continue;

      ActiveComponentType::iterator itl = mActiveEntityComponents.find(fam);
      if (itl == mActiveEntityComponents.end())
        mActiveEntityComponents.insert(std::make_pair(fam, orklist<ork::ent::ComponentInst*>()));
      itl = mActiveEntityComponents.find(fam);

      (itl->second).push_back(cinst);
    }
  } else {
    orkprintf("WARNING, activating an already active entity <%p>\n", pent);
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::DeActivateEntity(ent::Entity* pent) {
  AssertOnOpQ2(UpdateSerialOpQ());
  // printf( "DeActivateEntity<%p:%s>\n",  pent,
  // pent->GetEntData().GetName().c_str() );

  EntitySet::iterator listit = mActiveEntities.find(pent);

  const Archetype* parch = pent->GetEntData().GetArchetype();
  if (listit == mActiveEntities.end()) {
    PoolString parchname = (parch != 0) ? parch->GetName() : AddPooledLiteral("none");
    PoolString pentname  = pent->GetEntData().GetName();

    orkprintf("uhoh, someone is deactivating an entity<%p:%s> of arch<%s> that "
              "is not active!!!\n",
              pent,
              pentname.c_str(),
              parchname.c_str());
    return;
  }
  OrkAssert(listit != mActiveEntities.end());

  mActiveEntities.erase(listit);

  /////////////////////////////////////////
  // deactivate components
  /////////////////////////////////////////

  ent::ComponentTable::LutType& EntComps = pent->GetComponents().GetComponents();
  for (ent::ComponentTable::LutType::const_iterator itc = EntComps.begin(); itc != EntComps.end(); itc++) {
    ent::ComponentInst* cinst = (*itc).second;

    cinst->deactivate(this);

    const PoolString& fam = cinst->GetFamily();

    if (fam.empty())
      continue;

    ActiveComponentType::iterator itl = mActiveEntityComponents.find(fam);
    if (itl != mActiveEntityComponents.end()) {
      orklist<ent::ComponentInst*>& thelist = (*itl).second;

      orklist<ent::ComponentInst*>::iterator itc2 = std::find(thelist.begin(), thelist.end(), cinst);

      // Might not be active if we didn't add it in the first place
      if (itc2 != thelist.end())
        thelist.erase(itc2);
    }
  }

  if (parch)
    parch->StopEntity(this, pent);
}

bool Simulation::IsEntityActive(Entity* pent) const {
  auto listit = mActiveEntities.find(pent);
  return (listit != mActiveEntities.end());
}
///////////////////////////////////////////////////////////////////////////
void Simulation::ServiceDeactivateQueue() {
  AssertOnOpQ2(UpdateSerialOpQ());
  // Copy queue so we can queue more inside Stop
  orkvector<ent::Entity*> deactivate_queue = mEntityDeactivateQueue;
  mEntityDeactivateQueue.clear();

  for (orkvector<ent::Entity*>::const_iterator it = deactivate_queue.begin(); it != deactivate_queue.end(); it++) {
    ent::Entity* pent = (*it);
    OrkAssert(pent);

    DeActivateEntity(pent);
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::ServiceActivateQueue() {
  AssertOnOpQ2(UpdateSerialOpQ());
  // Copy queue so we can queue more inside Start
  orkvector<EntityActivationQueueItem> activate_queue = mEntityActivateQueue;
  mEntityActivateQueue.clear();

  for (orkvector<EntityActivationQueueItem>::const_iterator it = activate_queue.begin(); it != activate_queue.end(); it++) {
    const EntityActivationQueueItem& item = (*it);
    ent::Entity* pent                     = item.mpEntity;
    const fmtx4& mtx                      = item.mMatrix;
    OrkAssert(pent);

    // printf( "Activating Entity (Q) : ent<%p>\n", pent );
    if (const Archetype* parch = pent->GetEntData().GetArchetype()) {
      // printf( "Activating Entity (QQ) : ent<%p> arch<%p>\n", pent, parch );
      parch->StartEntity(this, mtx, pent);
    }
    // printf( "Activating Entity (U) : %p\n", pent );

    ActivateEntity(pent);
    // printf( "Activated Entity (Q) : %p\n", pent );
  }
}

///////////////////////////////////////////////////////////////////////////

Entity* Simulation::SpawnDynamicEntity(const ent::EntData* spawn_rec) {
  // printf( "SpawnDynamicEntity ed<%p>\n", spawn_rec );
  auto newent = new Entity(*spawn_rec, this);
  auto arch   = spawn_rec->GetArchetype();
  arch->ComposeEntity(newent);
  arch->LinkEntity(this, newent);
  EntityActivationQueueItem qi(fmtx4::Identity, newent);
  this->QueueActivateEntity(qi);
  mEntities[spawn_rec->GetName()] = newent;
  return newent;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static void CopyCameraData(const Simulation::CameraLut& srclut, CameraLut& dstlut) {
  dstlut.clear();
  int idx = 0;
  // printf( "Copying CameraData\n" );
  for (Simulation::CameraLut::const_iterator itCAM = srclut.begin(); itCAM != srclut.end(); itCAM++) {
    const PoolString& CameraName  = itCAM->first;
    const CameraData* pcameradata = itCAM->second;
    const lev2::Camera* pcam      = pcameradata ? pcameradata->getEditorCamera() : 0;
    // printf( "CopyCameraData Idx<%d> CamName<%s> pcamdata<%p> pcam<%p>\n",
    // idx, CameraName.c_str(), pcameradata, pcam );
    if (pcameradata) {
      dstlut.AddSorted(CameraName, *pcameradata);
    }
    idx++;
  }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void Simulation::updateThreadTick() {
  auto dbuf = ork::lev2::DrawableBuffer::LockWriteBuffer(7);
  OrkAssert(dbuf);
  float frame_rate           = desiredFrameRate();
  bool externally_fixed_rate = (frame_rate != 0.0f);
  if (externally_fixed_rate) {
    lev2::RenderSyncToken syntok;
    if (lev2::DrawableBuffer::mOfflineUpdateSynchro.try_pop(syntok)) {
      syntok.mFrameIndex++;
      this->Update();
      lev2::DrawableBuffer::mOfflineRenderSynchro.push(syntok);
    }
  } else {
    this->Update();
    this->enqueueDrawablesToBuffer(*dbuf);
  }
  ork::lev2::DrawableBuffer::UnLockWriteBuffer(dbuf);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

void Simulation::enqueueDrawablesToBuffer(ork::lev2::DrawableBuffer& buffer) const {
  AssertOnOpQ2(UpdateSerialOpQ());

  buffer.Reset();

  ////////////////////////////////////////////////////////////////
  // copy camera data from simulation to dbuffer
  ////////////////////////////////////////////////////////////////

  CopyCameraData(_cameraDataLUT, buffer._cameraDataLUT);

  ////////////////////////////////////////////////////////////////

  float t0 = ork::OldSchool::GetRef().GetLoResTime();

  ////////////////////////////////////////////////////////////////
  // per entity drawables
  ////////////////////////////////////////////////////////////////

  for (const auto& it : mEntities) {
    const ork::ent::Entity* pent = it.second;
      printf("sim::enqueue ent<%p>\n", pent);

    const Entity::LayerMap& entlayers = pent->GetLayers();

    lev2::DrawQueueXfData xfdata;

    if (pent->_renderMtxProvider != nullptr) {
      xfdata.mWorldMatrix = pent->_renderMtxProvider();
    } else {
      xfdata.mWorldMatrix = pent->GetEffectiveMatrix();
    }

    for (auto L : entlayers) {
      const PoolString& layer_name = L.first;

      printf("sim::enqueue layer_name<%s>\n", layer_name.c_str());

      const ent::Entity::DrawableVector* dv = L.second;
      lev2::DrawableBufLayer* buflayer      = buffer.MergeLayer(layer_name);
      if (dv && buflayer) {
        size_t inumdv = dv->size();
        printf("sim::enqueue buflayer<%p> inumdv<%zu>\n", buflayer, inumdv);
        for (size_t i = 0; i < inumdv; i++) {
          lev2::Drawable* pdrw = dv->operator[](i);
          if (pdrw && pdrw->IsEnabled()) {
            printf("queue drw<%p>\n", pdrw);
            pdrw->QueueToLayer(xfdata, *buflayer);
          }
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////
  // per system drawables
  ////////////////////////////////////////////////////////////////

  for (auto sys : _updsyslutcopy)
    sys.second->enqueueDrawables(buffer);

  ////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ent::Entity* Simulation::FindEntity(PoolString entity) const {
  orkmap<PoolString, Entity*>::const_iterator it = mEntities.find(entity);
  return (it == mEntities.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
ent::Entity* Simulation::FindEntityLoose(PoolString entity) const {
  const int kmaxtries = 128;

  orkmap<PoolString, Entity*>::const_iterator it = mEntities.find(entity);
  if (it == mEntities.end()) {
    ArrayString<512> basebuffer;
    ArrayString<512> buffer;
    MutableString basestr(basebuffer);
    MutableString name_attempt(buffer);
    name_attempt = entity;

    int counter = 0;

    int i = int(name_attempt.size()) - 1;
    for (; i >= 0; i--)
      if (!isdigit(name_attempt.c_str()[i]))
        break;
    basestr = name_attempt.substr(0, i + 1);

    name_attempt = basestr;
    name_attempt += CreateFormattedString("%d", ++counter).c_str();
    PoolString pooled_name = AddPooledString(name_attempt);
    while (it == mEntities.end() && (counter < kmaxtries)) {
      name_attempt = basestr;
      name_attempt += CreateFormattedString("%d", ++counter).c_str();
      pooled_name = AddPooledString(name_attempt);
      it          = mEntities.find(entity);
    }
  }

  if (it != mEntities.end())
    return it->second;
  return NULL;
}
///////////////////////////////////////////////////////////////////////////////
const Simulation::ComponentList& Simulation::GetActiveComponents(ork::PoolString family) const {
  ActiveComponentType::const_iterator found = mActiveEntityComponents.find(family);
  if (found != mActiveEntityComponents.end())
    return (*found).second;
  else
    return mEmptyList;
}
///////////////////////////////////////////////////////////////////////////
Simulation::ComponentList& Simulation::GetActiveComponents(ork::PoolString family) {
  ActiveComponentType::iterator found = mActiveEntityComponents.find(family);
  if (found != mActiveEntityComponents.end())
    return (*found).second;
  else {
    return mEmptyList;
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::UpdateActiveComponents(ork::PoolString family) { UpdateEntityComponents(GetActiveComponents(family)); }
///////////////////////////////////////////////////////////////////////////
void Simulation::AddLayer(const PoolString& name, lev2::Layer* player) {
  auto it = mLayers.find(name);
  OrkAssert(it == mLayers.end());
  mLayers[name]      = player;
  player->mLayerName = name;
}
lev2::Layer* Simulation::GetLayer(const PoolString& name) {
  lev2::Layer* rval = 0;
  auto it           = mLayers.find(name);
  if (it != mLayers.end())
    rval = it->second;
  return rval;
}
const lev2::Layer* Simulation::GetLayer(const PoolString& name) const {
  const lev2::Layer* rval = 0;
  auto it                 = mLayers.find(name);
  if (it != mLayers.end())
    rval = it->second;
  return rval;
}
///////////////////////////////////////////////////////////////////////////

struct MyTimer {
  float mfTimeStart;
  float mfTimeEnd;
  float mfTimeAcc;
  int miCounter;
  std::string mName;

  MyTimer(const char* name)
      : mfTimeStart(0.0f)
      , mfTimeEnd(0.0f)
      , mfTimeAcc(0.0f)
      , miCounter(0)
      , mName(name) {}
  void Start() { mfTimeStart = ork::OldSchool::GetRef().GetLoResTime(); }
  void Stop() {
    mfTimeEnd = ork::OldSchool::GetRef().GetLoResTime();
    mfTimeAcc += (mfTimeEnd - mfTimeStart);
    miCounter++;
    if ((miCounter % 30) == 0) {
      float favgtime = (mfTimeAcc / 30.0f);
      orkprintf("PS<%s> msec<%f>\n", mName.c_str(), favgtime * 1000.0f);
      mfTimeAcc = 0.0f;
    }
  }
};

float Simulation::desiredFrameRate() const {
  auto cmci                  = compositingSystem();
  float frame_rate           = 0.0f;
  bool externally_fixed_rate = (frame_rate != 0.0f);
  // todo:  hook into new NodeCompositorFile node
  return frame_rate;
}

void Simulation::Update() {
  AssertOnOpQ2(UpdateSerialOpQ());
  // ork::msleep(1);
  ComputeDeltaTime();
  static int ictr = 0;

  if (mDeltaTimeAccum > 1.0f)
    mDeltaTimeAccum = 1.0f;

  switch (this->GetSimulationMode()) {
    case ork::ent::ESCENEMODE_PAUSE: {
      ork::lev2::InputManager::poll();
      if (mApplication)
        mApplication->PreUpdate();
      if (mApplication)
        mApplication->PostUpdate();
      break;
    }
    case ork::ent::ESCENEMODE_RUN: {
      ork::PerfMarkerPush("ork.simulation.update.begin");

      ///////////////////////////////
      // Update Components
      ///////////////////////////////

      float frame_rate           = desiredFrameRate();
      bool externally_fixed_rate = (frame_rate != 0.0f);

      // float fdelta = 1.0f/60.0f; //GetDeltaTime();
      float fdelta = GetDeltaTime();

      float step = 0.0f; // ideally should be (1.0f/vsync rate) / some integer

      if (externally_fixed_rate) {
        mDeltaTimeAccum = fdelta;
        step            = fdelta; //(1.0f/120.0f); // ideally should be (1.0f/vsync rate) /
                                  // some integer
      } else {
        mDeltaTimeAccum += fdelta;
        step = 1.0f / 60.0f; //(1.0f/120.0f); // ideally should be (1.0f/vsync
                             // rate) / some integer
      }

      // Nasa - We are doing our own accumulator because there are frame-rate
      // independence bugs in bullet when we are not using a fixed time step
      // around the call to bullet's stepSimulation. Go figure. Nasa - I just
      // verified again that we are still not framerate independent if we take out
      // our own accumulator. 1-30-09.

      mDeltaTimeAccum = fdelta;
      step            = fdelta;

      while (mDeltaTimeAccum >= step) {
        mDeltaTimeAccum -= step;

        ork::lev2::InputManager::poll();

        SetDeltaTime(step);

        bool update = true;
        if (mApplication)
          update = mApplication->PreUpdate();

        if (update) {

          UpdateEntityComponents(GetActiveComponents(sInputFamily));
          UpdateEntityComponents(GetActiveComponents(sControlFamily));
          // timer1.Start();
          UpdateEntityComponents(GetActiveComponents(sPhysicsFamily));
          // timer1.Stop();
          // timer2.Start();
          UpdateEntityComponents(GetActiveComponents(sFrustumFamily));
          // timer2.Stop();
          UpdateEntityComponents(GetActiveComponents(sLightFamily));
          UpdateEntityComponents(GetActiveComponents(sAnimateFamily));
          UpdateEntityComponents(GetActiveComponents(sParticleFamily));
          UpdateEntityComponents(GetActiveComponents(sAudioFamily));

          ///////////////////////////////
          // update the spawn/despawn queues
          ///////////////////////////////

          ServiceDeactivateQueue();
          ServiceActivateQueue();

          mEntityUpdateCount += mActiveEntities.size();
        }

        if (mApplication)
          mApplication->PostUpdate();
      }

      SetDeltaTime(step);

      UpdateEntityComponents(GetActiveComponents(sCameraFamily));

      // todo this atomic will go away when we
      //  complete opq refactor
      _systems.atomicOp([&](const SystemLut& syslut) { _updsyslutcopy = syslut; });
      for (auto sys : _updsyslutcopy)
        sys.second->Update(this);

      ork::PerfMarkerPush("ork.simulation.update.end");

      ///////////////////////////////
      break;
    }
    default:
      break;
  }
  ictr++;
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::addSystem(systemkey_t key, System* system) {
  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](SystemLut& syslut) {
    assert(syslut.find(key) == syslut.end());
    syslut.AddSorted(key, system);
  });
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::beginRenderFrame() const {
  _systems.atomicOp([&](const SystemLut& syslut) { _rensyslutcopy = syslut; });
  for (auto sys : _rensyslutcopy)
    sys.second->beginRenderFrame(this);
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::endRenderFrame() const {
  for (auto sys : _rensyslutcopy)
    sys.second->beginRenderFrame(this);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
