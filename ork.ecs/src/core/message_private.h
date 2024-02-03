////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/string/PoolString.h>
#include <ork/ecs/types.h>

///////////////////////////////////////////////////////////////////////////////
// ECS Private Message Structs
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs::impl {

struct _SpawnNamedDynamic {
	SpawnNamedDynamic _SND;
  spawndata_constptr_t _spawn_rec;
  ent_ref_t _entref;
};

struct _SpawnAnonDynamic {
	SpawnAnonDynamic _SAD;
  spawndata_constptr_t _spawn_rec;
  ent_ref_t _entref;
};

struct _Despawn {
  ent_ref_t _entref;
};

struct _EntBarrier {
  ent_ref_t _entref;
};

using entbarrier_ptr_t = std::shared_ptr<_EntBarrier>;

struct _FindSystem {
  sys_ref_t _sysref;
  systemkey_t _syskey;
};

struct _FindComponent {
  ent_ref_t _entref;
  comp_ref_t _compref;
  rtti::Class* _compclazz = nullptr;
};

struct _SystemEvent {
  svar64_t _eventData;
  sys_ref_t _sysref;
  token_t _eventID;
};

struct _SimulationRequest {
  svar64_t _eventData;
  token_t _requestID;
  response_ref_t _respref;
};

struct _SystemRequest {
  svar64_t _eventData;
  sys_ref_t _sysref;
  token_t _requestID;
  response_ref_t _respref;
};
struct _SystemResponse {
  svar64_t _eventData;
  svar64_t _responseData;
  sys_ref_t _sysref;
  token_t _requestID;
  response_ref_t _respref;
};

struct _ComponentEvent {
  svar64_t _eventData;
  comp_ref_t _compref;
  token_t _eventID;
};
struct _ComponentRequest {
  svar64_t _eventData;
  comp_ref_t _compref;
  token_t _requestID;
  response_ref_t _respref;
};
struct _ComponentResponse {
  svar64_t _eventData;
  svar64_t _responseData;
  comp_ref_t _compref;
  token_t _requestID;
  response_ref_t _respref;
};
struct _SimulationResponse {
  svar64_t _eventData;
  svar64_t _responseData;
  token_t _requestID;
  response_ref_t _respref;
};

} //namespace ork::ecs::impl {
