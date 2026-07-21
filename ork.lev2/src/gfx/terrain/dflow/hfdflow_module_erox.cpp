////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::EroxModuleData, "terrain::EroxModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// EroxModule — PHYSICAL hydraulic erosion (Mei et al. 2007 virtual-pipes), in
// METERS / SECONDS so the bake is RESOLUTION-INDEPENDENT. Unlike the texel droplet:
//   - cell_size_m = extent_m/dim bridges grid<->world; heights are NATURAL UNITS
//     (meters), so real slope = dH_m/cell_size_m (no vertical scale).
//   - the timestep dt is CFL-derived (dt = CFL*cell/flow_speed_max), so iterations =
//     ceil(sim_time_s/dt) scale with dim to hold the SAME physical time + diffusion.
//   - every RATE (rain/evap/erode/deposit) is multiplied by dt, so more-iterations-at-
//     higher-dim does not change the result -> RI. Physical params hashed dim-FREE.
// Internal field units: terr, water, sed all in METERS; velocity in m/s; flux in
// m^3/s. 4 passes/iter, each its own submit (the
// one-descriptor-set-per-pipeline rule). Materials-ready: terr is the total column,
// sed is suspended load, and the erosion strength is an isolated per-cell-READY scalar.
///////////////////////////////////////////////////////////////////////////////

// The physical scalars live in a PARAMS SSBO (si_p, float P[16]) read at runtime — NOT
// baked into the text. The grid dim is ALSO runtime now (P[13]) and the storage arrays are
// runtime-sized, so the shader text is dim-INDEPENDENT; the disk shader cache (DataBlockCache,
// keyed by text hash) hits across all param values AND dims AND runs, and changing an erosion
// knob or resolution no longer recompiles. P layout (filled by _fillParams):
//   0 DT  1 CELL  2 (reserved)  3 KFLUX  4 CELLAREA  5 RAIN  6 EVAP  7 KC  8 KSDT  9 KDDT
//  10 VMAX  11 KDIFF  12 SEDDIFF  13 DIM
static std::string _erox_text(const char* name, const char* sifaces, const char* siflist,
                              const char* body) {
  // dim is RUNTIME (params SSBO P[13]); the arrays are runtime-sized and the text no longer
  // carries dim, so one compile serves every resolution.
  return std::string("\nfxconfig fxcfg_default {}\n") + sifaces +
    "storage_interface si_p (descriptor_set 0) { buffer layout(std430) pb { float P[16]; }; }\n"
    "compute_interface iface { storage { " + siflist + " si_p } inputs { layout(local_size_x = 8, local_size_y = 8, local_size_z = 1); } }\n"
    "compute_shader " + name + " : iface {\n"
    "  uint u_dim = uint(P[13]);\n"
    "  if (gl_GlobalInvocationID.x >= u_dim || gl_GlobalInvocationID.y >= u_dim) { return; }\n"
    "  int  xi = int(gl_GlobalInvocationID.x);\n"
    "  int  yi = int(gl_GlobalInvocationID.y);\n"
    "  int  W  = int(u_dim);\n"
    "  uint i  = uint(yi) * u_dim + uint(xi);\n"
    "  float DT=P[0]; float CELL=P[1]; float KFLUX=P[3]; float CELLAREA=P[4];\n"
    "  float RAIN=P[5]; float EVAP=P[6]; float KC=P[7]; float KSDT=P[8]; float KDDT=P[9];\n"
    "  float VMAX=P[10]; float KDIFF=P[11]; float SEDDIFF=P[12];\n"
    + body + "\n}\n";
}

