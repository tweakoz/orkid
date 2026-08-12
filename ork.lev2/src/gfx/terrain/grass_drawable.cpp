////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// grass_drawable.cpp (GRASS phase 2) — see grass_drawable.h. The buffer-side half of the
// GrassFieldSource contract: the field SSBO layout here MUST stay in lockstep with the
// generated GLSL (the Python class is the single source of truth for the shader side).
//
// TASK-EVIDENCE LAW. Every failure mode below is a NAMED refusal that draws nothing —
// deliberately the opposite of the terrain chunk drawable's mesh->pull step-down cascade.
// Grass has no taskless form: a "plausible" carpet drawn without the amplification stage
// would be a different picture claiming to be this one.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/terrain/grass_drawable.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/renderphasestats.h> // CullStats: the perf HUD's emitted-workgroup row
#include <ork/util/logger.h>
#include <cstring>

#include <rapidjson/document.h>
#include <OpenImageIO/imageio.h>

#include <fstream>
#include <sstream>

namespace ork::lev2::terrain {

static logchannel_ptr_t logchan_grass = logger()->configureChannel("GRASS", fvec3(0.5, 0.9, 0.4));

///////////////////////////////////////////////////////////////////////////////

void GrassDrawableData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("hf_asset", &GrassDrawableData::_hf_asset_name);
  clazz->directProperty("material_asset", &GrassDrawableData::_material_asset_name);
  clazz->directProperty("field_dim", &GrassDrawableData::_field_dim);
  clazz->directProperty("tile_size", &GrassDrawableData::_tile_size);
  clazz->directProperty("grid_dim", &GrassDrawableData::_grid_dim);
  clazz->directProperty("supertile", &GrassDrawableData::_supertile);
  clazz->directProperty("hzb_occlusion", &GrassDrawableData::_hzb_occlusion);
  clazz->directProperty("hzb_bias", &GrassDrawableData::_hzb_bias);
  clazz->directProperty("lod0_radius", &GrassDrawableData::_lod0_radius);
  clazz->directProperty("lod1_radius", &GrassDrawableData::_lod1_radius);
  clazz->directProperty("cull_radius", &GrassDrawableData::_cull_radius);
  clazz->directProperty("fade_pow", &GrassDrawableData::_fade_pow);
  clazz->directProperty("lod_ceiling", &GrassDrawableData::_lod_ceiling);
  clazz->directProperty("height_channel", &GrassDrawableData::_height_channel);
  clazz->directProperty("normal_channel", &GrassDrawableData::_normal_channel);
  clazz->directProperty("density_channel", &GrassDrawableData::_density_channel);
  clazz->directProperty("dryness_channel", &GrassDrawableData::_dryness_channel);
  clazz->directProperty("density_scale", &GrassDrawableData::_density_scale);
  clazz->directProperty("blade_height", &GrassDrawableData::_blade_height);
  clazz->directProperty("blade_height_var", &GrassDrawableData::_blade_height_var);
  clazz->directProperty("blade_width", &GrassDrawableData::_blade_width);
  clazz->directProperty("blade_taper", &GrassDrawableData::_blade_taper);
  clazz->directProperty("clump_radius", &GrassDrawableData::_clump_radius);
  clazz->directProperty("clump_lean", &GrassDrawableData::_clump_lean);
  clazz->directProperty("clump_phase_var", &GrassDrawableData::_clump_phase_var);
  clazz->directProperty("clump_hue_var", &GrassDrawableData::_clump_hue_var);
  clazz->directProperty("clump_height_var", &GrassDrawableData::_clump_height_var);
  clazz->directProperty("color_a", &GrassDrawableData::_color_a);
  clazz->directProperty("color_b", &GrassDrawableData::_color_b);
  clazz->directProperty("dry_color", &GrassDrawableData::_dry_color);
  clazz->directProperty("dry_fraction", &GrassDrawableData::_dry_fraction);
  clazz->directProperty("dry_fade_start", &GrassDrawableData::_dry_fade_start);
  clazz->directProperty("dry_fade_end", &GrassDrawableData::_dry_fade_end);
  clazz->directProperty("backlit", &GrassDrawableData::_backlit);
  clazz->directProperty("wind_dir", &GrassDrawableData::_wind_dir);
  clazz->directProperty("wind_amp", &GrassDrawableData::_wind_amp);
  clazz->directProperty("wind_freq", &GrassDrawableData::_wind_freq);
  clazz->directProperty("stereo_widen", &GrassDrawableData::_stereo_widen);
}

