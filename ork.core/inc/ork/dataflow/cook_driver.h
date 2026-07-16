////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// CookGraphDriver — the FAMILY-NEUTRAL seam the generic composite runtime
// (SubGraphModuleInst / LoopModuleInst) drives to cook a nested subgraph.
//
// A family (terrain, and later hypermesh / particles) implements the GPU- and
// memory-model-specific hooks and STOCKS a driver on the host GraphInst's _impl
// (TypeKeyedVars, keyed by THIS base type) right where it stocks its cook
// environment. The composite insts resolve the driver from there and orchestrate
// it. Everything family-neutral — per-iteration salt, promoted-input forwarding,
// loop control, iter-feeds, forced-root demand, accounting — lives in the composite
// insts and needs NO family types (see subgraph_module.cpp). Everything the driver
// exposes is a GPU / memory-model concern (the essential divergence catalogued in
// the cook-driver dossier): the nested cook, the boundary-resource device blits, the
// phase choreography.
//
// This header carries NO GPU / lev2 types — only core dataflow types — so any family
// can implement it. The reference implementation is terrain's TerrainCookDriver in
// hfdflow.cpp; see JUL13_DFLOW.md §3 E4 + Appendix S.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/dataflow/dataflow.h>
#include <vector>
#include <cstdint>

namespace ork::dataflow {

struct SubGraphModuleData;
struct LoopModuleData;

///////////////////////////////////////////////////////////////////////////////
// nested-cook accounting a driver's runNested returns; a composite may sum these
// across a loop's iterations for cache instrumentation (computes = nodes that
// dispatched; loaded = nodes served from the cook cache).
///////////////////////////////////////////////////////////////////////////////

struct CompositeCookCounts {
  int _computes = 0;
  int _loaded   = 0;
};

///////////////////////////////////////////////////////////////////////////////
// CookGraphDriver — the family hooks the generic composite runtime calls. Each hook
// is a GPU / memory-model concern; the composite insts own the family-neutral rest.
///////////////////////////////////////////////////////////////////////////////

struct CookGraphDriver {
  virtual ~CookGraphDriver() = default;

  // build a nested GraphInst for `subgraph` that SHARES this driver's cook environment
  // (family register pools + env + cook cache). The composite forwards promoted inputs
  // and iter-feeds onto it, then runs it via runNested — once for a subnet, per-iter for
  // a loop.
  virtual graphinst_ptr_t makeNestedInst(graphdata_ptr_t subgraph) = 0;

  // phase choreography — the composite closes the host's dispatch phase on ENTRY and
  // reopens it on EXIT; the nested cook and the boundary copies manage their own phases
  // in between (a family that runs one flat phase makes these no-ops).
  virtual void closeHostPhase() = 0;
  virtual void reopenHostPhase() = 0;

  // run the nested GraphInst's cook loop under a per-iteration context salt, forcing +
  // pinning `forced_roots` (the promoted-output inner insts) so the composite can read
  // their outputs after the run returns. allow_disk_cache=false forces recompute-every-
  // run (a captureless nested body under a non-cacheable host — the salt-valid <=> disk-
  // cache invariant). Returns nested-node accounting.
  virtual CompositeCookCounts runNested(
      graphinst_ptr_t nested,
      ui::updatedata_ptr_t updata,
      uint64_t iteration_salt,
      const std::vector<dgmoduleinst_ptr_t>& forced_roots,
      bool allow_disk_cache) = 0;

  // LOOP carry init: allocate each carry's boundary-OUTPUT resource (its persistent carry
  // buffer) and initialize it from the composite's external input (a device copy; an unfed
  // carry self-defends to zeros). Batched in its own submit+wait phase.
  virtual void initCarries(DgModuleInst* composite, const LoopModuleData* ld) = 0;

  // publish a SUBNET's promoted outputs: allocate each boundary-output resource and device-
  // blit the nested inner output into it (batched, own submit+wait phase), releasing the
  // pinned inner resources afterward.
  virtual void publishSubgraphOutputs(
      DgModuleInst* composite, graphinst_ptr_t nested, const SubGraphModuleData* sd) = 0;

  // publish a LOOP iteration's outputs: carries copy EVERY iteration (feeding the next);
  // non-carry promoted outputs copy on the LAST iteration only. Same device-blit + own-
  // phase + release shape as publishSubgraphOutputs.
  virtual void publishLoopOutputs(
      DgModuleInst* composite, graphinst_ptr_t nested, const LoopModuleData* ld,
      int iteration, int count) = 0;
};

using cookgraphdriver_ptr_t = std::shared_ptr<CookGraphDriver>;

} // namespace ork::dataflow
