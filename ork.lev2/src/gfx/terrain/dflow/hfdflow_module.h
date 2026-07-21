////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hfdflow_module.h — shared contract for the terrain compute modules. Each module's
// implementation lives in its own hfdflow_module_<name>.cpp and includes this. The
// common stuff (TerrainModuleData, the Capture sink, the bake driver, the self-tests)
// stays in hfdflow.cpp.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h>
#include <ork/lev2/gfx/image.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
#include <ork/kernel/string/string.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/kernel/datacache.h> // DataBlockCache — per-node cook cache

namespace ork::lev2::terrain {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// TerrainComputeInst — shared base for the GPU compute ops (everything but the
// Capture sink). Holds the output image plug `_output` and implements the cook-
// cache hooks: a node's output is the W*H float field in its SSBO, (de)serialized
// to a datablock. The bake driver syncs per op, so cookStore reads valid data.
// cookLoad uploads a cached field so downstream ops consume it without recompute.
///////////////////////////////////////////////////////////////////////////////

struct TerrainComputeInst : public dflow::DgModuleInst, public dflowgfx::IPrePhaseParams {
  TerrainComputeInst(const dflow::DgModuleData* d, dflow::GraphInst* g)
      : dflow::DgModuleInst(d, g) {
  }
  // IPrePhaseParams — REALTIME-PARAMS direction: a module whose params live in an SSBO
  // overrides this to re-read its plugs (+ the env clock) every live eval, making it
  // live-editable (editor pokes land next frame, no recompile). Default no-op; the bake
  // driver also calls it pre-phase (uniform mechanism — t=0 makes it equal onActivate).
  void writeParams(Context* ctx) override {}

  // WS4 FRONTIER: a module's GPU buffer acquisition (plug outputs + internal
  // scratch) lives HERE, not in onActivate. Under a legacy/live driver the
  // onActivate shim below invokes it at activate time — identical behavior to the
  // old code. The per-op-synced bake driver sets env->_lazy_acquire and calls
  // bakeAcquire itself just before the node runs (and only if the demand plan
  // says the node runs at all), so a bake's peak is the live frontier, not the
  // whole graph. CONTRACT: after the bake driver releases a node's scratch back
  // to the pool, the module's scratch members dangle — safe for a bake (each
  // node runs exactly once); live graphs never release, so their members stay
  // valid across per-frame computes.
  virtual void bakeAcquire(dflow::GraphInst* inst) {}

  void onActivate(dflow::GraphInst* inst) override {
    auto env = inst->_impl.getShared<BakeEnv>();
    if (not(env and env->_lazy_acquire))
      bakeAcquire(inst);
  }
  // the node's output field (resolved by name so we don't shadow each derived
  // inst's own _output member).
  gpucomputeimage2d_inst_ptr_t _outImg() const {
    auto self = const_cast<TerrainComputeInst*>(this);
    auto outp = self->typedOutputNamed<HfImagePlugTraits>("Out");
    return outp ? outp->_value : nullptr;
  }

  // cook-cache format magic. Bump if the on-disk layout below changes; also makes any pre-existing
  // (single-output, mono) cache entries self-invalidate (their first int won't equal this) -> recompute.
  // v4 (WS4 fp16 wave): per-output PRECISION byte after ch; prec==1 -> the plane is stored as
  // fp16 halves on disk AND was quantized to the fp16 grid IN THE LIVE SSBO at production
  // (cookStore writes the quantized values back), so a warm load is BIT-IDENTICAL to the cold
  // cook that produced it — quantize-at-production keeps the cache contract exact.
  static constexpr int kCookFmt = 0x7c0d0004;

  // WS4 fp16 wave: which outputs quantize/store at fp16. OPT-IN by output name; default
  // fp32. Owner rules: heights stay fp32 (any plane feeding the height chain); relaxed-uv
  // planes stay fp32 (fp16 uv = ~2 atlas texels of error at 4096 — the frame-pack analysis).
  // A module that opts an output in MUST bump its cookComputeHash version salt — the
  // quantization changes the output, so downstream Merkle keys must change with it.
  virtual bool cookHalfOutput(const std::string& output_name) const { return false; }

