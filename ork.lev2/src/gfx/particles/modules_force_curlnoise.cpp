////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// CurlNoiseForce — divergence-free procedural flow field.
//
// The vector potential ψ(p,t) is three independent scalar noise fields
// evaluated at the particle's position. The force we apply is F = ∇ × ψ.
// Curl of any vector field is divergence-free, so particles in this field
// neither converge nor diverge — they swirl. That's what makes it look
// like organic fluid motion, in contrast to additive noise (Turbulence)
// which has random divergence and produces clumpy / streaky motion.
//
// Time evolution: each axis of ψ uses an offset position p + axis_offset
// + t*Speed*axis_dir, so the field rotates / scrolls smoothly over time.
//
// Curl via finite differences (central differences, step = Epsilon):
//   Fx = (ψz(y+ε) - ψz(y-ε))/(2ε) - (ψy(z+ε) - ψy(z-ε))/(2ε)
//   Fy = (ψx(z+ε) - ψx(z-ε))/(2ε) - (ψz(x+ε) - ψz(x-ε))/(2ε)
//   Fz = (ψy(x+ε) - ψy(x-ε))/(2ε) - (ψx(y+ε) - ψx(y-ε))/(2ε)
// → six noise evaluations per particle (two per ψ-axis × three axes;
// the pairs share most computation but we keep the literal form here).

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////
// Hash-based 3D value noise. Cheap, deterministic, ~no allocations.
// Trilinear interpolation between integer-corner samples with a smoothstep
// fade. Output range ≈ [-1, 1]. Good enough for visual swirl; if you want
// gradient noise (simplex/perlin) later, swap this out.
///////////////////////////////////////////////////////////////////////////////

static inline uint32_t _hash3(int x, int y, int z, uint32_t seed) {
  // Simple integer hash — Wang/Marsaglia-flavored, deterministic per
  // (x,y,z,seed). Not crypto-grade; visual-quality only.
  uint32_t h = uint32_t(x) * 73856093u
             ^ uint32_t(y) * 19349663u
             ^ uint32_t(z) * 83492791u
             ^ seed        * 2654435761u;
  h = (h ^ (h >> 16)) * 0x85ebca6bu;
  h = (h ^ (h >> 13)) * 0xc2b2ae35u;
  h = (h ^ (h >> 16));
  return h;
}

static inline float _hash3_unit(int x, int y, int z, uint32_t seed) {
  // Map to [-1, 1].
  return (float(_hash3(x, y, z, seed)) / float(0x7fffffffu)) - 1.0f;
}

