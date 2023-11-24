////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/system.h>
#include <ork/ecs/component.h>
#include <ork/ecs/entity.h>
#include <ork/kernel/orklut.hpp>
#include "message_private.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

void Simulation::_serviceEventQueues() {

  //////////////////////////////////////////////////////////
  // delayed operations
  //////////////////////////////////////////////////////////

  Controller::delayed_opv_t delayed_ops;
  _controller->_pollDelayedOps(this,delayed_ops);
  for(auto op : delayed_ops ){
    op();
  }

  //////////////////////////////////////////////////////////
  // poll current events from controller
  //////////////////////////////////////////////////////////

  _current_events.clear();
  bool in_barrier = _controller->_pollEvents(this,_current_events);

  //////////////////////////////////////////////////////////
  // process events
  //////////////////////////////////////////////////////////

  for (const auto& e : _current_events) {
    bool do_continue = true;
    if( auto as_ev = e.tryAs<Controller::Event>() ){
      if(_controller->_tracewriter)
        _controller->_tracewriter->_traceEvent(as_ev.value());

      do_continue = _onControllerEvent(as_ev.value());
    }
    else if( auto as_req = e.tryAs<Controller::Request>() ){
      if(_controller->_tracewriter)
        _controller->_tracewriter->_traceRequest(as_req.value());
      do_continue = _onControllerRequest(as_req.value());
    }
    else{
      OrkAssert(false);
    }
    if(not do_continue)
      return;
  }

  //////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////
void Simulation::_onSimulationRequest(impl::sim_response_ptr_t response, token_t evID, svar64_t data){

}
///////////////////////////////////////////////////////////////////////////////
bool Simulation::_onControllerEvent(const Controller::Event& event) {
  switch (event._eventID) {
    ///////////////////////////////////////////////////////////////
    case Controller::EventID::SYSTEM_EVENT: {
      const auto& SEV = event._payload.get<impl::_SystemEvent>();
      uint64_t sysid     = SEV._sysref._sysID;
      System* the_system = nullptr;
      _controller->_mutateObject([sysid, &the_system](const Controller::id2obj_map_t& unlocked_id2objmap) {
        auto it = unlocked_id2objmap.find(sysid);
        OrkAssert(it != unlocked_id2objmap.end());
        auto& system_var = it->second;
        the_system       = system_var.get<System*>();
        OrkAssert(the_system!=nullptr);
      });
      the_system->_notify(SEV._eventID,SEV._eventData);
      break;
    }
    ///////////////////////////////////////////////////////////////
    case Controller::EventID::COMPONENT_EVENT: {
      const auto& CEV = event._payload.get<impl::_ComponentEvent>();
      auto the_component = this->_findComponentFromRef(CEV._compref);
      the_component->_notify(this, CEV._eventID,CEV._eventData);
      break;
    }
    ///////////////////////////////////////////////////////////////
    case Controller::EventID::FIND_SYSTEM: {
      const auto& FEV = event._payload.get<impl::_FindSystem>();
      System* the_system = nullptr;
      _systems.atomicOp([&](const SystemLut& unlocked) {
        auto it    = unlocked.find(FEV._syskey);
        bool found = (it != unlocked.end());
        if (found)
          the_system = (it->second);
      });
      _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { //
        OrkAssert(the_system!=nullptr);
        unlocked[FEV._sysref._sysID].set<System*>(the_system);                         //
      });
      break;
    }
    ///////////////////////////////////////////////////////////////
    case Controller::EventID::FIND_COMPONENT: {
      const auto& FCOMP = event._payload.get<impl::_FindComponent>();
      Entity* ent = nullptr;      
        fflush(stdout);
      _controller->_mutateObject([&](const Controller::id2obj_map_t& unlocked) { //
        auto it = unlocked.find(FCOMP._entref._entID);
        OrkAssert(it!=unlocked.end());
        ent = it->second.get<Entity*>();
      });
      auto the_component = ent->GetComponentByClass(FCOMP._compclazz);
      _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { //
        unlocked[FCOMP._compref._compID].set<Component*>(the_component);                         //
      });
      break;
    }
    ///////////////////////////////////////////////////////////////
    case Controller::EventID::DESPAWN: {
      const auto& FDESP = event._payload.get<impl::_Despawn>();
      Entity* ent = nullptr;      
      _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { //
        auto it = unlocked.find(FDESP._entref._entID);
        OrkAssert(it!=unlocked.end());
        ent = it->second.get<Entity*>();
        unlocked.erase(it);
      });
      enqueueDespawnEntity(ent);
      break;
    }
    ///////////////////////////////////////////////////////////////
    case Controller::EventID::ENTITY_BARRIER: {
      break;
    }
    ///////////////////////////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
    ///////////////////////////////////////////////////////////////
  }
  return true; // always continue
}

