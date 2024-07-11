////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "types.h"
#include <ork/math/cmatrix4.h>
#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/lev2/lev2_types.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

struct SystemDataClass : public object::ObjectClass {
  DeclareExplicitX(SystemDataClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
  SystemDataClass(const rtti::RTTIData&);

  PoolString GetFamily() const {
    return mFamily;
  }
  void SetFamily(PoolString family) {
    mFamily = family;
  }

private:
  PoolString mFamily;
};

///////////////////////////////////////////////////////////////////////////////

struct SystemFragmentDataClass : public object::ObjectClass {
  DeclareExplicitX(SystemFragmentDataClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
  SystemFragmentDataClass(const rtti::RTTIData&);
};

///////////////////////////////////////////////////////////////////////////////

struct SystemFragmentData : public Object {
  DeclareExplicitX(SystemFragmentData, Object, rtti::AbstractPolicy, SystemFragmentDataClass);

public:
  SystemFragmentData();

  virtual SystemFragment* createFragment(System* s) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct SystemData : public Object {
  DeclareExplicitX(SystemData, Object, rtti::AbstractPolicy, SystemDataClass);

public:
  virtual System* createSystem(ork::ecs::Simulation* psi) const = 0;
  PoolString GetFamily() const;

protected:
  SystemData() {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct SystemFragment {
protected:
  SystemFragment(const SystemFragmentData* data, System* system);
};

///////////////////////////////////////////////////////////////////////////////

struct ECSTOK { 
  constexpr ECSTOK(const char *str, size_t len) 
    : _str(str)
    , _len(len)
    , _token(_str)
    , _hashed(_token._hashed) {}
  const char* _str;
  const size_t _len;
  const token_t _token;
  const uint64_t _hashed;
};

constexpr ECSTOK operator ""_ecstok(const char* s, size_t len) { 
    return ECSTOK(s,len);
}

using system_update_lambda_t = std::function<void(simulation_ptr_t)>;

struct System : public ork::Object {

  DeclareAbstractX(System,ork::Object);

public:
  virtual systemkey_t systemTypeDynamic() = 0;

  Simulation* simulation() {
    return _simulation;
  }

  inline const SystemData* sysdata() const { return _systemData; }

  void _notify(token_t evID, evdata_t data);

protected:
  friend Controller;
  friend Simulation;

  System(const SystemData* scd, Simulation* pinst);
  virtual ~System();

  void _beginRender() { _onBeginRender(); }
  void _endRender() { _onEndRender(); }

  bool _initialize(Simulation* psi);
  void _uninitialize(Simulation* psi);
  bool _link(Simulation* psi);
  void _unlink(Simulation* psi);
  bool _stage(Simulation* psi);
  void _unstage(Simulation* psi);
  bool _activate(Simulation* psi);
  void _deactivate(Simulation* psi);

  void _update(Simulation* inst);
  
  void _render(Simulation* psi, ui::drawevent_constptr_t drwev);
  void _renderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe);
  void _request(impl::sys_response_ptr_t response, token_t evID, evdata_t data);

  virtual void _onGpuInit(Simulation* psi, lev2::Context* ctx);
  virtual void _onGpuLink(Simulation* psi, lev2::Context* ctx);
  virtual void _onGpuExit(Simulation* psi, lev2::Context* ctx);
  virtual void _onUpdate(Simulation* inst);

  virtual bool _onInitialize(Simulation* psi);
  virtual void _onUninitialize(Simulation* psi);
  virtual bool _onLink(Simulation* psi);
  virtual void _onUnLink(Simulation* psi);
  virtual bool _onStage(Simulation* psi);
  virtual void _onUnstage(Simulation* psi);
  virtual bool _onActivate(Simulation* psi);
  virtual void _onDeactivate(Simulation* psi);

  virtual void _onBeginRender() {}
  virtual void _onEndRender() {}
  virtual void _onRender(Simulation* psi, ui::drawevent_constptr_t drwev);
  virtual void _onRenderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe);
  virtual void _onNotify(token_t evID, evdata_t data);
  virtual void _onRequest(impl::sys_response_ptr_t response, token_t evID, evdata_t data);

  const SystemData* _systemData = nullptr;
  Simulation* _simulation       = nullptr;
  bool _started                 = false;
};

using pysystem_ptr_t = ork::python::unmanaged_ptr<System>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////

#define DeclareToken(x) static constexpr token_t x = #x##_tok;
#define ImplementToken(x) tokenize(#x);