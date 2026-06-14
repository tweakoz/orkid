////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hmdflow_module.h"

ImplementReflectionX(ork::lev2::hypermesh::DisplaceBySdfData, "hypermesh::DisplaceBySdfData");

namespace ork::lev2::hypermesh {

// DisplaceBySdf (M4b) — the cross-family SDF edge (see hmdflow.h). "In" = GpuMesh, "Field" =
// the SdfGrid interchange plug (SdfEval/Csg/Redistance ride the SAME MeshEnv -> no foreign env).
//   conform : Gauss-Newton shrinkwrap onto the iso-surface (+ TANGENTIAL RELAX for even retopo)
//   offset  : P += amount * normalize(grad)
//   scalar  : P += amount * (phi-iso_level) * N
// Produces POSITION (displaced) AND NORMAL (= normalize(grad phi), the EXACT analytic surface
// normal — far better than re-averaging the bunched triangulation). Topology + uv/color pass through.
//
// RELAX (even distribution): projection conform alone clusters verts where the surface curves
// toward the base mesh. relax_steps iterations of [per-vertex 1-ring centroid -> TANGENTIAL move
// (normal component removed via grad phi) -> re-project onto the surface] redistribute them. The
// 1-ring centroid is accumulated per-face with fixed-point int atomics (the mesh_to_sdf pn idiom).

// φ + trilinear gradient at the (outer-scope) `P` -> writes outer-scope `phi`,`grad`. Inlined.
static const char* _sdf_sample_block() {
  return R"S(
  {
    vec3 bc = (P - vec3(p_ox, p_oy, p_oz)) / p_voxel;        // P in grid coords (may be out of brick)
    vec3 mxc = vec3(float(p_dimx - 1u), float(p_dimy - 1u), float(p_dimz - 1u));
    vec3 cl = clamp(bc, vec3(0.0), mxc);                     // clamped sample point (inside the brick)
    // floor clamped to dim-2 so the +1 neighbour (and the forward difference) is ALWAYS valid and
    // non-degenerate even on the far boundary face — without this, x0=dim-1 makes c100-c000==0 and
    // the boundary-normal gradient component collapses to 0 (breaking the out-of-brick extrapolation).
    uint x0 = min(uint(floor(cl.x)), p_dimx - 2u);
    uint y0 = min(uint(floor(cl.y)), p_dimy - 2u);
    uint z0 = min(uint(floor(cl.z)), p_dimz - 2u);
    uint x1 = x0 + 1u; uint y1 = y0 + 1u; uint z1 = z0 + 1u;
    vec3 f = cl - vec3(float(x0), float(y0), float(z0));     // in [0,1] (==1 on the far boundary cell)
    uint sx = 1u; uint sy = p_dimx; uint sz = p_dimx * p_dimy;
    float c000 = SDF[x0*sx + y0*sy + z0*sz]; float c100 = SDF[x1*sx + y0*sy + z0*sz];
    float c010 = SDF[x0*sx + y1*sy + z0*sz]; float c110 = SDF[x1*sx + y1*sy + z0*sz];
    float c001 = SDF[x0*sx + y0*sy + z1*sz]; float c101 = SDF[x1*sx + y0*sy + z1*sz];
    float c011 = SDF[x0*sx + y1*sy + z1*sz]; float c111 = SDF[x1*sx + y1*sy + z1*sz];
    float c00 = mix(c000, c100, f.x); float c10 = mix(c010, c110, f.x);
    float c01 = mix(c001, c101, f.x); float c11 = mix(c011, c111, f.x);
    phi = mix(mix(c00, c10, f.y), mix(c01, c11, f.y), f.z);
    float gx = mix(mix(c100 - c000, c110 - c010, f.y), mix(c101 - c001, c111 - c011, f.y), f.z) / p_voxel;
    float gy = mix(mix(c010 - c000, c110 - c100, f.x), mix(c011 - c001, c111 - c101, f.x), f.z) / p_voxel;
    float gz = mix(mix(c001 - c000, c101 - c100, f.x), mix(c011 - c010, c111 - c110, f.x), f.y) / p_voxel;
    grad = vec3(gx, gy, gz);
    // OUT-OF-BRICK EXTRAPOLATION: the field exists only inside the brick. For a vertex OUTSIDE
    // (bc != cl) treat the field as locally linear (distance-like) and extrapolate phi from the
    // boundary along the boundary gradient. A mesh anywhere outside is then pulled toward the
    // iso-surface — DECOUPLING the base mesh from the brick: no clamp-collapse / "locking to extent"
    // spikes. d_out is the world-space outside offset (zero when P is inside).
    vec3 d_out = (bc - cl) * p_voxel;
    phi = phi + dot(grad, d_out);
  }
)S";
}

