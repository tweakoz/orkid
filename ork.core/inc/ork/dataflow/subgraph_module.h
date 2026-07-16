////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// SubGraphModule / LoopModule — composite dgmodules that OWN a nested GraphData
// (the Houdini subnet model). A group / loop becomes a REAL DgModuleData subclass
// so bypass, output-marker, badges, serialization and (later) editor dive-into all
// fall out of the ONE module code path.
//
// This header carries the FAMILY-NEUTRAL schema only: the nested subgraph, the
// boundary PROMOTION tables (which inner plug each promoted boundary plug maps to),
// and (for loops) the count + CARRY declarations + the per-iteration index feed.
//
// The composite RUNTIME is GENERIC and lives here (SubGraphModuleInst /
// LoopModuleInst compute()): per-iteration salt, promoted-input forwarding, loop
// control, iter-feeds and forced-root demand are all family-neutral. The
// GPU/memory-model-specific work (the nested cook, the boundary-resource device
// blits, the phase choreography) is delegated to a CookGraphDriver (cook_driver.h)
// the family STOCKS on the host GraphInst's _impl. See hfdflow.cpp
// (terrain::TerrainCookDriver) for the reference driver, and JUL13_DFLOW.md §3 E4.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/module.h>
#include <vector>
#include <string>

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////
// One PROMOTED boundary plug: a plug on the composite module's boundary
// (named `_outer`) forwards to/from (`_inner_module`, `_inner_plug`) inside the
// nested subgraph. Used symmetrically for inputs (outer input -> inner input) and
// outputs (inner output -> outer output). A reflected Object so the table
// round-trips inside the embedded subgraph (GraphData-in-module-in-GraphData).
///////////////////////////////////////////////////////////////////////////////

struct SubGraphPromotion : public ork::Object {
  DeclareConcreteX(SubGraphPromotion, ork::Object);
  std::string _outer;        // boundary plug name on the composite module
  std::string _inner_module; // inner module name inside _subgraph
  std::string _inner_plug;   // inner plug name on that module
};
using subgraphpromotion_ptr_t = std::shared_ptr<SubGraphPromotion>;

///////////////////////////////////////////////////////////////////////////////
// One loop CARRY: an ordered (promoted-input <-> promoted-output) pair fed back
// each iteration. Iteration i reads the carry on `_promoted_input`; the value it
// leaves on `_promoted_output` becomes iteration i+1's `_promoted_input` value.
// The carry's INITIAL value is whatever the composite's `_promoted_input` boundary
// plug is fed from outside; its FINAL value is published on `_promoted_output`.
///////////////////////////////////////////////////////////////////////////////

struct LoopCarry : public ork::Object {
  DeclareConcreteX(LoopCarry, ork::Object);
  std::string _name;
  std::string _promoted_input;  // boundary INPUT plug the carry feeds each iter
  std::string _promoted_output; // boundary OUTPUT plug producing the next value
};
using loopcarry_ptr_t = std::shared_ptr<LoopCarry>;

///////////////////////////////////////////////////////////////////////////////
// The per-iteration index feed (the doc layer's L.i): each iteration writes a
// per-iteration value into an inner scalar plug. Two modes:
//   * AFFINE  (default): value = i*_scale + _bias — covers the common L.i usage
//     (L.i*0.02, 0.8 - L.i*.., 2.0 + L.i*0.5).
//   * TABLE   (_values non-empty WINS over affine): value = _values[i] — a
//     per-iteration VALUE TABLE the doc layer fills by evaluating an arbitrary
//     (e.g. quadratic) L.i expression once per iteration, EXACTLY as the unroll
//     would (bit-exact parity by construction). i is clamped LOUDLY to the last
//     entry if the loop count outruns the table.
// A8-clean either way: the per-iteration value is a PLUG value (never inlined into
// shader text). Default scale=1,bias=0,_values={} = raw index.
///////////////////////////////////////////////////////////////////////////////

struct LoopIterFeed : public ork::Object {
  DeclareConcreteX(LoopIterFeed, ork::Object);
  std::string _inner_module; // inner module the feed writes into
  std::string _inner_plug;   // inner FLOAT input plug on that module
  float _scale = 1.0f;
  float _bias  = 0.0f;
  std::vector<float> _values; // per-iteration value table; when non-empty WINS over affine
};
using loopiterfeed_ptr_t = std::shared_ptr<LoopIterFeed>;

