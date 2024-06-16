////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>

#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/ui/event.h>

#include <ork/ecs/ReferenceArchetype.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/system.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/scene.inl>
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<const ork::object::ObjectClass*, ork::ecs::System*>;

namespace ork::ecs {

static logchannel_ptr_t logchan_simulation = logger()->createChannel("ecs-simulation",fvec3(0.8,0.8,0));

using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;

///////////////////////////////////////////////////////////////////////////////
Simulation::Simulation(Controller* c)
    : _controller(c) {

  _dbufctxSIM = std::make_shared<lev2::DrawBufContext>();
  _dbufctxSIM->_name = "DBC.Simulation";

  ///////////////////////////////////////////////////////////

  _buildStateMachine();

  ///////////////////////////////////////////////////////////

  _dynname_serno.store(0);

  auto default_layer = new lev2::LayerData;
  AddLayerData("Default", default_layer);

  _InputFamily     = "input"_pool;
  _AudioFamily     = "audio"_pool;
  _CameraFamily    = "camera"_pool;
  _ControlFamily   = "control"_pool;
  _PhysicsFamily   = "physics"_pool;
  _FrustumFamily   = "frustum"_pool;
  _AnimateFamily   = "animate"_pool;
  _ParticleFamily  = "particle"_pool;
  _LightFamily     = "lighting"_pool;
  _PreRenderFamily = "PreRender"_pool;

  ////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
Simulation::~Simulation() {

  ork::opq::assertOnQueue(opq::mainSerialQueue());

    _systems.atomicOp([&](SystemLut& unlocked) { 
      for( auto item : unlocked ){
        auto system = item.second;
        delete system;
      }
      unlocked.clear();
    });

}

///////////////////////////////////////////////////////////////////////////////

void Simulation::_stashRenderThreadDestructable(svar64_t var){
  _renderthreaddestructables.atomicOp([=](destructables_vect_t& unlocked){
    unlocked.push_back(var);
  });
}

///////////////////////////////////////////////////////////////////////////////
void Simulation::gpuExit(lev2::Context* ctx){
    SystemLut render_systems;
    _systems.atomicOp([&](const SystemLut& unlocked) { render_systems = unlocked; });

    for (auto sys : render_systems) {
      sys.second->_onGpuExit(this, ctx);
    }
    _renderThreadSM->update();
    _renderThreadSM->update();
    _renderThreadSM->update();
    _renderThreadSM = nullptr;

    // deferred renderthread destructable destruction

  _renderthreaddestructables.atomicOp([=](destructables_vect_t& unlocked){
    unlocked.clear();
  });

}
void Simulation::updateExit(){
  SetSimulationMode(ESimulationMode::TERMINATED);
  _updateThreadSM->update();
  _updateThreadSM->update();
  _updateThreadSM->update();
  _updateThreadSM = nullptr;
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_mutateControllerObject(std::function<void(Controller::id2obj_map_t&)> operation){
  _controller->_mutateObject(operation);
}
///////////////////////////////////////////////////////////////////////////
scenedata_constptr_t Simulation::GetData() const {
  return _controller->_scenedata;
}
///////////////////////////////////////////////////////////////////////////

PoolString Simulation::genDynamicEntityName() {
  int i = _dynname_serno.fetch_add(1);
  return AddPooledString(FormatString("dynamic_entity%d", i).c_str());
}

///////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_REFL_REGISTRATION)

CompositingSystem* Simulation::compositingSystem() {
  return findSystem<CompositingSystem>();
}
const CompositingSystem* Simulation::compositingSystem() const {
  return findSystem<CompositingSystem>();
}

#endif

///////////////////////////////////////////////////////////////////////////////

float Simulation::random(float mmin, float mmax) {
  float r = float(rand() & 0xffff) / 65536.0f;
  return mmin + (r * (mmax - mmin));
}


///////////////////////////////////////////////////////////////////////////////
void Simulation::setCameraData(const std::string& name, lev2::cameradata_constptr_t camdat) {
  _cameraDataLUT[name] = camdat;

  lev2::UiCamera* pcam = (camdat != 0) ? camdat->getUiCamera() : 0;

  // logchan_simulation->log( "Simulation::setCameraData() name<%s> camdat<%p> l2cam<%p>\n",
  // name.c_str(), camdat, pcam );
}

///////////////////////////////////////////////////////////////////////////////
lev2::cameradata_constptr_t Simulation::cameraData(const std::string& name) const {
  return _cameraDataLUT.find(name);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::enqueueActivateDynamicEntity(const EntityActivationQueueItem& item) {
  mEntityActivateQueue.push_back(item);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::enqueueDespawnEntity(Entity* pent) {
  mEntityDeactivateQueue.push_back(pent);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::registerActivatedEntity(ecs::Entity* pent) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  //debugBanner(255, 255, 0, "ACTIVATING DYNAMIC ENTITY<%p>\n", (void*) pent );

  //if (not mActiveEntities.contains(pent)) {
  if (mActiveEntities.find(pent)==mActiveEntities.end()) {
    mActiveEntities.insert(pent);

    /////////////////////////////////////////
    // activate components
    /////////////////////////////////////////

    ecs::ComponentTable::LutType& EntComps = pent->GetComponents().GetComponents();
    for (auto itc : EntComps ) {
      ecs::Component* cinst = itc.second;
      cinst->_activate(this);
    }

  } else {
    logchan_simulation->log("WARNING, activating an already active entity <%p>\n", pent);
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::registerDeactivatedEntity(ecs::Entity* pent) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
   //printf( "DeActivateEntity<%p:%s>\n",  pent,
   //pent->data()->GetName().c_str() );

  EntitySet::iterator listit = mActiveEntities.find(pent);

  auto parch = pent->data()->GetArchetype();
  if (listit == mActiveEntities.end()) {
    PoolString parchname = (parch != 0) ? parch->GetName() : AddPooledLiteral("none");
    PoolString pentname  = pent->name();

    logchan_simulation->log(
        "uhoh, someone is deactivating an entity<%p:%s> of arch<%s> that "
        "is not active!!!",
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

  ecs::ComponentTable::LutType& EntComps = pent->GetComponents().GetComponents();
  for (ecs::ComponentTable::LutType::const_iterator itc = EntComps.begin(); itc != EntComps.end(); itc++) {
    ecs::Component* cinst = (*itc).second;
    cinst->_deactivate(this);
  }

  if (parch){
    parch->deactivateEntity(this, pent);
    parch->unstageEntity(this, pent);
    parch->unlinkEntity(this, pent);
    //parch->decomposeEntity(pent);
    //delete pent;

    for( auto it = mEntities.begin(); it!=mEntities.end(); ){
      bool match = (it->second==pent);
      it = match ? mEntities.erase(it) : std::next(it);
    }

  }
}

bool Simulation::IsEntityActive(Entity* pent) const {
  auto listit = mActiveEntities.find(pent);
  return (listit != mActiveEntities.end());
}
///////////////////////////////////////////////////////////////////////////

Entity* Simulation::_spawnAnonDynamicEntity(spawndata_constptr_t spawn_rec, int entref,decompxf_ptr_t ovxf) {
  auto name   = genDynamicEntityName();
  auto newent = _spawnNamedDynamicEntity(spawn_rec, name,entref,ovxf);
  return newent;
}

Entity* Simulation::_spawnNamedDynamicEntity(spawndata_constptr_t spawn_rec, PoolString name, int entref,decompxf_ptr_t ovxf) {
  auto newent = new Entity(spawn_rec, this,entref);
  /////////////////////////////////////
  // per spawn override of intial transform ?
  /////////////////////////////////////
  if(ovxf){
    newent->_override_initial_xf = ovxf;
    newent->setTransform(ovxf);
  }
  /////////////////////////////////////
  auto arch   = spawn_rec->GetArchetype();
  arch->composeEntity(this,newent);
  arch->linkEntity(this, newent);
  auto xform = std::make_shared<DecompTransform>();
  EntityActivationQueueItem qi(xform, newent);
  //debugBanner(255, 0, 0, "_spawnNamedDynamicEntity<%s:%p>\n", name.c_str(), (void*) newent );
  this->enqueueActivateDynamicEntity(qi);
  mEntities[name] = newent;
  return newent;
}
///////////////////////////////////////////////////////////////////////////////
ecs::Entity* Simulation::findEntity(PoolString entity) const {
  orkmap<PoolString, Entity*>::const_iterator it = mEntities.find(entity);
  return (it == mEntities.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
ecs::Entity* Simulation::findEntityLoose(PoolString entity) const {
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
///////////////////////////////////////////////////////////////////////////
void Simulation::AddLayerData(const std::string& name, lev2::LayerData* player) {
  auto it = _layerdataMap.find(name);
  OrkAssert(it == _layerdataMap.end());
  _layerdataMap[name] = player;
  player->_layerName  = name;
}
lev2::LayerData* Simulation::GetLayerData(const std::string& name) {
  lev2::LayerData* rval = 0;
  auto it               = _layerdataMap.find(name);
  if (it != _layerdataMap.end())
    rval = it->second;
  return rval;
}
const lev2::LayerData* Simulation::GetLayerData(const std::string& name) const {
  const lev2::LayerData* rval = 0;
  auto it                     = _layerdataMap.find(name);
  if (it != _layerdataMap.end())
    rval = it->second;
  return rval;
}

///////////////////////////////////////////////////////////////////////////

float Simulation::desiredFrameRate() const {
  float frame_rate           = 0.0f;
  bool externally_fixed_rate = (frame_rate != 0.0f);
  // todo:  hook into new NodeCompositorFile node
  return frame_rate;
}

void Simulation::_enqueueDeferredInvokation(deferred_script_invokation_ptr_t i){
  _deferred_invokations.push_back(i);
}
std::vector<deferred_script_invokation_ptr_t> Simulation::dequeueDeferredInvokations(){
  auto copy = _deferred_invokations;
  _deferred_invokations.clear();
  return copy;
}

///////////////////////////////////////////////////////////////////////////////
void Simulation::addSystem(systemkey_t key, System* system) {
  // todo this atomic will go away when we
  //  complete opq refactor
  _systems.atomicOp([&](SystemLut& syslut) {
    // assert(syslut.find(key) == syslut.end());
    auto it = syslut.find(key);
    if (it != syslut.end()) {
      auto old_sys = it->second;
      syslut.erase(it);
      // todo - dont delete yet, we need to unlink and relink..
    }
    syslut.AddSorted(key, system);
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::render(ui::drawevent_constptr_t drwev) {
  _currentdrwev = drwev;
  _renderThreadSM->update();
  _currentdrwev = nullptr;
}
///////////////////////////////////////////////////////////////////////////
void Simulation::renderWithStandardCompositorFrame(lev2::standardcompositorframe_ptr_t sframe){
  _currentdrwev = sframe->_drawEvent;
  _renderThreadSM->setVar("sframe"_crc,sframe);
  _renderThreadSM->update();
  _currentdrwev = nullptr;
}
///////////////////////////////////////////////////////////////////////////////

Component* Simulation::_findComponentFromRef(comp_ref_t ref) {
  Component* the_component = nullptr;
  uint64_t compid          = ref._compID;
  _controller->_mutateObject([compid, &the_component](const Controller::id2obj_map_t& unlocked) {
    auto it = unlocked.find(compid);
    OrkAssert(it != unlocked.end());
    auto& component_var = it->second;
    the_component       = component_var.get<Component*>();
  });
  return the_component;
}
///////////////////////////////////////////////////////////////////////////////

Entity* Simulation::_findEntityFromRef(ent_ref_t ref) {
  Entity* the_entity = nullptr;
  uint64_t entid          = ref._entID;
  _controller->_mutateObject([entid, &the_entity](const Controller::id2obj_map_t& unlocked) {
    auto it = unlocked.find(entid);
    if(it != unlocked.end()){
      auto& var = it->second;
      the_entity       = var.get<Entity*>();
    }
  });
  return the_entity;
}

///////////////////////////////////////////////////////////////////////////////

System* Simulation::_findSystemFromRef(sys_ref_t ref) {
  System* the_system = nullptr;
  uint64_t sysid     = ref._sysID;
  _controller->_mutateObject([sysid, &the_system](const Controller::id2obj_map_t& unlocked) {
    auto it = unlocked.find(sysid);
    OrkAssert(it != unlocked.end());
    the_system = it->second.get<System*>();
  });
  return the_system;
}
///////////////////////////////////////////////////////////////////////////////
impl::sys_response_ptr_t Simulation::_findSystemResponseFromRef(response_ref_t ref){
  impl::sys_response_ptr_t the_response;
  uint64_t respid     = ref._responseID;
  _controller->_mutateObject([respid, &the_response](const Controller::id2obj_map_t& unlocked) {
    auto it = unlocked.find(respid);
    OrkAssert(it != unlocked.end());
    the_response = it->second.get<impl::sys_response_ptr_t>();
  });
  return the_response;
}
///////////////////////////////////////////////////////////////////////////////
impl::comp_response_ptr_t Simulation::_findComponentResponseFromRef(response_ref_t ref){
  impl::comp_response_ptr_t the_response;
  uint64_t respid     = ref._responseID;
  _controller->_mutateObject([respid, &the_response](const Controller::id2obj_map_t& unlocked) {
    auto it = unlocked.find(respid);
    OrkAssert(it != unlocked.end());
    the_response = it->second.get<impl::comp_response_ptr_t>();
  });
  return the_response;
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::debugBanner( int r, int g, int b, const char* formatstring, ... ){
  char formatbuffer[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf( &formatbuffer[0], sizeof(formatbuffer), formatstring, args );
  va_end(args);
  std::string pstr = deco::asciic_rgb256(r,g,b)+formatbuffer+deco::asciic_reset();
  printf( "%s", pstr.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