struct EroxModuleInst : public TerrainComputeInst {
  EroxModuleInst(const EroxModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output  = typedOutputNamed<HfImagePlugTraits>("Out");
    _input   = typedInputNamed<HfImagePlugTraits>("In");
    _simTime = _floatPlug(this, _d, "sim_time_s");
    _rain    = _floatPlug(this, _d, "rain_mps");
    _evap    = _floatPlug(this, _d, "evaporation_per_s");
    _vmax    = _floatPlug(this, _d, "flow_speed_max_mps");
    _cap     = _floatPlug(this, _d, "capacity_Kc");
    _eros    = _floatPlug(this, _d, "erosion_rate_per_s");
    _depo    = _floatPlug(this, _d, "deposition_rate_per_s");
    _creep   = _floatPlug(this, _d, "creep_m2ps");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    _allocOut(env.get(), _output->_value); // terr A (= output)
    size_t nf = size_t(dim) * size_t(dim);
    _terrB = env->createStorageBuffer(nf * sizeof(float));
    _water = env->createStorageBuffer(nf * sizeof(float));
    _sedA  = env->createStorageBuffer(nf * sizeof(float));
    _sedB  = env->createStorageBuffer(nf * sizeof(float));
    _flux  = env->createStorageBuffer(nf * 4 * sizeof(float));
    _vel   = env->createStorageBuffer(nf * 2 * sizeof(float));
    _params= env->createStorageBuffer(16 * sizeof(float)); // physical scalars, filled per-compute

    // shaders are now param- AND dim-INDEPENDENT -> compiled once / disk-cache hits across
    // param tweaks and resolutions; the physical scalars + dim are uploaded to _params.
    auto ET = [&](const char* nm, const char* ifc, const char* names, const char* body) {
      return fxi->computeShader(fxi->shaderFromShaderText(nm, _erox_text(nm, ifc, names, body)), nm);
    };
    _fillParams(env.get()); // fill the params SSBO here (pre-dispatch-phase: a host map mid-phase is not visible)
    // --- INIT: terr=In, water/sed/flux=0. binds 0 terr(w) 1 in(r) 2 water(w) 3 sed(w) 4 flux(w)
    {
      const char* ifc =
        "storage_interface si_t (descriptor_set 0) { buffer layout(std430) tb { float terr[]; }; }\n"
        "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float idata[]; }; }\n"
        "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float water[]; }; }\n"
        "storage_interface si_s (descriptor_set 0) { buffer layout(std430) sb { float sed[]; }; }\n"
        "storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float flux[]; }; }\n";
      const char* body =
        "  terr[i] = idata[i];\n  water[i] = 0.0;\n  sed[i] = 0.0;\n"
        "  flux[4u*i+0u]=0.0; flux[4u*i+1u]=0.0; flux[4u*i+2u]=0.0; flux[4u*i+3u]=0.0;";
      _csInit = ET("cs_erox_init", ifc, "si_t si_i si_w si_s si_f", body);
    }
    // --- FLUX: outflow accel from PHYSICAL head (terr_m + water_m), clamped to available
    //     water. binds 0 flux(rw) 1 terr(r) 2 water(r)
    {
      const char* ifc =
        "storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float flux[]; }; }\n"
        "storage_interface si_t (descriptor_set 0) { buffer layout(std430) tb { float terr[]; }; }\n"
        "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float water[]; }; }\n";
      const char* body =
        "  float Hc = terr[i] + water[i];\n"                                     // head in meters (terr is meters)
        // OPEN boundary: off-map is DRY ground at the local bed (terr, NO water), so the
        // edge head difference equals water[i] -> standing water DRAINS off the map. The old
        // ':Hc' (zero gradient) trapped ALL water in the domain -> it filled to a uniform
        // evaporation-balanced sheet (~rain/evap) and could only sheet-flow, never channelize.
        "  float HL = (xi>0)   ? (terr[i-1u]+water[i-1u]) : terr[i];\n"
        "  float HR = (xi<W-1) ? (terr[i+1u]+water[i+1u]) : terr[i];\n"
        "  float HD = (yi>0)   ? (terr[i-uint(W)]+water[i-uint(W)]) : terr[i];\n"
        "  float HU = (yi<W-1) ? (terr[i+uint(W)]+water[i+uint(W)]) : terr[i];\n"
        "  float fL = max(0.0, flux[4u*i+0u] + KFLUX*(Hc-HL));\n"
        "  float fR = max(0.0, flux[4u*i+1u] + KFLUX*(Hc-HR));\n"
        "  float fD = max(0.0, flux[4u*i+2u] + KFLUX*(Hc-HD));\n"
        "  float fU = max(0.0, flux[4u*i+3u] + KFLUX*(Hc-HU));\n"
        "  float totalout = (fL+fR+fD+fU)*DT;\n"                                 // m^3 leaving this step
        "  float avail = water[i]*CELLAREA;\n"                                   // m^3 available
        "  float K = (totalout>1e-12) ? min(1.0, avail/totalout) : 1.0;\n"
        "  flux[4u*i+0u]=fL*K; flux[4u*i+1u]=fR*K; flux[4u*i+2u]=fD*K; flux[4u*i+3u]=fU*K;";
      _csFlux = ET("cs_erox_flux", ifc, "si_f si_t si_w", body);
    }
    // --- WATER+ERODE: update depth from flux divergence, derive PHYSICAL velocity + slope,
    //     capacity C=KC*sin(slope)*|v|, erode (C>s) or deposit (C<s), add rain, evaporate.
    //     binds 0 terrOut(w) 1 terrIn(r) 2 water(rw) 3 flux(r) 4 sed(rw) 5 vel(w)
    {
      const char* ifc =
        "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float terr_o[]; }; }\n"
        "storage_interface si_t (descriptor_set 0) { buffer layout(std430) tb { float terr_i[]; }; }\n"
        "storage_interface si_w (descriptor_set 0) { buffer layout(std430) wb { float water[]; }; }\n"
        "storage_interface si_f (descriptor_set 0) { buffer layout(std430) fb { float flux[]; }; }\n"
        "storage_interface si_s (descriptor_set 0) { buffer layout(std430) sb { float sed[]; }; }\n"
        "storage_interface si_v (descriptor_set 0) { buffer layout(std430) vb { float vel[]; }; }\n";
      const char* body =
        "  float b = terr_i[i];\n  float d = water[i] + RAIN;\n"
        "  float inL = (xi>0)   ? flux[4u*(i-1u)+1u] : 0.0;\n"        // left's R
        "  float inR = (xi<W-1) ? flux[4u*(i+1u)+0u] : 0.0;\n"        // right's L
        "  float inD = (yi>0)   ? flux[4u*(i-uint(W))+3u] : 0.0;\n"   // down's U
        "  float inU = (yi<W-1) ? flux[4u*(i+uint(W))+2u] : 0.0;\n"   // up's D
        "  float fL=flux[4u*i+0u], fR=flux[4u*i+1u], fD=flux[4u*i+2u], fU=flux[4u*i+3u];\n"
        "  float outflow = fL+fR+fD+fU;\n"
        "  float dnew = d + DT*((inL+inR+inD+inU) - outflow)/CELLAREA;\n"        // meters
        "  if (dnew < 0.0) dnew = 0.0;\n"
        "  float dWx = ((inL - fL) + (fR - inR))*0.5;\n"                          // net flow m^3/s
        "  float dWy = ((inD - fD) + (fU - inU))*0.5;\n"
        "  float davg = max((d+dnew)*0.5, 1e-2);\n"                              // mean depth (m); floor avoids dry-cell spikes
        "  float u = dWx/(CELL*davg);\n  float v = dWy/(CELL*davg);\n"          // m/s
        "  float vmag = sqrt(u*u+v*v);\n"
        "  if (vmag > VMAX) { float sc = VMAX/vmag; u *= sc; v *= sc; vmag = VMAX; }\n" // cap to the CFL flow_speed_max
        "  float bL=(xi>0)?terr_i[i-1u]:b, bR=(xi<W-1)?terr_i[i+1u]:b;\n"
        "  float bD=(yi>0)?terr_i[i-uint(W)]:b, bU=(yi<W-1)?terr_i[i+uint(W)]:b;\n"
        "  float gx=(bR-bL)/(2.0*CELL), gy=(bU-bD)/(2.0*CELL);\n"                // real slope rise_m/run_m (terr is meters)
        "  float slope = sqrt(gx*gx+gy*gy);\n"
        "  float sina = slope/sqrt(slope*slope+1.0);\n"
        "  sina = max(sina, 1e-3);\n"
        "  float C = KC*sina*vmag;\n"                                            // capacity (m)
        "  float s = sed[i];\n  float bnew=b, snew=s;\n"
        "  float Kerod = KSDT;\n"                                                // MATERIALS HOOK (M-C: per-cell erodibility*dt)
        "  if (C > s) { float amt = Kerod*(C-s);        bnew = b - amt; snew = s + amt; }\n" // erode (terr is meters)
        "  else       { float amt = min(KDDT*(s-C), s); bnew = b + amt; snew = s - amt; }\n" // deposit (<= available)
        "  bnew += KDIFF*(bL + bR + bD + bU - 4.0*b);\n"  // creep/diffusion: kills the Nyquist checkerboard, hillslope creep
        "  terr_o[i] = bnew;\n  water[i] = dnew*(1.0 - EVAP);\n  sed[i] = max(snew, 0.0);\n"
        "  vel[2u*i+0u]=u; vel[2u*i+1u]=v;";
      _csWater = ET("cs_erox_water", ifc, "si_o si_t si_w si_f si_s si_v", body);
    }
    // --- TRANSPORT: semi-Lagrangian advect sediment by PHYSICAL velocity (v*dt/cell texels;
    //     no ADV hack). binds 0 sedOut(w) 1 sedIn(r) 2 vel(r)
    {
      const char* ifc =
        "storage_interface si_o (descriptor_set 0) { buffer layout(std430) ob { float sed_o[]; }; }\n"
        "storage_interface si_i (descriptor_set 0) { buffer layout(std430) ib { float sed_i[]; }; }\n"
        "storage_interface si_v (descriptor_set 0) { buffer layout(std430) vb { float vel[]; }; }\n";
      const char* body =
        "  float u=vel[2u*i+0u], v=vel[2u*i+1u];\n"
        "  float sx = clamp(float(xi) - u*DT/CELL, 0.0, float(W-1));\n"           // backtrace u*dt meters -> texels
        "  float sy = clamp(float(yi) - v*DT/CELL, 0.0, float(W-1));\n"
        "  int x0=int(floor(sx)), y0=int(floor(sy));\n"
        "  int x1=min(x0+1,W-1), y1=min(y0+1,W-1);\n"
        "  float fx=sx-float(x0), fy=sy-float(y0);\n"
        "  float s00=sed_i[uint(y0*W+x0)], s10=sed_i[uint(y0*W+x1)];\n"
        "  float s01=sed_i[uint(y1*W+x0)], s11=sed_i[uint(y1*W+x1)];\n"
        "  sed_o[i] = mix(mix(s00,s10,fx), mix(s01,s11,fx), fy);";
      _csXport = ET("cs_erox_xport", ifc, "si_o si_i si_v", body);
    }
  }
  // derive the physical per-step scalars from the meters model + plug values and upload them
  // to the params SSBO. Reads the PLUGS at compute() time (not bakeAcquire), so a sweep can
  // vary params and re-run compute() without recompiling. Also sets _iterations.
  void _fillParams(BakeEnv* env) {
    int dim = env->_w;
    const float g = 9.81f, cfl = 0.5f;                        // gravity-wave CFL safety
    float cell = (dim > 0) ? (env->_extent_m / float(dim)) : 1.0f; // cell_size_m
    float vmax = std::max(_vmax->value(), 0.01f);
    float dt   = cfl * cell / vmax;                           // CFL-bounded timestep (s)
    _iterations = std::max(1, int(std::ceil(_simTime->value() / dt)));
    float P[16] = {0};
    P[0]  = dt;
    P[1]  = cell;
    P[2]  = 0.0f;                                             // reserved (was HS; heights are meters -> no vertical scale)
    P[3]  = dt * cell * g;                                    // KFLUX = dt*A*g/L, A=cell^2 L=cell
    P[4]  = cell * cell;                                      // CELLAREA
    P[5]  = _rain->value() * dt;                              // rain meters/step
    P[6]  = std::min(_evap->value() * dt, 1.0f);              // evaporated fraction/step
    P[7]  = _cap->value();                                    // KC
    P[8]  = _eros->value() * dt;                              // KSDT (erosion rate * dt)
    P[9]  = _depo->value() * dt;                              // KDDT (deposition rate * dt)
    P[10] = vmax;
    P[11] = std::min(_creep->value() * dt / (cell * cell), 0.2f);        // KDIFF hillslope creep
    P[12] = std::min(2.0f * _creep->value() * dt / (cell * cell), 0.1f); // SEDDIFF sediment mixing
    P[13] = float(dim);                                                  // grid dim (runtime, was baked into text)
    auto fxi = env->_ctx->FXI();
    auto m   = fxi->mapStorageBuffer(_params, 0, sizeof(P), BufferMapAccess::WRITE_ONLY);
    std::memcpy(m->_mappedaddr, P, sizeof(P));
    fxi->unmapStorageBuffer(m.get());
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto ci  = env->_ctx->CI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int g = (env->_w + 7) / 8;
    FxShaderStorageBuffer* terr[2] = {_output->_value->_ssbo, _terrB};
    FxShaderStorageBuffer* sed[2]  = {_sedA, _sedB};
    auto submitNext = [&]() { ci->endDispatchPhase(); ci->beginDispatchPhase(); }; // descriptor-set gotcha
    // init (driver already opened the first phase)
    ci->bindStorageBuffer(_csInit, 0, terr[0]);
    ci->bindStorageBuffer(_csInit, 1, in->_ssbo);
    ci->bindStorageBuffer(_csInit, 2, _water);
    ci->bindStorageBuffer(_csInit, 3, sed[0]);
    ci->bindStorageBuffer(_csInit, 4, _flux);
    ci->bindStorageBuffer(_csInit, 5, _params);
    ci->dispatchCompute(_csInit, g, g, 1);
    int tc = 0, sc = 0;
    for (int it = 0; it < _iterations; it++) {
      submitNext(); // pass 1: flux (in place)
      ci->bindStorageBuffer(_csFlux, 0, _flux);
      ci->bindStorageBuffer(_csFlux, 1, terr[tc]);
      ci->bindStorageBuffer(_csFlux, 2, _water);
      ci->bindStorageBuffer(_csFlux, 3, _params);
      ci->dispatchCompute(_csFlux, g, g, 1);
      submitNext(); // pass 2: water + erode/deposit (terr ping-pong, sed in place, vel out)
      ci->bindStorageBuffer(_csWater, 0, terr[1 - tc]);
      ci->bindStorageBuffer(_csWater, 1, terr[tc]);
      ci->bindStorageBuffer(_csWater, 2, _water);
      ci->bindStorageBuffer(_csWater, 3, _flux);
      ci->bindStorageBuffer(_csWater, 4, sed[sc]);
      ci->bindStorageBuffer(_csWater, 5, _vel);
      ci->bindStorageBuffer(_csWater, 6, _params);
      ci->dispatchCompute(_csWater, g, g, 1);
      tc = 1 - tc;
      submitNext(); // pass 3: sediment transport (sed ping-pong)
      ci->bindStorageBuffer(_csXport, 0, sed[1 - sc]);
      ci->bindStorageBuffer(_csXport, 1, sed[sc]);
      ci->bindStorageBuffer(_csXport, 2, _vel);
      ci->bindStorageBuffer(_csXport, 3, _params);
      ci->dispatchCompute(_csXport, g, g, 1);
      sc = 1 - sc;
    }
    _output->_value->_ssbo = terr[tc]; // eroded terrain
  }
  // a full CFL-stepped virtual-pipes sim (hundreds of dispatches per node) — recompute
  // dwarfs a blob load, same class of cost as ThermalErode/Flow3D.
  bool cookCacheDefault() const final { return true; }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.erox.v9"); // v9: heights in meters (dropped HS scale + per-op exag plug); v8: OPEN boundaries
    // hash the PHYSICAL params (dim-free identity); the cook CONTEXT hash carries
    // (dim, extent_m), so two resolutions share node id, differ in context.
    h->accumulateItem<float>(_simTime->value());
    h->accumulateItem<float>(_rain->value());
    h->accumulateItem<float>(_evap->value());
    h->accumulateItem<float>(_vmax->value());
    h->accumulateItem<float>(_cap->value());
    h->accumulateItem<float>(_eros->value());
    h->accumulateItem<float>(_depo->value());
    h->accumulateItem<float>(_creep->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const EroxModuleData* _d;
  hfimg_outpluginst_ptr_t _output;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _simTime, _rain, _evap, _vmax, _cap, _eros, _depo, _creep;
  FxShaderStorageBuffer *_terrB = nullptr, *_water = nullptr, *_sedA = nullptr, *_sedB = nullptr, *_flux = nullptr, *_vel = nullptr;
  FxShaderStorageBuffer *_params = nullptr; // 16 physical scalars, uploaded per-compute (no recompile on tweak)
  const FxComputeShader *_csInit = nullptr, *_csFlux = nullptr, *_csWater = nullptr, *_csXport = nullptr;
  int _iterations = 1; // derived per-bake (sim_time_s / CFL dt), NOT stored/serialized
};

static void _reshapeEroxIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // all PHYSICAL (meters / seconds); iterations + dt are DERIVED per-bake, never stored.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "sim_time_s")->setValue(60.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "rain_mps")->setValue(0.05f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "evaporation_per_s")->setValue(0.05f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "flow_speed_max_mps")->setValue(8.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "capacity_Kc")->setValue(1.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "erosion_rate_per_s")->setValue(1.0f);
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "deposition_rate_per_s")->setValue(1.0f);
  // hillslope creep / numerical diffusion (m^2/s); also the anti-checkerboard stabilizer.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "creep_m2ps")->setValue(4.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
}
EroxModuleData::EroxModuleData() {}
std::shared_ptr<EroxModuleData> EroxModuleData::createShared() {
  auto d = std::make_shared<EroxModuleData>(); _reshapeEroxIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t EroxModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<EroxModuleInst>(this, g);
}
void EroxModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return EroxModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeEroxIOs(m); });
  // E1-close add-palette (reflection-carried; see hfdflow_module_thermal.cpp for the vocabulary).
  clazz->annotateTyped<ConstString>("dsl.verb", "erox");
  clazz->annotateTyped<bool>("editor.palette", true);
  clazz->annotateTyped<int>("editor.palette.sort", 1);
  // no baked scalar property — iterations are derived per-bake; all tunables are plugs.
}

} // namespace ork::lev2::terrain
