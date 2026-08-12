////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"
#include <cmath>
#include <cstring>

ImplementReflectionX(ork::lev2::hypermesh::LeafScatterModuleData, "hypermesh::LeafScatterModuleData");

namespace ork::lev2::hypermesh {

namespace dgfx = ork::lev2::dflowgfx;

// read the connected XfNodeGraph value (the _srcXfng pattern, shared with LSweep).
static ::ork::hyper::xfnodegraph_inst_ptr_t _srcXfng(dgfx::xfng_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<dgfx::xfng_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

// column-major 4x4 multiply (out = A * B) — the XfNode frame composed with an XfSlot's local xform.
static inline void _mul44(const float* A, const float* B, float* out) {
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++) {
      float s = 0.0f;
      for (int k = 0; k < 4; k++)
        s += A[k * 4 + r] * B[c * 4 + k];
      out[c * 4 + r] = s;
    }
}

// minimum azimuth separation (degrees) two cards of one spray must hold. Only closed-spiral
// aliases fall under it: the golden angle keeps >=20 deg out to 12 cards, while every rational
// divergence collapses to 0 exactly (90 x 5, 120 x 4, 144 x 6, 180 x 3, ...).
static constexpr float kAzMinDeg = 1.0f;

// canonical azimuth of spray index j, wrapped into [0,360).
static inline float _azOf(int j, float roll) {
  float a = std::fmod(float(j) * roll, 360.0f);
  return (a < 0.0f) ? (a + 360.0f) : a;
}

// stateless hash -> uint32, deterministic per (seed, node, leaf) — for reproducible per-leaf jitter.
static inline uint32_t _lhash(int a, int b, int c) {
  uint32_t x = uint32_t(a) * 2654435761u + uint32_t(b) * 2246822519u + uint32_t(c) * 3266489917u + 374761393u;
  x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
  return x;
}

// BARK EMBEDDING — how far along the outward radial the card's BASE EDGE anchors. The wood surface is
// the LSweep ring (centerline + radius * radial, hmdflow_module_lsweep.cpp), so a base edge left on the
// centerline hangs INSIDE the wood on a thick segment and beside it on a thin one. Contract: with
// `embed` > 0 the base edge sits UNDER the bark (recessed `embed` of the local radius) while the tip
// stays free, so the blade emerges THROUGH the surface. The edge is 2*hw wide about the anchor, so the
// offset also clamps to the deepest value keeping BOTH base corners inside the wood cylinder:
//   |t*outdir +- hw*wp|^2 <= rad^2, wp = the width axis with its heading component removed
//   -> t <= sqrt(rad^2 - hw^2*|wp|^2 + c^2) - c,  c = hw*|outdir . wp|
// A card wider than its segment admits no positive t and keeps the centerline anchor: it cannot be
// contained, and pushing it out would only throw its corners further through the bark than before.
static inline float _baseOffset(float rad, float embed, float hw,
                                const fvec3& outdir, const fvec3& H, const fvec3& waxis) {
  if (embed <= 0.0f or rad <= 0.0f)
    return 0.0f; // off (or a placement with no wood around it) -> base edge on the centerline
  fvec3 wp = waxis - H * H.dotWith(waxis);
  float c  = hw * std::fabs(outdir.dotWith(wp));
  float q  = rad * rad - hw * hw * wp.dotWith(wp) + c * c;
  float t  = (q > 0.0f) ? (std::sqrt(q) - c) : 0.0f;
  return std::min(rad * std::max(0.0f, 1.0f - embed), std::max(0.0f, t));
}

///////////////////////////////////////////////////////////////////////////////
// LeafScatterModuleInst — reads the XfNodeGraph skeleton (same input LSweep skins) and emits a
// broadleaf-card GpuMesh: at every node with generation (_attrs[1]) >= _min_gen and radius (_attrs[0])
// within _max_radius, _per_node leaf cards placed by PHYLLOTAXIS (golden-angle roll around the node
// heading, drooped _pitch from it), base edge embedded in the wood by _embed. CPU build.
// _source==1 (SLOTS / instance_at_slots) instead walks the graph's _slots side-table and places at each
// slot's WORLD frame; everything downstream of the placement list (the card build) is shared.
///////////////////////////////////////////////////////////////////////////////
struct LeafScatterModuleInst : public MeshComputeInst {
  LeafScatterModuleInst(const LeafScatterModuleData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<dgfx::XfNodeGraphPlugTraits>("In");
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto xng = _srcXfng(_input);
    if (not xng or xng->_nodes.empty())
      return; // producer not ready yet
    if (xng->_version == _lastXngV)
      return; // up to date — the leaves are static cards (wind is VS-side); rebuild only on skeleton change
    _lastXngV = xng->_version;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto fxi = env->_ctx->FXI();

    const auto& nodes = xng->_nodes;
    const int   N     = int(nodes.size());
    const int   PER   = std::max(1, _d->_per_node);
    const int   QPL   = (_d->_style == 1) ? 2 : 1; // quads per leaf (cross = 2)

    // PHYLLOTAXIS ALIASING — card j sits at azimuth j*_roll about the node heading, so a divergence
    // that closes the circle puts two cards of the SAME spray on the SAME base point, near-coplanar
    // and separated only by the jitter (millimetres of depth over a card-length blade). That reads as
    // fill waste in mono and as a two-tone flicker in stereo — the pair carries two different per-leaf
    // hashes, and each eye's projection resolves the depth tie on its own. Emit the distinct azimuths
    // only; survivors keep their original j (the hash / stagger key), so the kept cards are unchanged.
    std::vector<int> keep;
    for (int j = 0; j < PER; j++) {
      float aj    = _azOf(j, _d->_roll);
      bool  alias = false;
      for (int k : keep) {
        float d = std::fabs(aj - _azOf(k, _d->_roll));
        if (std::min(d, 360.0f - d) < kAzMinDeg) { alias = true; break; }
      }
      if (not alias)
        keep.push_back(j);
    }
    const int PERK = int(keep.size());

    // PLACEMENTS — one frame per leaf cluster, in emission order. `key` seeds the per-placement
    // golden-angle stagger and the jitter hash (node index / slot index), so both sources stay
    // deterministic per (seed, placement, leaf). `rad` is the local segment radius the base edge
    // embeds into (0 = no wood to embed in).
    struct Placement { fvec3 R, H, U, P; int key; float rad; };
    std::vector<Placement> places;
    int  nradex = 0; // nodes that passed the generation gate but were vetoed as too thick
    auto pushFrame = [&](const float* m, int key, float rad) {
      places.push_back({fvec3(m[0], m[1], m[2]), fvec3(m[4], m[5], m[6]),
                        fvec3(m[8], m[9], m[10]), fvec3(m[12], m[13], m[14]), key, rad});
    };
    if (_d->_source == 1) {
      // SLOTS: the grammar's attachment points. World frame = owning node xform * slot local. The slot
      // origin is wherever the grammar put it (an areole already sits ON the surface), so it carries no
      // embedding radius — offsetting it again would double the displacement.
      const auto& slots = xng->_slots;
      for (int s = 0; s < int(slots.size()); s++) {
        OrkAssertIFMT(slots[s]._node < uint32_t(N),
          "[leafscatter] XfSlot %d references node %u of %d — the slot table outran its graph.",
          s, slots[s]._node, N);
        float w[16];
        _mul44(nodes[slots[s]._node]._xform, slots[s]._local, w);
        pushFrame(w, s, 0.0f);
      }
    } else {
      // NODES: generation (_attrs[1]) >= _min_gen — the outer twigs bear the canopy — AND radius
      // (_attrs[0]) within _max_radius (0 = off): a segment fatter than that is trunk/bough wood, and
      // foliage rooted in it reads as needles growing out of the bole. Generation alone cannot make
      // that cut — a monopodial leader carries ONE generation from its thick base to its thin apex.
      for (int i = 0; i < N; i++) {
        if (nodes[i]._attrs[1] < _d->_min_gen)
          continue;
        if (_d->_max_radius > 0.0f and nodes[i]._attrs[0] > _d->_max_radius) {
          nradex++;
          continue;
        }
        pushFrame(nodes[i]._xform, i, nodes[i]._attrs[0]);
      }
    }

    const int nleaves  = int(places.size()) * PERK;
    const int nverts   = nleaves * 4 * QPL;
    const int nfaces   = nleaves * QPL;
    const int ncorners = nfaces * 4;
    if (nleaves == 0)
      return; // no canopy at this _min_gen — leave the mesh empty (nothing to draw)

    auto mesh = _output->_value;
    allocMesh(env, mesh, nverts, ncorners, nfaces,
              {MeshChannel::POSITION, MeshChannel::NORMAL, MeshChannel::BINORMAL,
               MeshChannel::UV0, MeshChannel::COLOR});
    mesh->_faces["material_id"] = env->_pool->acquireChannel(4, std::max(1, nfaces));
    auto matc = mesh->_faces["material_id"];

    std::vector<float> P(size_t(nverts) * 4), Nr(size_t(nverts) * 4), Bn(size_t(nverts) * 4),
        UV(size_t(nverts) * 4), Cl(size_t(nverts) * 4, 1.0f);
    std::vector<uint32_t> VI(size_t(ncorners), 0u);
    std::vector<uint32_t> FO(size_t(nfaces) + 1, 0u);
    std::vector<uint32_t> MAT(size_t(std::max(1, nfaces)), 0u);
    fvec3 bbmin(1e30f, 1e30f, 1e30f), bbmax(-1e30f, -1e30f, -1e30f);

    const float DEG     = 0.01745329252f;
    const float rollRad = _d->_roll * DEG;
    int vbase = 0, fbase = 0, cc = 0;

    auto putVert = [&](const fvec3& p, const fvec3& n, const fvec3& w, float u, float vv, float flut, float lhash) {
      int v = vbase++;
      P[v * 4] = p.x;  P[v * 4 + 1] = p.y;  P[v * 4 + 2] = p.z;  P[v * 4 + 3] = 1;
      Nr[v * 4] = n.x; Nr[v * 4 + 1] = n.y; Nr[v * 4 + 2] = n.z; Nr[v * 4 + 3] = 0;
      Bn[v * 4] = w.x; Bn[v * 4 + 1] = w.y; Bn[v * 4 + 2] = w.z; Bn[v * 4 + 3] = 0; // width axis -> tangent
      UV[v * 4] = u;   UV[v * 4 + 1] = vv;  UV[v * 4 + 2] = 0;   UV[v * 4 + 3] = 0; // card uv (material textures it)
      Cl[v * 4] = flut;                 // COLOR.x = flutter weight (0 petiole .. 1 tip)
      Cl[v * 4 + 1] = lhash;            // COLOR.y = per-leaf hash (hue/phase variation)
      Cl[v * 4 + 2] = 0; Cl[v * 4 + 3] = 1;
      bbmin.x = std::min(bbmin.x, p.x); bbmin.y = std::min(bbmin.y, p.y); bbmin.z = std::min(bbmin.z, p.z);
      bbmax.x = std::max(bbmax.x, p.x); bbmax.y = std::max(bbmax.y, p.y); bbmax.z = std::max(bbmax.z, p.z);
    };

    for (int e = 0; e < int(places.size()); e++) {
      const int   ni = places[e].key;
      // frame columns: X=right Y=heading Z=up T=pos
      const fvec3& R = places[e].R; const fvec3& H = places[e].H; const fvec3& U = places[e].U;
      const fvec3& Pn = places[e].P;
      const float  rad = places[e].rad;
      const float nodeBase = float(ni) * 2.399963f; // per-placement golden-angle stagger (so they don't align)

      for (int j : keep) {
        uint32_t h  = _lhash(_d->_seed, ni, j);
        float    j0 = float(h & 0xffff) / 65535.0f;          // [0,1)
        float    j1 = float((h >> 16) & 0xffff) / 65535.0f;  // [0,1)
        // second/third hash streams: the ANGULAR (degrees) jitter. Keyed on the same stable identity
        // (seed, placement, card) as j0/j1 — never a clock or rand() — so two cooks of one graph are
        // byte-identical. The alias dedupe above keys on the NOMINAL spiral azimuths only, so no amount
        // of jitter can un-alias a dropped card or collide a kept one.
        uint32_t h2 = _lhash(_d->_seed ^ 0x5bf03635, ni, j);
        uint32_t h3 = _lhash(_d->_seed ^ 0x27d4eb2f, ni, j);
        float    k0 = float(h2 & 0xffff) / 65535.0f;         // azimuth
        float    k1 = float((h2 >> 16) & 0xffff) / 65535.0f; // pitch
        float    k2 = float(h3 & 0xffff) / 65535.0f;         // twist
        float    jd    = _d->_jitter_deg * DEG;
        float    az    = nodeBase + float(j) * rollRad + _d->_jitter * (j0 * 2.0f - 1.0f) * 0.4f
                         + jd * (k0 * 2.0f - 1.0f);
        float    pitch = _d->_pitch * DEG * (1.0f + _d->_jitter * (j1 * 2.0f - 1.0f) * 0.5f)
                         + jd * (k1 * 2.0f - 1.0f);
        float    twist = _d->_twist * DEG + jd * (k2 * 2.0f - 1.0f);
        float    sz    = _d->_size * (1.0f + _d->_jitter * (j0 * 2.0f - 1.0f) * 0.4f);
        float    hw    = sz * _d->_aspect * 0.5f;
        float    lh    = float(h & 0xff) / 255.0f;           // per-leaf hash -> COLOR.y

        fvec3 outdir = R * std::cos(az) + U * std::sin(az);                 // radial (perp to heading)
        fvec3 L      = (H * std::cos(pitch) + outdir * std::sin(pitch));    // blade length axis (drooped)
        if (L.magnitude() < 1e-5f) continue;
        L = L.normalized();
        if (_d->_up_bias != 0.0f) {
          // bend the EMERGENCE DIRECTION toward world up (+) / down (-); the base anchor is radial and
          // stays put, so a biased blade still leaves the wood at the same point.
          fvec3 biased = L + fvec3(0.0f, 1.0f, 0.0f) * _d->_up_bias;
          if (biased.magnitude() > 1e-5f)
            L = biased.normalized();
        }
        fvec3 W = L.crossWith(H);                                          // width axis (tangential)
        if (W.magnitude() < 1e-4f) W = L.crossWith(R);                     // degenerate guard (L ~ H)
        W = W.normalized();
        fvec3 Nn = W.crossWith(L).normalized();                            // leaf face normal
        if (twist != 0.0f) {                                               // roll the card about its own length
          float ct = std::cos(twist), st = std::sin(twist);
          fvec3 W2 = W * ct - Nn * st;
          Nn       = Nn * ct + W * st;
          W        = W2;
        }

        // one quad (style 0) or a perpendicular cross (style 1). corners: petiole L/R -> tip R/L,
        // wound CCW from the front face. UV v = 0 at petiole, 1 at tip (= flutter weight). The petiole
        // edge anchors at Bp — inside the wood when _embed arms it (see _baseOffset), on the centerline
        // otherwise; the blade keeps its full length sz from that anchor.
        auto putQuad = [&](const fvec3& waxis, const fvec3& nrm) {
          fvec3 Bp = Pn + outdir * _baseOffset(rad, _d->_embed, hw, outdir, H, waxis);
          int v0 = vbase;
          putVert(Bp + waxis * (-hw),          nrm, waxis, 0.0f, 0.0f, 0.0f, lh);
          putVert(Bp + waxis * (hw),           nrm, waxis, 1.0f, 0.0f, 0.0f, lh);
          putVert(Bp + L * sz + waxis * (hw),  nrm, waxis, 1.0f, 1.0f, 1.0f, lh);
          putVert(Bp + L * sz + waxis * (-hw), nrm, waxis, 0.0f, 1.0f, 1.0f, lh);
          FO[fbase] = uint32_t(cc);
          VI[cc++]  = uint32_t(v0);
          VI[cc++]  = uint32_t(v0 + 1);
          VI[cc++]  = uint32_t(v0 + 2);
          VI[cc++]  = uint32_t(v0 + 3);
          MAT[fbase] = 0u;
          fbase++;
        };
        putQuad(W, Nn);
        if (_d->_style == 1)
          putQuad(Nn, W); // the perpendicular blade of the cross (its normal is the first blade's width)
      }
    }
    FO[nfaces] = uint32_t(ncorners); // CSR terminal

    // --- upload (header + channels) — same staging-copy path as LSweep ---
    {
      auto mp = fxi->mapStorageBuffer(mesh->_header, 0, 64, BufferMapAccess::WRITE_ONLY);
      auto hu = (uint32_t*)mp->_mappedaddr;
      auto hf = (float*)mp->_mappedaddr;
      hu[0] = uint32_t(nverts); hu[1] = uint32_t(ncorners); hu[2] = uint32_t(nfaces); hu[3] = 0u;
      hf[4] = bbmin.x; hf[5] = bbmin.y; hf[6] = bbmin.z; hf[7] = 1;
      hf[8] = bbmax.x; hf[9] = bbmax.y; hf[10] = bbmax.z; hf[11] = 1;
      fxi->unmapStorageBuffer(mp.get());
    }
    auto upf = [&](MeshChannel ch, const std::vector<float>& v) {
      auto ss = mesh->channel(ch)->_ssbo;
      auto mp = fxi->mapStorageBuffer(ss, 0, v.size() * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(float));
      fxi->unmapStorageBuffer(mp.get());
    };
    auto upu = [&](FxShaderStorageBuffer* ss, const std::vector<uint32_t>& v) {
      auto mp = fxi->mapStorageBuffer(ss, 0, v.size() * sizeof(uint32_t), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mp->_mappedaddr, v.data(), v.size() * sizeof(uint32_t));
      fxi->unmapStorageBuffer(mp.get());
    };
    upf(MeshChannel::POSITION, P);
    upf(MeshChannel::NORMAL, Nr);
    upf(MeshChannel::BINORMAL, Bn);
    upf(MeshChannel::UV0, UV);
    upf(MeshChannel::COLOR, Cl);
    upu(mesh->_vidx->_ssbo, VI);
    upu(mesh->_face_offsets->_ssbo, FO);
    upu(matc->_ssbo, MAT);

    mesh->markTopoChanged();
    if (not _announced) {
      if (PERK < PER)
        printf("LeafScatter<%s>: per_node %d roll %.1fdeg CLOSES THE SPIRAL — %d of %d cards per spray "
               "alias onto an earlier azimuth and are dropped (coincident cards z-fight in stereo). "
               "Author %d cards, or a divergence that does not divide 360.\n",
               _dgmodule_data->_name.c_str(), PER, _d->_roll, PER - PERK, PER, PERK);
      if (_d->_source == 1)
        printf("LeafScatter<%s>: %d leaves (%d verts, %d faces) on %d/%d slots\n",
               _dgmodule_data->_name.c_str(), nleaves, nverts, nfaces,
               int(places.size()), int(xng->_slots.size()));
      else
        printf("LeafScatter<%s>: %d leaves (%d verts, %d faces) on %d/%d nodes "
               "(gen>=%.1f, radmax %.3f [0=off] vetoed %d, embed %.2f)\n",
               _dgmodule_data->_name.c_str(), nleaves, nverts, nfaces, int(places.size()), N,
               _d->_min_gen, _d->_max_radius, nradex, _d->_embed);
      _announced = true;
    }
  }

