////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/rtti/Class.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/system.h>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/lev2/ui/event.h>

#include "message_private.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_controller = logger()->createChannel("ecs.controller",fvec3(0.7,0.7,0));

using namespace ::ork;

///////////////////////////////////////////////////////////////////////////////

Controller::Controller(stringpoolctx_ptr_t strpoolctx)
    : _stringpoolcontext(strpoolctx) {

  if (_stringpoolcontext == nullptr) {
    _stringpoolcontext = StringPoolStack::top();
  }

  ork::opq::assertOnQueue(opq::mainSerialQueue());
  _objectIdCounter.store(0);

}

void Controller::forceRetain(const svar64_t& item){
  _forceRetained.push_back(item);
}

void Controller::installRenderCallbackOnEzApp(lev2::orkezapp_ptr_t ezapp){
  ezapp->onDraw([this](ui::drawevent_constptr_t drwev){
    this->render(drwev);
  });
}


///////////////////////////////////////////////////////////////////////////////

void Controller::beginWriteTrace(file::Path outpath){
  _tracewriter = std::make_shared<TraceWriter>(this,outpath);
}

void Controller::readTrace(file::Path inpath){
  _tracereader = std::make_shared<TraceReader>(this,inpath);
}

///////////////////////////////////////////////////////////////////////////////

Controller::~Controller() {
  ork::opq::assertOnQueue(opq::mainSerialQueue());
}

///////////////////////////////////////////////////////////////////////////////

