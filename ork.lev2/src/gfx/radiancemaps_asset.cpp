////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/radiancemaps_asset.h>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/image.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/asset/catalog/catalog.h>
#include <ork/kernel/datacache.h>
#include <ork/asset/AssetManager.h>
#include <ork/asset/Asset.inl>
#include <ork/rtti/RTTIX.inl>
#include <ork/util/hexdump.inl>
// Vulkan-backend coupling: shape-2 fence-gated swap reaches into VkContext's
// _pendingOneShotSemas to know when our upload CBs have GPU-completed.
// XIR loading is already Vulkan-coupled (via _createFromLoadReq /
// initTextureArray2DFromData), so this is an existing boundary.
#include "vulkan/headers/vulkan_ctx.h"
#include "vulkan/headers/vk_synchro.h"

ImplementReflectionX(ork::lev2::RadianceMapsAsset, "RadianceMapsAsset");

namespace ork::lev2 {
extern context_ptr_t gloadercontext;

///////////////////////////////////////////////////////////////////////////////

// Forward declaration
void registerRadianceLoader();

void RadianceMapsAsset::describeX(class_t* clazz) {
  // Register the loader for XIR files
  registerRadianceLoader();
}

///////////////////////////////////////////////////////////////////////////////

RadianceMapsLoader::RadianceMapsLoader() {
  // Register for .xir extension
  // Note: The registration needs to happen when the loader is created,
  // typically during static initialization
}

///////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t RadianceMapsLoader::_doLoadFromDatablock(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t dblock) {
  return _loadFromXIR(loadreq, dblock);
}