  // STRATEGIC CACHE POINTS (measured cost model): only classes whose recompute beats a
  // blob load (~0.1s NVMe+upload) default to cook-caching — Flow3D, FillClosedBasins,
  // BasinFill, RelaxUv, ThermalErode per the measured cost model. Everything else
  // recomputes on warm bakes (the demand planner resumes per fork from the deepest
  // clean cached cut — no planner changes). Per-node DSL override: the reflected
  // "cachepoint" property on DgModuleData (-1 class default / 0 never / 1 always).
  // ORKID_COOK_CACHE_ALL=1 restores cache-everything (cross-machine blob audits).
  virtual bool cookCacheDefault() const { return false; }
  bool cookIsCachePoint() const {
    static const bool s_cache_all = (getenv("ORKID_COOK_CACHE_ALL") != nullptr);
    if (s_cache_all)
      return true;
    int ov = _dgmodule_data->_cachepoint;
    return (ov >= 0) ? (ov != 0) : cookCacheDefault();
  }

  // S4 VIEWABLE-vs-INTERNAL tagging (the cachepoint pattern's progressive-display
  // sibling): a VIEWABLE node's completed "Out" plane is a checkpoint-publish
  // candidate during an on_checkpoint cook (heights sweeping through the chain).
  // Class default TRUE — most terrain ops transform the height plane itself; the
  // mask/analysis generators (Slope, Curvature) override false (publishing a [0,1]
  // mask as the display heights would flash garbage). Per-node reflected override:
  // DgModuleData::_viewable (-1 class default / 0 internal / 1 viewable). The
  // publish site ALSO requires a mono plane at the bake dims, so RGBA outputs
  // (RelaxUv, Flow3D "Out") self-exclude regardless of tagging.
  virtual bool viewableDefault() const { return true; }
  bool isViewable() const {
    int ov = _dgmodule_data->_viewable;
    return (ov >= 0) ? (ov != 0) : viewableDefault();
  }

