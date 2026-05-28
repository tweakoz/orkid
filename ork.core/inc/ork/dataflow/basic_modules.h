////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// basic_modules — generic, type-agnostic dataflow modules. Live at the
// ork.core/dataflow layer (not in any consumer-specific namespace like
// particle/audio/shader) so any dflow graph can use them.
//
// The HyperSyn DSL lowerer transparently emits these from Expr trees, but
// nothing about the modules is DSL-specific — direct authoring (graphdata.
// create(name, dflow.MinModule)) works identically.

#pragma once

// module.h declares DgModuleData but relies on its consumers to pre-include
// the plug-data types it references (inplugdata_ptr_t, ...). Match the
// canonical include chain used elsewhere.
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/plug_data.h>
#include <ork/dataflow/plug_inst.h>
#include <ork/dataflow/module.h>

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////
// MinModule / MaxModule — two scalar (FloatXf) inputs A, B → one scalar
// (float) output "value". Each input plug carries the full floatxf chain, so
// upstream scale/bias/sin/... stages still apply per side.
struct MinModuleData : public DgModuleData {
  DeclareConcreteX(MinModuleData, DgModuleData);

public:
  static std::shared_ptr<MinModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const final;
  MinModuleData();
};

using minmodule_ptr_t = std::shared_ptr<MinModuleData>;

struct MaxModuleData : public DgModuleData {
  DeclareConcreteX(MaxModuleData, DgModuleData);

public:
  static std::shared_ptr<MaxModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const final;
  MaxModuleData();
};

using maxmodule_ptr_t = std::shared_ptr<MaxModuleData>;

///////////////////////////////////////////////////////////////////////////////
// LerpModule — three scalar (FloatXf) inputs A, B, T → one scalar (float)
// output "value" = A * (1 - T) + B * T. No clamping on T (matches GLSL mix
// semantics for T outside [0,1]).
struct LerpModuleData : public DgModuleData {
  DeclareConcreteX(LerpModuleData, DgModuleData);

public:
  static std::shared_ptr<LerpModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const final;
  LerpModuleData();
};

using lerpmodule_ptr_t = std::shared_ptr<LerpModuleData>;

///////////////////////////////////////////////////////////////////////////////
// PowModule — two scalar (FloatXf) inputs X, K → one scalar (float) output
// "value" = pow(X, K). The HyperSyn DSL lowerer only emits this when the
// exponent is itself a variable expression; pow(x, const) collapses into the
// existing floatxfpowdata chain stage on the consumer plug.
struct PowModuleData : public DgModuleData {
  DeclareConcreteX(PowModuleData, DgModuleData);

public:
  static std::shared_ptr<PowModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const final;
  PowModuleData();
};

using powmodule_ptr_t = std::shared_ptr<PowModuleData>;

///////////////////////////////////////////////////////////////////////////////
// Vec4CombineModule — four scalar (FloatXf) inputs X/Y/Z/W → one fvec4
// output "value". The HyperSyn DSL lowerer emits this for Vec4Expr bindings
// (e.g. emitter.Aux.x = ... emitter.Aux.y = ...). Each input plug carries
// the full floatxf chain so per-axis scale/bias/sine/... compose normally.
struct Vec4CombineModuleData : public DgModuleData {
  DeclareConcreteX(Vec4CombineModuleData, DgModuleData);

public:
  static std::shared_ptr<Vec4CombineModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const final;
  Vec4CombineModuleData();
};

using vec4combinemodule_ptr_t = std::shared_ptr<Vec4CombineModuleData>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
