////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Heightfield compute-dataflow — first slice. fbm -> SSBO -> readback -> EXR,
// authored as a serializable ork::dataflow GraphData of compute modules.
//
////////////////////////////////////////////////////////////////

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
#include <unordered_set>
#include "hfdflow_module.h"

ImplementReflectionX(ork::lev2::terrain::TerrainModuleData, "terrain::TerrainModuleData");
ImplementReflectionX(ork::lev2::terrain::CaptureModuleData, "terrain::CaptureModuleData");

// member of the INTERCHANGE namespace (B.3) — must be defined in an enclosing namespace of the class.
namespace ork::lev2::dflowgfx {
gpucomputeimage2d_inst_ptr_t HfImagePlugTraits::data_to_inst(gpucomputeimage2d_data_ptr_t inp) {
  return std::make_shared<GpuComputeImage2DInst>(inp);
}
} // namespace ork::lev2::dflowgfx

namespace ork::lev2::terrain {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// bases
///////////////////////////////////////////////////////////////////////////////

void TerrainModuleData::describeX(class_t* clazz) {
}
TerrainModuleData::TerrainModuleData() {
}

///////////////////////////////////////////////////////////////////////////////
// CaptureModule — records the request; the driver flushes after GPU submit.
///////////////////////////////////////////////////////////////////////////////

struct CaptureModuleInst : public dflow::DgModuleInst {
  CaptureModuleInst(const CaptureModuleData* data, dflow::GraphInst* ginst)
      : dflow::DgModuleInst(data, ginst)
      , _cmd(data) {
  }
  void onLink(dflow::GraphInst* inst) final {
    _input = typedInputNamed<HfImagePlugTraits>("In");
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    if (not env->_flushes_captures) { // ops self-defend: a non-bake host never flushes captures
      printf(
          "terrain Capture<%s>: this graph's driver does NOT flush captures (a capture in a "
          "cross-family/live graph would silently write nothing). Bake captures with a "
          "HeightField asset (bakeHeightfield).\n",
          _cmd->_name.c_str());
      OrkAssert(false);
    }
    // resolve the SOURCE field from the CONNECTED output (not our input's own
    // _value, which is an empty default) — that's where the producer set _ssbo.
    auto out = std::dynamic_pointer_cast<hfimg_outpluginst_t>(_input->_connectedOutput);
    OrkAssert(out);
    env->_captures.push_back(CaptureRequest{out->_value, _cmd->_channel, _cmd->_path});
  }
  const CaptureModuleData* _cmd;
  hfimg_inpluginst_ptr_t _input;
};

static void _reshapeCaptureIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
}

CaptureModuleData::CaptureModuleData() {
}
std::shared_ptr<CaptureModuleData> CaptureModuleData::createShared() {
  auto data = std::make_shared<CaptureModuleData>();
  _reshapeCaptureIOs(data);
  return data;
}
dflow::dgmoduleinst_ptr_t CaptureModuleData::createInstance(dflow::GraphInst* ginst) const {
  return std::make_shared<CaptureModuleInst>(this, ginst);
}
void CaptureModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return CaptureModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeCaptureIOs(mdata); });
  // serialize the channel identity so the graph self-describes its sinks (_path
  // is machine-specific and stays unreflected — derived from _channel at bake).
  clazz->directProperty("channel", &CaptureModuleData::_channel);
  // per-bake cook-cache opt-out (round-trips with the graph).
  clazz->directProperty("cache", &CaptureModuleData::_cache);
}

///////////////////////////////////////////////////////////////////////////////
// BakeEnv allocation arena — see the declaration notes in hfdflow.h.
///////////////////////////////////////////////////////////////////////////////

FxShaderStorageBuffer* BakeEnv::createStorageBuffer(size_t length) {
  auto buf = _ctx->FXI()->createStorageBuffer(length);
  _allocs.push_back(buf);
  return buf;
}

void BakeEnv::freeAllocs() {
  auto fxi = _ctx->FXI();
  std::unordered_set<FxShaderStorageBuffer*> freed;
  for (auto b : _allocs)
    if (b and freed.insert(b).second)
      fxi->destroyStorageBuffer(b);
  _allocs.clear();
}

///////////////////////////////////////////////////////////////////////////////
// driver
///////////////////////////////////////////////////////////////////////////////

// cache-hit count from the most recent cacheable bake (for terrainCacheTest).
static int s_lastCookHits = -1;

