////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/ezapp.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

struct Controller {

	static constexpr uint64_t K_NO_OBJECT_REF = 0xffffffffffffffff;

	///////////////////////////////////////////////////////////////////////////////

	enum struct EventID : crc_enum_t {
	  CrcEnum(SYSTEM_EVENT),
	  CrcEnum(COMPONENT_EVENT),
	  CrcEnum(FIND_SYSTEM),
	  CrcEnum(FIND_COMPONENT),
	  CrcEnum(DESPAWN),
	  CrcEnum(ENTITY_BARRIER),
	  CrcEnum(TRANSPORT_BARRIER),
	};

	enum struct RequestID : crc_enum_t {
	  CrcEnum(SIMULATION_REQUEST),
	  CrcEnum(SYSTEM_REQUEST),
	  CrcEnum(COMPONENT_REQUEST),
	  CrcEnum(SPAWN_DYNAMIC_ANON),
	  CrcEnum(SPAWN_DYNAMIC_NAMED),
	};

	///////////////////////////////////////////////////////////////////////////////

	using evq_t = std::vector<svar32_t>;
	using delayed_opq_t = std::multimap<float, void_lambda_t>;
  using delayed_opv_t = std::vector<void_lambda_t>;

	struct Event {
	  EventID _eventID;
	  svar160_t _payload;
	};
	struct Request {
	  RequestID _requestID;
	  svar160_t _payload;
	};
	using event_ptr_t = std::shared_ptr<Event>;
	using request_ptr_t = std::shared_ptr<Request>;

	struct Transaction{
		std::vector<svar256_t> _items;
	};

	using xact_ptr_t = std::shared_ptr<Transaction>;

	struct TraceWriter {
		TraceWriter(Controller* c, file::Path path);
		~TraceWriter();
		FILE* _output_file = nullptr;
		void _traceEvent(const Event& event);
		void _traceRequest(const Request& request);
		std::string _traceVar128(const svar160_t& var);
		std::string _traceVar64(const svar64_t& var);
		std::string _traceTable(const DataTable& table);
		bool _firstitem = true;
		Controller* _controller = nullptr;
		Timer _outtimer;
	};
	struct TraceReader {
		TraceReader(Controller* c, file::Path path);
		~TraceReader();
		FILE* _input_file = nullptr;
		svar64_t _impl;
		Controller* _controller = nullptr;
	};

	using tracewriter_ptr_t = std::shared_ptr<TraceWriter>;
	using tracereader_ptr_t = std::shared_ptr<TraceReader>;

	///////////////////////////////////////////////////////////////////////////////

	Controller(stringpoolctx_ptr_t strpoolctx=nullptr);
	~Controller();

  float random(float mmin, float mmax);

  void beginWriteTrace(file::Path outpath);
  void readTrace(file::Path inpath);

	///////////////////////////////////////////////////////////////////////////////

	token_t declareToken(std::string name);
	void createSimulation();
	void startSimulation();
	void stopSimulation();
	void endSimulation();
	void bindScene(scenedata_ptr_t scene);
	void gpuInit(lev2::Context* ctx);
	void gpuExit(lev2::Context* ctx);
	void updateExit();
	void render(ui::drawevent_constptr_t drwev);
	void renderWithStandardCompositorFrame(lev2::standardcompositorframe_ptr_t sframe);
	void installRenderCallbackOnEzApp(lev2::orkezapp_ptr_t ezapp);
	void installUpdateCallbackOnEzApp(lev2::orkezapp_ptr_t ezapp);
	void uninstallRenderCallbackOnEzApp(lev2::orkezapp_ptr_t ezapp);
	void uninstallUpdateCallbackOnEzApp(lev2::orkezapp_ptr_t ezapp);

	scenedata_constptr_t scenedata() const { return _scenedata; }
	///////////////////////////////////////////////////////////////////////////////

	void update();
  ent_ref_t spawnNamedDynamicEntity(const SpawnNamedDynamic& SND);
  ent_ref_t spawnAnonDynamicEntity(sad_ptr_t SAD);
	void despawnEntity(const ent_ref_t& EREF);
  
  template <typename T> sys_ref_t findSystem();
  sys_ref_t findSystemWithClassName(std::string clazzname);
  template <typename T> comp_ref_t findEntityComponent(ent_ref_t ent);
  comp_ref_t findComponentWithClassName(ent_ref_t entity, std::string clazzname);

	void realtimeDelayedOperation(float timestamp,void_lambda_t op);
	void presimDelayedOperation(float timestamp,void_lambda_t op);

  void entBarrier(ent_ref_t EREF);

  void systemNotify(sys_ref_t sys, token_t evID, svar64_t data);
  response_ref_t systemRequest(sys_ref_t sys, token_t evID, svar64_t data);

  void componentNotify(comp_ref_t comp, token_t evID, svar64_t data);
  response_ref_t componentRequest(comp_ref_t comp, token_t evID, svar64_t data);

	///////////////////////////////////////////////////////////////////////////////

  response_ref_t simulationRequest(token_t evID, svar64_t data);

	///////////////////////////////////////////////////////////////////////////////

  void forceRetain(const svar64_t& item);

	using id2obj_map_t = tsl::robin_map<uint64_t,svar64_t>;

	LockedResource<simulation_ptr_t> _simulation;

	void_lambda_t _onSimulationExit;
private:
	
	friend struct Simulation;
	friend struct TraceReader;
	friend struct LuaContext;

	void _enqueueEvent(event_ptr_t event);
	void _enqueueRequest(request_ptr_t request);

  void _mutateObject(std::function<void(id2obj_map_t&)> operation);

  void _pollDelayedOps(Simulation* unlocked_sim, delayed_opv_t& opvect);
  bool _pollEvents(Simulation* unlocked_sim, evq_t& evect);

	///////////////////////////////////////////////////////////////////////////////

  using retainvec_t = std::vector<svar64_t>;

	xact_ptr_t _currentXact;

	stringpoolctx_ptr_t _stringpoolcontext;

	LockedResource<delayed_opq_t> _delopq;

	LockedResource<TokMap> _tokmaps;
	scenedata_constptr_t _scenedata;
	

	LockedResource<evq_t> _eventQueue;

	std::atomic<uint64_t> _objectIdCounter;
	LockedResource<id2obj_map_t> _id2objmap;

	tracewriter_ptr_t _tracewriter;
	tracereader_ptr_t _tracereader;
	retainvec_t _forceRetained;

	Timer _timer;
	bool _needsGpuInit = true;

	std::vector<std::shared_ptr<std::string>> _retained_strings;
	
	tsl::robin_map<uint64_t,comp_ref_t> _component_cache;

};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