static inline float _fade(float t) {
  // Smootherstep (6t^5 - 15t^4 + 10t^3) for C2 continuity.
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline float _noise3(float x, float y, float z, uint32_t seed) {
  int xi = int(std::floor(x)), yi = int(std::floor(y)), zi = int(std::floor(z));
  float xf = x - float(xi), yf = y - float(yi), zf = z - float(zi);
  float u  = _fade(xf), v = _fade(yf), w = _fade(zf);

  float n000 = _hash3_unit(xi,     yi,     zi,     seed);
  float n100 = _hash3_unit(xi + 1, yi,     zi,     seed);
  float n010 = _hash3_unit(xi,     yi + 1, zi,     seed);
  float n110 = _hash3_unit(xi + 1, yi + 1, zi,     seed);
  float n001 = _hash3_unit(xi,     yi,     zi + 1, seed);
  float n101 = _hash3_unit(xi + 1, yi,     zi + 1, seed);
  float n011 = _hash3_unit(xi,     yi + 1, zi + 1, seed);
  float n111 = _hash3_unit(xi + 1, yi + 1, zi + 1, seed);

  float nx00 = n000 * (1.0f - u) + n100 * u;
  float nx10 = n010 * (1.0f - u) + n110 * u;
  float nx01 = n001 * (1.0f - u) + n101 * u;
  float nx11 = n011 * (1.0f - u) + n111 * u;
  float nxy0 = nx00 * (1.0f - v) + nx10 * v;
  float nxy1 = nx01 * (1.0f - v) + nx11 * v;
  return nxy0 * (1.0f - w) + nxy1 * w;
}

// The three components of the vector potential ψ — different seeds give
// three statistically independent scalar noise fields. Each is time-
// scrolled in a different direction so the resulting flow field rotates
// and evolves over time without obvious axis-aligned patterns.
static inline fvec3 _psi(fvec3 p, float t, float speed) {
  float tt = t * speed;
  return fvec3(
    _noise3(p.x,        p.y,        p.z + tt,   /*seed*/ 0x12345678u),
    _noise3(p.x + tt,   p.y,        p.z,        /*seed*/ 0x9abcdef0u),
    _noise3(p.x,        p.y + tt,   p.z,        /*seed*/ 0x5a5a5a5au));
}

///////////////////////////////////////////////////////////////////////////////

struct CurlNoiseForceModuleInst : public ParticleModuleInst {

  CurlNoiseForceModuleInst(const CurlNoiseForceModuleData* d, dataflow::GraphInst* ginst)
      : ParticleModuleInst(d, ginst) {}

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_strength       = typedInputNamed<FloatXfPlugTraits>("Strength");
    _input_frequency      = typedInputNamed<FloatXfPlugTraits>("Frequency");
    _input_speed          = typedInputNamed<FloatXfPlugTraits>("Speed");
    _input_epsilon        = typedInputNamed<FloatXfPlugTraits>("Epsilon");
    _input_cellsize       = typedInputNamed<FloatXfPlugTraits>("CellSize");
    _input_levels         = typedInputNamed<FloatXfPlugTraits>("Levels");
    _input_ripple         = typedInputNamed<FloatXfPlugTraits>("Ripple");
    _input_ripple_freq    = typedInputNamed<FloatXfPlugTraits>("RippleFreq");
    _input_inp_noise      = typedInputNamed<FloatXfPlugTraits>("InpNoise");
    _input_inp_noise_freq = typedInputNamed<FloatXfPlugTraits>("InpNoiseFreq");
    _input_rot            = typedInputNamed<Vec3XfPlugTraits>("Rot");
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    float strength = _input_strength->value();
    float freq     = _input_frequency->value();
    float speed    = _input_speed->value();
    float eps      = _input_epsilon->value();
    float cell     = _input_cellsize->value();
    int   levels   = int(_input_levels->value());
    float ripple_amp  = _input_ripple->value();
    float ripple_freq = _input_ripple_freq->value();
    float inp_amp     = _input_inp_noise->value();
    float inp_freq    = _input_inp_noise_freq->value();
    fvec3 rot         = _input_rot->value();
    if (eps <= 0.0f) eps = 0.05f;            // safety floor
    float inv_2eps = 1.0f / (2.0f * eps);
    float t        = updata->_abstime;
    float dt       = updata->_dt;

    // Precompute rotation trig — cheap per-frame, avoids per-particle redo.
    bool   want_rot = (std::abs(rot.x) + std::abs(rot.y) + std::abs(rot.z)) > 1e-6f;
    float  cx = std::cos(rot.x), sx = std::sin(rot.x);
    float  cy = std::cos(rot.y), sy = std::sin(rot.y);
    float  cz = std::cos(rot.z), sz = std::sin(rot.z);

    // Euler XYZ rotation — same convention as most DCC tools (Maya/Houdini
    // default). Identity when all angles are zero (we skip the math via
    // the want_rot gate so the unwarped fast path isn't penalized).
    auto euler = [&](fvec3 v) -> fvec3 {
      if (not want_rot) return v;
      // Rx
      fvec3 a(v.x, v.y * cx - v.z * sx, v.y * sx + v.z * cx);
      // Ry
      fvec3 b(a.x * cy + a.z * sy, a.y, -a.x * sy + a.z * cy);
      // Rz
      return fvec3(b.x * cz - b.y * sz, b.x * sz + b.y * cz, b.z);
    };

    // Radial sine ripple — displaces p outward along its own direction by
    // sin(|p| * RippleFreq) * Ripple. Concentric "breathing" rings warp
    // the noise field.
    auto ripple = [&](fvec3 v) -> fvec3 {
      if (ripple_amp == 0.0f) return v;
      float d = v.magnitude();
      if (d < 1.0e-6f) return v;
      float disp = std::sin(d * ripple_freq) * ripple_amp;
      return v + v * (disp / d);
    };

    // Domain warping via a separate noise vector. Each component of p
    // gets offset by an independent noise channel evaluated at p*InpFreq.
    // Standard "iq-style" domain warp — produces organic, fractal-shaped
    // distortion of the underlying flow.
    auto warp_noise = [&](fvec3 v) -> fvec3 {
      if (inp_amp == 0.0f) return v;
      fvec3 q = v * inp_freq;
      fvec3 offset(
        _noise3(q.x, q.y, q.z, /*seed*/ 0x11223344u),
        _noise3(q.x, q.y, q.z, /*seed*/ 0x55667788u),
        _noise3(q.x, q.y, q.z, /*seed*/ 0x99aabbccu));
      return v + offset * inp_amp;
    };

    // Position-quantization helper. When cell > 0, snap p to the
    // bottom-left corner of its cell so all in-cell samples (p±ε) land
    // on the SAME cell as long as ε < cell — yielding constant ψ within
    // a cell. ε > cell would re-introduce smoothness; we don't guard
    // against that (author can pick coherent values).
    auto snap = [cell](fvec3 v) -> fvec3 {
      if (cell <= 0.0f) return v;
      return fvec3(
        std::floor(v.x / cell) * cell,
        std::floor(v.y / cell) * cell,
        std::floor(v.z / cell) * cell);
    };

    // Potential-quantization helper. When levels > 0, snap each ψ
    // component to one of N discrete levels in [-1, 1].
    auto quantize_psi = [levels](fvec3 v) -> fvec3 {
      if (levels <= 0) return v;
      float scale = float(levels) * 0.5f;     // map [-1,1] → [-levels/2, levels/2]
      float inv   = 1.0f / scale;
      return fvec3(
        std::floor(v.x * scale) * inv,
        std::floor(v.y * scale) * inv,
        std::floor(v.z * scale) * inv);
    };

    const int n = _pool->GetNumAlive();
    for (int i = 0; i < n; i++) {
      BasicParticle* ptc = _pool->GetActiveParticle(i);
      fvec3 p = ptc->mPosition * freq;
      // Input-space warps: rotation first, then radial ripple, then
      // domain-warp noise. They compose; each is a no-op when its
      // amplitude/angle is zero (the gate inside each helper short-
      // circuits, so the unwarped fast path matches the original cost).
      p = warp_noise(ripple(euler(p)));

      // Central differences on each axis of ψ. Snap each sample
      // position so position-quantization applies, then quantize the
      // sampled ψ value so potential-quantization applies.
      fvec3 psi_xp = quantize_psi(_psi(snap(p + fvec3( eps, 0, 0)), t, speed));
      fvec3 psi_xn = quantize_psi(_psi(snap(p + fvec3(-eps, 0, 0)), t, speed));
      fvec3 psi_yp = quantize_psi(_psi(snap(p + fvec3(0,  eps, 0)), t, speed));
      fvec3 psi_yn = quantize_psi(_psi(snap(p + fvec3(0, -eps, 0)), t, speed));
      fvec3 psi_zp = quantize_psi(_psi(snap(p + fvec3(0, 0,  eps)), t, speed));
      fvec3 psi_zn = quantize_psi(_psi(snap(p + fvec3(0, 0, -eps)), t, speed));

      fvec3 curl;
      curl.x = (psi_yp.z - psi_yn.z) * inv_2eps - (psi_zp.y - psi_zn.y) * inv_2eps;
      curl.y = (psi_zp.x - psi_zn.x) * inv_2eps - (psi_xp.z - psi_xn.z) * inv_2eps;
      curl.z = (psi_xp.y - psi_xn.y) * inv_2eps - (psi_yp.x - psi_yn.x) * inv_2eps;

      ptc->mVelocity += curl * (strength * dt);
    }
  }

  floatxf_inp_pluginst_ptr_t  _input_strength;
  floatxf_inp_pluginst_ptr_t  _input_frequency;
  floatxf_inp_pluginst_ptr_t  _input_speed;
  floatxf_inp_pluginst_ptr_t  _input_epsilon;
  floatxf_inp_pluginst_ptr_t  _input_cellsize;
  floatxf_inp_pluginst_ptr_t  _input_levels;
  floatxf_inp_pluginst_ptr_t  _input_ripple;
  floatxf_inp_pluginst_ptr_t  _input_ripple_freq;
  floatxf_inp_pluginst_ptr_t  _input_inp_noise;
  floatxf_inp_pluginst_ptr_t  _input_inp_noise_freq;
  fvec3xf_inp_pluginst_ptr_t  _input_rot;
};

