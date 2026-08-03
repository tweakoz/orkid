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
#include <ork/kernel/async_tracker.h>
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

// ORKID_XIR_DEBUG traces the XIR load/upload/handoff pipeline (the prime suspect in
// silent linux load deaths). Value semantics: any value = trace to stdout; an ABSOLUTE
// PATH = write the trace to that file instead. Every line is flushed, so a hang or
// crash leaves the trail on disk.
static FILE* _xirdbgStream() {
  static FILE* s = []() -> FILE* {
    const char* v = getenv("ORKID_XIR_DEBUG");
    if (nullptr == v)
      return nullptr;
    if (v[0] == '/')
      if (FILE* f = fopen(v, "w"))
        return f;
    return stdout;
  }();
  return s;
}
#define XIRDBG(...)                                                                                                    \
  do {                                                                                                                 \
    if (FILE* xf_ = _xirdbgStream()) {                                                                                 \
      fprintf(xf_, __VA_ARGS__);                                                                                       \
      fflush(xf_);                                                                                                     \
    }                                                                                                                  \
  } while (0)

asset::asset_ptr_t RadianceMapsLoader::_loadFromXIR(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t xir_data) {

  // Create asset
  auto asset = std::make_shared<RadianceMapsAsset>();
  auto irrmaps = std::make_shared<pbr::RadianceMaps>();
  asset->_radiance_maps = irrmaps;
  // The maps object carries its OWN request, so any later holder of it can ask
  // whether the decode+upload+publish chain has finished without re-deriving the
  // path or re-entering the asset manager. Set before the concurrent decode is
  // enqueued: a consumer can be handed these maps on the very next line.
  irrmaps->_loadRequest = loadreq;
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

  // Join-narrowing v1 (JUL05 Appendix C step 1): envmap/radiance is the FIRST
  // STREAMABLE asset class — de-facto streamed today (lazy, post-link) but
  // silently in-limbo because it never told anyone. Register the WHOLE
  // decode+upload+publish chain as async work so the settle gate
  // (asyncWorkPending) waits for the skybox instead of exiting on the black
  // settle-race frame (#27). Paired 1:1 with the partial-load counter above:
  // begin here, asyncWorkEnd at EVERY decrement site (both error returns + the
  // terminal render-thread swap) so the tracker covers the deferred GPU publish.
  asyncWorkBegin("radiancemaps");

  auto op = [=](){
    XIRDBG("[VKMT-DBG] op start path<%s> gloadercontext<%p> requesting_ctx<%p>\n",
           asset->_name.c_str(), (void*)gloadercontext.get(), (void*)requesting_ctx);

      // Use XIRReader to get raw datablocks
    auto xir_data_result = xir::XIRReader::readXirDatablocks(xir_data);

    if (!xir_data_result._valid) {
      printf("XIR data invalid\n");
      loadreq->_assetStatus = "NoData"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      loadreq->decrementPartialLoadCount();
      asyncWorkEnd("radiancemaps"); // streamable chain terminated (load failed)
      return;
    }

    if (!xir_data_result._is_array_format) {
      printf("ERROR: XIR v1 legacy format no longer supported. Please regenerate radiance maps.\n");
      loadreq->_assetStatus = "InvalidFormat"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      loadreq->decrementPartialLoadCount();
      asyncWorkEnd("radiancemaps"); // streamable chain terminated (load failed)
      return;
    }

    XIRDBG("XIR v2 array format: %d roughness levels\n", xir_data_result._num_roughness_levels);

    // The container's DIFFUSE STREAM IS NOT READ (W4-S9). Every .xir on disk
    // still carries the prefiltered irradiance chain it was baked with, and
    // nothing samples it any more: the ambient is projected from specular
    // level 0 below.

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
    // THE AMBIENT (W4-S9) — the authored sky's nine L2 coefficients, projected
    // HERE, on the decode thread, from roughness level 0 (the prefilter's
    // identity level, i.e. the authored equirect itself). This is the whole
    // diffuse ambient of a baked scene; it is published with the textures in
    // the swap-op below so a frame can never see maps without their sky.
    ////////////////////////////////////

    pbr::RadianceSH radiance_sh;
    if (not specular_images.empty())
      radiance_sh = pbr::projectRadianceSH(*specular_images[0]);
    OrkAssertIFMT(
        radiance_sh._valid,
        "radiance maps <%s> carry no projectable roughness-0 level - the scene would have NO diffuse ambient",
        base_name.c_str());
    
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
    // (specular_images, xir_data_result). A `[=]`
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
        radiance_sh,
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
      XIRDBG("[VKMT-DBG] handoffOp target_ctx<%p> num_semas<%zu>\n", (void*)target_ctx, our_semas->size());
      auto swap_op = [
          irrmaps,
          loadreq,
          num_roughness_levels,
          roughness_values,
          radiance_sh,
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
        XIRDBG("[VKMT-DBG] swap_op FIRED on ctx<%p>\n", (void*)render_ctx_drain);
        if (render_ctx_drain) {
          auto old_specular = irrmaps->_filtenvSpecularMapArray;
          // BRDF maps are global singletons (cached in PBRMaterial), so
          // dropping these refs just decrements; never destructs. Skip
          // the delayed-destroy for those — saves queue churn.
          if (old_specular) {
            render_ctx_drain->enqueueDelayedDestroy(
              [old_specular]() {
                // captures destruct here on render thread after delay
              },
              kDelayFrames);
          }
        }
        irrmaps->_numRoughnessLevels      = num_roughness_levels;
        irrmaps->_specularRoughnessValues = roughness_values;
        pbr::assignRadianceSH(irrmaps, radiance_sh);
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
        asyncWorkEnd("radiancemaps"); // streamable chain terminated (final swap published)
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
      auto poll_op    = std::make_shared<::ork::void_lambda_t>();
      auto poll_count = std::make_shared<int>(0);
      *poll_op = [target_ctx, swap_op, our_semas, loader_ctx, poll_op, poll_count]() {
        int signaled = 0;
        for (auto& s : *our_semas)
          if (s->isSignalled())
            signaled++;
        bool all_signaled = (signaled == int(our_semas->size()));
        // hang signature: repeating STILL-WAITING = upload CBs never GPU-complete
        // (submission/sema bug); NO further lines at all = the loader thread stopped
        // pumping (poll re-enqueue starved). ~1s cadence at the loader's 500µs tick.
        if ((++(*poll_count) % 2000) == 0)
          XIRDBG("[VKMT-DBG] poll_op STILL WAITING polls<%d> semas<%d/%zu signaled>\n",
                 *poll_count, signaled, our_semas->size());
        if (all_signaled) {
          XIRDBG("[VKMT-DBG] poll_op all semas signaled, enqueueing swap_op on ctx<%p>\n", (void*)target_ctx);
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
    XIRDBG("[VKMT-DBG] op submitting LoadingPhase with 4 ops to gloadercontext<%p>\n",
           (void*)gloadercontext.get());
    // This decode runs on the concurrent queue and routinely outlives the
    // frame that requested it: stopLoaderThread() can release gloadercontext
    // while we are still decoding (app teardown), leaving nothing to upload
    // to. Terminate the chain the same way the decode error paths do — the
    // partial-load counter and the async-work tracker MUST be released here
    // or a sync waiter (requestRadianceMapsSync / settle) blocks forever.
    auto loader_ctx = gloadercontext;
    if (nullptr == loader_ctx) {
      printf("ERROR: radiancemaps<%s>: loader context released mid-load (app teardown) - GPU upload skipped\n",
             asset->_name.c_str());
      loadreq->_assetStatus = "NoLoaderContext"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      loadreq->decrementPartialLoadCount();
      asyncWorkEnd("radiancemaps"); // streamable chain terminated (no upload target)
      return;
    }
    loader_ctx->submitLoadingPhase(loading_phase);
    XIRDBG("[VKMT-DBG] op end path<%s>\n", asset->_name.c_str());

    XIRDBG("XIR asset<%p> irrmaps<%p> stexarray<%p> roughness_levels<%d> sh_lum<%g>\n",
           (void*) asset.get(), (void*) irrmaps.get(),
           specular_texarray.get(), num_roughness_levels, double(radiance_sh._luminance));
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