std::vector<fieldstats_ptr_t> bakeHeightfield(
    dflow::graphdata_ptr_t graph, Context* ctx, int dim, float extent_m, float height_scale_m) {
  // topo-sort. The sorter allocates a register per connected output plug from a
  // per-type pool, so the GpuComputeImage2D type MUST have a register block
  // (keyed by the out-plug's data_type_t = GpuComputeImage2DData) or the sort
  // asserts. (Distinct per-format pools would need distinct C++ types — later.)
  auto dgctx = std::make_shared<dflow::dgcontext>();
  dgctx->createRegisters<GpuComputeImage2DData>("hf_img", 64);
  // register pools for the scalar plug types a terrain graph can carry on a
  // CONNECTED edge. Today every dataflow edge is a GpuComputeImage2D and the
  // vec2/vec4/float plugs are UNIFORM inputs (which the sorter never allocates a
  // register for) — but a future module that OUTPUTS a vec2/vec4/float field
  // would have its output plug keyed here, so register the pools up front (the
  // sort asserts a non-null register for any connected output, dataflow_sorter
  // line ~204). Cheap (a small pool each) and keeps the machine complete.
  dgctx->createRegisters<float>("hf_float", 256);
  dgctx->createRegisters<fvec2>("hf_vec2", 256);
  dgctx->createRegisters<fvec4>("hf_vec4", 256);
  auto sorter = std::make_shared<dflow::DgSorter>(graph.get(), dgctx);
  auto topo   = sorter->generateTopology();
  OrkAssert(topo);

  // instantiate
  auto ginst = dflow::GraphData::createGraphInst(graph);

  auto env             = std::make_shared<BakeEnv>();
  env->_ctx            = ctx;
  env->_w              = dim;
  env->_h              = dim;
  env->_extent_m       = extent_m;       // world units -> resolution-independent meter params
  env->_height_scale_m = height_scale_m;
  env->_per_op_sync      = true; // THIS driver syncs per op (the cook loop / capture flush below)
  env->_flushes_captures = true;
  ginst->_impl.setShared<BakeEnv>(env);

  ginst->updateTopology(topo);

  // run the compute (modules dispatch; capture modules record requests)
  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_abstime = 0.0f;
  updata->_dt      = 0.0f;

  ctx->beginFrame();
  auto ci = ctx->CI();
  if (graph->_cacheable) {
    // per-node cook cache: Merkle-hash every node (cheap scalars), then per node
    // either upload its cached field (hit) or dispatch it IN ITS OWN dispatch
    // phase + read the result back to cache (miss). Per-op submit+wait makes the
    // inline readback valid. The Capture sink has no cookStore, so it just runs.
    //
    // The DISK cook cache is gated separately from this per-op synced loop: any
    // CaptureModule with cache=false (self.capture(..., cache=False)) turns off
    // the find/store for the WHOLE bake (the cache is whole-bake, per-node), while
    // the synced compute below — which terrain needs for cross-node SSBO ordering
    // — still runs. So cache=False = recompute every run, never touch dflowcache.
    bool do_disk_cache = true;
    for (size_t i = 0; i < graph->numModules(); i++) {
      if (auto cap = std::dynamic_pointer_cast<CaptureModuleData>(graph->module(i))) {
        if (not cap->_cache) {
          do_disk_cache = false;
          break;
        }
      }
    }
    // cook context = everything outside the graph that changes a node's OUTPUT:
    // the bake resolution AND the world units (meters->texels depends on dim/extent;
    // real slope depends on height_scale). Node hashes carry the resolution-independent
    // meter params; this context folds in the per-bake resolution + scale.
    if (do_disk_cache) {
      {
        auto ch = DataBlock::createHasher();
        ch->accumulateItem<int>(dim);
        ch->accumulateItem<float>(extent_m);
        ch->accumulateItem<float>(height_scale_m);
        ch->finish();
        ginst->_cookContextHash = ch->result();
      }
      ginst->computeNodeHashes();
    }
    int cook_hits = 0, cook_computes = 0;
    for (auto inst : ginst->_ordered_module_insts) {
      // terrain cook datablocks (full-res field readbacks) land in the evictable
      // <staging>/dflowcache namespace, separate from the shared dblockcache.
      auto db = do_disk_cache ? DataBlockCache::findDataBlock("dflowcache", inst->_cookHash) : nullptr;
      if (db and inst->cookLoad(db)) {
        // cache HIT — cached field uploaded to the node's SSBO; no GPU dispatch
        cook_hits++;
      } else {
        // family-neutral pre-phase host write (E.1b realtime-params mechanism; at the
        // bake's t=0 this equals the onActivate fill — uniform, not behavioral).
        if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst))
          pp->writeParams(ctx);
        ci->beginDispatchPhase();
        inst->compute(ginst.get(), updata);
        ci->endDispatchPhase(); // submit + WAIT -> this node's output is now valid
        if (do_disk_cache) {
          if (auto store = inst->cookStore())
            DataBlockCache::setDataBlock("dflowcache", inst->_cookHash, store);
        }
        cook_computes++;
      }
    }
    if (do_disk_cache)
      printf("[cook] cacheable bake: %d cache-hits, %d computed\n", cook_hits, cook_computes);
    else
      printf("[cook] cache DISABLED (capture cache=False): %d computed, 0 disk I/O\n", cook_computes);
    s_lastCookHits = cook_hits;
  } else {
    for (auto inst : ginst->_ordered_module_insts) // family-neutral pre-phase (see above)
      if (auto pp = std::dynamic_pointer_cast<dflowgfx::IPrePhaseParams>(inst))
        pp->writeParams(ctx);
    ci->beginDispatchPhase();
    ginst->compute(updata);
    ci->endDispatchPhase();
  }
  ctx->endFrame();

  // flush captures: readback each source SSBO and encode by file extension. BOTH
  // paths are SINGLE-CHANNEL and NORMALIZED to the field's [min,max] (auto-exposed
  // for max contrast/precision; great for masks/curv that live in a sub-range):
  //   .exr -> R32F float (lossless, no quantization — preferred source for normals).
  //   .png -> 16-bit grayscale (R16UI; PNG can't hold float, R16UI forces full depth).
  // The absolute scale is recoverable from the printed/returned FieldStats (min/max),
  // since both encodings are field-relative.
  std::vector<fieldstats_ptr_t> stats;
  auto fxi = ctx->FXI();
  for (auto& req : env->_captures) {
    auto img  = req._img; // GpuComputeImage2DInst (the producer's output value)
    int w     = img->_w;
    int h     = img->_h;
    size_t n  = size_t(w) * size_t(h);

    // MULTI-CHANNEL capture (e.g. flow3d's RGBA flow field): the producer wrote `_channels`
    // interleaved floats/cell, already display-ready. Pass through to an RGBA32F EXR verbatim
    // (no [min,max] normalize, no synthesized normal). EXR only (float); PNG would quantize.
    if (img->_channels >= 2) {
      int nch = img->_channels;
      auto mcmap = fxi->mapStorageBuffer(img->_ssbo, 0, n * size_t(nch) * sizeof(float), BufferMapAccess::READ_ONLY);
      const float* mc = (const float*)mcmap->_mappedaddr;
      std::vector<float> rgba(n * 4, 0.0f);
      for (size_t i = 0; i < n; i++)
        for (int c = 0; c < 4; c++)
          rgba[i * 4 + c] = (c < nch) ? mc[i * size_t(nch) + size_t(c)] : (c == 3 ? 1.0f : 0.0f);
      fxi->unmapStorageBuffer(mcmap.get());
      // split the (comma-joined) channel names and emit the SAME RGBA under each name (the list
      // form of capture() — one readback, one image per name). All names get the verbatim RGBA.
      std::vector<std::string> mchans;
      { const std::string cs = req._channels; size_t start = 0;
        while (start <= cs.size()) {
          size_t comma = cs.find(',', start);
          size_t len   = (comma == std::string::npos) ? std::string::npos : (comma - start);
          std::string tk = cs.substr(start, len);
          if (not tk.empty()) mchans.push_back(tk);
          if (comma == std::string::npos) break;
          start = comma + 1;
        }
        if (mchans.empty()) mchans.push_back("field"); }
      for (auto& ch : mchans) {
        std::string path = ork::file::Path::expandPathString(std::string(req._path.c_str()));
        { const std::string mark = "{channel}"; size_t pos = path.find(mark);
          if (pos != std::string::npos) path.replace(pos, mark.size(), ch); }
        Image oimg; oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
        memcpy((void*)oimg._data->data(), rgba.data(), rgba.size() * sizeof(float));
        oimg.writeToFile(ork::file::Path(path.c_str()), /*linear=*/false);
        printf("[terrain bake] wrote <%s> (%dx%d, RGBA32F, %dch passthrough)\n", path.c_str(), w, h, nch);
        auto fs = std::make_shared<FieldStats>(); fs->_min = 0.0f; fs->_max = 1.0f; fs->_mean = 0.0f;
        fs->_channel = ch;
        stats.push_back(fs);
      }
      continue;
    }

    auto mapping = fxi->mapStorageBuffer(img->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    const float* src = (const float*)mapping->_mappedaddr;

    // pass 1: field stats.
    float vmin = 1e30f, vmax = -1e30f;
    double vsum = 0.0;
    for (size_t i = 0; i < n; i++) {
      float v = src[i];
      vmin = (v < vmin) ? v : vmin;
      vmax = (v > vmax) ? v : vmax;
      vsum += v;
    }
    float vmean = float(vsum / double(n));

    // the NORMALIZED [min,max] -> [0,1] field (auto-exposure). The "height" output IS
    // this; the "normal" output is the gradient of (this * height_scale_m). Computed
    // ONCE here and shared by every channel this capture emits — the efficiency win of
    // capture(node, ["height","normal"]) vs two separate captures (one readback).
    float range = vmax - vmin;
    float inv   = (range > 1e-12f) ? (1.0f / range) : 0.0f;
    std::vector<float> hn(n);
    for (size_t i = 0; i < n; i++) {
      float t = (src[i] - vmin) * inv;
      hn[i]   = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    fxi->unmapStorageBuffer(mapping.get());
    if(0)printf("[terrain bake] field stats: min<%g> max<%g> mean<%g>  (normalized to [min,max])\n",
           vmin, vmax, vmean);

    // split the capture's channel list ("height", or "height,normal", ...).
    std::vector<std::string> channels;
    {
      const std::string cs = req._channels;
      size_t start = 0;
      while (start <= cs.size()) {
        size_t comma   = cs.find(',', start);
        size_t len     = (comma == std::string::npos) ? std::string::npos : (comma - start);
        std::string tk = cs.substr(start, len);
        if (not tk.empty()) channels.push_back(tk);
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
      if (channels.empty()) channels.push_back("height");
    }

    // world scale for the normal gradient (assumes a square field, W==H==dim).
    float texel_m  = (w > 0) ? (env->_extent_m / float(w)) : 1.0f;
    float vscale_m = env->_height_scale_m;

    // emit one image per requested channel — the source field was read back ONCE above.
    for (auto& ch : channels) {
      // resolve the per-channel path: substitute "{channel}" in the template if present
      // (multi-channel asset bakes), else use the path verbatim (single-channel / test
      // callers that pass a literal path).
      std::string path = ork::file::Path::expandPathString(std::string(req._path.c_str()));
      {
        const std::string mark = "{channel}";
        size_t pos = path.find(mark);
        if (pos != std::string::npos)
          path.replace(pos, mark.size(), ch);
      }
      bool as_png = path.size() >= 4 && (path.compare(path.size() - 4, 4, ".png") == 0);

      Image oimg;
      if (ch == "normal") {
        // SYNTHESIZE the highest-quality normal: a SCHARR gradient (8-neighbor,
        // rotationally-symmetric) of the surface height (normalized * height_scale_m),
        // in world meters. n = normalize(-dH/dx, +1, -dH/dz), +Y up. EXR -> signed xyz
        // float (RGBA32F, a=1); PNG -> n*0.5+0.5 (RGBA8). NOTE: the normal is baked at
        // env->_height_scale_m — a consumer must render the height at that same scale
        // for the normal to be consistent.
        std::vector<float>   nf; // .exr
        std::vector<uint8_t> nb; // .png
        if (as_png) nb.resize(n * 4); else nf.resize(n * 4);
        auto Hm = [&](int x, int y) -> float {
          x = (x < 0) ? 0 : (x >= w ? w - 1 : x);   // CLAMP_TO_EDGE
          y = (y < 0) ? 0 : (y >= h ? h - 1 : y);
          return hn[size_t(y) * size_t(w) + size_t(x)] * vscale_m;
        };
        auto enc8 = [](float v) -> uint8_t {        // signed [-1,1] -> unorm byte
          float u = (v * 0.5f + 0.5f) * 255.0f + 0.5f;
          return uint8_t(u < 0.0f ? 0.0f : (u > 255.0f ? 255.0f : u));
        };
        float ginv = 1.0f / (32.0f * texel_m);      // Scharr norm (radius 1; span 2*texel_m)
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            float tl = Hm(x - 1, y - 1), tt = Hm(x, y - 1), tr = Hm(x + 1, y - 1);
            float ll = Hm(x - 1, y),                        rr = Hm(x + 1, y);
            float bl = Hm(x - 1, y + 1), bb = Hm(x, y + 1), br = Hm(x + 1, y + 1);
            float gx = (-3.0f * tl + 3.0f * tr - 10.0f * ll + 10.0f * rr - 3.0f * bl + 3.0f * br) * ginv;
            float gz = (-3.0f * tl - 10.0f * tt - 3.0f * tr + 3.0f * bl + 10.0f * bb + 3.0f * br) * ginv;
            float nx = -gx, ny = 1.0f, nz = -gz;
            float il = 1.0f / sqrtf(nx * nx + ny * ny + nz * nz);
            nx *= il; ny *= il; nz *= il;
            size_t i = size_t(y) * size_t(w) + size_t(x);
            if (as_png) {
              nb[i * 4 + 0] = enc8(nx); nb[i * 4 + 1] = enc8(ny);
              nb[i * 4 + 2] = enc8(nz); nb[i * 4 + 3] = 255;
            } else {
              nf[i * 4 + 0] = nx; nf[i * 4 + 1] = ny; nf[i * 4 + 2] = nz; nf[i * 4 + 3] = 1.0f;
            }
          }
        }
        if (as_png) {
          oimg.initWithFormat(w, h, EBufferFormat::RGBA8);
          memcpy((void*)oimg._data->data(), nb.data(), nb.size());
        } else {
          oimg.initWithFormat(w, h, EBufferFormat::RGBA32F);
          memcpy((void*)oimg._data->data(), nf.data(), nf.size() * sizeof(float));
        }
      } else {
        // scalar field (height / mask): the normalized [0,1] field.
        // EXR -> R32F float (no quantization); PNG -> R16UI (×65535).
        if (as_png) {
          std::vector<uint16_t> g16(n);
          for (size_t i = 0; i < n; i++)
            g16[i] = uint16_t(hn[i] * 65535.0f + 0.5f);
          oimg.initWithFormat(w, h, EBufferFormat::R16UI);
          memcpy((void*)oimg._data->data(), g16.data(), n * sizeof(uint16_t));
        } else {
          oimg.initWithFormat(w, h, EBufferFormat::R32F);
          memcpy((void*)oimg._data->data(), hn.data(), n * sizeof(float));
        }
      }
      // heightfields/masks/normals are LINEAR data — tag PNG linear (engine PNG default
      // is sRGB). EXR is always linear float.
      oimg.writeToFile(ork::file::Path(path.c_str()), /*linear_colorspace=*/ as_png);
      if(0)printf("[terrain bake] wrote <%s> (%dx%d, %s/%s)\n", path.c_str(), w, h,
             ch.c_str(), as_png ? "png" : "exr");

      // one FieldStats per emitted channel (all derive from the same field), so the
      // returned vector lines up index-for-index with the caller's flat channel list.
      auto fs      = std::make_shared<FieldStats>();
      fs->_min     = vmin;
      fs->_max     = vmax;
      fs->_mean    = vmean;
      fs->_channel = ch;
      stats.push_back(fs);
    }
  }

  // the bake's outputs are fully consumed (cook cache stored, captures encoded to
  // files) and ginst dies at return — free the whole graph's GPU buffers. GPU-idle
  // holds here: the cacheable path syncs per op, and every storage-buffer map above
  // is itself a pending-dispatch hazard point that waits.
  env->freeAllocs();
  return stats;
}

