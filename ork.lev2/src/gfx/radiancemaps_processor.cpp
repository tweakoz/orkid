////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <atomic>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <boost/filesystem.hpp>
#include <ork/pch.h>
#include <ork/util/logger.h>
#include <ork/lev2/gfx/radiancemaps_processor.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gpumicrotask.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/image.h>
#include <ork/asset/Asset.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/async_tracker.h>
#include <ork/file/file.h>

namespace ork::lev2 {

static logchannel_ptr_t logchan_gen = logger()->configureChannel("ENVMAPGEN", fvec3(0.8, 0.8, 0.1), true);

extern context_ptr_t gloadercontext;

////////////////////////////////////////////////////////////////////////////////
// Shader path for environment filtering (copied from material_pbr_gen.cpp)
////////////////////////////////////////////////////////////////////////////////

static file::Path filterenv_shader_path() {
  return file::Path("orkshader://pbr_filterenv.fxv2");
}

////////////////////////////////////////////////////////////////////////////////
// TileParams implementation
////////////////////////////////////////////////////////////////////////////////

fvec4 TileParams::getNDC(int tex_width, int tex_height) const {
  float ndc_x = (float)x / tex_width * 2.0f - 1.0f;
  float ndc_y = (float)y / tex_height * 2.0f - 1.0f;
  float ndc_w = (float)width / tex_width * 2.0f;
  float ndc_h = (float)height / tex_height * 2.0f;
  return fvec4(ndc_x, ndc_y, ndc_w, ndc_h);
}

fvec4 TileParams::getUV(int tex_width, int tex_height) const {
  float uv_x = (float)x / tex_width;
  float uv_y = (float)y / tex_height;
  float uv_w = (float)width / tex_width;
  float uv_h = (float)height / tex_height;
  return fvec4(uv_x, uv_y, uv_w, uv_h);
}

////////////////////////////////////////////////////////////////////////////////
// Tile rendering methods
////////////////////////////////////////////////////////////////////////////////

void EnvMapProcessor::renderSpecularTile(
    Context* ctx,
    const TileParams& tile,
    texture_ptr_t src_tex,
    rtbuffer_ptr_t target_buffer,
    float roughness,
    int num_samples) {

  // This will be called from a task on the GPU thread
  // Render a single tile of the specular filtered environment map

  auto dwi       = ctx->DWI();
  int tex_width  = target_buffer->_width;
  int tex_height = target_buffer->_height;

  // Calculate NDC and UV coordinates for this tile
  fvec4 ndc = tile.getNDC(tex_width, tex_height);
  fvec4 uv  = tile.getUV(tex_width, tex_height);

  // Render the tile using quad2D
  // dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
}

////////////////////////////////////////////////////////////////////////////////
// renderDiffuseTile
//////////////////////////////////////////////////////////////////////////////////

void EnvMapProcessor::renderDiffuseTile(Context* ctx, const TileParams& tile, texture_ptr_t src_tex, rtbuffer_ptr_t target_buffer) {

  // This will be called from a task on the GPU thread
  // Render a single tile of the diffuse filtered environment map

  auto dwi       = ctx->DWI();
  int tex_width  = target_buffer->_width;
  int tex_height = target_buffer->_height;

  // Calculate NDC and UV coordinates for this tile
  fvec4 ndc = tile.getNDC(tex_width, tex_height);
  fvec4 uv  = tile.getUV(tex_width, tex_height);

  // Render the tile using quad2D
  // dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
}

////////////////////////////////////////////////////////////////////////////////
// MT2 (JUL05_GPUMICROTASK §2.6): shared per-level filtering primitives.
//
// The filtering math is deterministic + stateless per tile (T12): the SAME
// helpers below drive BOTH the burst taskgraph (createFilteringTaskGraph, one
// TaskPhase per level on gloadercontext) AND the sliced microtask
// (RadiancePrefilterMicrotask, one Context::executeInlineGpuJob per level on
// any context). Shared code = byte-identical output (§5 determinism gate),
// regardless of which context filters or how the submits are sequenced.
//
// A8: the filterenv material's tweakable inputs (roughness / sample count /
// image dims) ride bindParam* UBO uniforms, NEVER inlined into shader text.
////////////////////////////////////////////////////////////////////////////////

namespace {

struct EnvFilterState {
  // inputs (frozen for the whole job — T12: no per-slice-varying state)
  texture_ptr_t _rawenvmap;
  bool          _is_equirectangular = false;
  bool          _is_hdr_source      = false;
  int           _tex_width          = 0;
  int           _tex_height         = 0;
  std::string   _tex_name;
  EBufferFormat _capture_format       = EBufferFormat::RGBA8;
  EBufferFormat _render_target_format = EBufferFormat::RGBA32F;
  int                _num_roughness_levels = 10;
  std::vector<float> _roughness_values;
  std::vector<int>   _diffuse_mips; // mip indices to render (4x4 floor)
  // materials (built once by initFilterMaterials)
  std::shared_ptr<FreestyleMaterial> _specular_material;
  std::shared_ptr<FreestyleMaterial> _diffuse_material;
  // outputs (appended one entry per rendered level, in level order)
  rtgroup_list_t                   _spec_rtgroups, _diff_rtgroups;
  rtbuffer_list_t                  _spec_rtbuffers, _diff_rtbuffers;
  std::vector<captureasync_ptr_t>  _spec_futures, _diff_futures;
  std::vector<capturebuffer_ptr_t> _spec_capbufs, _diff_capbufs;
  image_list_t                     _debug_spec_images, _debug_diff_images;
};

void initEnvFilterState(EnvFilterState& st, texture_ptr_t rawenvmap, bool is_equirectangular, bool is_hdr_source) {
  st._rawenvmap            = rawenvmap;
  st._is_equirectangular   = is_equirectangular;
  st._is_hdr_source        = is_hdr_source;
  st._tex_width            = rawenvmap->_width;
  st._tex_height           = rawenvmap->_height;
  st._tex_name             = file::Path(rawenvmap->_debugName).toBFS().stem().string();
  // HDR: render RGBA32F, capture RGBA16F to preserve dynamic range. LDR: capture RGBA8.
  st._capture_format       = is_hdr_source ? EBufferFormat::RGBA16F : EBufferFormat::RGBA8;
  st._render_target_format = EBufferFormat::RGBA32F;
  st._num_roughness_levels = 10;
  const float roughness_power = 0.5f;
  for (int i = 0; i < st._num_roughness_levels; i++)
    st._roughness_values.push_back(powf(float(i) / 9.0f, roughness_power));
  int dw = st._tex_width, dh = st._tex_height, mip = 0;
  while (dw >= 4 && dh >= 4) { // stop at 4x4, don't go smaller
    st._diffuse_mips.push_back(mip);
    dw >>= 1;
    dh >>= 1;
    mip++;
  }
}

void initFilterMaterials(Context* ctx, EnvFilterState& st) {
  auto specular_material = std::make_shared<FreestyleMaterial>();
  auto diffuse_material  = std::make_shared<FreestyleMaterial>();

  specular_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  specular_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  specular_material->_rasterstate->setCullTest(ECullTest::OFF);
  specular_material->gpuInit(ctx, filterenv_shader_path());

  diffuse_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  diffuse_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  diffuse_material->_rasterstate->setCullTest(ECullTest::OFF);
  diffuse_material->gpuInit(ctx, filterenv_shader_path());

  st._specular_material = specular_material;
  st._diffuse_material  = diffuse_material;
}

void renderSpecularLevel(Context* ctx, EnvFilterState& st, int rough_idx) {
  const int   tex_width  = st._tex_width;
  const int   tex_height = st._tex_height;
  const float roughness  = st._roughness_values[rough_idx];

  auto rtgroup         = std::make_shared<RtGroup>(ctx, tex_width, tex_height, MsaaSamples::MSAA_1X);
  auto rtbuffer        = rtgroup->createRenderTarget(st._render_target_format);
  rtbuffer->_debugName = FormatString("%s-spc-rtb-%d", st._tex_name.c_str(), rough_idx);
  rtgroup->_name       = FormatString("%s-spc-rtg-%d", st._tex_name.c_str(), rough_idx);
  st._spec_rtgroups.push_back(rtgroup);
  st._spec_rtbuffers.push_back(rtbuffer);

  auto RCFD = std::make_shared<RenderContextFrameData>(ctx);
  auto fbi  = ctx->FBI();
  auto dwi  = ctx->DWI();

  fbi->PushRtGroup(rtgroup.get());

  auto technique_name = st._is_equirectangular                          //
                            ? "tek_filterSpecularMapEquirectangular"    //
                            : "tek_filterSpecularMapStandard";
  auto material  = st._specular_material;
  auto technique = material->technique(technique_name);
  OrkAssert(technique);
  material->begin(technique, RCFD);

  auto param_mvp                   = material->param("mvp");
  auto param_pfm                   = material->param("prefiltmap");
  auto param_ruf                   = material->param("roughness");
  auto param_imgdim                = material->param("imgdim");
  auto param_numsamples            = material->param("numsamples");
  auto param_viewport_size         = material->param("ViewportSize");
  auto param_inv_viewport_size_vtx = material->param("InvViewportSize");
  auto param_inv_viewport_size_frg = material->param("InvViewportSizeFrg");

  OrkAssert(param_mvp);
  OrkAssert(param_pfm);
  OrkAssert(param_ruf);
  OrkAssert(param_imgdim);
  OrkAssert(param_numsamples);

  material->bindParamMatrix(param_mvp, fmtx4::Identity());
  material->bindParamTexture(param_pfm, st._rawenvmap.get());
  material->bindParamFloat(param_ruf, roughness);
  material->bindParamVec2(param_imgdim, fvec2(tex_width, tex_height));
  material->bindParamU32(param_numsamples, 8192);
  if (param_viewport_size)
    material->bindParamVec2(param_viewport_size, fvec2(tex_width, tex_height));
  if (param_inv_viewport_size_vtx)
    material->bindParamVec2(param_inv_viewport_size_vtx, fvec2(1.0f / tex_width, 1.0f / tex_height));
  if (param_inv_viewport_size_frg)
    material->bindParamVec2(param_inv_viewport_size_frg, fvec2(1.0f / tex_width, 1.0f / tex_height));

  material->commit();

  const int tile_size   = EnvMapProcessor::SPECULAR_TILE_SIZE;
  const int num_tiles_x = (tex_width + tile_size - 1) / tile_size;
  const int num_tiles_y = (tex_height + tile_size - 1) / tile_size;
  for (int ty = 0; ty < num_tiles_y; ty++) {
    for (int tx = 0; tx < num_tiles_x; tx++) {
      TileParams tile;
      tile.x         = tx * tile_size;
      tile.y         = ty * tile_size;
      tile.width     = std::min(tile_size, tex_width - tile.x);
      tile.height    = std::min(tile_size, tex_height - tile.y);
      tile.roughness = roughness;
      fvec4 ndc      = tile.getNDC(tex_width, tex_height);
      fvec4 uv       = tile.getUV(tex_width, tex_height);
      dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
    }
  }

  material->end(RCFD);
  fbi->PopRtGroup();

  auto capbuf = std::make_shared<CaptureBuffer>();
  st._spec_capbufs.push_back(capbuf);
  auto future = fbi->captureAsFormat(rtbuffer.get(), capbuf, st._capture_format);
  st._spec_futures.push_back(future);
}

void renderDiffuseLevel(Context* ctx, EnvFilterState& st, int mip) {
  const int output_width  = st._tex_width >> mip;
  const int output_height = st._tex_height >> mip;

  auto rtgroup         = std::make_shared<RtGroup>(ctx, output_width, output_height, MsaaSamples::MSAA_1X);
  auto rtbuffer        = rtgroup->createRenderTarget(st._render_target_format);
  rtbuffer->_debugName = FormatString("%s-dif-rtb-mip%d", st._tex_name.c_str(), mip);
  rtgroup->_name       = FormatString("%s-dif-rtg-mip%d", st._tex_name.c_str(), mip);
  st._diff_rtgroups.push_back(rtgroup);
  st._diff_rtbuffers.push_back(rtbuffer);

  auto RCFD = std::make_shared<RenderContextFrameData>(ctx);
  auto fbi  = ctx->FBI();
  auto dwi  = ctx->DWI();

  fbi->PushRtGroup(rtgroup.get());

  auto technique_name = st._is_equirectangular                         //
                            ? "tek_filterDiffuseMapEquirectangular"    //
                            : "tek_filterDiffuseMapStandard";
  auto material  = st._diffuse_material;
  auto technique = material->technique(technique_name);
  OrkAssert(technique);
  material->begin(technique, RCFD);

  auto param_mvp                   = material->param("mvp");
  auto param_pfm                   = material->param("prefiltmap");
  auto param_ruf                   = material->param("roughness");
  auto param_imgdim                = material->param("imgdim");
  auto param_numsamples            = material->param("numsamples");
  auto param_viewport_size         = material->param("ViewportSize");
  auto param_inv_viewport_size_vtx = material->param("InvViewportSize");
  auto param_inv_viewport_size_frg = material->param("InvViewportSizeFrg");

  OrkAssert(param_mvp);
  OrkAssert(param_pfm);
  OrkAssert(param_ruf);
  OrkAssert(param_imgdim);
  OrkAssert(param_numsamples);

  material->bindParamMatrix(param_mvp, fmtx4::Identity());
  material->bindParamTexture(param_pfm, st._rawenvmap.get());
  material->bindParamFloat(param_ruf, 1.0f); // diffuse uses roughness=1
  material->bindParamVec2(param_imgdim, fvec2(output_width, output_height));
  material->bindParamU32(param_numsamples, 4096);
  if (param_viewport_size)
    material->bindParamVec2(param_viewport_size, fvec2(output_width, output_height));
  if (param_inv_viewport_size_vtx)
    material->bindParamVec2(param_inv_viewport_size_vtx, fvec2(1.0f / output_width, 1.0f / output_height));
  if (param_inv_viewport_size_frg)
    material->bindParamVec2(param_inv_viewport_size_frg, fvec2(1.0f / output_width, 1.0f / output_height));

  material->commit();

  const int tile_size   = EnvMapProcessor::DIFFUSE_TILE_SIZE;
  const int num_tiles_x = (output_width + tile_size - 1) / tile_size;
  const int num_tiles_y = (output_height + tile_size - 1) / tile_size;
  for (int ty = 0; ty < num_tiles_y; ty++) {
    for (int tx = 0; tx < num_tiles_x; tx++) {
      TileParams tile;
      tile.x         = tx * tile_size;
      tile.y         = ty * tile_size;
      tile.width     = std::min(tile_size, output_width - tile.x);
      tile.height    = std::min(tile_size, output_height - tile.y);
      tile.mip_level = mip;
      fvec4 ndc      = tile.getNDC(output_width, output_height);
      fvec4 uv       = tile.getUV(output_width, output_height);
      dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
    }
  }

  material->end(RCFD);
  fbi->PopRtGroup();

  auto capbuf = std::make_shared<CaptureBuffer>();
  st._diff_capbufs.push_back(capbuf);
  auto future = fbi->captureAsFormat(rtbuffer.get(), capbuf, st._capture_format);
  st._diff_futures.push_back(future);
}

static int captureBytesPerChannel(EBufferFormat fmt) {
  return (fmt == EBufferFormat::RGBA16F) ? 2 : (fmt == EBufferFormat::RGBA32F) ? 4 : 1;
}

// Wait for every capture, then package the roughness array + diffuse mipchain
// into the array-format XIR datablock (identical bytes to the burst path's
// package_results task + on_graph_complete writer). Also stashes the captured
// images into st for optional debug output.
datablock_ptr_t packageFilterResult(EnvFilterState& st) {
  for (auto& future : st._spec_futures)
    future->wait(nullptr);
  for (auto& future : st._diff_futures)
    future->wait(nullptr);

  // Package each specular roughness level as its own single-level XTX.
  std::vector<datablock_ptr_t> specular_datablocks;
  for (size_t i = 0; i < st._spec_capbufs.size(); i++) {
    auto capbuf = st._spec_capbufs[i];
    auto future = st._spec_futures[i];
    if (!future->_failed && capbuf->_image) {
      CompressedImageMipChain single_level_chain;
      CompressedImage base_level;
      base_level._width           = capbuf->width();
      base_level._height          = capbuf->height();
      base_level._depth           = 1;
      base_level._format          = st._capture_format;
      base_level._numcomponents   = 4;
      base_level._bytesPerChannel = captureBytesPerChannel(st._capture_format);
      base_level._data            = capbuf->_image->_data;

      single_level_chain._width         = base_level._width;
      single_level_chain._height        = base_level._height;
      single_level_chain._depth         = 1;
      single_level_chain._format        = base_level._format;
      single_level_chain._numcomponents = 4;
      single_level_chain._levels        = {base_level};

      auto roughness_datablock = std::make_shared<DataBlock>();
      single_level_chain.writeXTX(roughness_datablock);
      specular_datablocks.push_back(roughness_datablock);
      st._debug_spec_images.push_back(capbuf->_image);
    }
  }

  // Package the diffuse mip chain.
  CompressedImageMipChain diffuse_mipchain;
  CompressedImageMipChain::miplevels_t diff_levels;
  for (size_t i = 0; i < st._diff_capbufs.size(); i++) {
    auto capbuf = st._diff_capbufs[i];
    auto future = st._diff_futures[i];
    if (!future->_failed && capbuf->_image) {
      CompressedImage miplevel;
      miplevel._width           = capbuf->width();
      miplevel._height          = capbuf->height();
      miplevel._depth           = 1;
      miplevel._format          = st._capture_format;
      miplevel._numcomponents   = 4;
      miplevel._bytesPerChannel = captureBytesPerChannel(st._capture_format);
      miplevel._data            = capbuf->_image->_data;
      diff_levels.push_back(miplevel);
      st._debug_diff_images.push_back(capbuf->_image);
    }
  }
  if (!diff_levels.empty()) {
    const auto& first_level         = diff_levels[0];
    diffuse_mipchain._width         = first_level._width;
    diffuse_mipchain._height        = first_level._height;
    diffuse_mipchain._depth         = 1;
    diffuse_mipchain._format        = st._capture_format;
    diffuse_mipchain._numcomponents = 4;
    diffuse_mipchain._levels        = diff_levels;
  }
  auto diffuse_datablock = std::make_shared<DataBlock>();
  diffuse_mipchain.writeXTX(diffuse_datablock);

  return xir::XIRWriter::writeXirDatablocksWithArray(diffuse_datablock, specular_datablocks, st._roughness_values);
}

////////////////////////////////////////////////////////////////////////////////
// In-scene live swap (MT2 §2.6). Decodes the freshly-packaged XIR datablock the
// EXACT way the load-time swap machinery does (radiancemaps_asset.cpp
// _loadFromXIR — "the hardest part exists") so the published maps are
// byte-identical to a normal load, then assigns-new every field on the live
// `target` RadianceMaps and hands the outgoing textures to the N-frame deferred
// destroy (§1.6 rule 3, kDelayFrames=3). Runs on the render thread (ctx owner)
// synchronously — no fence-gated handoff needed, the microtask already GPU-
// completed every capture via executeInlineGpuJob.
////////////////////////////////////////////////////////////////////////////////

void publishRefilterToTarget(Context* ctx, datablock_ptr_t xir_datablock, pbr::radiancemaps_ptr_t target, const std::string& base_name) {
  auto xir = xir::XIRReader::readXirDatablocks(xir_datablock);
  if (!xir._valid || !xir._is_array_format) {
    logchan_gen->log("RadiancePrefilter: refilter datablock invalid — refusing to publish (fail loud)");
    OrkAssert(false);
    return;
  }

  // Diffuse texture.
  texture_ptr_t diffuse_tex;
  bool has_diffuse = xir._diffuse_data && xir._diffuse_data->length() > 0;
  if (has_diffuse) {
    auto diffuse_cmipchain = std::make_shared<CompressedImageMipChain>();
    diffuse_cmipchain->readXTX(xir._diffuse_data);
    diffuse_tex             = std::make_shared<Texture>();
    diffuse_tex->_debugName = base_name + ".ibldiff";
    auto diffuse_loadreq        = std::make_shared<TexLoadReq>();
    diffuse_loadreq->ptex       = diffuse_tex;
    diffuse_loadreq->_cmipchain = diffuse_cmipchain;
    diffuse_loadreq->_texname   = base_name + ".irrdiff";
    ctx->TXI()->_createFromLoadReq(diffuse_loadreq);
    diffuse_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
    diffuse_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
    diffuse_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
    ctx->TXI()->ApplySamplingMode(diffuse_tex.get());
  }

  // Specular roughness texture array.
  int num_roughness_levels = xir._num_roughness_levels;
  std::vector<image_ptr_t> specular_images;
  for (int i = 0; i < num_roughness_levels; i++) {
    auto cmipchain = std::make_shared<CompressedImageMipChain>();
    cmipchain->readXTX(xir._specular_datablocks[i]);
    auto image = std::make_shared<Image>();
    cmipchain->_levels[0].convertToImage(*image);
    specular_images.push_back(image);
  }
  auto specular_texarray             = std::make_shared<TextureArray>();
  specular_texarray->_tex->_debugName = base_name + ".iblspec_array";
  TextureArrayInitData TID;
  TID._slices.resize(num_roughness_levels);
  for (int i = 0; i < num_roughness_levels; i++) {
    uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
    TID._slices[i]    = TextureArrayInitSubItem{usage_id, specular_images[i]};
  }
  ctx->TXI()->initTextureArray2DFromData(specular_texarray.get(), TID);
  specular_texarray->_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
  specular_texarray->_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
  specular_texarray->_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
  ctx->TXI()->ApplySamplingMode(specular_texarray->_tex.get());

  // Snapshot outgoing textures, defer their destroy past MAX_FRAMES_IN_FLIGHT
  // (synchronous vkDestroy of large in-flight maps → black frame, §1.6 rule 3).
  constexpr int kDelayFrames = 3;
  auto old_diffuse  = target->_filtenvDiffuseMap;
  auto old_specular = target->_filtenvSpecularMapArray;
  if (old_diffuse || old_specular)
    ctx->enqueueDelayedDestroy([old_diffuse, old_specular]() {}, kDelayFrames);

  // Assign-new every published field from the single owner thread (render).
  target->_numRoughnessLevels      = num_roughness_levels;
  target->_specularRoughnessValues = xir._roughness_values;
  if (has_diffuse)
    target->_filtenvDiffuseMap = diffuse_tex;
  target->_filtenvSpecularMapArray  = specular_texarray;
  target->_brdfIntegrationMapGGX    = PBRMaterial::brdfIntegrationMap(ctx, "GGX");
  target->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx, "GGXVELVET");
  target->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx, "GGXRIM");
  target->_brdfIntegrationMapBlinn  = PBRMaterial::brdfIntegrationMap(ctx, "BLINN");
  target->_brdfIntegrationMapPhong  = PBRMaterial::brdfIntegrationMap(ctx, "PHONG");
}

