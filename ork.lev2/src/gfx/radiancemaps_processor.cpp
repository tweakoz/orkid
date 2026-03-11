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
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/image.h>
#include <ork/asset/Asset.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
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
// TaskGraph-based filtering implementation
////////////////////////////////////////////////////////////////////////////////

taskgraph_ptr_t EnvMapProcessor::createFilteringTaskGraph(texture_ptr_t rawenvmap, bool is_equirectangular, bool is_hdr_source) {

  auto graph = std::make_shared<TaskGraph>();

  // Get actual texture dimensions
  int tex_width        = rawenvmap->_width;
  int tex_height       = rawenvmap->_height;
  std::string tex_name = file::Path(rawenvmap->_debugName).toBFS().stem().string();

  // HDR sources capture as RGBA16F to preserve dynamic range, LDR as RGBA8
  // Render target matches capture format for HDR; LDR uses RGBA32F (shaders need float, capture clamps to RGBA8)
  auto capture_format       = is_hdr_source ? EBufferFormat::RGBA16F : EBufferFormat::RGBA8;
  auto render_target_format = is_hdr_source ? EBufferFormat::RGBA16F : EBufferFormat::RGBA32F;

  ///////////////////////////////////////
  // Create executors
  ///////////////////////////////////////

  auto gpu_executor     = gloadercontext->createContextExecutor();
  auto primary_executor = TaskExecutor::createSerial(); // on primary execution thread

  ///////////////////////////////////////
  // Phase 1: Setup and initialization
  ///////////////////////////////////////

  auto setup_phase = TaskGraph::phase(graph, tex_name + ".setup", gpu_executor);

  // Capture all the needed data for filtering
  // Using lambda captures instead of varmap for simplicity

  // Storage for render targets and materials (shared across phases)
  auto specular_rtgroups  = std::make_shared<rtgroup_list_t>();
  auto specular_rtbuffers = std::make_shared<rtbuffer_list_t>();
  auto diffuse_rtgroups   = std::make_shared<rtgroup_list_t>();
  auto diffuse_rtbuffers  = std::make_shared<rtbuffer_list_t>();

  auto specular_material = std::make_shared<FreestyleMaterial>();
  auto diffuse_material  = std::make_shared<FreestyleMaterial>();
  auto spec_futures = std::make_shared<captureasync_list_t>();
  auto diff_futures = std::make_shared<captureasync_list_t>();
  auto spec_capbufs = std::make_shared<capturebuffer_list_t>();
  auto diff_capbufs = std::make_shared<capturebuffer_list_t>();
  
  // Pre-declare spec_roughness_values here
  auto spec_roughness_values = std::make_shared<std::vector<float>>();

  graph->_varmap.atomicOp([=](varmap::VarMap& unlocked) {
    // retain stuff with graph
    unlocked.set<texture_ptr_t>("rawenvmap", rawenvmap);
    unlocked.set<bool>("is_equirectangular", is_equirectangular);
    unlocked.set<rtgroup_list_ptr_t>("specular_rtgroups", specular_rtgroups);
    unlocked.set<rtbuffer_list_ptr_t>("specular_rtbuffers", specular_rtbuffers);
    unlocked.set<rtgroup_list_ptr_t>("diffuse_rtgroups", diffuse_rtgroups);
    unlocked.set<rtbuffer_list_ptr_t>("diffuse_rtbuffers", diffuse_rtbuffers);
    unlocked.set<material_ptr_t>("specular_material", specular_material);
    unlocked.set<material_ptr_t>("diffuse_material", diffuse_material);
    unlocked.set<captureasync_list_ptr_t>("spec_futures", spec_futures);
    unlocked.set<captureasync_list_ptr_t>("diff_futures", diff_futures);
    unlocked.set<capturebuffer_list_ptr_t>("spec_capbufs", spec_capbufs);
    unlocked.set<capturebuffer_list_ptr_t>("diff_capbufs", diff_capbufs);
    unlocked.set<std::shared_ptr<std::vector<float>>>("spec_roughness_values", spec_roughness_values);
  });

  setup_phase->task(
      "initialize_materials",
      [specular_material,                       //
       diffuse_material](taskgraph_wkptr_t g) { //
        if(0)logchan_gen->log("EnvMapProcessor: Setup phase starting");
        if(0)logchan_gen->log("EnvMapProcessor: Using context %p for material initialization", gloadercontext.get());

        // Initialize specular filtering material
        specular_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
        specular_material->_rasterstate->setDepthTest(EDepthTest::OFF);
        specular_material->_rasterstate->setCullTest(ECullTest::OFF);
        specular_material->gpuInit(gloadercontext.get(), filterenv_shader_path());

        // Initialize diffuse filtering material
        diffuse_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
        diffuse_material->_rasterstate->setDepthTest(EDepthTest::OFF);
        diffuse_material->_rasterstate->setCullTest(ECullTest::OFF);
        diffuse_material->gpuInit(gloadercontext.get(), filterenv_shader_path());

        if(0)logchan_gen->log("EnvMapProcessor: Setup phase completed");
      });

  ///////////////////////////////////////
  // Frame barrier after setup to ensure rtgroups are initialized
  ///////////////////////////////////////

  ContextExecutor::emptyFrame(graph, tex_name + ".setup-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 2: Specular filtering - one phase per roughness level
  // Each phase contains multiple tile tasks that run in parallel
  ///////////////////////////////////////

  const int num_roughness_levels = 10;
  const float roughness_power    = 0.5f;

  for (int rough_idx = 0; rough_idx < num_roughness_levels; rough_idx++) {
    float roughness        = powf(float(rough_idx) / 9.0f, roughness_power);
    spec_roughness_values->push_back(roughness); // Store the original roughness value
    std::string phase_name = tex_name + ".specular_roughness_" + std::to_string(rough_idx);

    auto specular_phase = TaskGraph::phase(graph, phase_name, gpu_executor);

    // Calculate number of tiles needed
    const int tile_size   = SPECULAR_TILE_SIZE;
    const int num_tiles_x = (tex_width + tile_size - 1) / tile_size;
    const int num_tiles_y = (tex_height + tile_size - 1) / tile_size;

    // Single task for this roughness level that renders all tiles
    std::string task_name = "spec_roughness_" + std::to_string(rough_idx);

    specular_phase->task(                  //
        task_name,                         //
        [rawenvmap,                        //
         specular_material,                //
         num_tiles_x,                      //
         num_tiles_y,                      //
         tex_width,                        //
         tex_height,                       //
         tex_name,                         //
         specular_rtgroups,                //
         specular_rtbuffers,               //
         spec_futures,                     //
         spec_capbufs,                     //
         roughness,                        //
         tile_size,                        //
         is_equirectangular,               //
         capture_format,                   //
         render_target_format,             //
         rough_idx](taskgraph_wkptr_t g) { //
          // logchan_gen->log("EnvMapProcessor: starting specular filtering for roughness %d (%f)", rough_idx, roughness);

          auto rtgroup         = std::make_shared<RtGroup>(gloadercontext.get(), tex_width, tex_height, MsaaSamples::MSAA_1X);
          auto rtbuffer        = rtgroup->createRenderTarget(render_target_format);
          rtbuffer->_debugName = FormatString("%s-spc-rtb-%d", tex_name.c_str(), rough_idx);
          rtgroup->_name       = FormatString("%s-spc-rtg-%d", tex_name.c_str(), rough_idx);
          specular_rtgroups->push_back(rtgroup);
          specular_rtbuffers->push_back(rtbuffer);

          // logchan_gen->log("Specular filtering: Using rtgroup<%p> rtbuffer<%p> for roughness %d",
          //                  rtgroup.get(), rtbuffer.get(), rough_idx);

          // Set up render context
          auto RCFD = std::make_shared<RenderContextFrameData>(gloadercontext.get());
          auto fbi  = gloadercontext.get()->FBI();
          auto dwi  = gloadercontext.get()->DWI();

          // Push render target ONCE for all tiles
          fbi->PushRtGroup(rtgroup.get());

          // Get the appropriate technique based on format
          auto technique_name = is_equirectangular                           //
                                    ? "tek_filterSpecularMapEquirectangular" //
                                    : "tek_filterSpecularMapStandard";
          auto technique      = specular_material->technique(technique_name);
          OrkAssert(technique);
          // Begin material pass
          specular_material->begin(technique, RCFD);

          // Bind parameters
          auto param_mvp        = specular_material->param("mvp");
          auto param_pfm        = specular_material->param("prefiltmap");
          auto param_ruf        = specular_material->param("roughness");
          auto param_imgdim     = specular_material->param("imgdim");
          auto param_numsamples = specular_material->param("numsamples");
          auto param_viewport_size = specular_material->param("ViewportSize");
          auto param_inv_viewport_size_vtx = specular_material->param("InvViewportSize");  // Vertex uniform
          auto param_inv_viewport_size_frg = specular_material->param("InvViewportSizeFrg"); // Fragment uniform

          OrkAssert(param_mvp);
          OrkAssert(param_pfm);
          OrkAssert(param_ruf);
          OrkAssert(param_imgdim);
          OrkAssert(param_numsamples);

          specular_material->bindParamMatrix(param_mvp, fmtx4::Identity());
          specular_material->bindParamTexture(param_pfm, rawenvmap.get());
          specular_material->bindParamFloat(param_ruf, roughness);
          specular_material->bindParamVec2(param_imgdim, fvec2(tex_width, tex_height));
          specular_material->bindParamU32(param_numsamples, 8192);
          
          // Set viewport size parameters
          if (param_viewport_size) {
            specular_material->bindParamVec2(param_viewport_size, fvec2(tex_width, tex_height));
          }
          // Set vertex InvViewportSize
          if (param_inv_viewport_size_vtx) {
            specular_material->bindParamVec2(param_inv_viewport_size_vtx, fvec2(1.0f / tex_width, 1.0f / tex_height));
          }
          // Set fragment InvViewportSizeFrg
          if (param_inv_viewport_size_frg) {
            specular_material->bindParamVec2(param_inv_viewport_size_frg, fvec2(1.0f / tex_width, 1.0f / tex_height));
          }

          specular_material->commit();

          // Render all tiles sequentially within this single task
          for (int ty = 0; ty < num_tiles_y; ty++) {
            for (int tx = 0; tx < num_tiles_x; tx++) {
              // Create tile parameters
              TileParams tile;
              tile.x         = tx * tile_size;
              tile.y         = ty * tile_size;
              tile.width     = std::min(tile_size, tex_width - tile.x);
              tile.height    = std::min(tile_size, tex_height - tile.y);
              tile.roughness = roughness;

              // Calculate NDC and UV coordinates for this tile
              fvec4 ndc = tile.getNDC(tex_width, tex_height);
              fvec4 uv  = tile.getUV(tex_width, tex_height);

              // Render the tile
              dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));

              // logchan_gen->log("  Would render tile<%d,%d> for roughness %d", tx, ty, rough_idx);
            }
          }

          // End material pass
          specular_material->end(RCFD);

          // Pop render target ONCE after all tiles
          fbi->PopRtGroup();
                    
          auto capbuf = std::make_shared<CaptureBuffer>();
          spec_capbufs->push_back(capbuf);
          auto future = fbi->captureAsFormat(rtbuffer.get(), capbuf, capture_format);
          spec_futures->push_back(future);

          // logchan_gen->log("EnvMapProcessor: completed specular filtering for roughness %d", rough_idx);
        });

    // Note: we don't need the nested for loops for tiles anymore

    // Frame barrier after each specular roughness level
    ContextExecutor::emptyFrame(graph, "specular-barrier", gpu_executor);
  }

  ///////////////////////////////////////
  // Phase 3: Diffuse filtering - one phase per mip level
  ///////////////////////////////////////

  int diff_w = tex_width;
  int diff_h = tex_height;
  int mip    = 0;
  while (diff_w >= 4 && diff_h >= 4) { // Stop at 4x4, don't go smaller
    auto rtgroup         = std::make_shared<RtGroup>(gloadercontext.get(), diff_w, diff_h, MsaaSamples::MSAA_1X);
    auto rtbuffer        = rtgroup->createRenderTarget(render_target_format);
    rtbuffer->_debugName = FormatString("%s-dif-rtb-mip%d", tex_name.c_str(), mip);
    rtgroup->_name       = FormatString("%s-dif-rtg-mip%d", tex_name.c_str(), mip);
    diffuse_rtgroups->push_back(rtgroup);
    diffuse_rtbuffers->push_back(rtbuffer);
    if(0)logchan_gen->log("Setup: Created diffuse rtgroup<%p> rtbuffer<%p> for mip %d", rtgroup.get(), rtbuffer.get(), mip);
    std::string phase_name = tex_name + ".diffuse_mip_" + std::to_string(mip);

    auto diffuse_phase = TaskGraph::phase(graph, phase_name, gpu_executor);

    // Calculate output size for this mip level
    const int output_width  = tex_width >> mip;
    const int output_height = tex_height >> mip;
    const int tile_size     = DIFFUSE_TILE_SIZE;
    const int num_tiles_x   = (output_width + tile_size - 1) / tile_size;
    const int num_tiles_y   = (output_height + tile_size - 1) / tile_size;

    // Single task for this mip level that renders all tiles
    std::string task_name = "diff_mip_" + std::to_string(mip);

    diffuse_phase->task(                   //
        task_name,                         //
        [rawenvmap,                        //
         diffuse_material,                 //
         num_tiles_x,                      //
         num_tiles_y,                      //
         output_width,                     //
         output_height,                    //
         tex_name,                         //
         rtgroup,                          //
         rtbuffer,                         //
         diff_futures,                     //
         diff_capbufs,                     //
         tile_size,                        //
         mip,                              //
         is_equirectangular,               //
         capture_format](taskgraph_wkptr_t g) { //
          if(0)logchan_gen->log("EnvMapProcessor<%s>: starting diffuse filtering for mip %d", tex_name.c_str(), mip);

          // logchan_gen->log("Diffuse filtering: Using rtgroup<%p> rtbuffer<%p> for mip %d",
          //                  rtgroup.get(), rtbuffer.get(), mip);

          // Set up render context
          auto RCFD = std::make_shared<RenderContextFrameData>(gloadercontext.get());
          auto fbi  = gloadercontext.get()->FBI();
          auto dwi  = gloadercontext.get()->DWI();

          // Push render target ONCE for all tiles
          fbi->PushRtGroup(rtgroup.get());

          // Get the appropriate technique based on format
          auto technique_name = is_equirectangular                          //
                                    ? "tek_filterDiffuseMapEquirectangular" //
                                    : "tek_filterDiffuseMapStandard";
          auto technique      = diffuse_material->technique(technique_name);
          OrkAssert(technique);

          // Begin material pass
          diffuse_material->begin(technique, RCFD);

          // Bind parameters
          auto param_mvp        = diffuse_material->param("mvp");
          auto param_pfm        = diffuse_material->param("prefiltmap");
          auto param_ruf        = diffuse_material->param("roughness");
          auto param_imgdim     = diffuse_material->param("imgdim");
          auto param_numsamples = diffuse_material->param("numsamples");
          auto param_viewport_size = diffuse_material->param("ViewportSize");
          auto param_inv_viewport_size_vtx = diffuse_material->param("InvViewportSize");  // Vertex uniform
          auto param_inv_viewport_size_frg = diffuse_material->param("InvViewportSizeFrg"); // Fragment uniform

          OrkAssert(param_mvp);
          OrkAssert(param_pfm);
          OrkAssert(param_ruf);
          OrkAssert(param_imgdim);
          OrkAssert(param_numsamples);

          diffuse_material->bindParamMatrix(param_mvp, fmtx4::Identity());
          diffuse_material->bindParamTexture(param_pfm, rawenvmap.get());
          diffuse_material->bindParamFloat(param_ruf, 1.0f); // Diffuse uses roughness=1
          diffuse_material->bindParamVec2(param_imgdim, fvec2(output_width, output_height));
          diffuse_material->bindParamU32(param_numsamples, 4096);
          
          // Set viewport size parameters
          if (param_viewport_size) {
            diffuse_material->bindParamVec2(param_viewport_size, fvec2(output_width, output_height));
          }
          // Set vertex InvViewportSize
          if (param_inv_viewport_size_vtx) {
            diffuse_material->bindParamVec2(param_inv_viewport_size_vtx, fvec2(1.0f / output_width, 1.0f / output_height));
          }
          // Set fragment InvViewportSizeFrg
          if (param_inv_viewport_size_frg) {
            diffuse_material->bindParamVec2(param_inv_viewport_size_frg, fvec2(1.0f / output_width, 1.0f / output_height));
          }

          diffuse_material->commit();

          // Render all tiles sequentially within this single task
          for (int ty = 0; ty < num_tiles_y; ty++) {
            for (int tx = 0; tx < num_tiles_x; tx++) {
              // Create tile parameters
              TileParams tile;
              tile.x         = tx * tile_size;
              tile.y         = ty * tile_size;
              tile.width     = std::min(tile_size, output_width - tile.x);
              tile.height    = std::min(tile_size, output_height - tile.y);
              tile.mip_level = mip;

              // Calculate NDC and UV coordinates for this tile
              fvec4 ndc = tile.getNDC(output_width, output_height);
              fvec4 uv  = tile.getUV(output_width, output_height);

              // Render the tile
              dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));

              // logchan_gen->log("  Would render tile<%d,%d> for mip %d", tx, ty, mip);
            }
          }

          // End material pass
          diffuse_material->end(RCFD);

          // Pop render target ONCE after all tiles
          fbi->PopRtGroup();
                   
          auto capbuf = std::make_shared<CaptureBuffer>();
          diff_capbufs->push_back(capbuf);
          auto future = fbi->captureAsFormat(rtbuffer.get(), capbuf, capture_format);
          diff_futures->push_back(future);

          // logchan_gen->log("EnvMapProcessor: completed diffuse filtering for mip %d", mip);
        }); // diffuse_phase->task(

    // Note: we don't need the nested for loops for tiles anymore

    ContextExecutor::emptyFrame(graph, tex_name + ".diffuse-barrier", gpu_executor);
    diff_w >>= 1;
    diff_h >>= 1;
    mip++;
  }

  ///////////////////////////////////////
  // Final frame barrier before capture to ensure all filtering is complete
  ///////////////////////////////////////

  ContextExecutor::emptyFrame(graph, tex_name + ".final-frame-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 5: Wait for captures and package datablocks
  ///////////////////////////////////////

  auto package_phase = TaskGraph::phase(graph, tex_name + ".package_datablocks", primary_executor);

  package_phase->task("package_results", [spec_futures, //
                                          diff_futures, //
                                          spec_capbufs, //
                                          diff_capbufs, //
                                          spec_roughness_values, //
                                          capture_format //
                                        ](taskgraph_wkptr_t g) {
    size_t num_specs = spec_futures->size();
    size_t num_diffs = diff_futures->size();
    logchan_gen->log("EnvMapProcessor: Waiting for async captures spc<%zu> dif<%zu> to complete...", num_specs, num_diffs);

    // Wait for all specular captures
    for (size_t i = 0; i < num_specs; i++) {
      auto future = (*spec_futures)[i];
      future->wait(nullptr);
    }

    // Wait for all diffuse captures
    for (size_t i = 0; i < num_diffs; i++) {
      auto future = (*diff_futures)[i];
      future->wait(nullptr);
    }

    logchan_gen->log("EnvMapProcessor: All captures complete, packaging datablocks...");

    // Collect debug images
    image_list_t debug_spec_images;
    image_list_t debug_diff_images;

    // Package each specular roughness level separately for texture array
    std::vector<datablock_ptr_t> specular_datablocks;
    
    for (size_t i = 0; i < spec_capbufs->size(); i++) {
      auto capbuf = (*spec_capbufs)[i];
      auto future = (*spec_futures)[i];

      if (!future->_failed && capbuf->_image) {
        // Create single-level mipchain for this roughness (GPU will generate mips)
        CompressedImageMipChain single_level_chain;
        CompressedImage base_level;
        base_level._width = capbuf->width();
        base_level._height = capbuf->height();
        base_level._depth = 1;
        base_level._format = capture_format;
        base_level._numcomponents = 4;
        base_level._data = capbuf->_image->_data;
        
        single_level_chain._width = base_level._width;
        single_level_chain._height = base_level._height;
        single_level_chain._depth = 1;
        single_level_chain._format = base_level._format;
        single_level_chain._numcomponents = 4;
        single_level_chain._levels = {base_level};
        
        // Write this roughness level to its own datablock
        auto roughness_datablock = std::make_shared<DataBlock>();
        single_level_chain.writeXTX(roughness_datablock);
        specular_datablocks.push_back(roughness_datablock);
        
        // Store image for debug output
        debug_spec_images.push_back(capbuf->_image);
        
        logchan_gen->log("EnvMapProcessor: Packaged specular roughness level %zu (%dx%d)", 
                        i, base_level._width, base_level._height);
      }
    }

    // Create proper XTX format for diffuse data
    CompressedImageMipChain diffuse_mipchain;
    CompressedImageMipChain::miplevels_t diff_levels;
    
    for (size_t i = 0; i < diff_capbufs->size(); i++) {
      auto capbuf = (*diff_capbufs)[i];
      auto future = (*diff_futures)[i];

      if (!future->_failed && capbuf->_image) {
        // Create mip level from capture buffer
        CompressedImage miplevel;
        miplevel._width = capbuf->width();
        miplevel._height = capbuf->height();
        miplevel._depth = 1;
        miplevel._data = capbuf->_image->_data;
        diff_levels.push_back(miplevel);
        
        // Store image for debug output
        debug_diff_images.push_back(capbuf->_image);

        logchan_gen->log("EnvMapProcessor: Added diffuse mip level %zu (%dx%d)", 
                        i, miplevel._width, miplevel._height);
      }
    }
    
    // Initialize diffuse mipchain
    if (!diff_levels.empty()) {
      const auto& first_level = diff_levels[0];
      diffuse_mipchain._width = first_level._width;
      diffuse_mipchain._height = first_level._height;
      diffuse_mipchain._depth = 1;
      diffuse_mipchain._format = capture_format;
      diffuse_mipchain._numcomponents = 4;
      diffuse_mipchain._levels = diff_levels;
    }
    
    // Write diffuse to XTX format
    auto diffuse_datablock = std::make_shared<DataBlock>();
    diffuse_mipchain.writeXTX(diffuse_datablock);

    logchan_gen->log("EnvMapProcessor: XTX diffuse datablock: %zu bytes", diffuse_datablock->length());
    logchan_gen->log("EnvMapProcessor: Packaged %zu specular roughness levels", specular_datablocks.size());

    // Store datablocks and debug images in varmap for retrieval
    g.lock()->_varmap.atomicOp([=](varmap::VarMap& vmap) {
      vmap.set<std::vector<datablock_ptr_t>>("specular_datablocks", specular_datablocks);
      vmap.set<std::vector<float>>("specular_roughness_values", *spec_roughness_values);
      vmap.set<int>("num_roughness_levels", spec_roughness_values->size());
      vmap.set<datablock_ptr_t>("diffuse_datablock", diffuse_datablock);
      vmap.set<image_list_t>("debug_spec_images", debug_spec_images);
      vmap.set<image_list_t>("debug_diff_images", debug_diff_images);
    });

    logchan_gen->log("EnvMapProcessor: Packaging complete!");
    logchan_gen->log("  Diffuse datablock size: %zu bytes", diffuse_datablock->length());
    logchan_gen->log("  Specular roughness levels: %zu", specular_datablocks.size());
  });

  return graph;
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
  
    // Get a context executor (will use main thread for GPU ops)
    // Need to get context from somewhere - will be set up properly later
    // For now, we'll assume it gets passed through the varmap
    // auto ctx_executor = context->createContextExecutor();

    // Execute the graph - this will handle GPU operations properly

    auto on_graph_complete = [future](taskgraph_wkptr_t g) {
      // When graph completes, extract results and package as XIR
      std::vector<datablock_ptr_t> specular_datablocks;
      std::vector<float> specular_roughness_values;
      datablock_ptr_t diffuse_data;
      image_list_t debug_spec_images;
      image_list_t debug_diff_images;
      datablock_ptr_t result_data;

      g.lock()->_varmap.atomicOp([&](varmap::VarMap& unlocked) {
        specular_datablocks = unlocked.typedValueForKey<std::vector<datablock_ptr_t>>("specular_datablocks").value();
        specular_roughness_values = unlocked.typedValueForKey<std::vector<float>>("specular_roughness_values").value();
        diffuse_data = unlocked.typedValueForKey<datablock_ptr_t>("diffuse_datablock").value();
        debug_spec_images = unlocked.typedValueForKey<image_list_t>("debug_spec_images").value();
        debug_diff_images = unlocked.typedValueForKey<image_list_t>("debug_diff_images").value();
        // Use new array writer
        result_data = xir::XIRWriter::writeXirDatablocksWithArray(diffuse_data, specular_datablocks, specular_roughness_values);
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