  // fp32<->fp16 bit converters (same semantics as image_fmt_convert / pack_frame5).
  static uint16_t _f32tof16(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exp32 = int32_t((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = (bits & 0x007FFFFF);
    if (exp32 <= 0) return uint16_t(sign);           // underflow to zero
    if (exp32 >= 31) return uint16_t(sign | 0x7C00); // overflow to inf
    return uint16_t(sign | (uint32_t(exp32) << 10) | (mant >> 13));
  }
  static float _f16tof32(uint16_t h) {
    uint32_t sign = (uint32_t(h) & 0x8000u) << 16;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    uint32_t bits;
    if (exp == 0) {
      bits = sign; // zero (denorms flushed at pack time)
    } else if (exp == 31) {
      bits = sign | 0x7F800000u | (mant << 13); // inf/nan
    } else {
      bits = sign | ((exp - 15 + 127) << 23) | (mant << 13);
    }
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
  }

  // fp32 scratch reused across nodes (grow-once, thread-local): the fp16 paths need a
  // host-side fp32 image of the plane; a per-node malloc re-pays ~63MB of first-touch
  // page faults on every node of a cold cook.
  static std::vector<float>& _cookScratch() {
    static thread_local std::vector<float> s;
    return s;
  }

  // Cache ALL output plugs at their TRUE size (w*h*channels floats). The original version cached only
  // the "Out" plug at mono size — which silently corrupted any multi-output and/or multi-channel (RGBA)
  // module (e.g. flow3d) on a cache HIT: the extra outputs were never restored (compute is skipped) and
  // an RGBA "Out" got only its first 1/4 back.
  datablock_ptr_t cookStore() const final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    auto db  = std::make_shared<DataBlock>();
    int nout = numOutputs();
    // EXACT pre-size (kills the append-growth realloc/memmove chain), then each
    // plane is read back ONCE, straight into its final resting place in the
    // datablock (readStorageBuffer: no per-plane map temp, no second memcpy).
    size_t total = 2 * sizeof(int);
    for (int o = 0; o < nout; o++) {
      auto op  = std::dynamic_pointer_cast<hfimg_outpluginst_t>(output(o));
      auto img = op ? op->_value : nullptr;
      if (not(img and img->_ssbo)) { total += sizeof(int); continue; }
      int ch     = (img->_channels < 1) ? 1 : img->_channels;
      size_t cnt = size_t(img->_w) * size_t(img->_h) * size_t(ch);
      bool half  = cookHalfOutput(op->_plugdata->_name);
      total += 5 * sizeof(int) + cnt * (half ? sizeof(uint16_t) : sizeof(float));
    }
    db->reserve(total);
    db->addItem<int>(kCookFmt);
    db->addItem<int>(nout);
    for (int o = 0; o < nout; o++) {
      auto op  = std::dynamic_pointer_cast<hfimg_outpluginst_t>(output(o));
      auto img = op ? op->_value : nullptr;
      if (not(img and img->_ssbo)) { db->addItem<int>(0); continue; } // absent -> marker 0
      int ch     = (img->_channels < 1) ? 1 : img->_channels;
      size_t cnt = size_t(img->_w) * size_t(img->_h) * size_t(ch);
      bool half  = cookHalfOutput(op->_plugdata->_name);
      db->addItem<int>(1);
      db->addItem<int>(img->_w);
      db->addItem<int>(img->_h);
      db->addItem<int>(ch);
      db->addItem<int>(half ? 1 : 0); // precision (v4)
      if (not half) {
        void* dst = db->allocateBlock(cnt * sizeof(float));
        fxi->readStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), dst);
      } else {
        // QUANTIZE AT PRODUCTION: halves to disk, and the quantized-expanded values
        // written BACK to the live SSBO — downstream consumers and captures see
        // exactly what a warm load will upload (cache contract stays bit-exact).
        auto& scratch = _cookScratch();
        if (scratch.size() < cnt)
          scratch.resize(cnt);
        fxi->readStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), scratch.data());
        auto* hd = (uint16_t*)db->allocateBlock(cnt * sizeof(uint16_t));
        for (size_t i = 0; i < cnt; i++) {
          hd[i]      = _f32tof16(scratch[i]);
          scratch[i] = _f16tof32(hd[i]); // rounded, in place
        }
        fxi->writeStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), scratch.data());
      }
    }
    return db;
  }

  bool cookLoad(datablock_constptr_t db) final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    DataBlockInputStream istr(db);
    if (istr.getItem<int>() != kCookFmt)   return false; // old/foreign format -> recompute
    int nout = istr.getItem<int>();
    if (nout != numOutputs())              return false;
    for (int o = 0; o < nout; o++) {
      int present = istr.getItem<int>();
      if (not present) continue;
      int w    = istr.getItem<int>();
      int h    = istr.getItem<int>();
      int ch   = istr.getItem<int>();
      int prec = istr.getItem<int>(); // v4: 0=f32, 1=f16
      auto op  = std::dynamic_pointer_cast<hfimg_outpluginst_t>(output(o));
      auto img = op ? op->_value : nullptr;
      if (not(img and img->_ssbo)) return false;
      int curch = (img->_channels < 1) ? 1 : img->_channels;
      if (w != img->_w or h != img->_h or ch != curch) return false; // dim/channels changed -> recompute
      size_t cnt = size_t(w) * size_t(h) * size_t(ch);
      if (prec == 0) {
        // upload straight from the datablock's memory — no map temp, no extra copy
        fxi->writeStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), istr.current());
        istr.advance(cnt * sizeof(float));
      } else {
        const uint16_t* halves = (const uint16_t*)istr.current();
        auto& scratch          = _cookScratch();
        if (scratch.size() < cnt)
          scratch.resize(cnt);
        for (size_t i = 0; i < cnt; i++)
          scratch[i] = _f16tof32(halves[i]);
        fxi->writeStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), scratch.data());
        istr.advance(cnt * sizeof(uint16_t));
      }
    }
    return true;
  }

  // WS4 FRONTIER: validate a cache entry BEFORE any buffer exists (the demand plan
  // classifies hit/miss up front; bakeAcquire runs only for nodes that run).
  // PREFIX-SAFE (RSS fix): the caller passes only the first ~256 bytes of the entry
  // (DataBlockCache::findDataBlockPrefix) — pulling whole 64MB planes off disk to
  // inspect headers made warm-bake planning read ~33GB it never used. So this checks
  // format magic, output count, and the FIRST present output's dims (later outputs'
  // headers sit past their predecessors' plane data, beyond any prefix — all outputs
  // share the bake dims by construction, and content is hash-protected). Channel
  // count is trusted (a channel-layout change is an algo change and MUST bump the
  // node's version salt -> different hash -> no entry found). If cookLoad fails
  // after this probe passed, that is a probe/load bug, not a recompute case.
  bool cookProbe(datablock_constptr_t db, int expect_w, int expect_h) const {
    DataBlockInputStream istr(db);
    if (istr.getItem<int>() != kCookFmt) return false;
    int nout = istr.getItem<int>();
    if (nout != numOutputs())            return false;
    for (int o = 0; o < nout; o++) {
      int present = istr.getItem<int>();
      if (not present) continue; // absent marker only — next output's header is adjacent
      int w    = istr.getItem<int>();
      int h    = istr.getItem<int>();
      int ch   = istr.getItem<int>();
      int prec = istr.getItem<int>(); // v4 precision byte
      return (w == expect_w and h == expect_h and ch >= 1 and (prec == 0 or prec == 1));
    }
    return true; // all outputs absent — nothing to contradict
  }

  // shared tail for every op's cookComputeHash: mix in the bake context (dim)
  // and the upstream node hashes. Each op prepends its own version salt + params.
  static void _mixTail(DataBlock::hasher_t h, uint64_t ctx, const std::vector<uint64_t>& ih) {
    h->accumulateItem<uint64_t>(ctx);
    for (auto x : ih)
      h->accumulateItem<uint64_t>(x);
  }
};


