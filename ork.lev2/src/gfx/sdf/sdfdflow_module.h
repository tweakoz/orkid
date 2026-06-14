////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// sdfdflow_module.h — shared contract for the sdfgrid GPU modules (the hypermesh
// hmdflow_module.h pattern at family scale). Each module's implementation lives
// in its own sdfdflow_module_<name>.cpp; commons + the M0 gate in sdfdflow.cpp.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/sdf/sdfdflow.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>

namespace ork::lev2::sdf {

// substitute %KEY% -> val in a shader template (the terrain/hypermesh _shadersub idiom;
// those live in family-private module headers, so each family carries its own copy).
inline void _shadersub(std::string& s, const std::string& key, const std::string& val) {
  size_t pos = 0;
  while ((pos = s.find(key, pos)) != std::string::npos) {
    s.replace(pos, key.size(), val);
    pos += val.size();
  }
}

///////////////////////////////////////////////////////////////////////////////
// SdfComputeInst — module-inst base. v1 env contract (ratified): sdf modules
// ride inside hypermesh graphs and use the GraphInst's MeshEnv (ctx + the
// family-neutral pow2 SSBO pool).
///////////////////////////////////////////////////////////////////////////////

struct SdfComputeInst : public dflow::DgModuleInst, public dflowgfx::IPrePhaseParams {
  SdfComputeInst(const dflow::DgModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g) {
  }
  hypermesh::meshenv_ptr_t env() const {
    return _graphinst->_impl.getShared<hypermesh::MeshEnv>();
  }
  // the node's grid output (resolved by name so derived insts keep their own members)
  dflowgfx::sdfgrid_inst_ptr_t _outGrid() const {
    auto self = const_cast<SdfComputeInst*>(this);
    auto outp = self->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
    return outp ? outp->_value : nullptr;
  }
  // the source grid for an input plug = the CONNECTED output's value
  static dflowgfx::sdfgrid_inst_ptr_t _srcGrid(dflowgfx::sdfgrid_inpluginst_ptr_t inp) {
    auto out = std::dynamic_pointer_cast<dflowgfx::sdfgrid_outpluginst_t>(inp->_connectedOutput);
    return out ? out->_value : nullptr;
  }
  // M4c (hash) — a CONTENT-distinct cook hash so a downstream cacheable mesh (SdfToMesh) folds the
  // SDF subgraph's identity into its Merkle. WITHOUT this, SDF nodes contribute 0 to input_hashes
  // and every `<sdf>.to_mesh()` static graph hashes IDENTICALLY -> the mesh cook cache COLLIDES
  // (serves a stale mesh from a different SDF graph). class name + reflected content (expression /
  // op / max_iterations + dim/extent/center plug values, uuid-stripped) + upstream hashes. The SDF
  // nodes themselves don't cookStore (recompute each run); this only fixes the DOWNSTREAM mesh key.
  uint64_t cookComputeHash(const std::vector<uint64_t>& input_hashes, uint64_t context) const override;
};

} // namespace ork::lev2::sdf