void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim) {
  // fbm -> remap -> capture. remap doubles + clamps, so the field stats shift
  // visibly (proves the input-reading module + multi-SSBO bind + barrier).
  auto graph = std::make_shared<dflow::GraphData>();
  auto fbm   = FbmModuleData::createShared();
  auto remap = RemapModuleData::createShared();
  auto cap   = CaptureModuleData::createShared();
  cap->_path = outpath;
  remap->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(2.0f);
  dflow::GraphData::addModule(graph, "fbm", fbm);
  dflow::GraphData::addModule(graph, "remap", remap);
  dflow::GraphData::addModule(graph, "capture", cap);
  graph->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
  graph->safeConnect(cap->inputNamed("In"), remap->outputNamed("Out"));
  bakeHeightfield(graph, ctx, dim);
}

///////////////////////////////////////////////////////////////////////////////
// op self-test — every case is driven by Const inputs (or a closed-form
// Gradient), so the expected min/max/mean is known analytically. This exercises:
//   Const     1 SSBO  generator
//   Gradient  1 SSBO  generator w/ spatial addressing (both axes)
//   Combine   3 SSBO  out=0,a=1,b=2  (the strongest multi-SSBO-bind test)
//   Terrace   2 SSBO  quantize math
///////////////////////////////////////////////////////////////////////////////

