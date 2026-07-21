////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>
#include <vector>

ImplementReflectionX(ork::lev2::hypermesh::LSystemModuleData, "hypermesh::LSystemModuleData");

// the family-neutral XfNodeGraph plug (G0a, ork::hyper::XfNodeGraph) — plug-data template
// reflection + instances, defined in the FIRST producing family's TU (the ScatterSource pattern).
// The data type + traits + inline data_to_inst already live in dflow/interchange.h.
namespace dflow = ::ork::dataflow;
namespace dgfx  = ork::lev2::dflowgfx;

template <> void dgfx::xfng_outplugdata_t::describeX(class_t*) {}
template <> void dgfx::xfng_inplugdata_t::describeX(class_t*) {}
template <> dflow::inpluginst_ptr_t dgfx::xfng_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dgfx::xfng_inpluginst_t>(this, minst);
}
template <> dflow::outpluginst_ptr_t dgfx::xfng_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<dgfx::xfng_outpluginst_t>(this, minst);
}
ImplementTemplateReflectionX(dgfx::xfng_outplugdata_t, "dflowgfx::xfngoutplug");
ImplementTemplateReflectionX(dgfx::xfng_inplugdata_t, "dflowgfx::xfnginplug");

namespace ork::lev2::hypermesh {

///////////////////////////////////////////////////////////////////////////////
// LSystemModule (L-system family, M1/GR1) — the PRODUCER of the XfNodeGraph spine.
//
// The skeleton is DERIVED from the module's reflected LRuleSet grammar (GR1.b
// evaluator, hmdflow_lruleset.cpp): rewrite + turtle-interpret into one XfNode
// per branch joint, which the LSweep skinner sweeps into tubes. Species are DATA
// (GR1.d): the four stock growth models are Python preset emitters
// (ork/hypergraph/dflow/lsystem/presets.py), not C++ procedures. This TU keeps
// only the shared frame math _buildFrames needs (_len3/_rot/_perp) + the
// module/upload plumbing.
///////////////////////////////////////////////////////////////////////////////

static inline float _len3(const fvec3& v) {
  return std::sqrt(v.dotWith(v));
}

// Rodrigues: rotate v around (unit) axis by ang.
static fvec3 _rot(const fvec3& v, const fvec3& axis, float ang) {
  fvec3 k = axis.normalized();
  float c = std::cos(ang), s = std::sin(ang);
  return v * c + k.crossWith(v) * s + k * (k.dotWith(v) * (1.0f - c));
}

// a vector perpendicular to d (the seed for rolling a bend axis around d).
static fvec3 _perp(const fvec3& d) {
  fvec3 ref = (std::fabs(d.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
  return ref.crossWith(d).normalized();
}

// parallel-transport (rotation-minimizing) frames in a forward pass: each node inherits
// its parent's right/up rotated by the minimal rotation aligning their headings, instead
// of rebuilding the frame from heading alone (which spins when heading ≈ ±Y → ring twist +
// pinch). Parents always precede children in index order, so one forward sweep suffices.
static void _buildFrames(ork::hyper::xfnode_vect& nodes) {
  int N = int(nodes.size());
  for (int i = 0; i < N; i++) {
    float* m = nodes[i]._xform;
    fvec3 head(m[4], m[5], m[6]);
    head = head.normalized();
    uint32_t par = nodes[i]._parent;
    fvec3 right, up;
    if (par == 0xffffffffu or int(par) >= i) { // root / unframed parent: stable seed frame
      fvec3 ref = (std::fabs(head.y) < 0.99f) ? fvec3(0, 1, 0) : fvec3(1, 0, 0);
      right     = ref.crossWith(head).normalized();
    } else {
      const float* pm = nodes[par]._xform;
      fvec3 pr(pm[0], pm[1], pm[2]), ph(pm[4], pm[5], pm[6]);
      fvec3 axis = ph.crossWith(head);
      float s    = _len3(axis);
      if (s < 1e-6f)
        right = pr; // (nearly) parallel headings: carry the parent frame unchanged
      else
        right = _rot(pr, axis * (1.0f / s), std::atan2(s, ph.dotWith(head)));
    }
    // re-orthonormalize right against head, rebuild up
    right     = right - head * head.dotWith(right);
    float rl  = _len3(right);
    right     = (rl > 1e-5f) ? right * (1.0f / rl) : _perp(head);
    up        = head.crossWith(right).normalized();
    m[0] = right.x; m[1] = right.y; m[2] = right.z;
    m[8] = up.x;    m[9] = up.y;    m[10] = up.z;
  }
}

struct LSystemModuleInst : public MeshComputeInst {
  LSystemModuleInst(const LSystemModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  const char* _cookSalt() const final;

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<dflowgfx::XfNodeGraphPlugTraits>("Out");
  }

  void onActivate(dflow::GraphInst* inst) final {
    _buildSkeleton(inst); // static: the skeleton is built once here (wind is VS-side, not a rebuild)
  }

  // _buildSkeleton is split out (re-callable) so a future TOPOLOGY animation (growth / L-system
  // rewrite) can drive it per-frame from a writeParams hook; the stable/non-stable upload below +
  // the LSweep version gate are the foundation for that. (Deformation anims like wind are VS-side.)
  void _buildSkeleton(dflow::GraphInst* inst) {
    auto env = inst->_impl.getShared<MeshEnv>();
    if (not env)
      return;
    auto fxi = env->_ctx->FXI();
    auto xng = _output->_value; // XfNodeGraphInst (created via data_to_inst)

    // --- run the rewrite + turtle interpret on the CPU ---
    xng->_nodes.clear();
    xng->_slots.clear();
    // GR1.d: the grammar is MANDATORY — species are DATA. A null grammar is authored-content
    // rot (a pre-GR1.d scene or a caller bypassing the DSL): fail LOUD + regenerate, no legacy
    // fallback (adjudication 11). The four stock growth models are the preset emitters in
    // ork/hypergraph/dflow/lsystem/presets.py, selected by H.lsystem(archetype=...).
    OrkAssertIFMT(bool(_d->_grammar),
        "[lsystem] module '%s' has no grammar. The legacy C++ archetype path was removed (GR1.d);"
        " author through H.lsystem(archetype=|grammar=) so a preset/DSL grammar is attached.",
        _dgmodule_data->_name.c_str());
    // derive the reflected grammar (rewrite + turtle-interpret). The 21 scalar props are the
    // LExpr PARAM env (A8).
    deriveLRuleSet(_d->_grammar.get(), _d, xng->_nodes, xng->_slots);
    _buildFrames(xng->_nodes); // parallel-transport frames → no ring twist on vertical runs
    const int N = int(xng->_nodes.size());
    xng->_count = N;

    // STABLE vs NON-STABLE topology. Count unchanged (wind deforming a fixed tree) = a POSITION-only
    // update: keep the buffers, re-upload only the frames (_xform), and markChanged (NOT markTopoChanged)
    // so the skinner re-skins positions WITHOUT realloc/re-triangulation. Count changed (growth /
    // L-system rewrite) = realloc + upload everything + markTopoChanged (a full rebuild downstream).
    bool topoChanged = (not xng->_xform) or (N != _lastN);
    if (topoChanged) {
      xng->_xform  = fxi->createStorageBuffer(size_t(N) * 16 * sizeof(float));
      xng->_parent = fxi->createStorageBuffer(size_t(N) * sizeof(uint32_t));
      xng->_attrs  = fxi->createStorageBuffer(size_t(N) * 4 * sizeof(float));
      xng->_tags   = fxi->createStorageBuffer(size_t(N) * sizeof(uint32_t));
      _lastN = N;
    }
    { // frames (positions + orientation) — re-uploaded on any (re)build
      auto mp  = fxi->mapStorageBuffer(xng->_xform, 0, size_t(N) * 16 * sizeof(float), BufferMapAccess::WRITE_ONLY);
      auto dst = (float*)mp->_mappedaddr;
      for (int i = 0; i < N; i++)
        for (int j = 0; j < 16; j++) dst[i * 16 + j] = xng->_nodes[i]._xform[j];
      fxi->unmapStorageBuffer(mp.get());
    }
    if (topoChanged) { // parentage / radius / tags only change with topology
      {
        auto mp  = fxi->mapStorageBuffer(xng->_parent, 0, size_t(N) * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
        auto dst = (uint32_t*)mp->_mappedaddr;
        for (int i = 0; i < N; i++) dst[i] = xng->_nodes[i]._parent;
        fxi->unmapStorageBuffer(mp.get());
      }
      {
        auto mp  = fxi->mapStorageBuffer(xng->_attrs, 0, size_t(N) * 4 * sizeof(float), BufferMapAccess::WRITE_ONLY);
        auto dst = (float*)mp->_mappedaddr;
        for (int i = 0; i < N; i++)
          for (int j = 0; j < 4; j++) dst[i * 4 + j] = xng->_nodes[i]._attrs[j];
        fxi->unmapStorageBuffer(mp.get());
      }
      {
        auto mp  = fxi->mapStorageBuffer(xng->_tags, 0, size_t(N) * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
        auto dst = (uint32_t*)mp->_mappedaddr;
        for (int i = 0; i < N; i++) dst[i] = xng->_nodes[i]._tags;
        fxi->unmapStorageBuffer(mp.get());
      }
    }
    if (topoChanged) xng->markTopoChanged(); else xng->markChanged();
    if (not _announced) {
      if(0)printf("LSystem<%s>: %d nodes (depth=%d budget=%d)\n",
             _dgmodule_data->_name.c_str(), N, _d->_depth, _d->_budget);
      _announced = true;
    }
  }

  void compute(dflow::GraphInst*, ui::updatedata_ptr_t) final {} // static graph: produced at onActivate

  const LSystemModuleData* _d;
  dflowgfx::xfng_outpluginst_ptr_t _output;
  int  _lastN     = -1;    // SSBO sizing guard (recreate only when node count changes)
  bool _announced = false; // print the node-count line once
};

// cook salt (T9): a CONTENT salt. A content salt does NOT auto-bust the cache on rebuild — so a
// C++ evaluator change (derive/turtle/RNG) leaves stale cooks in place SILENTLY. Therefore
// `evalv=N` is a MANDATORY MANUAL BUMP on ANY change to deriveLRuleSet / the turtle interpret /
// the counter-hash RNG in hmdflow_lruleset.cpp. Bump it or ship stale geometry.
//   evalv=2 @ GR1.d: moduleParam() lost the "archetype" PARAM-env entry with the legacy path
//   (a grammar naming PARAM("archetype") now evaluates 0) — bumped per the rule above. (The
//   reflected-prop removal already re-keys every lsystem module's identity hash regardless.)
const char* LSystemModuleInst::_cookSalt() const {
  return "lsystem lsys.v3-ruleset evalv=2";
}

static void _reshapeLSystemIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createOutputPlug<dflowgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
LSystemModuleData::LSystemModuleData() {
}
std::shared_ptr<LSystemModuleData> LSystemModuleData::createShared() {
  auto d = std::make_shared<LSystemModuleData>();
  _reshapeLSystemIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t LSystemModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<LSystemModuleInst>(this, g);
}
void LSystemModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return LSystemModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeLSystemIOs(m); });
  // the reflected grammar rides the module (directObjectProperty; REQUIRED at activation since
  // GR1.d). Flows into hypermeshModuleIdentityHash automatically.
  clazz->directObjectProperty("grammar", &LSystemModuleData::_grammar);
  clazz->directProperty("depth", &LSystemModuleData::_depth);
  clazz->directProperty("budget", &LSystemModuleData::_budget);
  clazz->directProperty("children", &LSystemModuleData::_children);
  clazz->directProperty("internodes", &LSystemModuleData::_internodes);
  clazz->directProperty("seed", &LSystemModuleData::_seed);
  clazz->directProperty("seg_len", &LSystemModuleData::_seg_len);
  clazz->directProperty("base_radius", &LSystemModuleData::_base_radius);
  clazz->directProperty("branch_angle", &LSystemModuleData::_branch_angle);
  clazz->directProperty("roll", &LSystemModuleData::_roll);
  clazz->directProperty("len_decay", &LSystemModuleData::_len_decay);
  clazz->directProperty("rad_decay", &LSystemModuleData::_rad_decay);
  clazz->directProperty("taper", &LSystemModuleData::_taper);
  clazz->directProperty("tropism", &LSystemModuleData::_tropism);
  clazz->directProperty("jitter", &LSystemModuleData::_jitter);
  clazz->directProperty("apical", &LSystemModuleData::_apical);
  clazz->directProperty("jit_azimuth", &LSystemModuleData::_jit_azimuth);
  clazz->directProperty("jit_pitch", &LSystemModuleData::_jit_pitch);
  clazz->directProperty("jit_length", &LSystemModuleData::_jit_length);
  clazz->directProperty("jit_spacing", &LSystemModuleData::_jit_spacing);
  clazz->directProperty("jit_drop", &LSystemModuleData::_jit_drop);
  clazz->directProperty("jit_wave", &LSystemModuleData::_jit_wave);
}

} // namespace ork::lev2::hypermesh