///////////////////////////////////////////////////////////////////////////////

bool Simulation::_onControllerRequest(const Controller::Request& request) {
  switch (request._requestID) {
    ///////////////////////////////////////////////////////////////
    case Controller::RequestID::SIMULATION_REQUEST:{
      if (auto as_SRQ = request._payload.tryAs<impl::_SimulationRequest>()) {
        const auto& SRQ  = as_SRQ.value();
        uint64_t respID   = SRQ._respref._responseID;
        auto response = std::make_shared<impl::_SimulationResponse>();
        response->_requestID = SRQ._requestID;
        response->_eventData = SRQ._eventData;
        response->_respref = SRQ._respref;
        switch(SRQ._requestID._hashed){
          //////////////////////////////////////////////
          case "EntityPositions"_crcu:{
            if(auto as_future = SRQ._eventData.tryAs<future_ptr_t>()){
              auto records = std::make_shared<entityposmap_t>();
              for( auto item : mEntities ){
                Entity* pent = item.second;
                EntityPosRecord posrec;
                posrec._name = item.first;
                posrec._entref = pent->_entref;
                posrec._archetype = pent->data()->GetArchetype();
                posrec._xform = pent->transform();
                records->push_back(posrec);
              }
              as_future.value()->Signal<entityposmap_ptr_t>(records);
            }
            break;
          }
          //////////////////////////////////////////////
          default:
            OrkAssert(false);
            break;
        }
        _controller->_mutateObject([=](Controller::id2obj_map_t& unlocked) { //
          unlocked[respID].set<impl::sim_response_ptr_t>(response); //
        });
      }
    }
    break;
    ///////////////////////////////////////////////////////////////
    case Controller::RequestID::SPAWN_DYNAMIC_ANON:
      if (auto as_SAD = request._payload.tryAs<impl::_SpawnAnonDynamic>()) {
        const auto& SAD  = as_SAD.value();
        const auto& SREC = SAD._spawn_rec;
        uint64_t objID   = SAD._entref._entID;
        auto entity      = _spawnAnonDynamicEntity(SREC,objID,SAD._SAD._overridexf);
        _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { unlocked[objID].set<Entity*>(entity); });
        fflush(stdout);
      }
      break;
    ///////////////////////////////////////////////////////////////
    case Controller::RequestID::SYSTEM_REQUEST:
      if (auto as_SRQ = request._payload.tryAs<impl::_SystemRequest>()) {
        const auto& SRQ  = as_SRQ.value();
        uint64_t respID   = SRQ._respref._responseID;

        /////////////////////////////
        // register response
        /////////////////////////////

        auto response = std::make_shared<impl::_SystemResponse>();
        response->_sysref = SRQ._sysref;
        response->_requestID = SRQ._requestID;
        response->_eventData = SRQ._eventData;
        response->_respref = SRQ._respref;

        _controller->_mutateObject([=](Controller::id2obj_map_t& unlocked) { //
          unlocked[respID].set<impl::sys_response_ptr_t>(response); //
        });

        /////////////////////////////
        // find target system
        /////////////////////////////

        System* the_system = this->_findSystemFromRef(SRQ._sysref);

        /////////////////////////////
        // perform the request
        /////////////////////////////

        the_system->_request( response, SRQ._requestID, SRQ._eventData );

        /////////////////////////////

      }
      break;
    ///////////////////////////////////////////////////////////////
    case Controller::RequestID::COMPONENT_REQUEST:
      if (auto as_CRQ = request._payload.tryAs<impl::_ComponentRequest>()) {
        const auto& CRQ  = as_CRQ.value();
        uint64_t respID   = CRQ._respref._responseID;

        /////////////////////////////
        // register response
        /////////////////////////////

        auto response = std::make_shared<impl::_ComponentResponse>();
        response->_compref = CRQ._compref;
        response->_requestID = CRQ._requestID;
        response->_eventData = CRQ._eventData;
        response->_respref = CRQ._respref;

        _controller->_mutateObject([=](Controller::id2obj_map_t& unlocked) { //
          unlocked[respID].set<impl::comp_response_ptr_t>(response); //
        });

        /////////////////////////////
        // find target component
        /////////////////////////////

        Component* the_component = this->_findComponentFromRef(CRQ._compref);

        /////////////////////////////
        // perform the request
        /////////////////////////////

        the_component->_request( this, response, CRQ._requestID, CRQ._eventData );

        /////////////////////////////

      }
      break;
    ///////////////////////////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
    ///////////////////////////////////////////////////////////////
  }
  return true; // dont continue for now TODO : put specific barriers in place
}

} //namespace ork::ecs {
