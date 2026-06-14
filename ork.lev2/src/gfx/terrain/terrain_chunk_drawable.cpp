////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// terrain_chunk_drawable.cpp (E.6 → D.5) — see terrain_chunk_drawable.h. The buffer-side
// port of terrain/gpu_chunk.py's ComputeDrawable consumer contract; the layout arithmetic
// MUST stay in lockstep with the Python class (the GLSL side's single source of truth).
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/terrain/terrain_chunk_drawable.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/util/logger.h>

#include <rapidjson/document.h>
#include <OpenImageIO/imageio.h>

#include <fstream>
#include <sstream>

namespace ork::lev2::terrain {

static logchannel_ptr_t logchan_tcd = logger()->configureChannel("TERRAIN", fvec3(0.4, 0.9, 0.4));

///////////////////////////////////////////////////////////////////////////////

void TerrainChunkDrawableData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("hf_asset", &TerrainChunkDrawableData::_hf_asset_name);
  clazz->directProperty("material_asset", &TerrainChunkDrawableData::_material_asset_name);
  clazz->directProperty("chunk", &TerrainChunkDrawableData::_chunk);
}

TerrainChunkDrawableData::TerrainChunkDrawableData() {
}
TerrainChunkDrawableData::~TerrainChunkDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

namespace {
struct TcBootstrap {
  bool _built  = false;
  bool _warned = false;
  std::shared_ptr<ComputeDrawableData> _cdd; // keeps configured state alive
};
} // namespace