// the shared params block (same layout, declared per-kernel-file at its own slot per the Metal trap)
static const char* _ds_par() {
  return R"S(
storage_interface dsf_ct (descriptor_set 0) { buffer layout(std430) dctb {
  uint p_nv; uint p_mode; uint p_steps; uint p_nfaces;
  uint p_dimx; uint p_dimy; uint p_dimz; uint p_u0;
  float p_ox; float p_oy; float p_oz; float p_voxel;
  float p_amount; float p_isolevel; float p_maxstep; float p_lambda; }; }
)S";
}

static void _sub(std::string& t) {
  size_t pos = 0; std::string blk = _sdf_sample_block();
  while ((pos = t.find("%SAMPLE%", pos)) != std::string::npos) { t.replace(pos, 8, blk); pos += blk.size(); }
  pos = 0; std::string par = _ds_par();
  while ((pos = t.find("%PAR%", pos)) != std::string::npos) { t.replace(pos, 5, par); pos += par.size(); }
}

// (1) conform / offset / scalar : iP(0) iN(1) oP(2) SDF(3) ct(4)
static std::string _displace_sdf_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface dsf_iP  (descriptor_set 0) { buffer layout(std430) dipb { vec4 iP[]; }; }
storage_interface dsf_iN  (descriptor_set 0) { buffer layout(std430) dinb { vec4 iN[]; }; }
storage_interface dsf_oP  (descriptor_set 0) { buffer layout(std430) dopb { vec4 oP[]; }; }
storage_interface dsf_sdf (descriptor_set 0) { buffer layout(std430) dsdb { float SDF[]; }; }
%PAR%
compute_interface iface { storage { dsf_iP dsf_iN dsf_oP dsf_sdf dsf_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_displace_sdf : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  vec3 P = iP[v].xyz;
  vec3 N = normalize(iN[v].xyz);
  float phi; vec3 grad;
  if (p_mode == 0u) {
    int steps = int(p_steps);
    for (int s = 0; s < steps; s = s + 1) {
      %SAMPLE%
      float g2 = dot(grad, grad);
      if (g2 > 1.0e-12) {
        vec3 d = (phi - p_isolevel) * grad * inversesqrt(g2);   // step the TRUE distance (no overshoot)
        float dl = length(d);
        if (dl > p_maxstep) { d = d * (p_maxstep / dl); }
        P = P - d;
      }
    }
  } else {
    %SAMPLE%
    if (p_mode == 1u) {
      float gl = length(grad);
      vec3 nrm = (gl > 1.0e-6) ? (grad / gl) : N;
      P = P + p_amount * nrm;
    } else {
      P = P + p_amount * (phi - p_isolevel) * N;
    }
  }
  oP[v] = vec4(P, 1.0);
}
)S";
  _sub(t);
  return t;
}

// (2) relax clear : ACC(0) ct(1)   — zero the fixed-point 1-ring accumulator (xyz*SCALE + count)
static std::string _relax_clear_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface rcf_acc (descriptor_set 0) { buffer layout(std430) racb { int ACC[]; }; }
%PAR%
compute_interface iface { storage { rcf_acc dsf_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_relax_clear : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  ACC[4u*v + 0u] = 0; ACC[4u*v + 1u] = 0; ACC[4u*v + 2u] = 0; ACC[4u*v + 3u] = 0;
}
)S";
  _sub(t);
  return t;
}