void Controller::gpuInit(lev2::Context* ctx) {
  _simulation.atomicOp([this,ctx](simulation_ptr_t& unlocked){
    if(unlocked){
      _simulation.atomicOp([ctx](simulation_ptr_t& unlocked){
        //unlocked = nullptr;
      });
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

void Controller::gpuExit(lev2::Context* ctx) {
  _forceRetained.clear();
  _simulation.atomicOp([ctx](simulation_ptr_t& unlocked){
    unlocked->gpuExit(ctx);
    unlocked = nullptr;
  });
}

///////////////////////////////////////////////////////////////////////////////

void Controller::updateExit() {
  //this->stopSimulation();
  _delopq.atomicOp([=](delayed_opq_t& unlocked){
      unlocked.clear();
  });
  _eventQueue.atomicOp([&](Controller::evq_t& unlocked) {
      unlocked.clear();
  });
  _simulation.atomicOp([](simulation_ptr_t& unlocked){
    unlocked->_serviceEventQueues();
  });
  _simulation.atomicOp([](simulation_ptr_t& unlocked){
    unlocked->updateExit();
  });
}

///////////////////////////////////////////////////////////////////////////

void Controller::render(ui::drawevent_constptr_t drwev) {
  if(_needsGpuInit){
    gpuInit(drwev->_target);
      _needsGpuInit = false;
  }
  auto sim = _simulation._unprotected_ref();
  if(sim){
    sim->render(drwev);
  }
}

///////////////////////////////////////////////////////////////////////////

void Controller::renderWithStandardCompositorFrame(lev2::standardcompositorframe_ptr_t sframe) {
  _simulation._unprotected_ref()->renderWithStandardCompositorFrame(sframe);
}

///////////////////////////////////////////////////////////////////////////////

void Controller::_enqueueEvent(event_ptr_t event) {
  _eventQueue.atomicOp([&](evq_t& unlocked) { unlocked.push_back(event); });
}

///////////////////////////////////////////////////////////////////////////////

void Controller::_enqueueRequest(request_ptr_t request) {
  _eventQueue.atomicOp([&](evq_t& unlocked) { unlocked.push_back(request); });
}

///////////////////////////////////////////////////////////////////////////////

void Controller::realtimeDelayedOperation(float timestamp, void_lambda_t op) {

  float abstime = _timer.SecsSinceStart()+timestamp;

  _delopq.atomicOp([=](delayed_opq_t& unlocked){
    auto p = std::pair<float,void_lambda_t>(abstime,op);
    unlocked.insert(p);
  });  
}

///////////////////////////////////////////////////////////////////////////////

void Controller::presimDelayedOperation(float timestamp, void_lambda_t op) {
  _delopq.atomicOp([=](delayed_opq_t& unlocked){
    auto p = std::pair<float,void_lambda_t>(timestamp,op);
    unlocked.insert(p);
  });  
}

///////////////////////////////////////////////////////////////////////////////

void Controller::_pollDelayedOps(Simulation* unlocked_sim, delayed_opv_t& opvect){
  float ctrlrtime = this->_timer.SecsSinceStart();
  this->_delopq.atomicOp([&](Controller::delayed_opq_t& unlocked) {
    bool delayed_events_finished = false;
    while (not delayed_events_finished) {
     delayed_events_finished = true;
     auto it         = unlocked.begin();
     if (it != unlocked.end()) {
        float timestamp = it->first;
        if (timestamp <= ctrlrtime) {
          opvect.push_back(it->second);
          unlocked.erase(it);
          delayed_events_finished = false; // try again
        }
     }
   }
  });

}

///////////////////////////////////////////////////////////////////////////////

bool Controller::_pollEvents(Simulation* unlocked_sim, evq_t& out_events){

  bool in_barrier = false;

  this->_eventQueue.atomicOp([&](Controller::evq_t& unlocked) {
    bool events_finished = false;

    int eventindex = 0;

    while (not events_finished) {
      events_finished = true;
      auto it         = unlocked.begin();
      if (it != unlocked.end()) {

        ////////////////////////////////////
        // first respect barriers
        ////////////////////////////////////

        auto& evreq = *it;

        if( auto as_ev = evreq.tryAs<Controller::event_ptr_t>() ){

          auto& payload = as_ev.value()->_payload;

          if(auto as_barrier = payload.tryAs<impl::entbarrier_ptr_t>() ){
            auto entref = as_barrier.value()->_entref;
            auto ent = unlocked_sim->_findEntityFromRef(entref);
            if( ent == nullptr ){
              in_barrier = true;
              events_finished = true;
              return in_barrier;
            }
            else{
              //OrkAssert(false);
            }

          }
          else if(auto as_barrier = payload.tryAs<impl::transportbarrier_ptr_t>() ){
            auto desired_state = as_barrier.value()->_waitForState;
            // barrier condition exists so long as
            //  the actual transport state does not match the desired transport state
            in_barrier = ( desired_state != unlocked_sim->_transportState );
            if(in_barrier){
              return true;              
            }
          }
        }

        ////////////////////////////////////
        // then respect timestamps
        ////////////////////////////////////

        out_events.push_back(evreq);
        unlocked.erase(it);
        events_finished = false; // try again

        ////////////////////////////////////

      }
      eventindex++;
    }
  });
  return in_barrier;
}

///////////////////////////////////////////////////////////////////////////////

ent_ref_t Controller::spawnNamedDynamicEntity(const SpawnNamedDynamic& SND) {
  /*
      ////////////////////////////////////////////////////////
      case RequestID::SPAWN_DYNAMIC_NAMED: {
        uint64_t objID = _objectIdCounter.fetch_add(1);
        auto& IMPL     = request._impl.make<impl::_SpawnNamedDynamic>();
        IMPL._objectID = objID;

        IMPL._SND       = request._payload.get<SpawnNamedDynamic>();
        IMPL._spawn_rec = _scenedata->findTypedObject<SpawnData>(IMPL._SND._edataname);
        OrkAssert(IMPL._spawn_rec);

        auto p = std::make_pair(request._timestamp, request);
        _requestQueue.atomicOp([&](reqq_t& unlocked) { unlocked.insert(p); });

        ent_ref_t eref;
        eref._entID      = objID;
        reponse._payload = eref;

        break;
      }

    Request req;

    req._requestID = RequestID::SPAWN_DYNAMIC_NAMED;
    req._payload.set<SpawnNamedDynamic>(SND);
    req._timestamp = timestamp;
    return _enqueueRequest(req);*/

  return ent_ref_t();
}

///////////////////////////////////////////////////////////////////////////////

ent_ref_t Controller::spawnAnonDynamicEntity(sad_ptr_t SAD) {
  auto req = std::make_shared<Request>();

  req->_requestID = RequestID::SPAWN_DYNAMIC_ANON;

  uint64_t objID = _objectIdCounter.fetch_add(1);

  auto& IMPL      = req->_payload.make<impl::_SpawnAnonDynamic>();
  IMPL._SAD       = SAD;

  if( SAD->_userspawndata ){
    IMPL._spawn_rec = SAD->_userspawndata;
  }
  else{
    //printf("FIND SAD->_edataname<%s>\n", SAD->_edataname.c_str());
    IMPL._spawn_rec = _scenedata->findTypedObject<SpawnData>(SAD->_edataname);
  }

  OrkAssert(IMPL._spawn_rec);

  ent_ref_t eref;
  IMPL._entref._entID = objID;

  _enqueueRequest(req);

  //printf( "SPAWNANONDYNAMIC<%zu>\n", objID );

  return IMPL._entref;
}

///////////////////////////////////////////////////////////////////////////////

void Controller::despawnEntity(const ent_ref_t& EREF){
  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::DESPAWN;
  auto& DEV           = simevent->_payload.make<impl::_Despawn>();

  DEV._entref    = EREF;

  //printf( "DESPAWN<%zu>\n", EREF._entID );

  _enqueueEvent(simevent);
}

///////////////////////////////////////////////////////////////////////////////

void Controller::entBarrier(ent_ref_t EREF){
  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::ENTITY_BARRIER;
  auto BEV = std::make_shared<impl::_EntBarrier>();
  BEV->_entref = EREF;
  simevent->_payload.make<impl::entbarrier_ptr_t>(BEV);
  _enqueueEvent(simevent);
}

///////////////////////////////////////////////////////////////////////////////

//void Controller::beginTransaction(){
  //_currentXact = std::make_shared<Transaction>();
//}
//void Controller::endTransaction(){

  //Event simevent;
  //simevent->_eventID   = EventID::TRANSACTION;
  //auto BEV = std::make_shared<impl::_EntBarrier>();
  //BEV->_entref = EREF;
  //simevent->_payload.make<impl::entbarrier_ptr_t>(BEV);
  //_enqueueEvent(simevent);
  //_currentXact = nullptr;

//}


///////////////////////////////////////////////////////////////////////////////

void Controller::systemNotify(sys_ref_t sys, token_t evID, svar64_t data) {

  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::SYSTEM_EVENT;
  auto& SEV           = simevent->_payload.make<impl::_SystemEvent>();

  SEV._sysref    = sys;
  SEV._eventID   = evID;
  SEV._eventData = data;

  _enqueueEvent(simevent);
}

///////////////////////////////////////////////////////////////////////////////

response_ref_t Controller::systemRequest(sys_ref_t sys, token_t reqID, svar64_t data) {
  auto simrequest = std::make_shared<Request>();
  simrequest->_requestID = RequestID::SYSTEM_REQUEST;

  uint64_t objID = _objectIdCounter.fetch_add(1);

  auto rref = ResponseRef{._responseID = objID};

  auto& SRQ = simrequest->_payload.make<impl::_SystemRequest>();

  SRQ._sysref    = sys;
  SRQ._requestID = reqID;
  SRQ._eventData = data;
  SRQ._respref   = rref;

  _enqueueRequest(simrequest);

  return rref;
}

///////////////////////////////////////////////////////////////////////////////

void Controller::componentNotify(comp_ref_t comp, token_t evID, svar64_t data) {

  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::COMPONENT_EVENT;
  auto& CEV           = simevent->_payload.make<impl::_ComponentEvent>();

  CEV._compref   = comp;
  CEV._eventID   = evID;
  CEV._eventData = data;

  _enqueueEvent(simevent);
}

///////////////////////////////////////////////////////////////////////////////

response_ref_t Controller::componentRequest(comp_ref_t comp, token_t reqID, svar64_t data) {

  auto simrequest = std::make_shared<Request>();
  simrequest->_requestID = RequestID::COMPONENT_REQUEST;
  auto& CRQ             = simrequest->_payload.make<impl::_ComponentRequest>();

  uint64_t objID = _objectIdCounter.fetch_add(1);

  auto rref = ResponseRef{._responseID = objID};

  CRQ._compref   = comp;
  CRQ._requestID = reqID;
  CRQ._eventData = data;
  CRQ._respref   = rref;

  _enqueueRequest(simrequest);

  return rref;
}

///////////////////////////////////////////////////////////////////////////////

response_ref_t Controller::simulationRequest(token_t reqID, svar64_t data) {

  auto simrequest = std::make_shared<Request>();
  simrequest->_requestID = RequestID::SIMULATION_REQUEST;
  auto& SRQ             = simrequest->_payload.make<impl::_SimulationRequest>();

  uint64_t objID = _objectIdCounter.fetch_add(1);

  auto rref = ResponseRef{._responseID = objID};

  SRQ._requestID = reqID;
  SRQ._eventData = data;
  SRQ._respref   = rref;

  _enqueueRequest(simrequest);

  return rref;
}

///////////////////////////////////////////////////////////////////////////////

token_t Controller::declareToken(std::string name) {
  token_t rval = nullptr;

  _tokmaps.atomicOp([name, &rval](TokMap& unlocked) {
    auto it = unlocked._str2tok_map.find(name);
    if (it == unlocked._str2tok_map.end()) {
      auto crc                            = CrcString(name.c_str());
      unlocked._str2tok_map[name]         = crc.hashed();
      unlocked._tok2str_map[crc.hashed()] = name;
    } else { // found
      rval = it->second;
    }
  });

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Controller::_mutateObject(std::function<void(id2obj_map_t&)> operation) {
  _id2objmap.atomicOp(operation);
}

///////////////////////////////////////////////////////////////////////////////

void Controller::bindScene(scenedata_ptr_t scene) {
  scene->prepareForSimulation();
  _scenedata = scene;
}

///////////////////////////////////////////////////////////////////////////////

void Controller::createSimulation() {
  OrkAssert(_scenedata);
  logchan_controller->log( "INSTANTIATING SIMULATION");
  _simulation.atomicOp([this](simulation_ptr_t& unlocked){
    unlocked = std::make_shared<Simulation>(this);
  });
}

///////////////////////////////////////////////////////////////////////////////

void Controller::startSimulation() {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto op = [this](){
    _timer.Start();
    logchan_controller->log( "STARTING SIMULATION");
    _simulation.atomicOp([](simulation_ptr_t& unlocked){
      unlocked->SetSimulationMode(ESimulationMode::ACTIVE);
      unlocked->_serviceEventQueues();
    });
  };
  opq::updateSerialQueue()->enqueue(op);
  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::TRANSPORT_BARRIER;
  auto TEV = std::make_shared<impl::_TransportBarrier>();
  TEV->_waitForState = ESimulationTransport::ACTIVATED;
  simevent->_payload.make<impl::transportbarrier_ptr_t>(TEV);
  _enqueueEvent(simevent);

}

///////////////////////////////////////////////////////////////////////////////

void Controller::stopSimulation() {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  logchan_controller->log( "STOPPING SIMULATION");
  _delopq.atomicOp([=](delayed_opq_t& unlocked){
      unlocked.clear();
  });
  _eventQueue.atomicOp([&](Controller::evq_t& unlocked) {
      unlocked.clear();
  });
  _simulation.atomicOp([](simulation_ptr_t& unlocked){
    unlocked->SetSimulationMode(ESimulationMode::EDIT);
    unlocked->_serviceEventQueues();
  });
  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::TRANSPORT_BARRIER;
  auto TEV = std::make_shared<impl::_TransportBarrier>();
  TEV->_waitForState = ESimulationTransport::TERMINATED;
  simevent->_payload.make<impl::transportbarrier_ptr_t>(TEV);
  _enqueueEvent(simevent);
}

///////////////////////////////////////////////////////////////////////////////

void Controller::update() {

  std::vector<deferred_script_invokation_ptr_t> _invokations;

  _simulation.atomicOp([&_invokations](simulation_ptr_t& unlocked){
    unlocked->_update();
    _invokations = unlocked->dequeueDeferredInvokations();
  });

  // deferred script invokations
  
  for (auto item : _invokations) {
    item->_cb(item->_data);
  }
}

///////////////////////////////////////////////////////////////////////////////

float Controller::random(float mmin, float mmax) {
  float r = float(rand() & 0xffff) / 65536.0f;
  return mmin + (r * (mmax - mmin));
}

sys_ref_t Controller::findSystemWithClassName(std::string clazzname) {
  uint64_t ID = _objectIdCounter.fetch_add(1);

  //////////////////////////////////////////////////////
  // notify sim to update reference
  //////////////////////////////////////////////////////

  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::FIND_SYSTEM;
  auto& FSYS          = simevent->_payload.make<impl::_FindSystem>();


  auto retain_str = std::make_shared<std::string>(clazzname);
  _retained_strings.push_back(retain_str);

  FSYS._sysref = SystemRef{._sysID = ID};
  FSYS._syskey = *retain_str;

  _enqueueEvent(simevent);

  //////////////////////////////////////////////////////
  // return opaque handle
  //////////////////////////////////////////////////////

  return FSYS._sysref;
}

comp_ref_t Controller::findComponentWithClassName(ent_ref_t ent, std::string clazzname) {

  boost::Crc64 crc;
  crc.init();
  crc.accumulateItem<uint64_t>(ent._entID);
  crc.accumulateString(clazzname);
  crc.finish();
  uint64_t cache_key = crc.result();

  auto it = _component_cache.find(cache_key);
  if(it!=_component_cache.end()){
    return it->second;
  }
  uint64_t ID = _objectIdCounter.fetch_add(1);

  //////////////////////////////////////////////////////
  // notify sim to update reference
  //////////////////////////////////////////////////////

  auto simevent = std::make_shared<Event>();
  simevent->_eventID   = EventID::FIND_COMPONENT;
  auto& FCOMP          = simevent->_payload.make<impl::_FindComponent>();

  auto clazz = ::ork::rtti::Class::FindClass(clazzname);
  //printf( "find class<%s> -> %p\n", clazzname.c_str(), (void*) clazz );
  FCOMP._entref = ent;
  FCOMP._compclazz = clazz;
  FCOMP._compref = ComponentRef({._compID=ID});

  _enqueueEvent(simevent);

  //////////////////////////////////////////////////////
  // write to cache and return opaque handle
  //////////////////////////////////////////////////////

  _component_cache[cache_key] = FCOMP._compref;

  return FCOMP._compref;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
