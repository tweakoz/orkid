////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// ExprForceModule (E2.5 S8) — the FIRST particle-force ExprIR consumer. Its per-particle
// acceleration is authored as a canonical ExprIR TREE (context "particles.force"; see
// ork.hypergraph.dflow.particles.exprir_particles). Three scalar exprs (force x/y/z) are
// stored as the reflected JSON strings the DSL emits (exprir.encode_json — the same storage
// form as terrain ExprModuleData._expr_tree); this module parses them ONCE and evaluates the
// tree per particle on the CPU (the modular-forces pattern — walk the pool, mutate mVelocity),
// so a sim advance is fully DETERMINISTIC. Symbols the context exposes evaluate honestly from
// the live particle: unit_age / age / random / pos.{x,y,z} / vel.{x,y,z} / speed. A `Strength`
// float plug scales the whole force (A8: the tweakable multiplier is a plug, never baked).

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <rapidjson/document.h>
#include <cmath>

using namespace ork::dataflow;

namespace ork::lev2::particle {

namespace {

// ---- ptc symbol vocabulary (particles.force context leaves; ParamRef kind "ptc") ----
enum ESym {
  SYM_UNIT_AGE = 0, SYM_AGE, SYM_RANDOM,
  SYM_POSX, SYM_POSY, SYM_POSZ, SYM_VELX, SYM_VELY, SYM_VELZ, SYM_SPEED,
  SYM_INVALID = -1
};
int _resolveSym(const std::string& n) {
  if (n == "unit_age") return SYM_UNIT_AGE;
  if (n == "age")      return SYM_AGE;
  if (n == "random")   return SYM_RANDOM;
  if (n == "pos_x")    return SYM_POSX;
  if (n == "pos_y")    return SYM_POSY;
  if (n == "pos_z")    return SYM_POSZ;
  if (n == "vel_x")    return SYM_VELX;
  if (n == "vel_y")    return SYM_VELY;
  if (n == "vel_z")    return SYM_VELZ;
  if (n == "speed")    return SYM_SPEED;
  return SYM_INVALID;
}

// ---- call op vocabulary ----
enum EOp {
  OP_ADD = 0, OP_SUB, OP_MUL, OP_DIV, OP_NEG,
  OP_SIN, OP_COS, OP_ABS, OP_SQRT, OP_FRACT,
  OP_MIN, OP_MAX, OP_STEP, OP_POW, OP_MOD,
  OP_CLAMP, OP_MIX, OP_SMOOTHSTEP,
  OP_INVALID = -1
};
int _resolveOp(const std::string& n) {
  if (n == "add") return OP_ADD;   if (n == "sub") return OP_SUB;
  if (n == "mul") return OP_MUL;   if (n == "div") return OP_DIV;
  if (n == "neg") return OP_NEG;   if (n == "sin") return OP_SIN;
  if (n == "cos") return OP_COS;   if (n == "abs") return OP_ABS;
  if (n == "sqrt") return OP_SQRT; if (n == "fract") return OP_FRACT;
  if (n == "min") return OP_MIN;   if (n == "max") return OP_MAX;
  if (n == "step") return OP_STEP; if (n == "pow") return OP_POW;
  if (n == "mod") return OP_MOD;   if (n == "clamp") return OP_CLAMP;
  if (n == "mix") return OP_MIX;   if (n == "smoothstep") return OP_SMOOTHSTEP;
  return OP_INVALID;
}

// compact parsed ExprIR node (the reflected JSON is lowered to this ONCE, then eval'd/particle).
struct ENode {
  int _kind = 0;               // 0=const, 1=ref, 2=call
  float _const = 0.0f;
  int _sym = SYM_INVALID;
  int _op  = OP_INVALID;
  std::vector<std::shared_ptr<ENode>> _args;
};
using enode_ptr_t = std::shared_ptr<ENode>;

struct EvalCtx { float sym[10]; };

float _eval(const ENode* n, const EvalCtx& c) {
  switch (n->_kind) {
    case 0: return n->_const;
    case 1: return (n->_sym >= 0 && n->_sym < 10) ? c.sym[n->_sym] : 0.0f;
    case 2: {
      auto A = [&](int i) { return _eval(n->_args[i].get(), c); };
      switch (n->_op) {
        case OP_ADD: return A(0) + A(1);
        case OP_SUB: return A(0) - A(1);
        case OP_MUL: return A(0) * A(1);
        case OP_DIV: { float d = A(1); return d != 0.0f ? A(0) / d : 0.0f; }
        case OP_NEG: return -A(0);
        case OP_SIN: return std::sin(A(0));
        case OP_COS: return std::cos(A(0));
        case OP_ABS: return std::fabs(A(0));
        case OP_SQRT: { float x = A(0); return x > 0.0f ? std::sqrt(x) : 0.0f; }
        case OP_FRACT: { float x = A(0); return x - std::floor(x); }
        case OP_MIN: return std::min(A(0), A(1));
        case OP_MAX: return std::max(A(0), A(1));
        case OP_STEP: return A(1) >= A(0) ? 1.0f : 0.0f; // step(edge, x)
        case OP_POW: return std::pow(A(0), A(1));
        case OP_MOD: { float b = A(1); return b != 0.0f ? A(0) - b * std::floor(A(0) / b) : 0.0f; }
        case OP_CLAMP: { float x = A(0), lo = A(1), hi = A(2); return std::min(std::max(x, lo), hi); }
        case OP_MIX: { float a = A(0), b = A(1), t = A(2); return a + (b - a) * t; }
        case OP_SMOOTHSTEP: {
          float e0 = A(0), e1 = A(1), x = A(2);
          float t = (e1 != e0) ? (x - e0) / (e1 - e0) : 0.0f;
          t = std::min(std::max(t, 0.0f), 1.0f);
          return t * t * (3.0f - 2.0f * t);
        }
      }
      OrkAssert(false); // OP_INVALID slipped past the parser guard
      return 0.0f;
    }
  }
  return 0.0f;
}

// lower one reflected exprir JSON value into an ENode tree. Loud on any out-of-vocabulary
// construct (ops self-defend — a malformed force expr FAILS at author/link, never silently 0).
enode_ptr_t _lower(const rapidjson::Value& v) {
  auto n = std::make_shared<ENode>();
  OrkAssert(v.IsObject() && v.HasMember("k"));
  std::string k = v["k"].GetString();
  if (k == "const") {
    n->_kind = 0;
    n->_const = float(v["v"].GetDouble());
  } else if (k == "ref") {
    n->_kind = 1;
    std::string kind = v["kind"].GetString();
    std::string name = v["name"].IsString() ? v["name"].GetString() : std::string();
    n->_sym = _resolveSym(name);
    OrkAssert(n->_sym != SYM_INVALID); // unknown particles.force leaf
  } else if (k == "call") {
    n->_kind = 2;
    n->_op = _resolveOp(v["name"].GetString());
    OrkAssert(n->_op != OP_INVALID);   // unknown particles.force op
    for (auto& a : v["args"].GetArray())
      n->_args.push_back(_lower(a));
  } else {
    OrkAssert(false); // bad ExprIR node tag
  }
  return n;
}

enode_ptr_t _parseExpr(const std::string& json) {
  if (json.empty()) return nullptr; // an unset axis -> constant 0 force on that component
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  OrkAssert(!doc.HasParseError()); // a malformed reflected force tree is a loud failure
  return _lower(doc);
}

} // anonymous namespace

struct ExprForceModuleInst : public ParticleModuleInst {