// (3) relax accum : vidx(0) fo(1) oP(2) ACC(3) ct(4)  — per FACE, add each corner's NEXT-corner
// vertex position into the corner-vertex's 1-ring accumulator (fixed-point int atomics).
static std::string _relax_accum_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface raf_vi  (descriptor_set 0) { buffer layout(std430) ravb { uint VId[]; }; }
storage_interface raf_fo  (descriptor_set 0) { buffer layout(std430) rafb { uint FOd[]; }; }
storage_interface raf_oP  (descriptor_set 0) { buffer layout(std430) rapb { vec4 oP[]; }; }
storage_interface raf_acc (descriptor_set 0) { buffer layout(std430) raab { int ACC[]; }; }
%PAR%
compute_interface iface { storage { raf_vi raf_fo raf_oP raf_acc dsf_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_relax_accum : iface {
  uint fc = gl_GlobalInvocationID.x;
  if (fc >= p_nfaces) { return; }
  uint o0 = FOd[fc]; uint o1 = FOd[fc + 1u];
  for (uint c = o0; c < o1; c++) {
    uint cn = (c + 1u == o1) ? o0 : (c + 1u);   // next corner (ring-wrap)
    uint v  = VId[c];
    vec3 pn = oP[VId[cn]].xyz;
    atomicAdd(ACC[4u*v + 0u], int(pn.x * 100000.0));
    atomicAdd(ACC[4u*v + 1u], int(pn.y * 100000.0));
    atomicAdd(ACC[4u*v + 2u], int(pn.z * 100000.0));
    atomicAdd(ACC[4u*v + 3u], 1);
  }
}
)S";
  _sub(t);
  return t;
}

// (4) relax apply : oP(0) ACC(1) ct(2)  — SURFACE FAIRING. Move each vertex toward its FULL 1-ring
// centroid (smooths the surface's normal-direction bumps AND evens the distribution). p_lambda is the
// SIGNED Taubin step the host alternates per pass (+lambda shrink / -mu inflate) so the low-pass is
// NON-SHRINKING: high-freq ridging is removed, the gross shape + genuine saddles preserved. NO
// reproject — a full reproject re-snaps every smoothed vertex back onto the bumpy iso-surface (the
// reason the old tangential+reproject relax could even out spacing but never remove SURFACE ridges).
static std::string _relax_apply_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface paf_oP  (descriptor_set 0) { buffer layout(std430) papb { vec4 oP[]; }; }
storage_interface paf_acc (descriptor_set 0) { buffer layout(std430) paab { int ACC[]; }; }
%PAR%
compute_interface iface { storage { paf_oP paf_acc dsf_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_relax_apply : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  int cnt = ACC[4u*v + 3u];
  if (cnt <= 0) { return; }
  vec3 cen = vec3(float(ACC[4u*v+0u]), float(ACC[4u*v+1u]), float(ACC[4u*v+2u])) / (100000.0 * float(cnt));
  vec3 P = oP[v].xyz;
  P = P + p_lambda * (cen - P);              // FULL 1-ring Laplacian (signed Taubin step)
  oP[v] = vec4(P, 1.0);
}
)S";
  _sub(t);
  return t;
}

// (5) normal : oP(0) oN(1) SDF(2) ct(3)  — oN = normalize(grad phi) at the final position.
static std::string _sdf_normal_text() {
  std::string t = R"S(
fxconfig fxcfg_default {}
storage_interface nf_oP  (descriptor_set 0) { buffer layout(std430) nopb { vec4 oP[]; }; }
storage_interface nf_oN  (descriptor_set 0) { buffer layout(std430) nonb { vec4 oN[]; }; }
storage_interface nf_sdf (descriptor_set 0) { buffer layout(std430) nsdb { float SDF[]; }; }
%PAR%
compute_interface iface { storage { nf_oP nf_oN nf_sdf dsf_ct } inputs { layout(local_size_x = 64); } }
compute_shader cs_sdf_normal : iface {
  uint v = gl_GlobalInvocationID.x;
  if (v >= p_nv) { return; }
  vec3 P = oP[v].xyz;
  float phi; vec3 grad;
  %SAMPLE%
  float gl = length(grad);
  oN[v] = vec4((gl > 1.0e-6) ? (grad / gl) : vec3(0.0, 1.0, 0.0), 0.0);
}
)S";
  _sub(t);
  return t;
}