  const char* _cookSalt() const final;

  const LeafScatterModuleData* _d;
  mesh_outpluginst_ptr_t       _output;
  dgfx::xfng_inpluginst_ptr_t  _input;
  uint64_t _lastXngV = ~0ull;
  bool     _announced = false;
};

// cook salt ties to THIS TU's compile time — any kernel change auto-invalidates the disk cook-cache.
const char* LeafScatterModuleInst::_cookSalt() const {
  return "leafscatter " __DATE__ " " __TIME__;
}

static void _reshapeLeafIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<dgfx::XfNodeGraphPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
LeafScatterModuleData::LeafScatterModuleData() {
}
std::shared_ptr<LeafScatterModuleData> LeafScatterModuleData::createShared() {
  auto d = std::make_shared<LeafScatterModuleData>();
  _reshapeLeafIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t LeafScatterModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<LeafScatterModuleInst>(this, g);
}
void LeafScatterModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return LeafScatterModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeLeafIOs(m); });
  clazz->directProperty("style", &LeafScatterModuleData::_style);
  clazz->directProperty("source", &LeafScatterModuleData::_source);
  clazz->directProperty("per_node", &LeafScatterModuleData::_per_node);
  clazz->directProperty("min_gen", &LeafScatterModuleData::_min_gen);
  clazz->directProperty("max_radius", &LeafScatterModuleData::_max_radius);
  clazz->directProperty("size", &LeafScatterModuleData::_size);
  clazz->directProperty("aspect", &LeafScatterModuleData::_aspect);
  clazz->directProperty("roll", &LeafScatterModuleData::_roll);
  clazz->directProperty("pitch", &LeafScatterModuleData::_pitch);
  clazz->directProperty("embed", &LeafScatterModuleData::_embed);
  clazz->directProperty("twist", &LeafScatterModuleData::_twist);
  clazz->directProperty("up_bias", &LeafScatterModuleData::_up_bias);
  clazz->directProperty("jitter", &LeafScatterModuleData::_jitter);
  clazz->directProperty("jitter_deg", &LeafScatterModuleData::_jitter_deg);
  clazz->directProperty("seed", &LeafScatterModuleData::_seed);
}

} // namespace ork::lev2::hypermesh