drawable_ptr_t TerrainChunkDrawableData::createDrawable() const {
  auto drw   = std::make_shared<ComputeDrawable>();
  auto state = std::make_shared<TcBootstrap>();
  auto self  = this;

  // lazy bootstrap (the D.3 pattern): the GPU side builds on the first onGpuUpdate,
  // after the wire step has resolved the manifest + material by name.
  drw->_liveRecompute = [self, state](Context* ctx, ComputeDrawable* drawable) {
    if (state->_built)
      return; // terrain is static: nothing per-frame (the cull passes run per-VP in onPreRender)
    if (self->_resolved_manifest.empty() or not self->_resolved_material) {
      if (not state->_warned) {
        logchan_tcd->log(
            "TerrainChunkDrawable: unresolved (manifest<%s> material<%s>) — waiting",
            self->_resolved_manifest.c_str(),
            self->_material_asset_name.c_str());
        state->_warned = true;
      }
      return;
    }
    //////////////////////////////////////////////////////////////////
    // 1. the manifest — the self-describing scale contract
    //////////////////////////////////////////////////////////////////
    std::ifstream mf(self->_resolved_manifest);
    if (not mf.good()) {
      logchan_tcd->log("TerrainChunkDrawable: manifest MISSING <%s>", self->_resolved_manifest.c_str());
      state->_built = true;
      return;
    }
    std::stringstream mstrm;
    mstrm << mf.rdbuf();
    std::string mjson = mstrm.str();
    rapidjson::Document doc;
    doc.Parse(mjson.c_str());
    OrkAssert(not doc.HasParseError());
    int dim        = doc["scale"]["dim"].GetInt();
    float extent_m = doc["scale"]["extent_m"].GetFloat();
    float height_m = doc["scale"]["height_m"].GetFloat();
    OrkAssert(doc["channels"].HasMember("height"));
    std::string hfile = doc["channels"]["height"]["file"].GetString();
    if (hfile.find('/') == std::string::npos) { // relative to the manifest's directory
      auto mdir = self->_resolved_manifest.substr(0, self->_resolved_manifest.find_last_of('/'));
      hfile     = mdir + "/" + hfile;
    }
    //////////////////////////////////////////////////////////////////
    // 2. the heights — channel 0 of the baked EXR
    //////////////////////////////////////////////////////////////////
    auto in = OIIO::ImageInput::open(hfile);
    if (not in) {
      logchan_tcd->log("TerrainChunkDrawable: height image MISSING <%s>", hfile.c_str());
      state->_built = true;
      return;
    }
    const auto& spec = in->spec();
    OrkAssert(spec.width == dim and spec.height == dim);
    std::vector<float> px(size_t(spec.width) * spec.height * spec.nchannels);
    in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, px.data());
    in->close();
    std::vector<float> heights(size_t(dim) * dim);
    for (size_t i = 0; i < heights.size(); i++)
      heights[i] = px[i * spec.nchannels];
    //////////////////////////////////////////////////////////////////
    // 3. the layout arithmetic — MUST mirror gpu_chunk.py
    //////////////////////////////////////////////////////////////////
    int chunk  = self->_chunk;
    int cps    = (dim + chunk - 1) / chunk; // chunks per side
    int nchunk = cps * cps;
    int vpc    = chunk * chunk * 6;         // verts per full chunk
    size_t CAM_OFF     = 0;
    size_t ARGS_OFF    = 160; // after CamBlk (64+64+16+16)
    size_t VLIST_OFF   = 192;
    size_t HEIGHTS_OFF = VLIST_OFF + size_t(nchunk) * 4;
    size_t TOTAL       = HEIGHTS_OFF + size_t(dim) * dim * 4;
    (void)vpc;
    (void)extent_m;
    (void)height_m; // baked into the material's generated GLSL at authoring
    //////////////////////////////////////////////////////////////////
    // 4. the SSBO + heights upload
    //////////////////////////////////////////////////////////////////
    auto fxi  = ctx->FXI();
    auto ssbo = fxi->createStorageBuffer(TOTAL);
    {
      auto m = fxi->mapStorageBuffer(ssbo, HEIGHTS_OFF, heights.size() * 4, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, heights.data(), heights.size() * 4);
      fxi->unmapStorageBuffer(m.get());
    }
    //////////////////////////////////////////////////////////////////
    // 5. the ComputeDrawable consumer contract (viewer2 parity)
    //////////////////////////////////////////////////////////////////
    auto mtl   = self->_resolved_material;
    auto fsmtl = mtl->_as_freestyle;
    OrkAssert(fsmtl);
    auto sif = fsmtl->storageBlock("sif_ptex_vtx");
    OrkAssert(sif);
    auto cdd       = std::make_shared<ComputeDrawableData>();
    cdd->_material = mtl;
    cdd->addGraphicsStorage(sif, ssbo);
    cdd->setCameraParams(ssbo, CAM_OFF);
    int cull_groups = (nchunk + 63) / 64;
    struct PassDef { const char* _name; int _gx; };
    PassDef passes[3] = {{"cs_terrain_reset", 1}, {"cs_terrain_cull", cull_groups}, {"cs_terrain_finalize", 1}};
    for (const auto& p : passes) {
      auto cs = fsmtl->computeShader(p._name);
      if (not cs) {
        logchan_tcd->log(
            "TerrainChunkDrawable: material<%s> lacks compute<%s> — was it authored with "
            "TerrainChunkVertexSource?",
            self->_material_asset_name.c_str(),
            p._name);
        state->_built = true;
        return;
      }
      cdd->addComputePass(cs, {{sif, ssbo}}, p._gx, 1, 1);
    }
    cdd->setIndirect(ssbo, ARGS_OFF, nullptr, PrimitiveType::TRIANGLES, 4);
    //////////////////////////////////////////////////////////////////
    // graft onto the live drawable (the D.3 copy block)
    //////////////////////////////////////////////////////////////////
    drawable->_passes          = cdd->_passes;
    drawable->_camParamsSSBO   = cdd->_camParamsSSBO;
    drawable->_camParamsOffset = cdd->_camParamsOffset;
    drawable->_material        = cdd->_material;
    drawable->_graphicsStorage = cdd->_graphicsStorage;
    drawable->_argsSSBO        = cdd->_argsSSBO;
    drawable->_argsOffset      = cdd->_argsOffset;
    drawable->_indexSSBO       = cdd->_indexSSBO;
    drawable->_primtype        = cdd->_primtype;
    drawable->_indexSize       = cdd->_indexSize;
    state->_cdd   = cdd;
    state->_built = true;
    logchan_tcd->log(
        "TerrainChunkDrawable: materialized (dim<%d> chunks<%dx%d> ssbo<%.1fMB> mtl<%s>)",
        dim,
        cps,
        cps,
        double(TOTAL) / 1e6,
        self->_material_asset_name.c_str());
  };

  auto draw_raw = drw.get();
  drw->setRenderLambda([draw_raw](RenderContextInstData& RCID) { draw_raw->_renderIndirect(RCID); });
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::terrain

ImplementReflectionX(ork::lev2::terrain::TerrainChunkDrawableData, "TerrainChunkDrawableData");
