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
#include <ork/lev2/gfx/envmap_processor.h>
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
  //dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
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
  //dwi->quad2D(ndc, uv, fvec4(0, 0, 0, 0));
}

////////////////////////////////////////////////////////////////////////////////
// TaskGraph-based filtering implementation
////////////////////////////////////////////////////////////////////////////////

taskgraph_ptr_t EnvMapProcessor::createFilteringTaskGraph(texture_ptr_t rawenvmap, bool is_equirectangular) {

  auto graph = std::make_shared<TaskGraph>();
  
  // Get actual texture dimensions
  int tex_width  = rawenvmap->_width;
  int tex_height = rawenvmap->_height;
  std::string tex_name = rawenvmap->_debugName;
  ///////////////////////////////////////
  // Create executors
  ///////////////////////////////////////

  auto gpu_executor     = gloadercontext->createContextExecutor();
  auto primary_executor = TaskExecutor::createSerial(); // on primary execution thread

  ///////////////////////////////////////
  // Store input parameters in the graph's varmap
  ///////////////////////////////////////

  graph->_varmap.atomicOp([=](varmap::VarMap& vmap) {
    vmap.set<texture_ptr_t>("rawenvmap", rawenvmap);
    vmap.set<bool>("is_equirectangular", is_equirectangular);
  });

  ///////////////////////////////////////
  // Phase 1: Setup and initialization
  ///////////////////////////////////////

  auto setup_phase = TaskGraph::phase(graph, tex_name+".setup", gpu_executor);

  // Capture all the needed data for filtering
  // Using lambda captures instead of varmap for simplicity

  // Storage for render targets and materials (shared across phases)
  auto specular_rtgroups  = std::make_shared<rtgroup_list_t>();
  auto specular_rtbuffers = std::make_shared<rtbuffer_list_t>();
  auto diffuse_rtgroups   = std::make_shared<rtgroup_list_t>();
  auto diffuse_rtbuffers  = std::make_shared<rtbuffer_list_t>();

  auto specular_material = std::make_shared<FreestyleMaterial>();
  auto diffuse_material  = std::make_shared<FreestyleMaterial>();

  setup_phase->task("initialize_materials", [=](taskgraph_ptr_t g) {
    logchan_gen->log("EnvMapProcessor: Setup phase starting");
    logchan_gen->log("EnvMapProcessor: Using context %p for material initialization", gloadercontext.get());
        
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

    // Create render targets for specular filtering (10 roughness levels)
    const int num_roughness = 10;
    for (int i = 0; i < num_roughness; i++) {
      auto rtgroup         = std::make_shared<RtGroup>(gloadercontext.get(), tex_width, tex_height, MsaaSamples::MSAA_1X);
      auto rtbuffer        = rtgroup->createRenderTarget(EBufferFormat::RGBA32F);
      rtbuffer->_debugName = FormatString("%s-spc-rtb%d", tex_name.c_str(), i);
      rtgroup->_name       = FormatString("%s-spc-rtg%d", tex_name.c_str(), i);
      specular_rtgroups->push_back(rtgroup);
      specular_rtbuffers->push_back(rtbuffer);
      //logchan_gen->log("Setup: Created specular rtgroup<%p> rtbuffer<%p> for roughness %d", 
      //                 rtgroup.get(), rtbuffer.get(), i);
    }

    // Create render targets for diffuse filtering (mip chain)
    int diff_w = tex_width;
    int diff_h = tex_height;
    int mip    = 0;
    while (diff_w >= 4 && diff_h >= 4) {  // Stop at 4x4, don't go smaller
      auto rtgroup         = std::make_shared<RtGroup>(gloadercontext.get(), diff_w, diff_h, MsaaSamples::MSAA_1X);
      auto rtbuffer        = rtgroup->createRenderTarget(EBufferFormat::RGBA32F);
      rtbuffer->_debugName = FormatString("%s-dif-rtb-mip%d", tex_name.c_str(), mip);
      rtgroup->_name       = FormatString("%s-dif-rtg-mip%d", tex_name.c_str(), mip);
      diffuse_rtgroups->push_back(rtgroup);
      diffuse_rtbuffers->push_back(rtbuffer);
      //logchan_gen->log("Setup: Created diffuse rtgroup<%p> rtbuffer<%p> for mip %d", 
      //                 rtgroup.get(), rtbuffer.get(), mip);
      diff_w >>= 1;
      diff_h >>= 1;
      mip++;
    }

    logchan_gen->log(
        "EnvMapProcessor: Created %zu specular and %zu diffuse render targets",
        specular_rtgroups->size(),
        diffuse_rtgroups->size());
    logchan_gen->log("EnvMapProcessor: Setup phase completed");
  });

  ///////////////////////////////////////
  // Frame barrier after setup to ensure rtgroups are initialized
  ///////////////////////////////////////
  
  ContextExecutor::emptyFrame(graph, tex_name+".setup-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 2: Specular filtering - one phase per roughness level
  // Each phase contains multiple tile tasks that run in parallel
  ///////////////////////////////////////

  const int num_roughness_levels = 10;
  const float roughness_power    = 0.5f;

  for (int rough_idx = 0; rough_idx < num_roughness_levels; rough_idx++) {
    float roughness        = powf(float(rough_idx) / 9.0f, roughness_power);
    std::string phase_name = tex_name+".specular_roughness_" + std::to_string(rough_idx);

    auto specular_phase = TaskGraph::phase(graph, phase_name, gpu_executor);

    // Calculate number of tiles needed
    const int tile_size   = SPECULAR_TILE_SIZE;
    const int num_tiles_x = (tex_width + tile_size - 1) / tile_size;
    const int num_tiles_y = (tex_height + tile_size - 1) / tile_size;

    // Single task for this roughness level that renders all tiles
    std::string task_name = "spec_roughness_" + std::to_string(rough_idx);

    specular_phase->task(task_name, [=](taskgraph_ptr_t g) {
      //logchan_gen->log("EnvMapProcessor: starting specular filtering for roughness %d (%f)", rough_idx, roughness);

      // Get the render target for this roughness level
      auto rtgroup  = (*specular_rtgroups)[rough_idx];
      auto rtbuffer = (*specular_rtbuffers)[rough_idx];
      
      //logchan_gen->log("Specular filtering: Using rtgroup<%p> rtbuffer<%p> for roughness %d", 
      //                 rtgroup.get(), rtbuffer.get(), rough_idx);

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
      auto param_mvp = specular_material->param("mvp");
      auto param_pfm = specular_material->param("prefiltmap");
      auto param_ruf = specular_material->param("roughness");
      auto param_imgdim = specular_material->param("imgdim");
      auto param_numsamples = specular_material->param("numsamples");

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
          
          //logchan_gen->log("  Would render tile<%d,%d> for roughness %d", tx, ty, rough_idx);
        }
      }

      // End material pass
      specular_material->end(RCFD);

      // Pop render target ONCE after all tiles
      fbi->PopRtGroup();

      //logchan_gen->log("EnvMapProcessor: completed specular filtering for roughness %d", rough_idx);
    });
    
    // Note: we don't need the nested for loops for tiles anymore
    
    // Frame barrier after each specular roughness level
    ContextExecutor::emptyFrame(graph, "specular-barrier", gpu_executor);
  }

  ///////////////////////////////////////
  // Phase 3: Diffuse filtering - one phase per mip level
  ///////////////////////////////////////

  int mip = 0;
  for (auto rtgroup : *diffuse_rtgroups) {

    auto rtbuffer = rtgroup->buffer(0);
    std::string phase_name = tex_name+".diffuse_mip_" + std::to_string(mip);

    auto diffuse_phase = TaskGraph::phase(graph, phase_name, gpu_executor);

    // Calculate output size for this mip level
    const int output_width  = tex_width >> mip;
    const int output_height = tex_height >> mip;
    const int tile_size     = DIFFUSE_TILE_SIZE;
    const int num_tiles_x   = (output_width + tile_size - 1) / tile_size;
    const int num_tiles_y   = (output_height + tile_size - 1) / tile_size;

    // Single task for this mip level that renders all tiles
    std::string task_name = "diff_mip_" + std::to_string(mip);

    diffuse_phase->task(task_name, [=](taskgraph_ptr_t g) {
      logchan_gen->log("EnvMapProcessor<%s>: starting diffuse filtering for mip %d", tex_name.c_str(), mip);
      
      OrkAssert(false);
      //logchan_gen->log("Diffuse filtering: Using rtgroup<%p> rtbuffer<%p> for mip %d", 
      //                 rtgroup.get(), rtbuffer.get(), mip);

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
      auto param_mvp = diffuse_material->param("mvp");
      auto param_pfm = diffuse_material->param("prefiltmap");
      auto param_ruf = diffuse_material->param("roughness");
      auto param_imgdim = diffuse_material->param("imgdim");
      auto param_numsamples = diffuse_material->param("numsamples");

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
          
          //logchan_gen->log("  Would render tile<%d,%d> for mip %d", tx, ty, mip);
        }
      }

      // End material pass
      diffuse_material->end(RCFD);

      // Pop render target ONCE after all tiles
      fbi->PopRtGroup();

      //logchan_gen->log("EnvMapProcessor: completed diffuse filtering for mip %d", mip);
    }); // diffuse_phase->task(
    
    // Note: we don't need the nested for loops for tiles anymore
    
    ContextExecutor::emptyFrame(graph, tex_name+".diffuse-barrier", gpu_executor);
    mip++;
  }

  ///////////////////////////////////////
  // Final frame barrier before capture to ensure all filtering is complete
  ///////////////////////////////////////
  
  ContextExecutor::emptyFrame(graph, tex_name+".final-frame-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 4: Capture results asynchronously
  ///////////////////////////////////////

  auto cap_phase = TaskGraph::phase(graph, tex_name+".capture_results", gpu_executor);

  // Storage for captured data
  auto spec_futures = std::make_shared<std::vector<captureasync_ptr_t>>();
  auto diff_futures = std::make_shared<std::vector<captureasync_ptr_t>>();

  auto spec_capbufs = std::make_shared<std::vector<capturebuffer_ptr_t>>();
  auto diff_capbufs = std::make_shared<std::vector<capturebuffer_ptr_t>>();

  cap_phase->task("capture_specular", [=](taskgraph_ptr_t g) {
    auto fbi = gloadercontext.get()->FBI();

    /////////////////////////////////////////
    // Capture all specular filtering results
    /////////////////////////////////////////

    int i = 0;
    for (auto rtg : *specular_rtgroups) {
      auto rtb = rtg->buffer(0);
      auto capbuf = std::make_shared<CaptureBuffer>();
      spec_capbufs->push_back(capbuf);
      auto future = fbi->captureAsFormat(rtb.get(), capbuf, EBufferFormat::RGBA8);
      spec_futures->push_back(future);
      i++;
    }
  });

  cap_phase->task(tex_name+".capture_diffuse", [=](taskgraph_ptr_t g) {
    auto fbi = gloadercontext.get()->FBI();

    /////////////////////////////////////////
    // Capture all diffuse filtering results
    /////////////////////////////////////////

    int i = 0;
    for (auto rtg : *diffuse_rtgroups) {
      auto rtb = rtg->buffer(0);
      auto capbuf = std::make_shared<CaptureBuffer>();
      diff_capbufs->push_back(capbuf);
      auto future = fbi->captureAsFormat(rtb.get(), capbuf, EBufferFormat::RGBA8);
      diff_futures->push_back(future);
      i++;
    }
  });

  ///////////////////////////////////////
  // Frame barrier after capture to ensure GPU commands are submitted
  ///////////////////////////////////////
  
  ContextExecutor::emptyFrame(graph, tex_name+".capture-submission-barrier", gpu_executor);

  ///////////////////////////////////////
  // Phase 5: Wait for captures and package datablocks
  ///////////////////////////////////////

  auto package_phase = TaskGraph::phase(graph, tex_name+".package_datablocks", primary_executor);

  package_phase->task("package_results", [=](taskgraph_ptr_t g) {

    size_t num_specs = spec_futures->size();
    size_t num_diffs = diff_futures->size();
    logchan_gen->log("EnvMapProcessor: Waiting for async captures spc<%zu> dif<%zu> to complete...",num_specs,num_diffs);

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

    // Create datablocks to hold all captured tiles as a single texture data stream
    auto specular_datablock = std::make_shared<DataBlock>();
    auto diffuse_datablock  = std::make_shared<DataBlock>();

    // Collect debug images
    image_list_t debug_spec_images;
    image_list_t debug_diff_images;

    // For now, package all specular data as raw concatenated tiles
    // TODO: This should be properly formatted as DDS or other texture format
    size_t total_spec_size = 0;
    for (size_t i = 0; i < spec_capbufs->size(); i++) {
      auto capbuf = (*spec_capbufs)[i];
      auto future = (*spec_futures)[i];

      if (!future->_failed && capbuf->_image) {
        // The data should already be available in capbuf->_image after capture completes
        size_t data_size = capbuf->width() * capbuf->height() * 4; // RGBA8
        
        // Verify the datablock has the expected size
        OrkAssert(capbuf->_image->_data->length() == data_size);

        // Add raw data to datablock
        specular_datablock->addData(capbuf->_image->_data->data(), data_size);
        total_spec_size += data_size;

        // Store image for debug output
        debug_spec_images.push_back(capbuf->_image);

        logchan_gen->log("EnvMapProcessor: Added specular roughness level %zu (%zu bytes)", i, data_size);
      }
    }

    // Package all diffuse data as raw concatenated mip levels
    size_t total_diff_size = 0;
    for (size_t i = 0; i < diff_capbufs->size(); i++) {
      auto capbuf = (*diff_capbufs)[i];
      auto future = (*diff_futures)[i];

      if (!future->_failed && capbuf->_image) {
        // The data should already be available in capbuf->_image after capture completes
        size_t data_size = capbuf->width() * capbuf->height() * 4; // RGBA8
        
        // Verify the datablock has the expected size
        OrkAssert(capbuf->_image->_data->length() == data_size);

        // Add raw data to datablock
        diffuse_datablock->addData(capbuf->_image->_data->data(), data_size);
        total_diff_size += data_size;

        // Store image for debug output
        debug_diff_images.push_back(capbuf->_image);

        logchan_gen->log("EnvMapProcessor: Added diffuse mip level %zu (%zu bytes)", i, data_size);
      }
    }

    logchan_gen->log("EnvMapProcessor: Total specular data: %zu bytes", total_spec_size);
    logchan_gen->log("EnvMapProcessor: Total diffuse data: %zu bytes", total_diff_size);

    // Store datablocks and debug images in varmap for retrieval
    graph->_varmap.atomicOp([=](varmap::VarMap& vmap) {
      vmap.set<datablock_ptr_t>("specular_datablock", specular_datablock);
      vmap.set<datablock_ptr_t>("diffuse_datablock", diffuse_datablock);
      vmap.set<image_list_t>("debug_spec_images", debug_spec_images);
      vmap.set<image_list_t>("debug_diff_images", debug_diff_images);
    });

    logchan_gen->log("EnvMapProcessor: Packaging complete!");
    logchan_gen->log("  Specular datablock size: %zu bytes", specular_datablock->length());
    logchan_gen->log("  Diffuse datablock size: %zu bytes", diffuse_datablock->length());
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

  // Create the TaskGraph for filtering
  auto taskgraph = createFilteringTaskGraph(rawenvmap, is_equirectangular);

  // Store the future and texture name in the graph's varmap so tasks can access it
  taskgraph->_varmap.atomicOp([future, texture_name](varmap::VarMap& vmap) { 
    vmap.set<xirprocessfuture_ptr_t>("xir_future", future);
    vmap.set<std::string>("texture_name", texture_name);
  });

  // Execute on a worker thread with ContextExecutor
  opq::concurrentQueue()->enqueue([=]() {
    // Get a context executor (will use main thread for GPU ops)
    // Need to get context from somewhere - will be set up properly later
    // For now, we'll assume it gets passed through the varmap
    // auto ctx_executor = context->createContextExecutor();

    // Execute the graph - this will handle GPU operations properly

    auto on_graph_complete = [future](taskgraph_ptr_t g) {
      // When graph completes, extract results and package as XIR
      g->_varmap.atomicOp([future](varmap::VarMap& vmap) {
        datablock_ptr_t specular_data;
        datablock_ptr_t diffuse_data;
        image_list_t debug_spec_images;
        image_list_t debug_diff_images;

        if (auto spec_opt = vmap.typedValueForKey<datablock_ptr_t>("specular_datablock")) {
          specular_data = spec_opt.value();
        } else {
          OrkAssert(false); // fail early (we cannot move forward until this passes)
        }
        if (auto diff_opt = vmap.typedValueForKey<datablock_ptr_t>("diffuse_datablock")) {
          diffuse_data = diff_opt.value();
        } else {
          OrkAssert(false); // fail early (we cannot move forward until this passes)
        }
        
        // Get debug images if available
        if (auto spec_img_opt = vmap.typedValueForKey<image_list_t>("debug_spec_images")) {
          debug_spec_images = spec_img_opt.value();
        }
        if (auto diff_img_opt = vmap.typedValueForKey<image_list_t>("debug_diff_images")) {
          debug_diff_images = diff_img_opt.value();
        }

        datablock_ptr_t result_data;
        if (specular_data && diffuse_data) {
          // Package as XIR
          result_data = xir::XIRWriter::writeIrradianceMaps(diffuse_data, specular_data);
        } else {
          OrkAssert(false); // fail early (we cannot move forward until this passes)
        }

        // Set debug images in the future
        future->setDebugImages(debug_spec_images, debug_diff_images);
        
        // Set the result in the future
        future->setResult(result_data);
      }); // g->_varmap.atomicOp([future](varmap::VarMap& vmap) {
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
// processDirectory
//////////////////////////////////////////////////////////////////////////////////

std::vector<xirprocessfuture_ptr_t> EnvMapProcessor::processDirectory(
    const file::Path& source_dir,
    const file::Path& output_dir,
    const std::vector<std::string>& extensions) {

  std::vector<xirprocessfuture_ptr_t> futures;

  namespace bfs = boost::filesystem;
  if (!bfs::exists(source_dir.toBFS()) || !bfs::is_directory(source_dir.toBFS())) {
    // Return empty vector if source dir doesn't exist
    return futures;
  }

  // Create output directory
  output_dir.ensureDirectoryExists();

  // Collect all files to process
  std::vector<file::Path> files_to_process;

  // Process each file
  bfs::path dir_path = source_dir.toBFS();
  if (bfs::exists(dir_path) && bfs::is_directory(dir_path)) {
    for (auto& entry : bfs::directory_iterator(dir_path)) {
      if (bfs::is_regular_file(entry.path())) {
        file::Path input_file(entry.path());
        auto file_ext = input_file.getExtension();

        logchan_gen->log("Checking file: %s, extension: '%s'", input_file.c_str(), file_ext.c_str());

        // Add dot to extension for comparison (getExtension returns without dot)
        std::string ext_with_dot = "." + file_ext;

        // Check if extension matches
        if (std::find(extensions.begin(), extensions.end(), ext_with_dot) != extensions.end()) {
          files_to_process.push_back(input_file);
        }
      }
    }
  }

  // Process each file asynchronously
  for (const auto& input_file : files_to_process) {
    // Create a future that will write to disk when complete
    auto future = processToXIRDataBlockAsync(input_file);
    futures.push_back(future);
  }

  return futures;
}

} // namespace ork::lev2