static dflowgfx::sdfgrid_inst_ptr_t _srcSdf(dflowgfx::sdfgrid_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<dflowgfx::sdfgrid_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}

struct DisplaceBySdfInst : public MeshComputeInst {
  DisplaceBySdfInst(const DisplaceBySdfData* d, dflow::GraphInst* g) : MeshComputeInst(d, g), _d(d) {}

  void onLink(dflow::GraphInst*) final {
    _output = typedOutputNamed<MeshPlugTraits>("Out");
    _input  = typedInputNamed<MeshPlugTraits>("In");
    _field  = typedInputNamed<dflowgfx::SdfGridPlugTraits>("Field");
  }

  bool onTopologyReady(Context* ctx) final {
    if (not _srcMesh(_input) or _cs)
      return false;
    if (not _srcSdf(_field)) {
      if (not _warned) {
        printf("DisplaceBySdf<%s>: Field input UNCONNECTED — passing the mesh through\n", _dgmodule_data->_name.c_str());
        _warned = true;
      }
      return false;
    }
    auto fxi  = ctx->FXI();
    _cs       = fxi->computeShader(fxi->shaderFromShaderText("hm_displace_sdf", _displace_sdf_text()), "cs_displace_sdf");
    _cs_clear = fxi->computeShader(fxi->shaderFromShaderText("hm_relax_clear", _relax_clear_text()), "cs_relax_clear");
    _cs_accum = fxi->computeShader(fxi->shaderFromShaderText("hm_relax_accum", _relax_accum_text()), "cs_relax_accum");
    _cs_apply = fxi->computeShader(fxi->shaderFromShaderText("hm_relax_apply", _relax_apply_text()), "cs_relax_apply");
    _cs_norm  = fxi->computeShader(fxi->shaderFromShaderText("hm_sdf_normal", _sdf_normal_text()), "cs_sdf_normal");
    return true;
  }

