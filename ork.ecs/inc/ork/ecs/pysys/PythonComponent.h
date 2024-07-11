////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include "../component.h"
#include "../system.h"
#include <Python.h>
#include <ork/python/pycodec.h>
#include <ork/python/context.h>
#include <ork/util/fast_set.inl>

#include <ork/ecs/SceneGraphComponent.h>

namespace ork { namespace ecs {

///////////////////////////////////////////////////////////////////////////////

struct PythonComponentData : public ecs::ComponentData {
  DeclareConcreteX(PythonComponentData, ecs::ComponentData);

public:
  PythonComponentData();

  const file::Path& GetPath() const {
    return mScriptPath;
  }
  void SetPath(const file::Path& pth) {
    mScriptPath = pth;
  }

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  file::Path mScriptPath;
  lev2::scenegraph::node_instance_data_ptr_t _INSTANCEDATA;
};

///////////////////////////////////////////////////////////////////////////////

struct PythonSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(PythonSystemData, ork::ecs::SystemData);

public:
  ///////////////////////////////////////////////////////
  PythonSystemData();
  ///////////////////////////////////////////////////////

  file::Path _sceneScriptPath;
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};

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

  any<64> mPythonData;
  lev2::instanceddrawinstancedata_ptr_t _idata;
  SceneGraphComponent* _mySGcomponentForInstancing = nullptr;
  int _sginstance_id = -1;

};

///////////////////////////////////////////////////////////////////////////////

struct PythonSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "PythonSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* pinst);

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

  template <typename... A> void __pcallargs(obind::object fn_object,A&&... args){
    try {

        auto process_args = [&](auto&&... processed_args) {
            fn_object(std::forward<decltype(processed_args)>(processed_args)...);
        };

        // Process each argument and expand them
        process_args(process_arg(std::forward<A>(args))...);

    } catch (const std::exception& e) {
      printf("Error executing Python script: %s\n", e.what());
      OrkAssert(false);
    }
  }
  using pyctx_t = ::ork::python::Context2;
  std::shared_ptr<pyctx_t> _pythonContext;
  std::string mScriptText;
  fast_set<PythonComponent*> _activeComponents;
  system_update_lambda_t _onSystemUpdate;
  obind::object _systemScript;
  obind::object _pymethodOnSystemUpdate;
  obind::object _pymethodOnSystemInit;
  obind::object _pymethodOnSystemLink;
  obind::object _pymethodOnSystemActivate;
  obind::object _pymethodOnSystemStage;
  obind::object _pymethodOnSystemNotify;

  obind::object _pymethodOnComponentActivate;
  obind::object _pymethodOnComponentDeactivate;

  //int mScriptRef;
};


using pypythonsystem_ptr_t = ::ork::python::unmanaged_ptr<PythonSystem>;

///////////////////////////////////////////////////////////////////////////////
struct ComponentArray {
  inline ComponentArray(std::vector<PythonComponent*>& components)
    : _components(components) {
  }
  inline pycomponent_ptr_t get(int idx) const {
    return _components[idx];
  }
  inline size_t size() const {
    return _components.size();
  }
  std::vector<PythonComponent*>& _components;
};
using pycomponentarray_ptr_t = std::shared_ptr<ComponentArray>;

}} // namespace ork::ecs