///////////////////////////////////////////////////////////////////////////////
// SubGraphModuleData — a composite dgmodule owning a nested GraphData + the
// boundary promotion tables. CONCRETE + serializable; its inst (SubGraphModuleInst)
// carries the GENERIC composite runtime and resolves a CookGraphDriver (stocked by
// the family) for the GPU work — a family need NOT subclass the inst, only stock a
// driver. childGraph() exposes the nested graph (the editor dive-into / group
// affordance).
///////////////////////////////////////////////////////////////////////////////

struct SubGraphModuleData : public DgModuleData {
  DeclareConcreteX(SubGraphModuleData, DgModuleData);
  SubGraphModuleData();
  static std::shared_ptr<SubGraphModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const override;
  graphdata_ptr_t childGraph() const override; // returns _subgraph (isGroup() -> true)

  // resolve a promoted boundary plug to its (inner module, inner plug) — nullptr if absent.
  subgraphpromotion_ptr_t promotedInput(const std::string& outer) const;
  subgraphpromotion_ptr_t promotedOutput(const std::string& outer) const;

  graphdata_ptr_t _subgraph;
  std::vector<subgraphpromotion_ptr_t> _promoted_inputs;
  std::vector<subgraphpromotion_ptr_t> _promoted_outputs;
};
using subgraphmoduledata_ptr_t = std::shared_ptr<SubGraphModuleData>;

///////////////////////////////////////////////////////////////////////////////
// LoopModuleData — a SubGraphModule whose nested body runs `_count` times, feeding
// CARRIES forward and (optionally) a per-iteration index into inner scalar plugs.
///////////////////////////////////////////////////////////////////////////////

struct LoopModuleData : public SubGraphModuleData {
  DeclareConcreteX(LoopModuleData, SubGraphModuleData);
  LoopModuleData();
  static std::shared_ptr<LoopModuleData> createShared();
  dgmoduleinst_ptr_t createInstance(GraphInst* ginst) const override;

  int _count = 1; // structural iteration count (>=0; 0 = pass-through the carry initials)
  std::vector<loopcarry_ptr_t> _carries;
  std::vector<loopiterfeed_ptr_t> _iter_feeds;

  // carry lookups (family-neutral) shared by the loop runtime and the family driver's
  // publish/init hooks: the carry whose promoted-INPUT is `outer` (null if none), and
  // whether `outer` names a carry's promoted-OUTPUT.
  loopcarry_ptr_t carryByInput(const std::string& outer) const;
  bool isCarryOutput(const std::string& outer) const;
};
using loopmoduledata_ptr_t = std::shared_ptr<LoopModuleData>;

///////////////////////////////////////////////////////////////////////////////
// SubGraphModuleInst — the GENERIC composite runtime for a plain subnet: forward
// every promoted input, run the nested body ONCE via the resolved CookGraphDriver,
// publish every promoted output. compute() self-defends LOUDLY if no driver is
// stocked (a composite must run under a family that supplies the GPU cook driver).
///////////////////////////////////////////////////////////////////////////////

struct SubGraphModuleInst : public DgModuleInst {
  SubGraphModuleInst(const SubGraphModuleData* d, GraphInst* g);
  // node CONTENT hash — stashes the salt base H(cook-context, [external input hashes]),
  // making the nested nodes sensitive to the external input (a promoted input's inner
  // plug is external to the nested graph so its Merkle input hash would otherwise be 0).
  uint64_t cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const override;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) override;

  const SubGraphModuleData* _sd = nullptr;
  mutable uint64_t _saltBase    = 0; // stashed by cookComputeHash, consumed by compute()
};

///////////////////////////////////////////////////////////////////////////////
// LoopModuleInst — the GENERIC loop runtime: run the nested body `_count` times,
// feeding carries forward with a per-iteration DISTINCT nested cook-context salt,
// applying the per-iteration index feeds. Multi-IO + multi-carry. The salt base
// EXCLUDES _count (raising N->N+k keeps the first N iterations' salts -> cache hits;
// only the k new iterations recompute — the loop-tail oracle).
///////////////////////////////////////////////////////////////////////////////

struct LoopModuleInst : public SubGraphModuleInst {
  LoopModuleInst(const LoopModuleData* d, GraphInst* g);
  uint64_t cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const override;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) override;
  uint64_t _saltForIter(int i) const; // H(_saltBase, i) — distinct per iteration

  const LoopModuleData* _ld = nullptr;
};

} // namespace ork::dataflow
