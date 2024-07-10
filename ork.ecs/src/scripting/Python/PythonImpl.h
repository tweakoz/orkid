////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include <ork/ecs/component.h>
#include <ork/ecs/system.h>
#include <ork/ecs/pysys/PythonComponent.h>
#include <ork/util/logger.h>

#include <Python.h>
#include <ork/python/pycodec.h>
#include <ork/python/context.h>

namespace ork::ecs::pysys {

typedef ork::FixedString<256> script_funcname_t;
struct PythonContext;
struct ScriptObject;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecssim {
  extern ::ork::python::pb11_typecodec_ptr_t simonly_codec_instance();
}
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

namespace pysys {

struct EcsGlobalState{
  EcsGlobalState();
  PyThreadState* _mainInterpreter = nullptr;
  PyThreadState* _globalInterpreter = nullptr;
};

using GSTATE = ::ork::python::GlobalState;
using gstate_ptr_t = std::shared_ptr<GSTATE>;

struct PythonContext;
using pythoncontext_ptr_t = std::shared_ptr<PythonContext>;

}

struct PythonComponent : public ecs::Component {
  DeclareAbstractX(PythonComponent, ecs::Component);

public:
  PythonComponent(const PythonComponentData& cd, ork::ecs::Entity* pent);
  const PythonComponentData& GetCD() const {
    return mCD;
  }

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data ) final;

  const PythonComponentData& mCD;
  pysys::ScriptObject* mScriptObject;

  any<64> mPythonData;
  //LuaIntf::LuaRef _luaentity; // its a table
};

///////////////////////////////////////////////////////////////////////////////

struct PythonSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "PythonSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* pinst);

  pysys::ScriptObject* FlyweightScriptObject(const ork::file::Path& key);

  friend struct PythonComponent;
  
  ~PythonSystem();

  void _onActivateComponent(PythonComponent* component);
  void _onDeactivateComponent(PythonComponent* component);

  bool _onLink(Simulation* psi) final;
  void _onUnLink(Simulation* psi) final;
  void _onUpdate(Simulation* inst) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* inst) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* inst) final;
  void _onNotify(token_t evID, evdata_t data) final;

  template <typename Arg>
  auto process_arg(Arg&& arg) {
      //auto type_codec = ecssim::simonly_codec_instance();
        auto type_codec = ork::python::obind_typecodec_t::instance();

      return type_codec->encode(std::forward<Arg>(arg));
  }

  template <typename... A> void __pcallargs(logchannel_ptr_t lchan, obind::object fn_object,A&&... args){
    try {

        auto process_args = [&](auto&&... processed_args) {
            fn_object(std::forward<decltype(processed_args)>(processed_args)...);
        };

        // Process each argument and expand them
        process_args(process_arg(std::forward<A>(args))...);

      //fn_object( args );
    /*} catch (obind::error_already_set& e) {
      lchan->log("Error executing Python script:\n");
      e.restore();
      PyErr_Print();
      OrkAssert(false);*/
    } catch (const std::exception& e) {
      lchan->log("Error executing Python script: %s\n", e.what());
      OrkAssert(false);
    }
  }
  //using pyctx_t = pysys::PythonContext;
  using pyctx_t = ::ork::python::Context2;
  std::shared_ptr<pyctx_t> _pythonContext;
  std::string mScriptText;
  std::map<ork::file::Path, pysys::ScriptObject*> mScriptObjects;
  std::unordered_set<PythonComponent*> _activeComponents;
  system_update_lambda_t _onSystemUpdate;
  obind::object _systemScript;
  obind::object _pymethodOnSystemUpdate;
  obind::object _pymethodOnSystemInit;
  obind::object _pymethodOnSystemLink;
  obind::object _pymethodOnSystemActivate;
  obind::object _pymethodOnSystemStage;
  obind::object _pymethodOnSystemNotify;

  //int mScriptRef;
};

} // namespace ork::ecs {

namespace ork::ecs::pysys {

struct ScriptObject {
  ScriptObject();
  ~ScriptObject();

  std::string mScriptPath;
  std::string mScriptText;
  std::string mMD5Digest;
  //int mOnEntInitialize       = LUA_NOREF;
  //int mOnEntUninitialize       = LUA_NOREF;
  //int mOnEntLink       = LUA_NOREF;
  //int mOnEntUnlink       = LUA_NOREF;
  //int mOnEntStage      = LUA_NOREF;
  //int mOnEntUnstage    = LUA_NOREF;
  //int mOnEntActivate   = LUA_NOREF;
  //int mOnEntDeactivate = LUA_NOREF;
  //int mOnEntUpdate     = LUA_NOREF;
  //int mOnNotify        = LUA_NOREF;
  //int mModTabRef       = LUA_NOREF;
  //int mScriptRef       = LUA_NOREF;
};

} // namespace ork::ecs::pysys {