///////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t RadianceMapsLoader::_loadFromXIR(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t xir_data) {

  static const bool s_xirdbg = (getenv("ORKID_XIR_DEBUG") != nullptr);

  // Create asset
  auto asset = std::make_shared<RadianceMapsAsset>();
  auto irrmaps = std::make_shared<pbr::RadianceMaps>();
  asset->_radiance_maps = irrmaps;
  asset->_name = loadreq->_asset_path.toStdString();
  std::string base_name = loadreq->_asset_path.getName();

  // Capture the target render context HERE on the requesting thread.
  // For modelviewer / typical scene init this is the main thread, whose
  // TLS is bound to the main render context. For future secondary-window
  // threads, their own TLS resolves to their own render context — the
  // capture naturally Does The Right Thing per-thread.
  //
  // Guard against being requested from the loader thread itself (whose
  // TLS = gloadercontext, TargetType::LOADING). Landing the swap there
  // would race renders. If TLS resolves to a LOADING context, fall back
  // to GetMainWindow()->context() at handoff time.
  Context* requesting_ctx = ork::lev2::contextForCurrentThread();
  if (requesting_ctx && requesting_ctx->meTargetType == TargetType::LOADING) {
    requesting_ctx = nullptr;
  }

  // Claim one pending unit on the LoadRequest's counter for the entire
  // concurrent-decode + deferred-GPU-upload chain. The terminal deferred
  // op below decrements once every upload has finished, letting sync
  // callers (CommonStuff::requestRadianceMapsSync) block on the counter
  // hitting zero. Error paths in the concurrent decode must also decrement
  // or the waiter hangs forever.
  loadreq->incrementPartialLoadCount();

  auto op = [=](){
    if(s_xirdbg)printf("[VKMT-DBG] op start path<%s> gloadercontext<%p> requesting_ctx<%p>\n",
           asset->_name.c_str(), (void*)gloadercontext.get(), (void*)requesting_ctx);
    //fflush(stdout);

      // Use XIRReader to get raw datablocks
    auto xir_data_result = xir::XIRReader::readXirDatablocks(xir_data);

    if (!xir_data_result._valid) {
      printf("XIR data invalid\n");
      loadreq->_assetStatus = "NoData"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      loadreq->decrementPartialLoadCount();
      return;
    }

    if (!xir_data_result._is_array_format) {
      printf("ERROR: XIR v1 legacy format no longer supported. Please regenerate radiance maps.\n");
      loadreq->_assetStatus = "InvalidFormat"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      loadreq->decrementPartialLoadCount();
      return;
    }

    bool has_diffuse = xir_data_result._diffuse_data && xir_data_result._diffuse_data->length() > 0;

    if(s_xirdbg)printf("XIR v2 array format: diffuse size: %zu, %d roughness levels\n",
           has_diffuse ? xir_data_result._diffuse_data->length() : 0,
           xir_data_result._num_roughness_levels);

    ////////////////////////////////////
    // Parse diffuse data (if present)
    ////////////////////////////////////

    std::shared_ptr<CompressedImageMipChain> diffuse_cmipchain;
    std::shared_ptr<Texture> diffuse_tex;
    if (has_diffuse) {
      diffuse_cmipchain = std::make_shared<CompressedImageMipChain>();
      diffuse_cmipchain->readXTX(xir_data_result._diffuse_data);
      diffuse_tex = std::make_shared<Texture>();
      diffuse_tex->_debugName = base_name + ".ibldiff";
    }
    
    ////////////////////////////////////
    // Parse specular roughness array
    ////////////////////////////////////

    std::vector<image_ptr_t> specular_images;
    int num_roughness_levels = xir_data_result._num_roughness_levels;
    
    for(int i = 0; i < num_roughness_levels; i++) {
      // Read single-level XTX for each roughness
      auto cmipchain = std::make_shared<CompressedImageMipChain>();
      cmipchain->readXTX(xir_data_result._specular_datablocks[i]);
      // Convert to Image for texture array
      auto image = std::make_shared<Image>();
      cmipchain->_levels[0].convertToImage(*image);
      specular_images.push_back(image);
    }
    
    ////////////////////////////////////
    // Create texture array for specular
    ////////////////////////////////////

    auto specular_texarray = std::make_shared<TextureArray>();
    specular_texarray->_tex->_debugName = base_name + ".iblspec_array";
    
    ////////////////////////////////////
    // Set radiance map properties
    ////////////////////////////////////

    //////////////////////////////////////////////////////////////
    // Sandbox build + handoff pattern (Phase 6.3 Variant B):
    //   1. The 3 upload ops populate brand-new texture objects owned
    //      only by the loader thread. They DO NOT touch `irrmaps`.
    //   2. The 4th (terminal) op enqueues a swap-op onto the RENDER
    //      context's _deferredOps. The swap-op writes every published
    //      field on `irrmaps` from the render thread — assign-new, not
    //      mutate-in-place — and decrements the load counter.
    //   3. Render thread reads/writes `irrmaps`'s fields from one thread
    //      only, so the cross-thread shared_ptr-instance race is gone.
    //////////////////////////////////////////////////////////////

    // Sandbox holders for the BRDF integration maps. brdfSetOp populates
    // them on the loader thread (PBRMaterial::brdfIntegrationMap is a
    // process-global cache; the holder just snapshots the pointer the
    // loader saw, so the swap-op can publish it without re-querying).
    struct BrdfHolder {
      texture_ptr_t _ggx, _velvet, _ggxrim, _blinn, _phong;
    };
    auto brdf_holder = std::make_shared<BrdfHolder>();

    // Build the LoadingPhase LOCALLY, populate with all ops, then submit
    // atomically. Using newLoadingPhase() here would publish an empty
    // phase that the loader-thread drainer could pop before we enqueue
    // any ops — orphaning them silently. submitLoadingPhase publishes
    // the fully-populated phase as one atomic step.
    auto loading_phase = std::make_shared<LoadingPhase>();

    // Shape-2 fence-gated swap state.
    // We snapshot vk_loader->_pendingOneShotSemas BEFORE the upload ops
    // run, then again AFTER, and diff to identify the completion
    // semaphores belonging to OUR upload CBs. handoffOp later polls
    // those semaphores; only once they all signal (GPU has completed
    // every upload) do we enqueue the swap-op on the render context.
    auto* vk_loader   = dynamic_cast<vulkan::VkContext*>(gloadercontext.get());
    auto before_semas = std::make_shared<vulkan::vkcompsema_set_t>();
    auto our_semas    = std::make_shared<std::vector<vulkan::vkcompletionsemaphore_ptr_t>>();

    auto snapshotBeforeOp = [vk_loader, before_semas](Context*) {
      if (!vk_loader) return;
      vk_loader->_pendingOneShotSemas.atomicOp(
        [&before_semas](vulkan::vkcompsema_set_t& semas) {
          *before_semas = semas;
        });
    };
    auto captureOurSemasOp = [vk_loader, before_semas, our_semas](Context*) {
      if (!vk_loader) return;
      vk_loader->_pendingOneShotSemas.atomicOp(
        [&before_semas, &our_semas](vulkan::vkcompsema_set_t& semas) {
          for (auto& s : semas) {
            if (before_semas->find(s) == before_semas->end()) {
              our_semas->push_back(s);
            }
          }
        });
    };

    // First op in the phase: snapshot the loader context's pending
    // completion-semaphore set BEFORE any of our upload ops run. The
    // diff against the post-upload state (captured later) gives us
    // exactly OUR semaphores.
    loading_phase->enqueueOperation(snapshotBeforeOp);

    //////////////////////////////////////////////////////////////
    // Diffuse upload op — populates sandbox texture only.
    //////////////////////////////////////////////////////////////

    if (has_diffuse) {
      auto diffuseUploadOp = [=](Context* ctx) {
        auto diffuse_loadreq = std::make_shared<TexLoadReq>();
        diffuse_loadreq->ptex = diffuse_tex;
        diffuse_loadreq->_cmipchain = diffuse_cmipchain;
        diffuse_loadreq->_texname = base_name + ".irrdiff";
        ctx->TXI()->_createFromLoadReq(diffuse_loadreq);
        // Equirectangular: U wraps (longitude seam), V clamps (no pole bleed
        // across the wraparound when bilinear filtering at V=0 / V=1).
        diffuse_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
        diffuse_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
        diffuse_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
        ctx->TXI()->ApplySamplingMode(diffuse_tex.get());
      };
      loading_phase->enqueueOperation(diffuseUploadOp);
    }

    //////////////////////////////////////////////////////////////
    // Specular texture-array upload op — populates sandbox only.
    //////////////////////////////////////////////////////////////

    auto initTexArrayOp = [=](Context* ctx) {
      auto txi = ctx->TXI();
      TextureArrayInitData TID;
      TID._slices.resize(num_roughness_levels);
      for(int i = 0; i < num_roughness_levels; i++) {
        // Use CRC for usage ID, similar to PBRMaterial
        uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
        TID._slices[i] = TextureArrayInitSubItem{usage_id, specular_images[i]};
      }
      txi->initTextureArray2DFromData(specular_texarray.get(), TID);
      specular_texarray->_tex->TexSamplingMode()._texAddrModeS = TextureAddressMode::WRAP;
      specular_texarray->_tex->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
      specular_texarray->_tex->TexSamplingMode()._texAddrModeR = TextureAddressMode::CLAMP;
      txi->ApplySamplingMode(specular_texarray->_tex.get());
    };

    loading_phase->enqueueOperation(initTexArrayOp);

    //////////////////////////////////////////////////////////////
    // BRDF integration maps — snapshot into sandbox holder.
    //////////////////////////////////////////////////////////////

    auto brdfSetOp = [=](Context* ctx) {
      brdf_holder->_ggx     = PBRMaterial::brdfIntegrationMap(ctx,"GGX");
      brdf_holder->_velvet  = PBRMaterial::brdfIntegrationMap(ctx,"GGXVELVET");
      brdf_holder->_ggxrim  = PBRMaterial::brdfIntegrationMap(ctx,"GGXRIM");
      brdf_holder->_blinn   = PBRMaterial::brdfIntegrationMap(ctx,"BLINN");
      brdf_holder->_phong   = PBRMaterial::brdfIntegrationMap(ctx,"PHONG");
    };

    loading_phase->enqueueOperation(brdfSetOp);

    //////////////////////////////////////////////////////////////
    // Terminal handoff op (still loader thread). Captures roughness
    // metadata + the sandbox texture objects + the BRDF holder and
    // enqueues a single swap-op onto the RENDER context's deferred
    // queue. That swap-op fires on the render thread inside its next
    // beginFrame, prior to render — writing every published field on
    // `irrmaps` from a single thread.
    //////////////////////////////////////////////////////////////

    auto roughness_values = xir_data_result._roughness_values;

    // Explicit captures for handoffOp / swap_op — DELIBERATELY NOT [=].
    // The outer `op` scope holds large CPU-side decoded image data
    // (diffuse_cmipchain, specular_images, xir_data_result). A `[=]`
    // capture here would drag all of that through into the swap_op
    // lambda, which runs on the render thread; when swap_op finishes,
    // the captures destruct on the render thread and free those large
    // CPU buffers inline — visible as a black frame proportional to
    // HDRI size. Explicit captures restrict the closure to just the
    // GPU handles + metadata the swap actually needs.
    auto handoffOp = [
        requesting_ctx,
        irrmaps,
        loadreq,
        num_roughness_levels,
        roughness_values,
        has_diffuse,
        diffuse_tex,
        specular_texarray,
        brdf_holder,
        our_semas
      ](Context* loader_ctx) {
      // Resolve the publish target:
      //   1. Prefer the requesting thread's context (captured up-top via TLS).
      //   2. Fall back to GfxEnv::mainRenderContext() — the canonical
      //      longest-lived destination (see gfxenv.h for the convention).
      Context* target_ctx = requesting_ctx;
      if (!target_ctx) {
        target_ctx = GfxEnv::mainRenderContext();
      }
      if (s_xirdbg)
        printf("[VKMT-DBG] handoffOp target_ctx<%p> num_semas<%zu>\n", (void*)target_ctx, our_semas->size());
      auto swap_op = [
          irrmaps,
          loadreq,
          num_roughness_levels,
          roughness_values,
          has_diffuse,
          diffuse_tex,
          specular_texarray,
          brdf_holder
        ](Context* render_ctx_drain) {
        // RENDER THREAD: assign-new every published field on irrmaps.
        // Subsequent render-thread reads see them in this thread's own
        // program order, so there's no cross-thread shared_ptr race.
        //
        // Snapshot the OUTGOING textures BEFORE reassignment and hand
        // them to the per-context delayed-destroy queue. Held there for
        // a few frames so the GPU finishes using them before vkDestroy*
        // runs — otherwise MoltenVK serializes synchronously on
        // vkFreeMemory and we get a black frame proportional to the
        // texture's mip count / total memory.
        constexpr int kDelayFrames = 3; // > MAX_FRAMES_IN_FLIGHT
        if (s_xirdbg)
          printf("[VKMT-DBG] swap_op FIRED on ctx<%p>\n", (void*)render_ctx_drain);
        if (render_ctx_drain) {
          auto old_diffuse = irrmaps->_filtenvDiffuseMap;
          auto old_specular = irrmaps->_filtenvSpecularMapArray;
          // BRDF maps are global singletons (cached in PBRMaterial), so
          // dropping these refs just decrements; never destructs. Skip
          // the delayed-destroy for those — saves queue churn.
          if (old_diffuse || old_specular) {
            render_ctx_drain->enqueueDelayedDestroy(
              [old_diffuse, old_specular]() {
                // captures destruct here on render thread after delay
              },
              kDelayFrames);
          }
        }
        irrmaps->_numRoughnessLevels      = num_roughness_levels;
        irrmaps->_specularRoughnessValues = roughness_values;
        if (has_diffuse) {
          irrmaps->_filtenvDiffuseMap = diffuse_tex;
        }
        irrmaps->_filtenvSpecularMapArray   = specular_texarray;
        irrmaps->_brdfIntegrationMapGGX     = brdf_holder->_ggx;
        irrmaps->_brdfIntegrationMapVelvet  = brdf_holder->_velvet;
        irrmaps->_brdfIntegrationMapGGXRIM  = brdf_holder->_ggxrim;
        irrmaps->_brdfIntegrationMapBlinn   = brdf_holder->_blinn;
        irrmaps->_brdfIntegrationMapPhong   = brdf_holder->_phong;
        if (loadreq->_on_load_complete) {
          loadreq->_on_load_complete();
        }
        loadreq->decrementPartialLoadCount();
      };
      if (!target_ctx) {
        // Headless / no main window — run inline so the counter still
        // makes forward progress and waiters unblock.
        swap_op(nullptr);
        return;
      }

      // Shape-2 fence gate: poll our captured upload semaphores until
      // they all signal (GPU has actually finished the uploads). Only
      // then enqueue the swap-op on the render context.
      //
      // The polling retries via gloadercontext's delayed-destroy queue
      // with delay=1 (re-fires on the next loader-thread beginFrame).
      // This is necessary because the upload CBs are only submitted at
      // the next loader iteration's _doPreBeginFrame → endFrame; an
      // inline vkWaitSemaphores here would deadlock waiting on signals
      // that can't happen until the loader thread continues.
      //
      // Net effect: handoffOp returns immediately, the loader thread
      // proceeds to endFrame (submits the upload CBs), and on each
      // subsequent iteration the poll retries until all uploads are
      // GPU-complete. Then swap_op fires on the render thread — minimal
      // cost, new textures fully resident, no render-frame stall.
      auto poll_op = std::make_shared<::ork::void_lambda_t>();
      *poll_op = [target_ctx, swap_op, our_semas, loader_ctx, poll_op]() {
        bool all_signaled = true;
        for (auto& s : *our_semas) {
          if (!s->isSignalled()) {
            all_signaled = false;
            break;
          }
        }
        if (all_signaled) {
          if (s_xirdbg)
            printf("[VKMT-DBG] poll_op all semas signaled, enqueueing swap_op on ctx<%p>\n", (void*)target_ctx);
          target_ctx->enqueueDeferredOp(swap_op);
          // poll_op self-ref drops naturally — no further re-enqueue.
        } else {
          auto self = poll_op;
          loader_ctx->enqueueDelayedDestroy([self]() { (*self)(); }, 1);
        }
      };
      (*poll_op)();
    };
    // Order matters:
    //   1. snapshotBeforeOp captures the prior pending-sema set
    //   2. upload ops queue their CBs (each adds a completion semaphore)
    //   3. captureOurSemasOp diffs the set to extract OUR semaphores
    //   4. handoffOp polls until our semaphores signal, then dispatches swap_op
    loading_phase->enqueueOperation(captureOurSemasOp);
    loading_phase->enqueueOperation(handoffOp);

    // Phase fully populated with all 4 ops — publish atomically. No race
    // window for the loader thread to pop an empty phase.
    if(s_xirdbg)printf("[VKMT-DBG] op submitting LoadingPhase with 4 ops to gloadercontext<%p>\n",
           (void*)gloadercontext.get());
    //fflush(stdout);
    gloadercontext->submitLoadingPhase(loading_phase);
    if(s_xirdbg)printf("[VKMT-DBG] op end path<%s>\n", asset->_name.c_str());
    //fflush(stdout);

    if(s_xirdbg)printf("XIR asset<%p> irrmaps<%p> dtex<%p> stexarray<%p> roughness_levels<%d>\n",
           (void*) asset.get(), (void*) irrmaps.get(),
           diffuse_tex ? diffuse_tex.get() : nullptr,
           specular_texarray.get(), num_roughness_levels);
  };
  opq::concurrentQueue()->enqueue(op);
  //op();
  return asset;
}

///////////////////////////////////////////////////////////////////////////////

// Static loader instance - will be created during static initialization

// Registration function to be called during initialization
void registerRadianceLoader() {
  static auto _Radiance_loader = std::make_shared<RadianceMapsLoader>();
  asset::registerLoader<RadianceMapsAsset>(_Radiance_loader);
  asset::AssetLoader::registerLoaderForExtension("xir", _Radiance_loader);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

template struct ork::asset::AssetManager<ork::lev2::RadianceMapsAsset>;