///////////////////////////////////////////////////////////////////////////////

CurlNoiseForceModuleData::CurlNoiseForceModuleData() {}

static void _reshapeCurlNoiseForceIOs(dataflow::moduledata_ptr_t data) {
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Strength")->_range  = {0.0f, 1000.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Frequency")->_range = {0.001f, 100.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Speed")->_range     = {0.0f, 10.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Epsilon")->_range   = {0.001f, 1.0f};
  // CellSize / Levels: both default 0 (off). When nonzero they make the
  // field piecewise constant → curl spikes at boundaries → discharge look.
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "CellSize")->_range  = {0.0f, 100.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Levels")->_range    = {0.0f, 64.0f};
  // Input-space warps. All default to amplitude 0 (= no warp) so the
  // pipeline matches the original CurlNoise when authors don't touch them.
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Ripple")->_range       = {0.0f, 10.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "RippleFreq")->_range   = {0.0f, 50.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "InpNoise")->_range     = {0.0f, 10.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "InpNoiseFreq")->_range = {0.0f, 50.0f};
  // Rot — Euler XYZ angles (radians). vec3(0) = identity.
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Rot")->_range           = {-100.0f, 100.0f};
}

std::shared_ptr<CurlNoiseForceModuleData> CurlNoiseForceModuleData::createShared() {
  auto data = std::make_shared<CurlNoiseForceModuleData>();
  _initPoolIOs(data);
  _reshapeCurlNoiseForceIOs(data);
  // Reasonable defaults so a default-constructed module produces visible
  // motion if the author binds nothing.
  auto in_strength  = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Strength"));
  auto in_frequency = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Frequency"));
  auto in_speed     = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Speed"));
  auto in_epsilon   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Epsilon"));
  auto in_cell      = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("CellSize"));
  auto in_levels    = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Levels"));
  if (in_strength)  in_strength->setValue(1.0f);
  if (in_frequency) in_frequency->setValue(0.5f);
  if (in_speed)     in_speed->setValue(0.1f);
  if (in_epsilon)   in_epsilon->setValue(0.05f);
  if (in_cell)      in_cell->setValue(0.0f);     // 0 = smooth (default)
  if (in_levels)    in_levels->setValue(0.0f);   // 0 = smooth (default)
  // Warp defaults: amplitudes 0 (off), frequencies 1 (neutral if amp
  // becomes nonzero), rotation identity.
  auto in_ripple        = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Ripple"));
  auto in_ripple_freq   = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("RippleFreq"));
  auto in_inp_noise     = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("InpNoise"));
  auto in_inp_noise_fq  = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("InpNoiseFreq"));
  auto in_rot           = std::dynamic_pointer_cast<inplugdata<Vec3XfPlugTraits>>(data->inputNamed("Rot"));
  if (in_ripple)       in_ripple->setValue(0.0f);
  if (in_ripple_freq)  in_ripple_freq->setValue(1.0f);
  if (in_inp_noise)    in_inp_noise->setValue(0.0f);
  if (in_inp_noise_fq) in_inp_noise_fq->setValue(1.0f);
  if (in_rot)          in_rot->setValue(fvec3(0, 0, 0));
  return data;
}

dgmoduleinst_ptr_t CurlNoiseForceModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<CurlNoiseForceModuleInst>(this, ginst);
}

void CurlNoiseForceModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return CurlNoiseForceModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapeCurlNoiseForceIOs(mdata);
  });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::CurlNoiseForceModuleData, "psys::CurlNoiseForceModuleData");
