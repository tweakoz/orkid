////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/crc.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/string/PoolString.h>
#include <unordered_map>
#include <ork/util/tsl/robin_map.h>
#include <ork/python/wraprawpointer.inl>

namespace ork {
  struct DecompTransform;
  using decompxf_ptr_t = std::shared_ptr<DecompTransform>;
  namespace lev2 {
    struct LayerData;
    struct Context;
  }
  namespace ui {
    struct DrawEvent;
    using drawevent_constptr_t = std::shared_ptr<const DrawEvent>;
  }
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

typedef std::string_view systemkey_t;

struct SystemDataClass;
struct ComponentDataClass;

struct SceneData;
struct SpawnData;
struct Archetype;
struct CompositeArchetype;
struct ReferenceArchetype;
struct SystemData;
struct ComponentData;
struct ComponentFragmentData;

struct Controller;
struct Simulation;
struct Entity;
struct Component;
struct ComponentFragment;
struct System;
struct SystemFragment;

struct CompositingSystem;
struct AudioSystem;

struct DagNodeData;
struct SceneObject;
struct SceneDagObject;
struct DagComponent;
struct SceneComposer;

struct SceneGraphComponentData;
struct SceneGraphSystemData;

struct EntRef;

struct ECSTOK;
struct DataTable;

struct ScriptWrapper;
struct LuaContext;

struct BulletSystemData;
struct BulletObjectComponentData;

struct PythonComponentData;
struct PythonSystemData;
struct PythonComponent;
struct PythonSystem;

///////////////////////////////////////////////////////////////////////////////

using datatable_ptr_t = std::shared_ptr<DataTable>;

///////////////////////////////////////////////////////////////////////////////

enum ESceneDataMode {
  ESCENEDATAMODE_NEW = 0,
  ESCENEDATAMODE_INIT,
  ESCENEDATAMODE_EDIT,
  ESCENEDATAMODE_RUN,
};

enum class ESimulationMode : uint64_t {
  NEW = 0,     // not attached
  READY, // not attached
  EDIT,    // editing
  ACTIVE,   // single stepping
  PAUSE,      // paused
  TERMINATED,  // terminated
  NONE,
};

enum class ESimulationTransport : uint64_t {
  NEW = 0,    
  INITIALIZED,
  COMPOSED,
  LINKED,
  STAGED,
  ACTIVATED,
  TERMINATED,
  NONE,
};

/*
enum EUpdateState {
  EUPD_STOPPED,
  EUPD_START,
  EUPD_RUNNING,
  EUPD_STOP,
};*/

///////////////////////////////////////////////////////////////////////////////

constexpr uint64_t NO_OBJECT_ID = 0xffffffffffffffff;

struct EntityRef{
  uint64_t _entID = NO_OBJECT_ID;
};

struct SystemRef{
  uint64_t _sysID = NO_OBJECT_ID;
};

struct SystemFragRef{
  uint64_t _sysFragID = NO_OBJECT_ID;
};

struct ComponentRef{
  uint64_t _compID = NO_OBJECT_ID;
};

struct SystemFragmentRef{
  uint64_t _sysFragID = NO_OBJECT_ID;
};

struct ResponseRef{
  uint64_t _responseID = NO_OBJECT_ID;
};

///////////////////////////////////////////////////////////////////////////////

consteval CrcString operator"" _tok(const char* s, size_t len) {
  return CrcString(crc32_recurse2(KENDHASH, s));
}

///////////////////////////////////////////////////////////////////////////////

using token_t = CrcString;

using str2tok_map_t = tsl::robin_map<std::string,uint64_t>;
using tok2str_map_t = tsl::robin_map<uint64_t,std::string>;
struct TokMap {
  str2tok_map_t _str2tok_map;
  tok2str_map_t _tok2str_map;
};

token_t tokenize(const char* str);
token_t tokenize(const ECSTOK& tok);
token_t tokenize(const std::string& str);
std::string detokenize(token_t token);

using evdata_t = svar64_t;

using script_cb_t = std::function<void(const evdata_t&)>;
struct deferred_script_invokation {
  script_cb_t _cb;
  evdata_t _data;
};

using deferred_script_invokation_ptr_t = std::shared_ptr<deferred_script_invokation>;

///////////////////////////////////////////////////////////////////////////////

using scriptwrapper_t = std::shared_ptr<ScriptWrapper>;

using ent_ref_t = EntityRef;
using comp_ref_t = ComponentRef;
using compfrag_ref_t = ComponentRef;
using sys_ref_t = SystemRef;
using sysfrag_ref_t = SystemFragmentRef;
using response_ref_t = ResponseRef;

using spawndata_ptr_t = std::shared_ptr<SpawnData>;
using scenedata_ptr_t = std::shared_ptr<SceneData>;
using archetype_ptr_t = std::shared_ptr<Archetype>;
using compositearchetype_ptr_t = std::shared_ptr<CompositeArchetype>;
using sceneobject_ptr_t = std::shared_ptr<SceneObject>;
using componentdata_ptr_t = std::shared_ptr<ComponentData>;
using systemdata_ptr_t = std::shared_ptr<SystemData>;
using dagnodedata_ptr_t = std::shared_ptr<DagNodeData>;
using scenedagobject_ptr_t = std::shared_ptr<SceneDagObject>;

using spawndata_constptr_t = std::shared_ptr<const SpawnData>;
using scenedata_constptr_t = std::shared_ptr<const SceneData>;
using archetype_constptr_t = std::shared_ptr<const Archetype>;
using sceneobject_constptr_t = std::shared_ptr<const SceneObject>;
using cfragmentdata_constptr_t = std::shared_ptr<const ComponentFragmentData>;
using componentdata_constptr_t = std::shared_ptr<const ComponentData>;
using componentdata_ptr_t = std::shared_ptr<ComponentData>;
using systemdata_constptr_t = std::shared_ptr<const SystemData>;
using dagnodedata_constptr_t = std::shared_ptr<const DagNodeData>;
using scenedagobject_constptr_t = std::shared_ptr<const SceneDagObject>;

using bulletsysdata_ptr_t = std::shared_ptr<BulletSystemData>;
using bulletcompdata_ptr_t = std::shared_ptr<BulletObjectComponentData>;

using sgcomponentdata_ptr_t = std::shared_ptr<SceneGraphComponentData>;

using pysysdata_ptr_t = std::shared_ptr<PythonSystemData>;
using pycompdata_ptr_t = std::shared_ptr<PythonComponentData>;

using controller_ptr_t = std::shared_ptr<Controller>;
using simulation_ptr_t = std::shared_ptr<Simulation>;

using sgsystemdata_ptr_t = std::shared_ptr<SceneGraphSystemData>;

struct SpawnNamedDynamic {
  PoolString _edataname;
  PoolString _entname;
};
struct SpawnAnonDynamic {
  PoolString _edataname;
  spawndata_ptr_t _userspawndata;
  decompxf_ptr_t _overridexf;
  datatable_ptr_t _table;
};

using sad_ptr_t = std::shared_ptr<SpawnAnonDynamic>;

struct ScriptWrapper {
  svar64_t _value;
};

namespace impl {

  struct _SimulationResponse;
  struct _SpawnNamedDynamic;
  struct _SpawnAnonDynamic;
  struct _FindSystem;
  struct _FindComponent;
  struct _SystemEvent;
  struct _SystemRequest;
  struct _SystemResponse;
  struct _ComponentEvent;
  struct _ComponentRequest;
  struct _ComponentResponse;

  using sys_response_ptr_t = std::shared_ptr<_SystemResponse>;
  using comp_response_ptr_t = std::shared_ptr<_ComponentResponse>;
  using sim_response_ptr_t = std::shared_ptr<_SimulationResponse>;

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

using stringpoolctx_ptr_t = std::shared_ptr<ork::StringPoolContext>;
using StringPoolStack = ork::util::GlobalStack<stringpoolctx_ptr_t>;