  ExprForceModuleInst(const ExprForceModuleData* d, dataflow::GraphInst* g)
      : ParticleModuleInst(d, g)
      , _d(d) {}

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_strength = typedInputNamed<FloatXfPlugTraits>("Strength");
    // lower the reflected ExprIR trees ONCE (author-time constant during the sim).
    _nx = _parseExpr(_d->_force_x);
    _ny = _parseExpr(_d->_force_y);
    _nz = _parseExpr(_d->_force_z);
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    if (!_nx && !_ny && !_nz) return; // no expression authored -> no-op (not a silent black force)
    float dt   = updata->_dt;
    float str  = _input_strength->value();
    const int n = _pool->GetNumAlive();
    for (int i = 0; i < n; i++) {
      BasicParticle* p = _pool->GetActiveParticle(i);
      EvalCtx c;
      c.sym[SYM_UNIT_AGE] = p->_unit_age;
      c.sym[SYM_AGE]      = p->mfAge;
      c.sym[SYM_RANDOM]   = p->mfRandom;
      c.sym[SYM_POSX]     = p->mPosition.x;
      c.sym[SYM_POSY]     = p->mPosition.y;
      c.sym[SYM_POSZ]     = p->mPosition.z;
      c.sym[SYM_VELX]     = p->mVelocity.x;
      c.sym[SYM_VELY]     = p->mVelocity.y;
      c.sym[SYM_VELZ]     = p->mVelocity.z;
      c.sym[SYM_SPEED]    = p->mVelocity.magnitude();
      fvec3 force(_nx ? _eval(_nx.get(), c) : 0.0f,
                  _ny ? _eval(_ny.get(), c) : 0.0f,
                  _nz ? _eval(_nz.get(), c) : 0.0f);
      p->mVelocity += force * (str * dt);
    }
  }

  const ExprForceModuleData* _d;
  floatxf_inp_pluginst_ptr_t _input_strength;
  enode_ptr_t _nx, _ny, _nz;
};

//////////////////////////////////////////////////////////////////////////

ExprForceModuleData::ExprForceModuleData() {}

static void _reshapeExprForceIOs(dataflow::moduledata_ptr_t data) {
  auto str = ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Strength");
  str->_range = {-1000.0f, 1000.0f};
  str->setValue(1.0f);
}

std::shared_ptr<ExprForceModuleData> ExprForceModuleData::createShared() {
  auto data = std::make_shared<ExprForceModuleData>();
  _initPoolIOs(data);
  _reshapeExprForceIOs(data);
  return data;
}

rtti::castable_ptr_t ExprForceModuleData::sharedFactory() {
  return ExprForceModuleData::createShared();
}

dgmoduleinst_ptr_t ExprForceModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<ExprForceModuleInst>(this, ginst);
}

void ExprForceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return ExprForceModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapeExprForceIOs(mdata);
  });
  // the canonical particles.force ExprIR trees (exprir.encode_json) per axis — the reflected
  // STORAGE form the DSL emits and this module parses + evaluates per particle.
  clazz->directProperty("force_x", &ExprForceModuleData::_force_x);
  clazz->directProperty("force_y", &ExprForceModuleData::_force_y);
  clazz->directProperty("force_z", &ExprForceModuleData::_force_z);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::ExprForceModuleData, "psys::ExprForceModuleData");
