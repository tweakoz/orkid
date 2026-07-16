////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// SubGraphModule / LoopModule — family-neutral schema + reflection + the GENERIC
// composite runtime (SubGraphModuleInst / LoopModuleInst compute()). The GPU /
// memory-model work is delegated to a family CookGraphDriver (cook_driver.h) the
// family stocks on the host GraphInst's _impl. See the header and JUL13_DFLOW.md §3 E4.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedVector.hpp> // std::vector<float> iter-feed table

#include <ork/dataflow/all.h>
#include <ork/dataflow/subgraph_module.h>
#include <ork/dataflow/cook_driver.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::dataflow::SubGraphPromotion, "dflow::SubGraphPromotion");
ImplementReflectionX(ork::dataflow::LoopCarry, "dflow::LoopCarry");
ImplementReflectionX(ork::dataflow::LoopIterFeed, "dflow::LoopIterFeed");
ImplementReflectionX(ork::dataflow::SubGraphModuleData, "dflow::SubGraphModuleData");
ImplementReflectionX(ork::dataflow::LoopModuleData, "dflow::LoopModuleData");
///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

void SubGraphPromotion::describeX(class_t* clazz) {
  clazz->directProperty("outer", &SubGraphPromotion::_outer);
  clazz->directProperty("inner_module", &SubGraphPromotion::_inner_module);
  clazz->directProperty("inner_plug", &SubGraphPromotion::_inner_plug);
}

void LoopCarry::describeX(class_t* clazz) {
  clazz->directProperty("name", &LoopCarry::_name);
  clazz->directProperty("promoted_input", &LoopCarry::_promoted_input);
  clazz->directProperty("promoted_output", &LoopCarry::_promoted_output);
}

void LoopIterFeed::describeX(class_t* clazz) {
  clazz->directProperty("inner_module", &LoopIterFeed::_inner_module);
  clazz->directProperty("inner_plug", &LoopIterFeed::_inner_plug);
  clazz->directProperty("scale", &LoopIterFeed::_scale);
  clazz->directProperty("bias", &LoopIterFeed::_bias);
  clazz->directVectorProperty("values", &LoopIterFeed::_values);
}

///////////////////////////////////////////////////////////////////////////////
// SubGraphModuleData
///////////////////////////////////////////////////////////////////////////////

void SubGraphModuleData::describeX(class_t* clazz) {
  // the nested body graph — the composite's payload. Reflected as a child object so
  // GraphData-in-module-in-GraphData round-trips (mind the dflow serialization
  // gotchas: every embedded class MUST be touched in a ClassToucher).
  clazz->directObjectProperty("subgraph", &SubGraphModuleData::_subgraph);
  // boundary promotion tables (which inner plug each promoted boundary plug maps to).
  clazz->directObjectVectorProperty("promoted_inputs", &SubGraphModuleData::_promoted_inputs);
  clazz->directObjectVectorProperty("promoted_outputs", &SubGraphModuleData::_promoted_outputs);
}

SubGraphModuleData::SubGraphModuleData() {
}

std::shared_ptr<SubGraphModuleData> SubGraphModuleData::createShared() {
  auto d       = std::make_shared<SubGraphModuleData>();
  d->_subgraph = std::make_shared<GraphData>();
  return d;
}

dgmoduleinst_ptr_t SubGraphModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<SubGraphModuleInst>(this, ginst);
}

graphdata_ptr_t SubGraphModuleData::childGraph() const {
  return _subgraph;
}