int terrainOpsSelfTest(Context* ctx, int dim) {
  int fails    = 0;
  const float D = float(dim);

  auto aeq = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  auto check = [&](const char* name, fieldstats_ptr_t s, float emin, float emax, float emean, float tol) {
    bool ok = aeq(s->_min, emin, tol) && aeq(s->_max, emax, tol) && aeq(s->_mean, emean, tol);
    printf(
        "[selftest] %-26s min<%.5f|%.5f> max<%.5f|%.5f> mean<%.5f|%.5f> : %s\n",
        name, s->_min, emin, s->_max, emax, s->_mean, emean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  };

  // --- builders ----------------------------------------------------------------
  auto mkConst = [&](float lvl) -> constmoduledata_ptr_t {
    auto m = ConstModuleData::createShared();
    m->typedInputNamed<dflow::FloatPlugTraits>("level")->setValue(lvl);
    return m;
  };
  auto capTo = [&](dflow::graphdata_ptr_t g, dflow::moduledata_ptr_t producer, const std::string& path) {
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(path.c_str());
    dflow::GraphData::addModule(g, "capture", cap);
    g->safeConnect(cap->inputNamed("In"), producer->outputNamed("Out"));
  };
  auto path = [](const char* nm) { return FormatString("/tmp/terrain_selftest_%s.exr", nm); };

  // --- Const -------------------------------------------------------------------
  {
    auto g = std::make_shared<dflow::GraphData>();
    auto c = mkConst(0.5f);
    dflow::GraphData::addModule(g, "c", c);
    capTo(g, c, path("const_half"));
    check("Const(0.5)", bakeHeightfield(g, ctx, dim)[0], 0.5f, 0.5f, 0.5f, 1e-4f);
  }
  {
    auto g = std::make_shared<dflow::GraphData>();
    auto c = mkConst(0.25f);
    dflow::GraphData::addModule(g, "c", c);
    capTo(g, c, path("const_quarter"));
    check("Const(0.25)", bakeHeightfield(g, ctx, dim)[0], 0.25f, 0.25f, 0.25f, 1e-4f);
  }

  // --- Gradient (closed form: linear ramp along an axis) -----------------------
  // value = uv.axis ; over xi=0..D-1 : min=0, max=(D-1)/D, mean=(D-1)/(2D)
  auto runGrad = [&](const char* nm, float dx, float dy) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(dx, dy));
    dflow::GraphData::addModule(g, "grad", gr);
    capTo(g, gr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("Gradient(x)", runGrad("grad_x", 1.0f, 0.0f), 0.0f, (D - 1.0f) / D, (D - 1.0f) / (2.0f * D), 2e-3f);
  check("Gradient(y)", runGrad("grad_y", 0.0f, 1.0f), 0.0f, (D - 1.0f) / D, (D - 1.0f) / (2.0f * D), 2e-3f);

  // --- Combine (3 SSBO: out,a,b) ----------------------------------------------
  auto runCombine = [&](const char* nm, int op, float a, float b, float t) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ca = mkConst(a);
    auto cb = mkConst(b);
    auto cm = CombineModuleData::createShared();
    cm->_op = op;
    cm->typedInputNamed<dflow::FloatPlugTraits>("t")->setValue(t);
    dflow::GraphData::addModule(g, "a", ca);
    dflow::GraphData::addModule(g, "b", cb);
    dflow::GraphData::addModule(g, "m", cm);
    g->safeConnect(cm->inputNamed("A"), ca->outputNamed("Out"));
    g->safeConnect(cm->inputNamed("B"), cb->outputNamed("Out"));
    capTo(g, cm, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("Combine(ADD,.3,.2)", runCombine("comb_add", int(CombineOp::ADD), 0.3f, 0.2f, 0.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(SUB,.8,.3)", runCombine("comb_sub", int(CombineOp::SUB), 0.8f, 0.3f, 0.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(MUL,.5,.5)", runCombine("comb_mul", int(CombineOp::MUL), 0.5f, 0.5f, 0.0f), 0.25f, 0.25f, 0.25f, 1e-4f);
  check("Combine(MIN,.3,.7)", runCombine("comb_min", int(CombineOp::MIN), 0.3f, 0.7f, 0.0f), 0.3f, 0.3f, 0.3f, 1e-4f);
  check("Combine(MAX,.3,.7)", runCombine("comb_max", int(CombineOp::MAX), 0.3f, 0.7f, 0.0f), 0.7f, 0.7f, 0.7f, 1e-4f);
  check("Combine(MIX,.2,.8,.5)", runCombine("comb_mix5", int(CombineOp::MIX), 0.2f, 0.8f, 0.5f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Combine(MIX,.2,.8,.25)", runCombine("comb_mix25", int(CombineOp::MIX), 0.2f, 0.8f, 0.25f), 0.35f, 0.35f, 0.35f, 1e-4f);

  // --- Terrace (2 SSBO) : quantize a constant to the nearest plateau -----------
  auto runTerrace = [&](const char* nm, float in, float steps, float sharp) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ci = mkConst(in);
    auto tr = TerraceModuleData::createShared();
    tr->typedInputNamed<dflow::FloatPlugTraits>("steps")->setValue(steps);
    tr->typedInputNamed<dflow::FloatPlugTraits>("sharpness")->setValue(sharp);
    dflow::GraphData::addModule(g, "c", ci);
    dflow::GraphData::addModule(g, "t", tr);
    g->safeConnect(tr->inputNamed("In"), ci->outputNamed("Out"));
    capTo(g, tr, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  // 0.6*4=2.4 -> plateau 2 -> 2/4=0.5 ; 0.7*4=2.8 -> riser -> 3/4=0.75 ; 0.9*2=1.8 -> 2/2=1.0
  check("Terrace(.6,4,1)", runTerrace("terr_06", 0.6f, 4.0f, 1.0f), 0.5f, 0.5f, 0.5f, 1e-4f);
  check("Terrace(.7,4,1)", runTerrace("terr_07", 0.7f, 4.0f, 1.0f), 0.75f, 0.75f, 0.75f, 1e-4f);
  check("Terrace(.9,2,1)", runTerrace("terr_09", 0.9f, 2.0f, 1.0f), 1.0f, 1.0f, 1.0f, 1e-4f);

  // --- Slope (pre-blurred gradient + soft rolloff, Mask by Feature) -----------
  // flat field -> slope 0 everywhere. A unit ramp has gradient magnitude exactly 1
  // (box-averaging a linear field is exact), so slope = soft_rolloff(1*scale) =
  // 1/(1+1) = 0.5 uniform in the interior; the border ring (margin radius+box) is 0.
  // bake the mask-generator cases with extent==height_scale==dim, so 1 texel == 1 m
  // (radius_m == radius_texels) and the slope factor height_scale/extent == 1.
  const float E = float(dim);
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto c  = mkConst(0.5f);
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f; // -> 2 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, sl, path("slope_flat"));
    check("Slope(flat)", bakeHeightfield(g, ctx, dim, E, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared();
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sl = SlopeModuleData::createShared();
    sl->_radius_m = 2.0f;
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "s", sl);
    g->safeConnect(sl->inputNamed("In"), gr->outputNamed("Out"));
    capTo(g, sl, path("slope_ramp"));
    auto s  = bakeHeightfield(g, ctx, dim, E, E)[0];
    bool ok = (s->_min < 1e-4f) && aeq(s->_max, 0.5f, 2e-3f) && (s->_mean > 0.3f);
    printf("[selftest] %-26s min<%.5f> max<%.5f> mean<%.5f> : %s\n", "Slope(ramp,x)",
           s->_min, s->_max, s->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  // --- MaskBlend (4 SSBO): per-texel mix(A,B,M) -------------------------------
  auto runBlend = [&](const char* nm, float a, float b, float mk) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto ca = mkConst(a);
    auto cb = mkConst(b);
    auto cm = mkConst(mk);
    auto mb = MaskBlendModuleData::createShared();
    dflow::GraphData::addModule(g, "a", ca);
    dflow::GraphData::addModule(g, "b", cb);
    dflow::GraphData::addModule(g, "m", cm);
    dflow::GraphData::addModule(g, "mb", mb);
    g->safeConnect(mb->inputNamed("A"), ca->outputNamed("Out"));
    g->safeConnect(mb->inputNamed("B"), cb->outputNamed("Out"));
    g->safeConnect(mb->inputNamed("M"), cm->outputNamed("Out"));
    capTo(g, mb, path(nm));
    return bakeHeightfield(g, ctx, dim)[0];
  };
  check("MaskBlend(.2,.8,0)",   runBlend("mask_0",  0.2f, 0.8f, 0.0f),  0.2f, 0.2f, 0.2f, 1e-4f);
  check("MaskBlend(.2,.8,1)",   runBlend("mask_1",  0.2f, 0.8f, 1.0f),  0.8f, 0.8f, 0.8f, 1e-4f);
  check("MaskBlend(.2,.8,.25)", runBlend("mask_25", 0.2f, 0.8f, 0.25f), 0.35f, 0.35f, 0.35f, 1e-4f);

  // --- Curvature (band-pass diff-of-box + soft rolloff, Mask by Feature) -------
  // a flat field has zero curvature (diff-of-box of a constant is 0, soft rolloff
  // of 0 is 0). A squared ramp uv.x^2 is concave everywhere (positive curvature),
  // so CONVEX (ridge) -> 0 everywhere and CONCAVE responds (>0, soft-rolled into (0,1)).
  {
    auto g  = std::make_shared<dflow::GraphData>();
    auto c  = mkConst(0.5f);
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = int(CurvatureMode::MAGNITUDE);
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    dflow::GraphData::addModule(g, "c", c);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(cv->inputNamed("In"), c->outputNamed("Out"));
    capTo(g, cv, path("curv_flat"));
    check("Curvature(flat)", bakeHeightfield(g, ctx, dim, E, E)[0], 0.0f, 0.0f, 0.0f, 1e-4f);
  }
  auto runCurv = [&](const char* nm, int mode, float scale) -> fieldstats_ptr_t {
    auto g  = std::make_shared<dflow::GraphData>();
    auto gr = GradientModuleData::createShared(); // value = uv.x
    gr->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(1.0f, 0.0f));
    auto sq = CombineModuleData::createShared(); // uv.x * uv.x = uv.x^2
    sq->_op = int(CombineOp::MUL);
    auto cv = CurvatureModuleData::createShared();
    cv->_mode     = mode;
    cv->_radius_m = 4.0f; // -> 4 texels at extent==dim
    cv->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(scale);
    dflow::GraphData::addModule(g, "grad", gr);
    dflow::GraphData::addModule(g, "sq", sq);
    dflow::GraphData::addModule(g, "cv", cv);
    g->safeConnect(sq->inputNamed("A"), gr->outputNamed("Out"));
    g->safeConnect(sq->inputNamed("B"), gr->outputNamed("Out"));
    g->safeConnect(cv->inputNamed("In"), sq->outputNamed("Out"));
    capTo(g, cv, path(nm));
    return bakeHeightfield(g, ctx, dim, E, E)[0];
  };
  // convex of a concave (valley-shaped) field -> 0 everywhere (no ridges); border zeroed.
  check("Curvature(x^2,convex)", runCurv("curv_convex", int(CurvatureMode::CONVEX), 4.0f), 0.0f, 0.0f, 0.0f, 1e-4f);
  // concave responds: min 0 (border ring), max in (0,1) (soft rolloff never saturates),
  // interior ~uniform for a quadratic. Robust property check (not an exact pin).
  {
    auto s  = runCurv("curv_concave", int(CurvatureMode::CONCAVE), 4.0f);
    bool ok = (s->_min < 1e-4f) && (s->_max > 0.05f) && (s->_max < 0.999f) && (s->_mean > 0.04f);
    printf("[selftest] %-26s min<%.5f> max<%.5f> mean<%.5f> : %s\n", "Curvature(x^2,concave)",
           s->_min, s->_max, s->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  printf("[selftest] %d case(s) FAILED\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// round-trip gate — the serialized GraphData is the portable, python-decoupled
// artifact (the future dflow UI editor loads exactly this JSON). This proves a
// terrain graph survives serialize -> deserialize -> bake with NO python and NO
// loss: build fbm(octaves=7) * const(0.5) via Combine(MUL), bake (stats A); JSON
// round-trip to a fresh clone; re-supply the capture path (wrapper-owned, not in
// the graph); assert the clone's _octaves/_op survived AND it bakes identical
// stats (so the baked scalars, float plug values, and connections all round-trip).
///////////////////////////////////////////////////////////////////////////////

int terrainRoundTripTest(Context* ctx, int dim) {
  int fails = 0;
  auto aeq  = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };

  auto build = [&](const char* cappath) -> dflow::graphdata_ptr_t {
    auto g    = std::make_shared<dflow::GraphData>();
    auto fbm  = FbmModuleData::createShared();
    fbm->_octaves = 7; // non-default baked scalar (default is 5)
    auto cst  = ConstModuleData::createShared();
    cst->typedInputNamed<dflow::FloatPlugTraits>("level")->setValue(0.5f);
    auto comb = CombineModuleData::createShared();
    comb->_op = int(CombineOp::MUL); // non-default baked scalar (default is ADD)
    // Gradient with a NON-default vec2 "dir" — exercises the vec2 plug VALUE
    // surviving the JSON round-trip (the inplugdata<Vec2fPlugTraits> reflection).
    auto grad = GradientModuleData::createShared();
    grad->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->setValue(fvec2(0.6f, 0.8f)); // default is (1,0)
    grad->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    auto comb2 = CombineModuleData::createShared();
    comb2->_op = int(CombineOp::MUL);
    auto cap  = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "cst", cst);
    dflow::GraphData::addModule(g, "comb", comb);
    dflow::GraphData::addModule(g, "grad", grad);
    dflow::GraphData::addModule(g, "comb2", comb2);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(comb->inputNamed("A"), fbm->outputNamed("Out"));
    g->safeConnect(comb->inputNamed("B"), cst->outputNamed("Out"));
    g->safeConnect(comb2->inputNamed("A"), comb->outputNamed("Out"));
    g->safeConnect(comb2->inputNamed("B"), grad->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), comb2->outputNamed("Out"));
    return g;
  };

  // (1) original bake
  auto g0 = build("/tmp/terrain_rt_orig.exr");
  auto s0 = bakeHeightfield(g0, ctx, dim);

  // (2) serialize -> JSON -> deserialize a fresh clone
  ork::reflect::serdes::JsonSerializer ser;
  ser.serializeRoot(g0);
  std::string json = ser.output();
  ork::object_ptr_t out;
  ork::reflect::serdes::JsonDeserializer deser(json.c_str());
  deser.deserializeTop(out);
  auto g1 = std::dynamic_pointer_cast<dflow::GraphData>(out);
  if (not g1) {
    printf("[roundtrip] deserialize -> GraphData FAILED\n");
    return 1;
  }

  // (3) direct assertions: the baked scalars survived the round-trip
  auto fbm1  = std::dynamic_pointer_cast<FbmModuleData>(g1->module("fbm"));
  auto comb1 = std::dynamic_pointer_cast<CombineModuleData>(g1->module("comb"));
  if (not fbm1 or fbm1->_octaves != 7) {
    printf("[roundtrip] _octaves LOST (got %d, want 7)\n", fbm1 ? fbm1->_octaves : -1);
    fails++;
  }
  if (not comb1 or comb1->_op != int(CombineOp::MUL)) {
    printf("[roundtrip] _op LOST (got %d, want %d=MUL)\n", comb1 ? comb1->_op : -1, int(CombineOp::MUL));
    fails++;
  }
  // the vec2 "dir" plug VALUE must survive JSON (inplugdata<Vec2fPlugTraits> reflection)
  auto grad1 = std::dynamic_pointer_cast<GradientModuleData>(g1->module("grad"));
  if (not grad1) {
    printf("[roundtrip] clone missing 'grad' module\n");
    fails++;
  } else {
    auto dir = grad1->typedInputNamed<dflow::Vec2fPlugTraits>("dir")->value();
    bool ok  = aeq(dir.x, 0.6f, 1e-6f) and aeq(dir.y, 0.8f, 1e-6f);
    printf("[roundtrip] gradient vec2 'dir' got(%.3f, %.3f) want(0.600, 0.800) : %s\n", dir.x, dir.y, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  }

  // (4) re-supply the capture path (wrapper-owned, deliberately NOT serialized),
  // then bake the clone and require byte-identical field stats.
  auto cap1 = std::dynamic_pointer_cast<CaptureModuleData>(g1->module("cap"));
  if (not cap1) {
    printf("[roundtrip] clone missing 'cap' module\n");
    return fails + 1;
  }
  cap1->_path = ork::file::Path("/tmp/terrain_rt_clone.exr");
  auto s1 = bakeHeightfield(g1, ctx, dim);

  if (s0.size() == 1 and s1.size() == 1) {
    bool ok = aeq(s0[0]->_min, s1[0]->_min, 1e-6f)   //
              and aeq(s0[0]->_max, s1[0]->_max, 1e-6f) //
              and aeq(s0[0]->_mean, s1[0]->_mean, 1e-6f);
    printf(
        "[roundtrip] bake-equivalence orig(min %.6f max %.6f mean %.6f) vs clone(min %.6f max %.6f mean %.6f) : %s\n",
        s0[0]->_min, s0[0]->_max, s0[0]->_mean, s1[0]->_min, s1[0]->_max, s1[0]->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  } else {
    printf("[roundtrip] capture-count mismatch s0=%zu s1=%zu\n", s0.size(), s1.size());
    fails++;
  }

  printf("[roundtrip] %d failure(s)\n", fails);
  return fails;
}

///////////////////////////////////////////////////////////////////////////////
// per-node cook cache gate — bake a cacheable graph twice. The cold bake
// computes + stores every node's field (anonymously, by content hash) in the
// DataBlockCache; the warm bake (same graph -> same node hashes) loads them, so
// it must (a) produce a byte-identical field and (b) report cache hits for the
// compute nodes (only the Capture sink recomputes).
///////////////////////////////////////////////////////////////////////////////

int terrainCacheTest(Context* ctx, int dim) {
  int fails = 0;

  auto build = [&](const char* cappath) -> dflow::graphdata_ptr_t {
    auto g        = std::make_shared<dflow::GraphData>();
    g->_cacheable = true; // opt in to the per-node cook cache
    auto fbm      = FbmModuleData::createShared();
    fbm->_octaves = 5;
    auto remap    = RemapModuleData::createShared();
    remap->typedInputNamed<dflow::FloatPlugTraits>("scale")->setValue(0.5f);
    remap->typedInputNamed<dflow::FloatPlugTraits>("bias")->setValue(0.5f);
    auto terr = TerraceModuleData::createShared();
    terr->typedInputNamed<dflow::FloatPlugTraits>("steps")->setValue(6.0f);
    auto cap   = CaptureModuleData::createShared();
    cap->_path = ork::file::Path(cappath);
    dflow::GraphData::addModule(g, "fbm", fbm);
    dflow::GraphData::addModule(g, "remap", remap);
    dflow::GraphData::addModule(g, "terr", terr);
    dflow::GraphData::addModule(g, "cap", cap);
    g->safeConnect(remap->inputNamed("In"), fbm->outputNamed("Out"));
    g->safeConnect(terr->inputNamed("In"), remap->outputNamed("Out"));
    g->safeConnect(cap->inputNamed("In"), terr->outputNamed("Out"));
    return g;
  };

  printf("[cachetest] COLD bake:\n");
  auto s1 = bakeHeightfield(build("/tmp/terrain_cache_cold.exr"), ctx, dim);
  int cold_hits = s_lastCookHits;

  printf("[cachetest] WARM bake (same graph -> expect cache hits):\n");
  auto s2 = bakeHeightfield(build("/tmp/terrain_cache_warm.exr"), ctx, dim);
  int warm_hits = s_lastCookHits;

  auto aeq = [](float a, float b, float tol) {
    float d = a - b;
    return (d < 0 ? -d : d) <= tol;
  };
  if (s1.size() == 1 and s2.size() == 1) {
    bool ok = aeq(s1[0]->_min, s2[0]->_min, 1e-6f)   //
              and aeq(s1[0]->_max, s2[0]->_max, 1e-6f) //
              and aeq(s1[0]->_mean, s2[0]->_mean, 1e-6f);
    printf("[cachetest] field cold(mean %.6f) vs warm(mean %.6f) : %s\n", s1[0]->_mean, s2[0]->_mean, ok ? "PASS" : "FAIL");
    if (not ok)
      fails++;
  } else {
    printf("[cachetest] capture-count mismatch\n");
    fails++;
  }
  // the warm bake must actually have hit the cache for the 3 compute nodes
  // (fbm/remap/terr); the Capture sink always recomputes.
  printf("[cachetest] cook hits: cold=%d warm=%d\n", cold_hits, warm_hits);
  if (warm_hits < 3) {
    printf("[cachetest] FAIL: warm bake hit cache only %d times (expected >= 3)\n", warm_hits);
    fails++;
  }

  printf("[cachetest] %d failure(s)\n", fails);
  return fails;
}

} // namespace ork::lev2::terrain

///////////////////////////////////////////////////////////////////////////////
// plug template instantiations (mirrors particle_plugs.cpp — custom plug types
// need explicit describeX/createInstance specializations + reflection or the
// vtables don't link).
///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;
namespace trn   = ork::lev2::terrain;

template <> //
void trn::hfimg_outplugdata_t::describeX(class_t* clazz) {
}
template <> //
void trn::hfimg_inplugdata_t::describeX(class_t* clazz) {
}

template <> //
dflow::inpluginst_ptr_t trn::hfimg_inplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<trn::hfimg_inpluginst_t>(this, minst);
}
template <> //
dflow::outpluginst_ptr_t trn::hfimg_outplugdata_t::createInstance(ModuleInst* minst) const {
  return std::make_shared<trn::hfimg_outpluginst_t>(this, minst);
}

ImplementTemplateReflectionX(trn::hfimg_outplugdata_t, "terrain::hfimgoutplug");
ImplementTemplateReflectionX(trn::hfimg_inplugdata_t, "terrain::hfimginpplug");