// the SOURCE field for an image input = the CONNECTED output's value.
///////////////////////////////////////////////////////////////////////////////

inline gpucomputeimage2d_inst_ptr_t _srcImg(hfimg_inpluginst_ptr_t inp) {
  auto out = std::dynamic_pointer_cast<hfimg_outpluginst_t>(inp->_connectedOutput);
  return out ? out->_value : nullptr;
}


///////////////////////////////////////////////////////////////////////////////
// shared helpers for the elementwise ops
///////////////////////////////////////////////////////////////////////////////

// grab a float input pluginst AND copy its data-plug default into the inst (an
// unconnected inst plug is not auto-populated — see the onLink notes above).
inline dflow::float_inp_pluginst_ptr_t
_floatPlug(dflow::DgModuleInst* inst, const dflow::DgModuleData* data, const char* name) {
  auto p    = inst->typedInputNamed<dflow::FloatPlugTraits>(name);
  p->_value = data->typedInputNamed<dflow::FloatPlugTraits>(name)->_value;
  return p;
}

// same, for a vec2 input plug (e.g. a packed 2D direction/offset).
inline dflow::fvec2_inp_pluginst_ptr_t
_vec2Plug(dflow::DgModuleInst* inst, const dflow::DgModuleData* data, const char* name) {
  auto p    = inst->typedInputNamed<dflow::Vec2fPlugTraits>(name);
  p->_value = data->typedInputNamed<dflow::Vec2fPlugTraits>(name)->_value;
  return p;
}

// substitute %KEY% -> val in a shader template
inline void _shadersub(std::string& s, const std::string& key, const std::string& val) {
  size_t pos = 0;
  while ((pos = s.find(key, pos)) != std::string::npos) {
    s.replace(pos, key.size(), val);
    pos += val.size();
  }
}

// every op's output buffer is W*H R32F; acquired via bakeAcquire (pre-node in a frontier bake, activate-time under legacy/live drivers)
inline void _allocOut(BakeEnv* env, gpucomputeimage2d_inst_ptr_t img) {
  img->_w        = env->_w;
  img->_h        = env->_h;
  img->_channels = 1;
  img->_ssbo     = env->createStorageBuffer(size_t(env->_w) * size_t(env->_h) * sizeof(float));
}


} // namespace ork::lev2::terrain
