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
  static constexpr int kCookFmt = 0x7c0d0003;

  // Cache ALL output plugs at their TRUE size (w*h*channels floats). The original version cached only
  // the "Out" plug at mono size — which silently corrupted any multi-output and/or multi-channel (RGBA)
  // module (e.g. flow3d) on a cache HIT: the extra outputs were never restored (compute is skipped) and
  // an RGBA "Out" got only its first 1/4 back.
  datablock_ptr_t cookStore() const final {
    auto env = _graphinst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    auto db  = std::make_shared<DataBlock>();
    int nout = numOutputs();
    db->addItem<int>(kCookFmt);
    db->addItem<int>(nout);
    for (int o = 0; o < nout; o++) {
      auto op  = std::dynamic_pointer_cast<hfimg_outpluginst_t>(output(o));
      auto img = op ? op->_value : nullptr;
      if (not(img and img->_ssbo)) { db->addItem<int>(0); continue; } // absent -> marker 0
      int ch     = (img->_channels < 1) ? 1 : img->_channels;
      size_t cnt = size_t(img->_w) * size_t(img->_h) * size_t(ch);
      db->addItem<int>(1);
      db->addItem<int>(img->_w);
      db->addItem<int>(img->_h);
      db->addItem<int>(ch);
      auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), BufferMapAccess::READ_ONLY);
      db->addData(mapping->_mappedaddr, cnt * sizeof(float));
      fxi->unmapStorageBuffer(mapping.get());
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
      int w  = istr.getItem<int>();
      int h  = istr.getItem<int>();
      int ch = istr.getItem<int>();
      auto op  = std::dynamic_pointer_cast<hfimg_outpluginst_t>(output(o));
      auto img = op ? op->_value : nullptr;
      if (not(img and img->_ssbo)) return false;
      int curch = (img->_channels < 1) ? 1 : img->_channels;
      if (w != img->_w or h != img->_h or ch != curch) return false; // dim/channels changed -> recompute
      size_t cnt   = size_t(w) * size_t(h) * size_t(ch);
      auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, cnt * sizeof(float), BufferMapAccess::WRITE_ONLY);
      std::memcpy(mapping->_mappedaddr, istr.current(), cnt * sizeof(float));
      fxi->unmapStorageBuffer(mapping.get());
      istr.advance(cnt * sizeof(float));
    }
    return true;
  }

  // WS4 FRONTIER: validate a cache entry BEFORE any buffer exists (the demand plan
  // classifies hit/miss up front; bakeAcquire runs only for nodes that run). Mirrors
  // every cookLoad rejection that doesn't need live state: format magic, output
  // count, per-output dims vs the bake dims. Channel count is trusted from the
  // datablock (a channel-layout change is an algo change and MUST bump the node's
  // version salt -> different hash -> no entry found). If cookLoad fails after this
  // probe passed, that is a probe/load disagreement — a bug, not a recompute case.
  bool cookProbe(datablock_constptr_t db, int expect_w, int expect_h) const {
    DataBlockInputStream istr(db);
    if (istr.getItem<int>() != kCookFmt) return false;
    int nout = istr.getItem<int>();
    if (nout != numOutputs())            return false;
    for (int o = 0; o < nout; o++) {
      int present = istr.getItem<int>();
      if (not present) continue;
      int w  = istr.getItem<int>();
      int h  = istr.getItem<int>();
      int ch = istr.getItem<int>();
      if (w != expect_w or h != expect_h or ch < 1) return false;
      istr.advance(size_t(w) * size_t(h) * size_t(ch) * sizeof(float));
    }
    return true;
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