GrassDrawableData::GrassDrawableData() {
  // The carpet is definitionally a draw-last surface: it is the densest overdraw in the
  // frame and it is the one drawable the forward node re-issues into a depth-WRITABLE
  // continuation of the color pass (ForwardPbrNodeImpl::_render_drawlast), which is
  // selected by exactly this sort key. Scenes may still override via the SortKey property.
  _sortkey = IRenderable::kLastRenderableSortKey;
}
GrassDrawableData::~GrassDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

namespace {

// how many onGpuUpdate frames the engagement watchdog waits before it calls the pass
// taskless. The first frames of a scene compile pipelines and may not draw this material.
constexpr int kTaskEvidenceFrames = 60;

struct GrassBootstrap {
  bool _built   = false; // bootstrap ran (successfully or refused) — never runs twice
  bool _refused = false; // it refused; suppress the engagement watchdog (nothing was drawn)
  bool _warned  = false; // the unresolved-assets line is once-only
  bool _task_verdict = false; // the engagement watchdog reported
  int _frames = 0;
  std::shared_ptr<ComputeDrawableData> _cdd; // keeps the configured state alive
  // Emitted-workgroup telemetry: the stats SSBO (a cumulative count the task stage atomically
  // adds its emitted mesh workgroups to) sampled at a frame boundary, so the per-frame figure is a
  // difference. The host zeroed it at creation and never writes it again — the only condition under
  // which a host read sees a shader's writes on this platform. Two consumers off the SAME read:
  // the perf HUD (per-frame, while CullStats readback is enabled) and the ORKID_GRASS_CULL_DEBUG
  // stdout line (a 60-frame average, its own mark).
  FxShaderStorageBuffer* _statsSSBO = nullptr;
  uint32_t _dbg_mark  = 0;
  int _dbg_frames     = 0;
  uint32_t _hud_mark  = 0;
  bool _hud_primed    = false; // false => the next read only re-marks (the gap since the last read is not one frame)
};

// how many frames the emitted-workgroup telemetry averages over before it prints a line.
constexpr int kCullDebugWindow = 60;

bool grassCullDebug() {
  static const bool s_on = []() {
    const char* e = getenv("ORKID_GRASS_CULL_DEBUG");
    return e and atoi(e) != 0;
  }();
  return s_on;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t GrassDrawableData::createDrawable() const {
  auto drw            = std::make_shared<ComputeDrawable>();
  drw->_drawable_type = "grass"_crcu; // enumerable via Scene::drawableNodesWithType
  drw->_sortkey       = _sortkey;     // hand-built ComputeDrawable: nothing else copies this
  auto state          = std::make_shared<GrassBootstrap>();
  auto self           = this;

  // THE DRAW ITSELF. A ComputeDrawable built by hand (rather than through
  // ComputeDrawableData::createDrawable) carries no render lambda, and a drawable with no
  // render lambda is enqueued and then silently does nothing — every bootstrap line prints,
  // the field uploads, and not one pass is ever recorded. Set it up front: _renderIndirect
  // returns immediately while the material is still unresolved.
  auto draw_raw = drw.get();
  drw->setRenderLambda([draw_raw](RenderContextInstData& RCID) { draw_raw->_renderIndirect(RCID); });

  // lazy bootstrap (the D.3 pattern): the GPU side builds on the first onGpuUpdate,
  // after the wire step has resolved the manifest + material by name.
  drw->_liveRecompute = [self, state](Context* ctx, ComputeDrawable* drawable) {
    if (state->_built) {
      //////////////////////////////////////////////////////////////////
      // TASK-EVIDENCE: the honest counter (VkContext bumps it only when the bound pass
      // actually carried a task stage). A carpet that renders without amplification is
      // a different picture — say so by name rather than let it pass as this one.
      //////////////////////////////////////////////////////////////////
      if (not state->_refused and not state->_task_verdict) {
        state->_frames++;
        if (state->_frames >= kTaskEvidenceFrames) {
          state->_task_verdict = true;
          int n = ctx->taskShaderDrawCount();
          if (n == 0)
            printf(
                "GRASS-TASK-NOT-ENGAGED: material<%s> drew %d frames but the context counted 0 "
                "task+mesh draws — the pass ran WITHOUT its amplification stage. The carpet has no "
                "taskless form; what is on screen is not this drawable.\n",
                self->_material_asset_name.c_str(),
                state->_frames);
          else
            logchan_grass->log("GrassDrawable: task stage ENGAGED — %d task+mesh draws in %d frames",
                               n, state->_frames);
        }
      }
      //////////////////////////////////////////////////////////////////
      // CULL TELEMETRY: the mesh workgroups the task stage actually emitted, which is the
      // whole point of the compaction — a number the picture cannot report. Two consumers
      // share ONE read: the perf HUD's per-frame row (only while the HUD has CullStats
      // readback on — off => zero readback cost, as for every other cull funnel) and the
      // ORKID_GRASS_CULL_DEBUG stdout line, which keeps its own 60-frame average.
      //////////////////////////////////////////////////////////////////
      const bool dbg_on = grassCullDebug();
      const bool hud_on = CullStats::instance().enabled();
      if (not hud_on)
        state->_hud_primed = false; // gap since the last read: the next delta is not one frame
      if (state->_statsSSBO and (dbg_on or hud_on)) {
        if (dbg_on)
          state->_dbg_frames++;
        const bool dbg_due = dbg_on and (state->_dbg_frames >= kCullDebugWindow);
        if (hud_on or dbg_due) {
          uint32_t cum = 0;
          auto m = ctx->FXI()->mapStorageBuffer(state->_statsSSBO, 0, 4, BufferMapAccess::READ_ONLY);
          std::memcpy(&cum, m->_mappedaddr, 4);
          m->unmap();
          if (hud_on) {
            const uint32_t per_frame = cum - state->_hud_mark; // unsigned: wraps correctly
            if (state->_hud_primed)
              CullStats::instance().setGrassWorkgroups(per_frame);
            state->_hud_primed = true;
            state->_hud_mark   = cum;
          }
          if (dbg_due) {
            const uint32_t emitted = cum - state->_dbg_mark; // unsigned: wraps correctly
            logchan_grass->log(
                "GrassDrawable: %u mesh workgroups emitted over %d frames = %.0f/frame "
                "(%d supertiles dispatched, ceiling %d tiles x %.0f clusters)",
                emitted,
                state->_dbg_frames,
                double(emitted) / double(state->_dbg_frames),
                (self->_grid_dim + self->_supertile - 1) / self->_supertile *
                    ((self->_grid_dim + self->_supertile - 1) / self->_supertile),
                self->_grid_dim * self->_grid_dim,
                self->_lod_ceiling);
            state->_dbg_mark   = cum;
            state->_dbg_frames = 0;
          }
        }
      }
      return; // the field is static; the task stage re-centers itself on the eye each frame
    }
    if (self->_resolved_manifest.empty() or not self->_resolved_material) {
      if (not state->_warned) {
        logchan_grass->log(
            "GrassDrawable: unresolved (manifest<%s> material<%s>) — waiting",
            self->_resolved_manifest.c_str(),
            self->_material_asset_name.c_str());
        state->_warned = true;
      }
      return;
    }
    auto refuse = [&]() { // one exit for every hard failure: built (no retry), drew nothing
      state->_built   = true;
      state->_refused = true;
    };
    //////////////////////////////////////////////////////////////////
    // 0. the DEVICE. No task shader, no grass — never a lesser picture.
    //////////////////////////////////////////////////////////////////
    if (not ctx->supportsTaskShader()) {
      printf(
          "GRASS-NO-TASK-SHADER: this device does not expose the task (amplification) stage; the "
          "grass carpet is built ENTIRELY by a task+mesh pass and has no taskless form — refusing "
          "to draw material<%s>.\n",
          self->_material_asset_name.c_str());
      refuse();
      return;
    }
    //////////////////////////////////////////////////////////////////
    // 1. the manifest — the self-describing scale contract (terrain pattern)
    //////////////////////////////////////////////////////////////////
    std::ifstream mf(self->_resolved_manifest);
    if (not mf.good()) {
      printf("GRASS-MANIFEST-MISSING: <%s> (hf asset<%s>) — refusing.\n",
             self->_resolved_manifest.c_str(),
             self->_hf_asset_name.c_str());
      refuse();
      return;
    }
    std::stringstream mstrm;
    mstrm << mf.rdbuf();
    std::string mjson = mstrm.str();
    rapidjson::Document doc;
    doc.Parse(mjson.c_str());
    OrkAssert(not doc.HasParseError());
    const int bake_dim   = doc["scale"]["dim"].GetInt();
    const float extent_m = doc["scale"]["extent_m"].GetFloat();
    auto mdir            = self->_resolved_manifest.substr(0, self->_resolved_manifest.find_last_of('/'));
    //////////////////////////////////////////////////////////////////
    // 2. the baked channels -> the field grid. Decode at bake resolution, filter down to
    //    _field_dim with the SAME resampler the terrain render/collider use, then DROP the
    //    CPU images immediately (an 8192^2 RGBA float EXR is 1GB — nothing survives this
    //    scope but the packed field).
    //////////////////////////////////////////////////////////////////
    const int fd = self->_field_dim;
    if (fd <= 0 or fd > bake_dim) {
      printf("GRASS-FIELD-DIM-INVALID: field_dim<%d> against a bake dim<%d> (must be 1..bake_dim) "
             "— refusing.\n",
             fd,
             bake_dim);
      refuse();
      return;
    }
    auto channelFile = [&](const std::string& ch, std::string& out) -> bool {
      if (ch.empty() or not doc["channels"].HasMember(ch.c_str()))
        return false;
      std::string f = doc["channels"][ch.c_str()]["file"].GetString();
      if (f.find('/') == std::string::npos)
        f = mdir + "/" + f;
      out = f;
      return true;
    };
    // decode one channel image and resample it to fd x fd. C = 1 (scalar) or 4 (vector).
    auto loadResampled = [&](const std::string& file, int C, std::vector<float>& out) -> bool {
      auto in = OIIO::ImageInput::open(file);
      if (not in)
        return false;
      const auto& spec = in->spec();
      std::vector<float> raw(size_t(spec.width) * spec.height * spec.nchannels);
      in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, raw.data());
      in->close();
      const int sd = spec.width;
      if (spec.height != sd)
        return false;
      std::vector<float> src(size_t(sd) * sd * C, 0.0f);
      const int nc = std::min(spec.nchannels, C);
      for (size_t i = 0; i < size_t(sd) * sd; i++)
        for (int c = 0; c < nc; c++)
          src[i * C + c] = raw[i * spec.nchannels + c];
      if (sd == fd) {
        out = std::move(src);
        return true;
      }
      Image simg;
      simg.initWithFormat(sd, sd, (C == 1) ? EBufferFormat::R32F : EBufferFormat::RGBA32F);
      std::memcpy((void*)simg._data->data(), src.data(), src.size() * sizeof(float));
      Image dimg;
      dimg.resampledOf(simg, fd, fd, Image::ResampleFilter::TRIANGLE);
      out.resize(size_t(fd) * fd * C);
      std::memcpy(out.data(), dimg._data->data(), out.size() * sizeof(float));
      return true;
    };
    // REQUIRED channels: without heights the blades float, without density the carpet is
    // uniform, without dryness every blade is the same green — the scene DECLARES which
    // channels it expects, so a missing one is a refusal, never a defaulted plane.
    struct ChanReq {
      const std::string& _name;
      int _comps;
      std::vector<float>* _dst;
    };
    std::vector<float> heights, normals, density, dryness;
    ChanReq required[4] = {
        {self->_height_channel, 1, &heights},
        {self->_normal_channel, 4, &normals},
        {self->_density_channel, 1, &density},
        {self->_dryness_channel, 1, &dryness},
    };
    for (const auto& req : required) {
      std::string file;
      if (not channelFile(req._name, file)) {
        printf("GRASS-CHANNEL-MISSING: manifest<%s> declares no channel<%s> — the terrain DSL must "
               "capture it (cache=True) before the carpet can read it. Refusing.\n",
               self->_resolved_manifest.c_str(),
               req._name.c_str());
        refuse();
        return;
      }
      if (not loadResampled(file, req._comps, *req._dst)) {
        printf("GRASS-CHANNEL-UNREADABLE: channel<%s> file<%s> failed to decode as a square float "
               "image — refusing.\n",
               req._name.c_str(),
               file.c_str());
        refuse();
        return;
      }
    }
    // the height range the shader unpacks against (manifest stats, TRUE METERS)
    float y_min = doc["channels"][self->_height_channel.c_str()]["min"].GetFloat();
    float y_max = doc["channels"][self->_height_channel.c_str()]["max"].GetFloat();
    //////////////////////////////////////////////////////////////////
    // 3. the field SSBO — [ CamBlk | g_meta | g_meta2 | g_field[] ], std430. MUST mirror
    //    GrassFieldSource (sif_grass_field). g_field is one vec4 per texel:
    //      .x = height_m
    //      .y = density01
    //      .z = packHalf2x16(nrm.x, nrm.z) bit-cast to float — the shader unpacks it and
    //           reconstructs nrm.y from the unit-length constraint. Half precision costs
    //           ~1e-4 per component (the terrain frame buffer packs its normals the same
    //           way); paying two floats for the pair would cost the dryness slot.
    //      .w = dryness01
    //////////////////////////////////////////////////////////////////
    const size_t CAM_OFF   = 0;   // CamBlk: mat4 vp, mat4 ivp, vec4 eye, vec4 misc
    const size_t META_OFF  = 160; // vec4 g_meta  (field_dim, extent_m, y_min, y_range)
    const size_t META2_OFF = 176; // vec4 g_meta2 (reserved)
    const size_t FIELD_OFF = 192; // vec4 g_field[fd*fd]
    const size_t TOTAL     = FIELD_OFF + size_t(fd) * fd * 16;

    // same semantics as image_fmt_convert float_to_half / the terrain frame pack
    auto f2h = [](float f) -> uint32_t {
      uint32_t bits;
      std::memcpy(&bits, &f, 4);
      uint32_t sign = (bits >> 16) & 0x8000;
      int32_t exp32 = int32_t((bits >> 23) & 0xFF) - 127 + 15;
      uint32_t mant = (bits & 0x007FFFFF);
      if (exp32 <= 0)
        return sign; // underflow to zero
      if (exp32 >= 31)
        return sign | 0x7C00; // overflow to inf
      return sign | (uint32_t(exp32) << 10) | (mant >> 13);
    };
    std::vector<float> field(size_t(fd) * fd * 4);
    for (size_t i = 0; i < size_t(fd) * fd; i++) {
      field[i * 4 + 0] = heights[i];
      field[i * 4 + 1] = density[i]; // density01 as baked; density_scale applies in-shader (A8)
      uint32_t nrm_h   = f2h(normals[i * 4 + 0]) | (f2h(normals[i * 4 + 2]) << 16);
      std::memcpy(&field[i * 4 + 2], &nrm_h, 4); // packHalf2x16(nrm.x, nrm.z)
      field[i * 4 + 3] = dryness[i];
    }
    heights.clear();
    heights.shrink_to_fit();
    normals.clear();
    normals.shrink_to_fit();
    density.clear();
    density.shrink_to_fit();
    dryness.clear();
    dryness.shrink_to_fit();

    auto fxi = ctx->FXI();
    // BAR: written once here, GPU-read every task workgroup; the only per-frame CPU touch is
    // the tiny CamBlk write in drawable_compute (same residency choice as the terrain field).
    auto ssbo = fxi->createStorageBuffer(TOTAL, StorageBufferUsage::DEFAULT, BufferResidency::BAR);
    auto upload = [&](size_t off, const void* src, size_t bytes) {
      auto m = fxi->mapStorageBuffer(ssbo, off, bytes, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, src, bytes);
      fxi->unmapStorageBuffer(m.get());
    };
    {
      float meta[4] = {float(fd), extent_m, y_min, std::max(y_max - y_min, 1e-6f)};
      upload(META_OFF, meta, sizeof(meta));
      float meta2[4] = {0, 0, 0, 0};
      upload(META2_OFF, meta2, sizeof(meta2));
    }
    upload(FIELD_OFF, field.data(), field.size() * 4);
    field.clear();
    field.shrink_to_fit();
    //////////////////////////////////////////////////////////////////
    // 4. the material contract. BOTH techniques must exist: the color pass and its depth-
    //    prepass twin share ONE task shader, so a material carrying only one of them would
    //    emit a different blade set per pass (z-fight / prepass disagreement).
    //////////////////////////////////////////////////////////////////
    auto mtl   = self->_resolved_material;
    auto fsmtl = mtl->_as_freestyle;
    OrkAssert(fsmtl);
    auto tek = fsmtl->technique("FWD_SSBO_CUSTOM_MESH");
    auto dpp = fsmtl->technique("FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS");
    if (not tek or not dpp) {
      printf(
          "GRASS-MATERIAL-INCOMPLETE: material<%s> lacks %s%s%s — a grass material MUST be authored "
          "with GrassFieldSource (task stage ON), and the color pass and its depth-prepass twin must "
          "share it. Refusing.\n",
          self->_material_asset_name.c_str(),
          tek ? "" : "FWD_SSBO_CUSTOM_MESH",
          (not tek and not dpp) ? " and " : "",
          dpp ? "" : "FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS");
      refuse();
      return;
    }
    auto sif = fsmtl->storageBlock("sif_grass_field");
    if (not sif) {
      printf("GRASS-FIELD-BLOCK-MISSING: material<%s> has the mesh techniques but no "
             "sif_grass_field storage block (stale material — re-materialize the scene). Refusing.\n",
             self->_material_asset_name.c_str());
      refuse();
      return;
    }
    // sif_ptex_vtx is emitted as a DUMMY block (a lone uint) by the grass codegen — the real
    // per-texel data lives in sif_grass_field — but the descriptor is still in the pipeline
    // layout, and an unbound one fails Vulkan validation at draw. Bind a token buffer.
    auto vtx_sif = fsmtl->storageBlock("sif_ptex_vtx");
    FxShaderStorageBuffer* dummySSBO = nullptr;
    // sif_hzb — the engine's 1-phase max-depth pyramid, read by the TASK stage's tile occlusion
    // test. It is a graphics-stage descriptor here (the cull is the amplification stage, not a
    // compute pass), so ComputeDrawable re-points it at the live pyramid every frame. Absent =>
    // a material generated before the occlusion test existed: the carpet still draws (the test is
    // gated on CamBlk.misc.yzw, which stays zero), so this is a log, never a refusal.
    auto hzb_sif   = fsmtl->storageBlock("sif_hzb");
    auto stats_sif = fsmtl->storageBlock("sif_grass_stats");
    if (not hzb_sif)
      logchan_grass->log(
          "GrassDrawable: material<%s> has no sif_hzb block — tile occlusion is INERT this run "
          "(re-materialize the scene to pick it up)",
          self->_material_asset_name.c_str());
    //////////////////////////////////////////////////////////////////
    // 5. A8 — every authored knob rides the material's UBO packs. NOTHING here is ever
    //    baked into shader text, and each is re-bindable live afterwards.
    //////////////////////////////////////////////////////////////////
    std::vector<std::string> unbound;
    auto bindV4 = [&](const char* name, fvec4 v) {
      auto par = fsmtl->param(name);
      if (par)
        mtl->bindParam(par, v);
      else
        unbound.push_back(name);
    };
    bindV4("GrassField", fvec4(self->_tile_size, float(self->_grid_dim), self->_cull_radius, self->_density_scale));
    bindV4("GrassLod", fvec4(self->_lod0_radius, self->_lod1_radius, self->_fade_pow, self->_lod_ceiling));
    bindV4("GrassBlade", fvec4(self->_blade_height, self->_blade_height_var, self->_blade_width, self->_blade_taper));
    bindV4("GrassClump", fvec4(self->_clump_radius, self->_clump_lean, self->_clump_phase_var, self->_clump_hue_var));
    // second clump pack: x = per-clump height amplitude, yzw reserved (must stay zero).
    bindV4("GrassClump2", fvec4(self->_clump_height_var, 0, 0, 0));
    bindV4("GrassColorA", fvec4(self->_color_a, 1.0f));
    bindV4("GrassColorB", fvec4(self->_color_b, 1.0f));
    bindV4("GrassDryColor", fvec4(self->_dry_color, 1.0f));
    bindV4("GrassDry", fvec4(self->_dry_fraction, self->_dry_fade_start, self->_dry_fade_end, self->_backlit));
    bindV4("GrassStereo", fvec4(self->_stereo_widen, 0, 0, 0));
    // COMPACTION + OCCLUSION. The supertile edge is authored, so it is checked HERE against the
    // capacity the shader baked: out of range means the dispatch below would cover tiles the task
    // stage clamps itself out of visiting — a carpet with holes in it, from a number nothing else
    // would ever complain about.
    int supertile = self->_supertile;
    if (supertile < 1 or supertile > kSupertileMax) {
      printf("GRASS-SUPERTILE-RANGE: supertile<%d> is outside [1..%d], the survivor capacity the "
             "task payload is built with — clamping (a larger block would leave tiles unvisited).\n",
             supertile,
             kSupertileMax);
      supertile = std::max(1, std::min(supertile, kSupertileMax));
    }
    // .w arms the emitted-workgroup atomic. ALWAYS on: the perf HUD's grass row is a
    // standing readout, and the cost is one workgroup-uniform atomic per supertile per
    // frame (a few hundred). What stays opt-in is the READBACK (CullStats) and the
    // ORKID_GRASS_CULL_DEBUG stdout line.
    bindV4("GrassCull",
           fvec4(float(supertile),
                 self->_hzb_bias,
                 (self->_hzb_occlusion and hzb_sif) ? 1.0f : 0.0f,
                 1.0f));
    // wind: amp <= 0 INHERITS whatever the scene bound (shared with the trees) — overriding
    // with a zero pack would freeze the carpet while the canopy still moved.
    if (self->_wind_amp > 0.0f) {
      bindV4("WindDir", fvec4(self->_wind_dir, 0.0f));
      bindV4("WindParams", fvec4(self->_wind_amp, self->_wind_freq, 0, 0));
    }
    if (not unbound.empty()) {
      std::string names;
      for (const auto& n : unbound)
        names += (names.empty() ? "" : ", ") + n;
      logchan_grass->log(
          "GrassDrawable: material<%s> exposes no param(s) [%s] — those knobs are INERT this run "
          "(the DSL surface must declare them in displace_params)",
          self->_material_asset_name.c_str(),
          names.c_str());
    }
    //////////////////////////////////////////////////////////////////
    // 6. the ComputeDrawable consumer contract. There is no compute pass and no indirect
    //    draw: the task stage IS the cull/LOD, so the counts below are TASK workgroups —
    //    one per SUPERTILE — and each amplifies into ONE mesh grid covering every tile of
    //    its block that survived radius, frustum and occlusion.
    //////////////////////////////////////////////////////////////////
    const uint32_t sdim = uint32_t((self->_grid_dim + supertile - 1) / supertile);
    auto cdd       = std::make_shared<ComputeDrawableData>();
    cdd->_material = mtl;
    cdd->addGraphicsStorage(sif, ssbo);
    if (vtx_sif) { // the dummy vertex block — bound so the descriptor set is complete
      dummySSBO = fxi->createStorageBuffer(16, StorageBufferUsage::DEFAULT, BufferResidency::BAR);
      cdd->addGraphicsStorage(vtx_sif, dummySSBO);
    }
    if (stats_sif) { // the carpet's ONE writable word: emitted mesh workgroups (cull telemetry)
      auto stats = fxi->createStorageBuffer(16, StorageBufferUsage::DEFAULT, BufferResidency::BAR);
      uint32_t zero[4] = {0, 0, 0, 0};
      {
        auto m = fxi->mapStorageBuffer(stats, 0, sizeof(zero), BufferMapAccess::WRITE_ONLY);
        std::memcpy(m->_mappedaddr, zero, sizeof(zero));
        fxi->unmapStorageBuffer(m.get());
      } // ... and never again: a host write in a frame shadows the shader's on readback here
      cdd->addGraphicsStorage(stats_sif, stats);
      state->_statsSSBO = stats;
    }
    cdd->setCameraParams(ssbo, CAM_OFF);
    cdd->setMeshDraw(tek, sdim, sdim, 1);
    state->_cdd = cdd;

    drawable->_material        = cdd->_material;
    drawable->_graphicsStorage = cdd->_graphicsStorage;
    drawable->_camParamsSSBO   = cdd->_camParamsSSBO;
    drawable->_camParamsOffset = cdd->_camParamsOffset;
    drawable->_meshTechnique   = cdd->_meshTechnique;
    drawable->_meshGroups[0]   = cdd->_meshGroups[0];
    drawable->_meshGroups[1]   = cdd->_meshGroups[1];
    drawable->_meshGroups[2]   = cdd->_meshGroups[2];
    drawable->_hzbGraphicsBlock = hzb_sif; // re-pointed at this frame's pyramid in _renderIndirect
    drawable->_spvrFamily      = "grass"; // names this producer in the [SPVR:CDSEL] bind-time line
    state->_built              = true;
    logchan_grass->log(
        "GrassDrawable: material<%s> field<%dx%d over %.1fm, y[%.2f..%.2f]> tiles<%dx%d @ %.2fm> "
        "cull<%.1fm> -> %u task workgroups (%dx%d supertiles of %d tiles, occlusion %s)",
        self->_material_asset_name.c_str(),
        fd,
        fd,
        extent_m,
        y_min,
        y_max,
        self->_grid_dim,
        self->_grid_dim,
        self->_tile_size,
        self->_cull_radius,
        sdim * sdim,
        sdim,
        sdim,
        supertile * supertile,
        (self->_hzb_occlusion and hzb_sif) ? "ON" : "off");
  };
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::terrain

ImplementReflectionX(ork::lev2::terrain::GrassDrawableData, "GrassDrawableData");