////////////////////////////////////////////////////////////////////////////////
// RadiancePrefilterMicrotask (MT2 §2.6) — first REAL microtask client.
//
// OPPORTUNISTIC. One slice = ONE step of the progress cursor:
//   step 0                         : build the two filter materials
//   steps [1 .. N]                 : one specular roughness level each
//   steps [N+1 .. N+M]             : one diffuse mip level each
//   final step                     : package XIR + (optional) live swap
// runSlice records/executes exactly one step and returns true until done, then
// triggers the swap machinery and returns false. The render/capture steps run
// through Context::executeInlineGpuJob (T7 option "a": own submit + fence wait)
// so each slice's measured CPU-wall reflects its real GPU cost and the §2.3
// budget loop throttles it across frames.
////////////////////////////////////////////////////////////////////////////////

struct RadiancePrefilterMicrotask final : public GpuMicrotask {

  RadiancePrefilterMicrotask(
      std::shared_ptr<EnvFilterState> state,
      pbr::radiancemaps_ptr_t target,
      std::function<void(datablock_ptr_t)> on_complete)
      : _state(state)
      , _target(target)
      , _on_complete(on_complete) {
    _name     = "RadiancePrefilter:" + state->_tex_name;
    _class    = MicrotaskClass::OPPORTUNISTIC;
    _numSteps = 1                          // materials
              + state->_num_roughness_levels
              + int(state->_diffuse_mips.size())
              + 1;                         // package/publish
  }

