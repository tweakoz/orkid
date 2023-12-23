////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "konoff.h"
#include <ork/math/cvector4.h>

namespace ork::audio::singularity {

struct LfoData;
struct FunData;

///////////////////////////////////////////////////////////////////////////////

struct ControlBlockData {
  template <typename T> std::shared_ptr<T> addController() {
    OrkAssert((_numcontrollers + 1) <= kmaxctrlperblock);
    auto c                    = std::make_shared<T>();
    _cdata[_numcontrollers++] = c;
    return c;
  }
  controllerdata_constptr_t _cdata[kmaxctrlperblock];
  size_t _numcontrollers = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct ControllerData : public ork::Object {

  DeclareAbstractX(ControllerData, ork::Object);

  virtual ControllerInst* instantiate(layer_ptr_t layer) const = 0;

  scopesource_ptr_t createScopeSource();
  std::string _name;
  scopesource_ptr_t _scopesource;
};

///////////////////////////////////////////////////////////////////////////////

struct ControllerInst {
  ControllerInst(layer_ptr_t layer);
  virtual ~ControllerInst() {
  }

  virtual void keyOn(const KeyOnInfo& KOI) = 0;
  virtual void keyOff()                    = 0;
  virtual void compute()                   = 0;
  void setFloatValue(float v);
  void setVec4Value(fvec4 v);
  float getFloatValue() const;
  fvec4 getVec4Value() const;
  fvec4 _value;
  layer_ptr_t _layer = nullptr;
  std::string _name;
};

struct ControlBlockInst {
  void compute();
  void keyOn(const KeyOnInfo& KOI, controlblockdata_constptr_t CBD);
  void keyOff();

  ControllerInst* _cinst[kmaxctrlperblock] = {0, 0, 0, 0};
};

using ctrlblockinst_ptr_t = std::shared_ptr<ControlBlockInst>;

///////////////////////////////////////////////////////////////////////////////

struct LfoData : public ControllerData {

  DeclareConcreteX(LfoData, ControllerData);

  LfoData();
  ControllerInst* instantiate(layer_ptr_t layer) const final;

  float _initialPhase;
  float _minRate;
  float _maxRate;
  std::string _controller;
  std::string _shape;
};

///////////////////////////////////////////////////////////////////////////////

struct LfoInst : public ControllerInst {
  LfoInst(const LfoData* data, layer_ptr_t layer);

  void reset();
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  void compute() final;
  ////////////////////////////
  const LfoData* _data;
  float _phaseInc;
  float _phase;
  float _currate;
  bool _enabled;
  float _rateLerp;
  float _bias;
  mapper_t _mapper;
};

///////////////////////////////////////////////////////////////////////////////

struct FunData : public ControllerData {

  DeclareConcreteX(FunData, ControllerData);

  ControllerInst* instantiate(layer_ptr_t layer) const final;

  std::string _a, _b, _op;
};

///////////////////////////////////////////////////////////////////////////////

struct FunInst : public ControllerInst {
  FunInst(const FunData* data, layer_ptr_t layer);
  void compute() final;
  void keyOn(const KeyOnInfo& KOI) final;
  void keyOff() final;
  ////////////////////////////
  const FunData* _data;
  controller_t _a;
  controller_t _b;
  controller_t _op;
};

///////////////////////////////////////////////////////////////////////////////
struct CustomControllerInst;

using customcontroller_computemethod_t = std::function<void(CustomControllerInst* cci)>;
using customcontroller_keyonmethod_t   = std::function<void(CustomControllerInst* cci, const KeyOnInfo& KOI)>;
using customcontroller_keyoffmethod_t  = std::function<void(CustomControllerInst* cci)>;

struct CustomControllerData final : public ControllerData {

  DeclareConcreteX(CustomControllerData, ControllerData);

  CustomControllerData();
  ControllerInst* instantiate(layer_ptr_t layer) const override;
  customcontroller_computemethod_t _oncompute;
  customcontroller_keyonmethod_t _onkeyon;
  customcontroller_keyoffmethod_t _onkeyoff;
};
struct CustomControllerInst final : public ControllerInst {
  CustomControllerInst(const CustomControllerData* data, layer_ptr_t layer);
  void compute() override;
  void keyOn(const KeyOnInfo& KOI) override;
  void keyOff() override;
  const CustomControllerData* _data = nullptr;
  customcontroller_computemethod_t _oncompute;
  customcontroller_keyonmethod_t _onkeyon;
  customcontroller_keyoffmethod_t _onkeyoff;
};

///////////////////////////////////////////////////////////////////////////////

struct ConstantControllerData : public ControllerData {

  DeclareConcreteX(ConstantControllerData, ControllerData);

  ControllerInst* instantiate(layer_ptr_t layer) const final;

  float _constvalue = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct ConstantInst final : public ControllerInst {
  ConstantInst(const ConstantControllerData* data, layer_ptr_t layer);
  void compute() override;
  void keyOn(const KeyOnInfo& KOI) override;
  void keyOff() override;
};

} // namespace ork::audio::singularity