  void writeParams(Context* ctx) final {
    auto in = _srcMesh(_input);
    if (not in)
      return;
    auto out = _output->_value;
    auto env = _graphinst->_impl.getShared<MeshEnv>();
    auto fxi = ctx->FXI();
    int nv   = in->_num_verts;
    auto fld = _srcSdf(_field);
    if (not _cs or not fld or not fld->_ssbo) { // eval 1 / fieldless: straight passthrough
      out->_channels = in->_channels; out->_faces = in->_faces; out->_vattrs = in->_vattrs;
      out->_vidx = in->_vidx; out->_face_offsets = in->_face_offsets; out->_header = in->_header;
      out->_capacity = in->_capacity; out->_num_verts = in->_num_verts;
      out->_num_corners = in->_num_corners; out->_num_faces = in->_num_faces;
      return;
    }
    if (meshNextPow2(nv) != _cap) {
      _outP = env->_pool->acquireChannel(MeshChannel::POSITION, nv);
      _outN = env->_pool->acquireChannel(MeshChannel::NORMAL, nv);
      _acc  = env->_pool->acquireChannel(16, nv); // int4 fixed-point 1-ring accumulator
      _cap  = meshNextPow2(nv);
    }
    if (not _ct)
      _ct = fxi->createStorageBuffer(64);
    float extent = fld->_voxel * float(fld->_dim[0]);
    struct {
      uint32_t nv, mode, steps, nfaces;
      uint32_t dimx, dimy, dimz, u0;
      float ox, oy, oz, voxel;
      float amount, isolevel, maxstep, lambda;
    } P{uint32_t(nv), uint32_t(_d->_mode), uint32_t(std::max(1, _d->_conform_steps)), uint32_t(in->_num_faces),
        uint32_t(fld->_dim[0]), uint32_t(fld->_dim[1]), uint32_t(fld->_dim[2]), 0u,
        fld->_origin[0], fld->_origin[1], fld->_origin[2], fld->_voxel,
        *(_d->typedInputNamed<dflow::FloatPlugTraits>("amount")->_value),
        *(_d->typedInputNamed<dflow::FloatPlugTraits>("iso_level")->_value),
        extent, *(_d->typedInputNamed<dflow::FloatPlugTraits>("relax_lambda")->_value)};
    auto cp = fxi->mapStorageBuffer(_ct, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(cp->_mappedaddr, &P, sizeof(P));
    fxi->unmapStorageBuffer(cp.get());
    static_assert(sizeof(P) == 64, "ds params must be 64 bytes (lambda at offset 60)");
    std::memcpy(_ctbytes, &P, sizeof(P)); // host copy: the relax loop rewrites only lambda (offset 60) per Taubin pass
    // topology + uv/color pass through; POSITION and NORMAL are produced.
    _outch                        = in->_channels;
    _outch[MeshChannel::POSITION] = _outP;
    _outch[MeshChannel::NORMAL]   = _outN;
    out->_channels     = _outch;
    out->_faces        = in->_faces;
    out->_vattrs       = in->_vattrs;
    out->_vidx         = in->_vidx;
    out->_face_offsets = in->_face_offsets;
    out->_header       = in->_header;
    out->_capacity     = in->_capacity;
    out->_num_verts    = in->_num_verts;
    out->_num_corners  = in->_num_corners;
    out->_num_faces    = in->_num_faces;
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto in  = _srcMesh(_input);
    auto fld = _srcSdf(_field);
    if (not in or not fld or not fld->_ssbo or not _cs)
      return;
    auto env = inst->_impl.getShared<MeshEnv>();
    auto ci  = env->_ctx->CI();
    auto fxi = env->_ctx->FXI();   // for the per-pass Taubin lambda rewrite into _ct
    int nv   = in->_num_verts;
    int nf   = in->_num_faces;
    int vg   = (nv + 63) / 64;
    auto ip  = in->channel(MeshChannel::POSITION)->_ssbo;
    auto inrm = in->channel(MeshChannel::NORMAL)->_ssbo;
    // (1) conform / offset / scalar -> _outP
    ci->bindStorageBuffer(_cs, 0, ip);
    ci->bindStorageBuffer(_cs, 1, inrm);
    ci->bindStorageBuffer(_cs, 2, _outP->_ssbo);
    ci->bindStorageBuffer(_cs, 3, fld->_ssbo);
    ci->bindStorageBuffer(_cs, 4, _ct);
    ci->dispatchCompute(_cs, vg, 1, 1);
    ci->storageBarrier();
    // (2) SURFACE FAIRING (conform mode only) — non-shrinking Taubin low-pass: full 1-ring Laplacian,
    // the step sign ALTERNATED per pass (lam shrink / mu inflate, |mu|>lam) so high-freq ridging is
    // removed while the gross shape + genuine saddles survive. Smooths the SURFACE (not just spacing).
    if (_d->_mode == 0 and _d->_relax_steps > 0 and _acc) {
      int fg     = (nf + 63) / 64;
      float lam  = *(_d->typedInputNamed<dflow::FloatPlugTraits>("relax_lambda")->_value);
      float kpb  = *(_d->typedInputNamed<dflow::FloatPlugTraits>("relax_passband")->_value);
      float mu   = -lam / (1.0f - kpb * lam); // Taubin: kpb = pass-band edge (cutoff) on the Laplacian
                                              // spectrum [0,2]; below kpb preserved, above attenuated.
                                              // LOWER kpb -> cutoff drops -> smooths MORE; higher -> gentler.
      for (int r = 0; r < _d->_relax_steps; r++) {
        ci->bindStorageBuffer(_cs_clear, 0, _acc->_ssbo);
        ci->bindStorageBuffer(_cs_clear, 1, _ct);
        ci->dispatchCompute(_cs_clear, vg, 1, 1);
        ci->storageBarrier();
        ci->bindStorageBuffer(_cs_accum, 0, in->_vidx->_ssbo);
        ci->bindStorageBuffer(_cs_accum, 1, in->_face_offsets->_ssbo);
        ci->bindStorageBuffer(_cs_accum, 2, _outP->_ssbo);
        ci->bindStorageBuffer(_cs_accum, 3, _acc->_ssbo);
        ci->bindStorageBuffer(_cs_accum, 4, _ct);
        ci->dispatchCompute(_cs_accum, fg, 1, 1);
        ci->storageBarrier();
        float step = (r % 2 == 0) ? lam : mu;           // alternate shrink / inflate (Taubin)
        std::memcpy(_ctbytes + 60, &step, 4);           // overwrite only lambda (offset 60)
        auto lp = fxi->mapStorageBuffer(_ct, 0, 64, BufferMapAccess::WRITE_ONLY);
        std::memcpy(lp->_mappedaddr, _ctbytes, 64);
        fxi->unmapStorageBuffer(lp.get());
        ci->bindStorageBuffer(_cs_apply, 0, _outP->_ssbo);
        ci->bindStorageBuffer(_cs_apply, 1, _acc->_ssbo);
        ci->bindStorageBuffer(_cs_apply, 2, _ct);
        ci->dispatchCompute(_cs_apply, vg, 1, 1);
        ci->storageBarrier();
      }
      // restore the baked lambda in _ct so a subsequent re-dispatch without re-running writeParams is clean
      std::memcpy(_ctbytes + 60, &lam, 4);
      auto rp = fxi->mapStorageBuffer(_ct, 0, 64, BufferMapAccess::WRITE_ONLY);
      std::memcpy(rp->_mappedaddr, _ctbytes, 64);
      fxi->unmapStorageBuffer(rp.get());
    }
    // (3) NORMAL = normalize(grad phi) at the final position (exact analytic surface normal)
    ci->bindStorageBuffer(_cs_norm, 0, _outP->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 1, _outN->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 2, fld->_ssbo);
    ci->bindStorageBuffer(_cs_norm, 3, _ct);
    ci->dispatchCompute(_cs_norm, vg, 1, 1);
    ci->storageBarrier();
  }

  const DisplaceBySdfData* _d;
  mesh_outpluginst_ptr_t _output;
  mesh_inpluginst_ptr_t _input;
  dflowgfx::sdfgrid_inpluginst_ptr_t _field;
  std::map<MeshChannel, gpuchannel_ptr_t> _outch;
  gpuchannel_ptr_t _outP, _outN, _acc;
  FxShaderStorageBuffer* _ct = nullptr;
  unsigned char _ctbytes[64]; // host copy of the _ct params (relax loop rewrites lambda @ offset 60 per pass)
  const FxComputeShader *_cs = nullptr, *_cs_clear = nullptr, *_cs_accum = nullptr, *_cs_apply = nullptr, *_cs_norm = nullptr;
  int _cap     = -1;
  bool _warned = false;
};

static void _reshapeDisplaceSdfIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "In");
  dflow::ModuleData::createInputPlug<dflowgfx::SdfGridPlugTraits>(data, dflow::EPR_UNIFORM, "Field");
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "amount")->setValue(1.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "iso_level")->setValue(0.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "relax_lambda")->setValue(0.5f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "relax_passband")->setValue(0.1f);
  dflow::ModuleData::createOutputPlug<MeshPlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
DisplaceBySdfData::DisplaceBySdfData() {
}
std::shared_ptr<DisplaceBySdfData> DisplaceBySdfData::createShared() {
  auto d = std::make_shared<DisplaceBySdfData>();
  _reshapeDisplaceSdfIOs(d);
  return d;
}
dflow::dgmoduleinst_ptr_t DisplaceBySdfData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<DisplaceBySdfInst>(this, g);
}
void DisplaceBySdfData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return DisplaceBySdfData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t m) { _reshapeDisplaceSdfIOs(m); });
  clazz->directProperty("mode", &DisplaceBySdfData::_mode);
  clazz->directProperty("conform_steps", &DisplaceBySdfData::_conform_steps);
  clazz->directProperty("relax_steps", &DisplaceBySdfData::_relax_steps);
}

} // namespace ork::lev2::hypermesh