  int64_t sliceEstimateUs() const override {
    // FIXED small seed — deliberately NOT _lastMeasuredUs. The scheduler's own
    // auto-halving (T1) grows est_of via _estimateScaleQ16 in controlled 2x
    // steps; feeding back the raw last-measured cost instead lets a ONE-TIME
    // pipeline-warmup slice (first specular level ~180ms on cold pipeline) pin
    // the estimate above MAX_BUDGET, after which the scheduler defers the task
    // forever (only the first slice is budget-exempt) and it never completes.
    return 3000;
  }

  float progress() const override {
    return (_numSteps > 0) ? float(_cursor) / float(_numSteps) : -1.0f;
  }

  static bool _trace() {
    static bool t = (std::getenv("ORKID_MT_TRACE") != nullptr);
    return t;
  }

  bool runSlice(MicrotaskContext& mctx) override {
    Context* ctx = mctx._gfxctx;
    auto&    st  = *_state;
    int      idx = _cursor++;
    if (_trace()) { fprintf(stderr, "[RPMT] slice step=%d/%d frame=%llu\n", idx, _numSteps, (unsigned long long)mctx._frameIndex); fflush(stderr); }

    // step 0: materials (CPU-side shader init; cached after first build).
    if (idx == 0) {
      initFilterMaterials(ctx, st);
      if (_trace()) { fprintf(stderr, "[RPMT] materials done\n"); fflush(stderr); }
      return true;
    }
    idx -= 1;

    // specular roughness levels — one self-contained submit each.
    if (idx < st._num_roughness_levels) {
      int ri = idx;
      ctx->executeInlineGpuJob([ctx, &st, ri]() { renderSpecularLevel(ctx, st, ri); });
      if (_trace()) { fprintf(stderr, "[RPMT] spec %d done\n", ri); fflush(stderr); }
      return true;
    }
    idx -= st._num_roughness_levels;

    // diffuse mip levels.
    if (idx < int(st._diffuse_mips.size())) {
      int mip = st._diffuse_mips[idx];
      ctx->executeInlineGpuJob([ctx, &st, mip]() { renderDiffuseLevel(ctx, st, mip); });
      if (_trace()) { fprintf(stderr, "[RPMT] diff mip%d done\n", mip); fflush(stderr); }
      return true;
    }

    // final step: package + (optional) live swap into the target maps.
    if (_trace()) { fprintf(stderr, "[RPMT] packaging...\n"); fflush(stderr); }
    auto xir = packageFilterResult(st);
    if (_target)
      publishRefilterToTarget(ctx, xir, _target, st._tex_name);
    if (_on_complete)
      _on_complete(xir);
    if (_trace()) { fprintf(stderr, "[RPMT] complete\n"); fflush(stderr); }
    return false; // complete
  }