subgraphpromotion_ptr_t SubGraphModuleData::promotedInput(const std::string& outer) const {
  for (auto p : _promoted_inputs)
    if (p and p->_outer == outer)
      return p;
  return nullptr;
}
subgraphpromotion_ptr_t SubGraphModuleData::promotedOutput(const std::string& outer) const {
  for (auto p : _promoted_outputs)
    if (p and p->_outer == outer)
      return p;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// LoopModuleData
///////////////////////////////////////////////////////////////////////////////

void LoopModuleData::describeX(class_t* clazz) {
  // structural iteration count — the raising-N-keeps-first-N-Merkle-hashes knob.
  clazz->directProperty("count", &LoopModuleData::_count);
  clazz->directObjectVectorProperty("carries", &LoopModuleData::_carries);
  clazz->directObjectVectorProperty("iter_feeds", &LoopModuleData::_iter_feeds);
}

LoopModuleData::LoopModuleData() {
}

std::shared_ptr<LoopModuleData> LoopModuleData::createShared() {
  auto d       = std::make_shared<LoopModuleData>();
  d->_subgraph = std::make_shared<GraphData>();
  return d;
}

dgmoduleinst_ptr_t LoopModuleData::createInstance(GraphInst* ginst) const {
  return std::make_shared<LoopModuleInst>(this, ginst);
}

loopcarry_ptr_t LoopModuleData::carryByInput(const std::string& outer) const {
  for (auto c : _carries)
    if (c and c->_promoted_input == outer)
      return c;
  return nullptr;
}
bool LoopModuleData::isCarryOutput(const std::string& outer) const {
  for (auto c : _carries)
    if (c and c->_promoted_output == outer)
      return true;
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// generic composite runtime — the family-neutral orchestration. GPU work delegates
// to the CookGraphDriver the family stocks on the host GraphInst's _impl.
///////////////////////////////////////////////////////////////////////////////

namespace {

// shared salt-base computation for a composite node: H(cook-context, [input hashes]).
// EXCLUDES anything that must not perturb existing iterations (the loop's _count). The
// salt STRINGS below are FROZEN for cook-cache byte-identity — they were minted terrain-
// side and must not change (renaming would invalidate every existing cache entry).
uint64_t _compositeSaltBase(uint64_t ctx, const std::vector<uint64_t>& input_hashes) {
  auto h = DataBlock::createHasher();
  h->accumulateString("dflow.subgraph.salt.v1");
  h->accumulateItem<uint64_t>(ctx);
  for (auto x : input_hashes)
    h->accumulateItem<uint64_t>(x);
  h->finish();
  return h->result();
}

// resolve a nested inner INPUT pluginst by (module, plug) — null if the module is absent
// (a missing plug asserts in inputNamed, same as the pre-generic terrain path).
inpluginst_ptr_t _innerInput(graphinst_ptr_t g, const std::string& mod, const std::string& plug) {
  auto it = g->_module_inst_map.find(mod);
  if (it == g->_module_inst_map.end())
    return nullptr;
  return it->second->inputNamed(plug);
}

// forced roots = every promoted-output inner inst (demanded + pinned by runNested so the
// composite can read the result out afterward).
std::vector<dgmoduleinst_ptr_t> _forcedRoots(graphinst_ptr_t nested, const SubGraphModuleData* sd) {
  std::vector<dgmoduleinst_ptr_t> roots;
  for (auto p : sd->_promoted_outputs) {
    auto it = nested->_module_inst_map.find(p->_inner_module);
    if (it != nested->_module_inst_map.end())
      roots.push_back(it->second);
  }
  return roots;
}

// resolve the stocked family cook driver; self-defend LOUDLY (a composite must run under a
// family that supplies the GPU cook driver — terrain: bakeHeightfield).
cookgraphdriver_ptr_t _resolveDriver(GraphInst* inst, const ModuleData* md) {
  auto driver = inst->_impl.getShared<CookGraphDriver>();
  if (not driver) {
    printf(
        "dflow SubGraphModule<%s>: no CookGraphDriver stocked on the host GraphInst — a "
        "composite (subgraph/loop) needs a family that supplies the GPU cook driver (terrain: "
        "bakeHeightfield).\n",
        md ? md->_name.c_str() : "?");
    OrkAssert(false);
  }
  return driver;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////
// SubGraphModuleInst — a plain subnet: forward every promoted input, run the nested
// body ONCE, publish every promoted output.
///////////////////////////////////////////////////////////////////////////////

SubGraphModuleInst::SubGraphModuleInst(const SubGraphModuleData* d, GraphInst* g)
    : DgModuleInst(d, g)
    , _sd(d) {
}

uint64_t SubGraphModuleInst::cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const {
  _saltBase = _compositeSaltBase(ctx, ih);
  auto h    = DataBlock::createHasher();
  h->accumulateString("terrain.subgraph.node.v1"); // FROZEN salt (byte-identity)
  h->accumulateItem<uint64_t>(_saltBase);
  h->finish();
  return h->result();
}

void SubGraphModuleInst::compute(GraphInst* inst, ui::updatedata_ptr_t updata) {
  auto driver = _resolveDriver(inst, _abstract_module_data);
  if (not driver)
    return;
  const bool host_cache = inst->_graphdata and inst->_graphdata->_cacheable;

  driver->closeHostPhase(); // close the host's phase; the nested cook opens its own per node
  auto nested = driver->makeNestedInst(_sd->_subgraph);

  // forward external inputs by pointer (no copy): the inner promoted-input plug reads from
  // whatever the composite's boundary-input plug is fed from outside.
  for (auto p : _sd->_promoted_inputs) {
    auto inner = _innerInput(nested, p->_inner_module, p->_inner_plug);
    auto b     = inputNamed(p->_outer);
    if (inner and b)
      inner->_connectedOutput = b->_connectedOutput;
  }

  auto roots = _forcedRoots(nested, _sd);
  driver->runNested(nested, updata, _saltBase, roots, host_cache);
  driver->publishSubgraphOutputs(this, nested, _sd);
  driver->reopenHostPhase(); // reopen for the host cook loop's balancing endDispatchPhase
}

///////////////////////////////////////////////////////////////////////////////
// LoopModuleInst — run the nested body _count times, feeding carries forward with a
// per-iteration DISTINCT nested cook-context salt. Multi-IO + multi-carry.
///////////////////////////////////////////////////////////////////////////////

LoopModuleInst::LoopModuleInst(const LoopModuleData* d, GraphInst* g)
    : SubGraphModuleInst(d, g)
    , _ld(d) {
}

uint64_t LoopModuleInst::cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const {
  // salt base EXCLUDES _count (oracle: raising N->N+k keeps the first N iterations' salts).
  _saltBase = _compositeSaltBase(ctx, ih);
  // the loop NODE's own host-chain identity INCLUDES _count (its output depends on it).
  auto h = DataBlock::createHasher();
  h->accumulateString("terrain.loop.node.v1"); // FROZEN salt (byte-identity)
  h->accumulateItem<uint64_t>(_saltBase);
  h->accumulateItem<int>(_ld->_count);
  h->finish();
  return h->result();
}

uint64_t LoopModuleInst::_saltForIter(int i) const {
  auto h = DataBlock::createHasher();
  h->accumulateItem<uint64_t>(_saltBase);
  h->accumulateItem<int>(i);
  h->finish();
  return h->result();
}

void LoopModuleInst::compute(GraphInst* inst, ui::updatedata_ptr_t updata) {
  auto driver = _resolveDriver(inst, _abstract_module_data);
  if (not driver)
    return;
  const bool host_cache = inst->_graphdata and inst->_graphdata->_cacheable;
  int K                 = _ld->_count;
  if (K < 0)
    K = 0;

  driver->closeHostPhase(); // close host phase; explicit phase mgmt below (nested cook self-phases)

  // each carry's boundary OUTPUT plug doubles as the persistent (GPU) carry buffer; the
  // driver allocates + initializes it from the carry's promoted-INPUT external value.
  driver->initCarries(this, _ld);
  if (K == 0) {               // pass-through: the boundary outputs already hold the carry initials
    driver->reopenHostPhase();
    return;
  }

  auto nested = driver->makeNestedInst(_ld->_subgraph);
  auto roots  = _forcedRoots(nested, _ld); // every promoted-output inner inst (demanded + pinned)
  for (int i = 0; i < K; i++) {
    // wire promoted inputs (carry buffer for carried inputs; external for constants)
    for (auto p : _ld->_promoted_inputs) {
      auto inner = _innerInput(nested, p->_inner_module, p->_inner_plug);
      if (not inner)
        continue;
      if (auto c = _ld->carryByInput(p->_outer))
        inner->_connectedOutput = outputNamed(c->_promoted_output);
      else if (auto b = inputNamed(p->_outer))
        inner->_connectedOutput = b->_connectedOutput;
    }
    // per-iteration index feed (L.i): a TABLE value (_values[i]) WINS over affine
    // (i*scale + bias). The table carries an arbitrary (e.g. quadratic) L.i expression
    // evaluated per iteration by the doc layer — bit-exact with the old unroll.
    for (auto f : _ld->_iter_feeds) {
      auto mit = nested->_module_inst_map.find(f->_inner_module);
      if (mit == nested->_module_inst_map.end())
        continue;
      auto fp = mit->second->typedInputNamed<FloatPlugTraits>(f->_inner_plug);
      if (not fp)
        continue;
      if (not f->_values.empty()) {
        int idx = i;
        if (idx >= int(f->_values.size())) { // count outran the table — clamp LOUDLY
          printf(
              "dflow LoopIterFeed<%s.%s>: iteration %d exceeds value-table size %zu — "
              "clamping to last entry (re-elaborate to regenerate the table for count=%d)\n",
              f->_inner_module.c_str(), f->_inner_plug.c_str(), i, f->_values.size(), K);
          idx = int(f->_values.size()) - 1;
        }
        fp->setValue(f->_values[idx]);
      } else {
        fp->setValue(float(i) * f->_scale + f->_bias);
      }
    }
    driver->runNested(nested, updata, _saltForIter(i), roots, host_cache);
    driver->publishLoopOutputs(this, nested, _ld, i, K);
  }
  driver->reopenHostPhase(); // reopen host phase for the host cook loop's endDispatchPhase
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