  std::shared_ptr<EnvFilterState>      _state;
  pbr::radiancemaps_ptr_t              _target;
  std::function<void(datablock_ptr_t)> _on_complete;
  int                                  _cursor   = 0;
  int                                  _numSteps = 0;
};

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// TaskGraph-based filtering implementation (burst path — bake-time / load
// screen, UNBOUNDED). Unchanged behavior; now delegates each level to the
// shared helpers above so its output is byte-identical to the microtask path.
////////////////////////////////////////////////////////////////////////////////

taskgraph_ptr_t EnvMapProcessor::createFilteringTaskGraph(texture_ptr_t rawenvmap, bool is_equirectangular, bool is_hdr_source) {

  auto graph = std::make_shared<TaskGraph>();
  auto state = std::make_shared<EnvFilterState>();
  initEnvFilterState(*state, rawenvmap, is_equirectangular, is_hdr_source);

  auto gpu_executor     = gloadercontext->createContextExecutor();
  auto primary_executor = TaskExecutor::createSerial(); // on primary execution thread

  // retain the state (+ the raw envmap) with the graph
  graph->_varmap.atomicOp([=](varmap::VarMap& unlocked) {
    unlocked.set<texture_ptr_t>("rawenvmap", rawenvmap);
    unlocked.set<std::shared_ptr<EnvFilterState>>("env_filter_state", state);
  });

  ///////////////////////////////////////
  // Phase 1: setup + one-frame barrier
  ///////////////////////////////////////

  auto setup_phase = TaskGraph::phase(graph, state->_tex_name + ".setup", gpu_executor);
  setup_phase->task("initialize_materials", [state](taskgraph_wkptr_t g) { //
    initFilterMaterials(gloadercontext.get(), *state);
  });
  ContextExecutor::emptyFrame(graph, state->_tex_name + ".setup-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 2: specular filtering — one phase (one submit) per roughness level
  ///////////////////////////////////////

  for (int rough_idx = 0; rough_idx < state->_num_roughness_levels; rough_idx++) {
    std::string phase_name = state->_tex_name + ".specular_roughness_" + std::to_string(rough_idx);
    auto        phase      = TaskGraph::phase(graph, phase_name, gpu_executor);
    phase->task("spec_roughness_" + std::to_string(rough_idx), [state, rough_idx](taskgraph_wkptr_t g) {
      renderSpecularLevel(gloadercontext.get(), *state, rough_idx);
    });
    ContextExecutor::emptyFrame(graph, "specular-barrier", gpu_executor);
  }

  ///////////////////////////////////////
  // Phase 3: diffuse filtering — one phase per mip level
  ///////////////////////////////////////

  for (int mip : state->_diffuse_mips) {
    std::string phase_name = state->_tex_name + ".diffuse_mip_" + std::to_string(mip);
    auto        phase      = TaskGraph::phase(graph, phase_name, gpu_executor);
    phase->task("diff_mip_" + std::to_string(mip), [state, mip](taskgraph_wkptr_t g) {
      renderDiffuseLevel(gloadercontext.get(), *state, mip);
    });
    ContextExecutor::emptyFrame(graph, state->_tex_name + ".diffuse-barrier", gpu_executor);
  }

  ContextExecutor::emptyFrame(graph, state->_tex_name + ".final-frame-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 5: wait for captures + package datablocks (primary thread)
  ///////////////////////////////////////

  auto package_phase = TaskGraph::phase(graph, state->_tex_name + ".package_datablocks", primary_executor);
  package_phase->task("package_results", [state](taskgraph_wkptr_t g) {
    logchan_gen->log("EnvMapProcessor: Waiting for async captures spc<%zu> dif<%zu> to complete...",
                     state->_spec_futures.size(), state->_diff_futures.size());
    auto xir_datablock = packageFilterResult(*state);
    logchan_gen->log("EnvMapProcessor: Packaging complete! xir<%zu bytes> spc<%zu> dif<%zu>",
                     xir_datablock ? xir_datablock->length() : 0,
                     state->_debug_spec_images.size(),
                     state->_debug_diff_images.size());
    g.lock()->_varmap.atomicOp([=](varmap::VarMap& vmap) {
      vmap.set<datablock_ptr_t>("xir_datablock", xir_datablock);
      vmap.set<image_list_t>("debug_spec_images", state->_debug_spec_images);
      vmap.set<image_list_t>("debug_diff_images", state->_debug_diff_images);
    });
  });

  return graph;
}

////////////////////////////////////////////////////////////////////////////////
// createRadiancePrefilterMicrotask (MT2 §2.6 factory)
////////////////////////////////////////////////////////////////////////////////

gpumicrotask_ptr_t EnvMapProcessor::createRadiancePrefilterMicrotask(
    texture_ptr_t rawenvmap,
    bool is_equirectangular,
    bool is_hdr_source,
    pbr::radiancemaps_ptr_t target,
    std::function<void(datablock_ptr_t)> on_complete) {

  auto state = std::make_shared<EnvFilterState>();
  initEnvFilterState(*state, rawenvmap, is_equirectangular, is_hdr_source);
  return std::make_shared<RadiancePrefilterMicrotask>(state, target, on_complete);
}

////////////////////////////////////////////////////////////////////////////////
// processToXIRDataBlockAsync
//////////////////////////////////////////////////////////////////////////////////

xirprocessfuture_ptr_t EnvMapProcessor::processToXIRDataBlockAsync(const file::Path& input_path) {

  // Create the future
  auto future = std::make_shared<XIRProcessFuture>();

  // Load source texture
  auto load_req = std::make_shared<asset::LoadRequest>(input_path);
  load_req->_gpu_load_async = false; // Load synchronously for now
  auto texasset = asset::AssetManager<TextureAsset>::load(load_req);
  if (!texasset || !texasset->GetTexture()) {
    future->setResult(nullptr);
    return future;
  }

  auto rawenvmap = texasset->GetTexture();

  // Ensure the texture is fully uploaded to GPU before proceeding.
  // _loadXTXTexture defers GPU upload to mainSerialQueue, so we must
  // process pending ops before using the texture in the filtering pipeline.
  while (opq::mainSerialQueue()->Process()) {}

  // Extract texture name for debug purposes
  std::string texture_name = input_path.getName();

  // Determine format from extension
  auto ext_str = input_path.getExtension();
  // Convert to lowercase manually
  std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
  // getExtension returns without dot, so compare without dot
  bool is_equirectangular = (ext_str == "exr" || ext_str == "hdr");
  bool is_hdr_source      = (ext_str == "exr" || ext_str == "hdr");

  // CRITICAL FIX: For equirectangular maps, U-axis must wrap to prevent seam at ±180°
  if (is_equirectangular) {
    rawenvmap->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;  // U-axis wraps
    rawenvmap->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP; // V-axis clamps at poles
    rawenvmap->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;

    // Apply the sampling mode to the GPU texture object
    auto txi = gloadercontext->TXI();
    txi->ApplySamplingMode(rawenvmap.get());

    logchan_gen->log("EnvMapProcessor: Set WRAP mode on U-axis for equirectangular texture %s",
                     texture_name.c_str());
  }

  // Create the TaskGraph for filtering

  // Execute on a worker thread with ContextExecutor
  opq::concurrentQueue()->enqueue([=]() {
    auto taskgraph = createFilteringTaskGraph(rawenvmap, is_equirectangular, is_hdr_source);

    auto on_graph_complete = [future](taskgraph_wkptr_t g) {
      // When graph completes, extract results and package as XIR
      datablock_ptr_t result_data;
      image_list_t    debug_spec_images;
      image_list_t    debug_diff_images;

      g.lock()->_varmap.atomicOp([&](varmap::VarMap& unlocked) {
        result_data       = unlocked.typedValueForKey<datablock_ptr_t>("xir_datablock").value();
        debug_spec_images = unlocked.typedValueForKey<image_list_t>("debug_spec_images").value();
        debug_diff_images = unlocked.typedValueForKey<image_list_t>("debug_diff_images").value();
      });
      // Set debug images in the future
      future->setDebugImages(debug_spec_images, debug_diff_images);
      future->setResult(result_data);
    };
    TaskGraph::execute(taskgraph, on_graph_complete);
  });

  return future;
}

////////////////////////////////////////////////////////////////////////////////
// processToXIRDataBlockAsyncViaMicrotask (MT2 §3 gate b byte-identity harness)
//
// Bakes `input_path` to XIR via the SLICED microtask path (enqueued on
// gloadercontext's scheduler, UNBOUNDED — the loader thread drains it exactly
// like the burst) instead of the burst taskgraph. Same shared helpers → its
// XIR datablock must be byte-identical to processToXIRDataBlockAsync's.
//////////////////////////////////////////////////////////////////////////////////

xirprocessfuture_ptr_t EnvMapProcessor::processToXIRDataBlockAsyncViaMicrotask(const file::Path& input_path) {

  auto future = std::make_shared<XIRProcessFuture>();

  auto load_req             = std::make_shared<asset::LoadRequest>(input_path);
  load_req->_gpu_load_async = false;
  auto texasset             = asset::AssetManager<TextureAsset>::load(load_req);
  if (!texasset || !texasset->GetTexture()) {
    future->setResult(nullptr);
    return future;
  }
  auto rawenvmap = texasset->GetTexture();
  while (opq::mainSerialQueue()->Process()) {}

  auto ext_str = input_path.getExtension();
  std::transform(ext_str.begin(), ext_str.end(), ext_str.begin(), ::tolower);
  bool is_equirectangular = (ext_str == "exr" || ext_str == "hdr");
  bool is_hdr_source      = (ext_str == "exr" || ext_str == "hdr");

  if (is_equirectangular) {
    rawenvmap->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
    rawenvmap->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
    rawenvmap->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
    gloadercontext->TXI()->ApplySamplingMode(rawenvmap.get());
  }

  // No live target (bake, not in-scene refilter) — the completion just fulfills
  // the future with the packaged datablock.
  auto task = createRadiancePrefilterMicrotask(
      rawenvmap, is_equirectangular, is_hdr_source, nullptr, [future](datablock_ptr_t xir) { future->setResult(xir); });

  gloadercontext->_microtaskScheduler.enqueue(task);
  return future;
}

////////////////////////////////////////////////////////////////////////////////
// processToXIR
//////////////////////////////////////////////////////////////////////////////////

bool EnvMapProcessor::processToXIR(const file::Path& input_path, const file::Path& output_path) {

  auto future   = processToXIRDataBlockAsync(input_path);
  auto xir_data = future->get(); // Block waiting for result
  if (!xir_data) {
    return false;
  }

  // Write to file
  auto result = File::saveDatablock(output_path, xir_data);
  return (result == EFEC_FILE_OK);
}

////////////////////////////////////////////////////////////////////////////////

static std::atomic<int> gXIRProcessFutureCount{0};

XIRProcessFuture::XIRProcessFuture() {
  int count = gXIRProcessFutureCount.fetch_add(1);
  logchan_gen->log("XIRProcessFuture<%p> Ctor count<%d>", this, count + 1);
}
XIRProcessFuture::~XIRProcessFuture() {
  int count = gXIRProcessFutureCount.fetch_sub(1);
  logchan_gen->log("XIRProcessFuture<%p> Dtor count<%d>", this, count - 1);
}

} // namespace ork::lev2
