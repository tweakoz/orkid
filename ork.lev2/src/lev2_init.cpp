////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/profiler.h>
#include <ork/dataflow/all.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/asset_gen.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#if defined(__APPLE__)
#include <pthread/qos.h>  // QOS_CLASS_UTILITY for loader thread
#endif
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPtx.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeSSSS.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHeatDistort.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHSVG.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeACES.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/scenegraph/sgnode_projectedgrid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_imposter.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/vr/openxr.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h>
#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <ork/lev2/gfx/hypermesh/hm_drawable.h>
#include <ork/lev2/gfx/sdf/sdfdflow.h> // E.7: the sdfgrid family
#include <ork/lev2/gfx/terrain/terrain_chunk_drawable.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
#include <ork/reflect/properties/codec.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/layer.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/controller.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/dsp_pmx.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/spectral.h>
#include <ork/lev2/aud/spatializer.h>
#include <ork/math/plane.hpp>

#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_test_objects.h>
#include <ork/lev2/ui/ged/ged_factory.h>
#include <ork/lev2/gfx/radiancemaps_asset.h>

#include <openvdb/openvdb.h>
#include <openvdb/points/PointDataGrid.h>
#include <openvdb/tools/PointIndexGrid.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/SignedFloodFill.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/util/NullInterrupter.h>
#include <openvdb_ax/compiler/Logger.h>
#include <openvdb_ax/compiler/VolumeExecutable.h>
#include <openvdb_ax/compiler/Compiler.h>
#include <llvm/Support/TargetSelect.h>

///////////////////////////////////////////////////////////////////////////////
// #define WIIEMU
///////////////////////////////////////////////////////////////////////////////

namespace ork {
void initModule(appinitdata_ptr_t init_data);
void exitModule(appinitdata_ptr_t init_data);
namespace lev2 {

appinitdata_ptr_t _ginitdata;
context_ptr_t gloadercontext;

////////////////////////////////////////////////////////////////////////////////
// Loader thread — owns gloadercontext's frame pump. Lifecycle is bound to
// gloadercontext: spawned automatically the moment gloadercontext is created
// (both initModule and ensureLoaderContext paths), stopped explicitly via
// stopLoaderThread() before gloadercontext is torn down.
//
// Why this lives in lev2_init.cpp (not ezapp.cpp): the Python OrkEzApp.create
// binding goes through initModule() directly and bypasses the free function
// lev2appinit, so any spawn point inside ezapp.cpp would silently no-op for
// the Python path. Co-locating with gloadercontext guarantees any caller that
// brings the context to life also gets a thread pumping it.
////////////////////////////////////////////////////////////////////////////////
namespace {
// Loader-thread hooks. Stored behind a mutex; callbacks are copied locally
// per-iteration so they can fire without holding the lock (so the Python
// callback body can take as long as it wants without blocking re-registration).
struct LoaderHooks {
  std::mutex mtx;
  loader_callback_t init_cb;
  loader_callback_t update_cb;
  loader_callback_t exit_cb;
};
static LoaderHooks g_loader_hooks;

struct LoaderThread {
  std::thread _thread;
  std::atomic<bool> _stop{false};
  std::atomic<bool> _started{false};
  void start() {
    bool expected = false;
    if (!_started.compare_exchange_strong(expected, true)) {
      return;
    }
    // Universal bare-exit safety net (#32, NVIDIA loader-race). Register an atexit
    // that winds the loader down. It is registered HERE — AFTER createLoaderContext()
    // dlopen'd the Vulkan driver .so — so glibc's LIFO atexit ordering runs it BEFORE
    // the driver library is torn down at process exit. Paths with an orderly GPU-exit
    // funnel (OrkEzApp::_onGpuExit, ecs.headless_exit) stop the loader earlier and
    // leave this a no-op (stopLoaderThread is idempotent). Without it, a bare C++
    // exit (test::harness, a leaked EzApp) leaves this thread pumping
    // gloadercontext->endFrame -> VkThreadedQueue::queueSubmit into an already-unloaded
    // driver -> a null-entrypoint SIGSEGV on the "loader" thread AFTER all output. The
    // static ~LoaderThread below still stops it too, but that dtor runs too late — the
    // runtime-dlopen'd driver unwinds before it (registered earlier => runs later).
    std::atexit(stopLoaderThread);
    _thread = std::thread([this]() {
      ork::SetCurrentThreadName("loader");
#if defined(__APPLE__)
      // Drop QoS so the kernel scheduler prefers the main/render thread
      // (USER_INTERACTIVE / USER_INITIATED) over loader work whenever
      // they're contending. The loader still gets full CPU when idle,
      // just yields under contention. Without this, large XIR uploads
      // (8K HDRI conversion loops in vulkan_txi_from_array.cpp) can
      // preempt the render thread for tens of ms → visible stutter.
      pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif
      ThreadGfxContext tls(gloadercontext.get());
      bool init_fired = false;
      while (!_stop.load(std::memory_order_acquire)) {
        // Fire init hook once, when first registered. Handles the race
        // where the loader spawns before Python sets the callback.
        if (!init_fired) {
          loader_callback_t cb;
          { std::lock_guard<std::mutex> lk(g_loader_hooks.mtx); cb = g_loader_hooks.init_cb; }
          if (cb) {
            cb(gloadercontext);
            init_fired = true;
          }
        }
        gloadercontext->beginFrame(false);
        // Fire update hook each iteration between begin/end so any
        // uploadTextureRegion/etc. work it queues lands in this frame's
        // submission.
        {
          loader_callback_t cb;
          { std::lock_guard<std::mutex> lk(g_loader_hooks.mtx); cb = g_loader_hooks.update_cb; }
          if (cb) cb(gloadercontext);
        }
        gloadercontext->endFrame();
        // Throttle: explicit yield + sleep between iterations. The yield
        // gives the scheduler an opportunity to switch to higher-QoS
        // threads (render) before this thread re-enters another full
        // beginFrame/endFrame cycle. The sleep caps the iteration rate
        // so the loader doesn't busy-spin when idle.
        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
      }
      // Fire exit hook before the thread function returns.
      {
        loader_callback_t cb;
        { std::lock_guard<std::mutex> lk(g_loader_hooks.mtx); cb = g_loader_hooks.exit_cb; }
        if (cb) cb(gloadercontext);
      }
    });
  }
  void stop() {
    _stop.store(true, std::memory_order_release);
    if (_thread.joinable()) _thread.join();
  }
  // atexit / static-destruction safety net. The orderly teardown (OrkEzApp
  // dtor, ecs pyext) calls stopLoaderThread() before the context dies. But a
  // bare interpreter exit (sys.exit -> Py_Exit -> __run_exit_handlers) can
  // finalize Python while the OrkEzApp object is still leaked through a
  // reference cycle, so that dtor never runs and this thread is left running
  // and joinable. The default std::thread dtor would then std::terminate
  // ("terminate called without an active exception") -> SIGABRT/Abort trap. It
  // could also still be pumping gloadercontext->beginFrame/endFrame into a
  // context whose backend is being torn down -> VkOffscreen::submit(ctxVK=0x0)
  // SIGSEGV. Stop+join here so the thread is always wound down first. This
  // static is defined AFTER gloadercontext in this TU, so it is destroyed
  // BEFORE it — the join completes while the loader context is still live.
  // Idempotent with stopLoaderThread()'s stop().
  ~LoaderThread() {
    stop();
  }
};
static LoaderThread g_loader_thread;
} // anon

void stopLoaderThread() {
  // 1. Stop the pump thread so it can no longer produce/consume work.
  g_loader_thread.stop();

  if (gloadercontext) {
    // 2. Backend teardown via Context::shutdown() — drains the
    //    deferred-op queue, calls _doShutdown() (subclass releases
    //    GPU resources, may enqueue more work), drains again, locks
    //    the queue against further enqueues. Bind this thread to the
    //    loader context so resource dtors run with the right gfx-ctx
    //    on TLS.
    ThreadGfxContext tls(gloadercontext.get());
    gloadercontext->shutdown();

    // 3. Release the loader context. Other shared_ptrs to it (e.g.
    //    GfxEnv-side lambdas captured during initializeWithContext)
    //    keep it alive until static destruction; the shutdown() flag
    //    set above ensures any late dtor activity on those refs is
    //    safe (enqueueDeferredOp silently drops post-shutdown).
    gloadercontext.reset();
  }
}

void setOnLoaderInit(loader_callback_t cb) {
  std::lock_guard<std::mutex> lk(g_loader_hooks.mtx);
  g_loader_hooks.init_cb = std::move(cb);
}
void setOnLoaderUpdate(loader_callback_t cb) {
  std::lock_guard<std::mutex> lk(g_loader_hooks.mtx);
  g_loader_hooks.update_cb = std::move(cb);
}
void setOnLoaderExit(loader_callback_t cb) {
  std::lock_guard<std::mutex> lk(g_loader_hooks.mtx);
  g_loader_hooks.exit_cb = std::move(cb);
}

uint64_t GRAPHICS_API = "VULKAN"_crcu;

namespace vulkan{
  lev2::context_ptr_t createLoaderContext();
  void touchClasses();
};
namespace dummy{
  lev2::context_ptr_t createLoaderContext();
  void touchClasses();
}

void registerEnums();
bool selectVrDeviceFromEnv(); // OPENXR X2 — defined below; selects OpenXR device pre-Vulkan.


struct ClassToucher {
  ClassToucher(appinitdata_ptr_t aid) {

    logger()->defaultChannel()->log( "ork.lev2 classes registered...");

    AllocationLabel label("ork::lev2::Init");

    Context::GetClassStatic();
    if(aid->_enable_graphics){
      vulkan::touchClasses();
      dummy::touchClasses();

      ////////////////////////////////////////

      std::string gfx_api_str;
      if( genviron.get("ORKID_GRAPHICS_API",gfx_api_str) ){
        if(gfx_api_str=="VULKAN"){
          GRAPHICS_API  = "VULKAN"_crcu;
        }
        else if(gfx_api_str=="DUMMY"){
          GRAPHICS_API  = "DUMMY"_crcu;
        }
      }

      ////////////////////////////////////////
      // OPENXR X2: select the VR device (from ORKID_VR_DRIVER) BEFORE the loader
      //  context is created, so an OpenXR device participates in the X1 pre-graphics
      //  seam. The non-deferred path creates the loader context immediately below;
      //  the deferred/subsystem path creates it later (GfxInit re-selects there).
      ////////////////////////////////////////

      selectVrDeviceFromEnv();

      ////////////////////////////////////////
      // Create loader context now unless deferred for subsystem mode
      ////////////////////////////////////////

      if (!aid->_defer_gpu_init) {
        switch(GRAPHICS_API){
          case "DUMMY"_crcu:{
            gloadercontext = dummy::createLoaderContext();
            //GfxEnv::setContextClass(clazz);
            OrkAssert(false);
            break;
          }
          case "VULKAN"_crcu:
          default: {
            gloadercontext = vulkan::createLoaderContext();
            if(0)printf("gloadercontext (VK) <%p>\n", (void*)gloadercontext.get());
            // Auto-spawn loader thread the moment gloadercontext exists.
            // Covers all initModule callers (Python OrkEzApp.create binding,
            // lev2appinit, etc.) without each having to remember to spawn.
            g_loader_thread.start();
            break;
          }
        }
      }
    }

    //////////////////////////////////////////
    // touch of class
    //////////////////////////////////////////

    ged::GedObject::GetClassStatic();
    ged::GedItemNode::GetClassStatic();
    ged::GedRootNode::GetClassStatic();
    ged::GedGroupNode::GetClassStatic();
    ged::GedLabelNode::GetClassStatic();
    ged::GedMapNode::GetClassStatic();
    ged::GedArrayNode::GetClassStatic();
    ged::GedFactoryNode::GetClassStatic();
    ged::GedObjNode::GetClassStatic();
    ged::GedBoolNode::GetClassStatic();
    ged::GedIntNode::GetClassStatic();
    ged::GedFloatNode::GetClassStatic();
    ged::GedCurve1DNode::GetClassStatic();
    ged::GedGradientNode::GetClassStatic();
    ged::GedAssetNode::GetClassStatic();
    ged::GedPlugNode::GetClassStatic();
    ged::GedEnumNode::GetClassStatic();
    ged::GedColorNode::GetClassStatic();

    ged::GedNodeFactory::GetClassStatic();
    ged::GedNodeFactoryCurve1D::GetClassStatic();
    ged::GedNodeFactoryGradient::GetClassStatic();
    ged::GedNodeFactoryAssetList::GetClassStatic();
    ged::GedNodeFactoryPlug::GetClassStatic();
    ged::GedNodeFactoryPlugFloatXF::GetClassStatic();
    ged::GedNodeFactoryColorV4::GetClassStatic();

    ged::TestObject::GetClassStatic();
    ged::TestObjectConfiguration::GetClassStatic();

    //////////////////////////////////////////

    LightData::GetClassStatic();
    PointLightData::GetClassStatic();
    DirectionalLightData::GetClassStatic();
    AmbientLightData::GetClassStatic();
    SpotLightData::GetClassStatic();

    pbr::SkyAtmosphereData::GetClassStatic();
    // a scene hands the medium over as a varmap value (SceneGraphSystemData's
    // "SkyAtmosphere" userparam) - without this the .ecs writes it as "null:".
    reflect::serdes::registerVarObjectCodec<pbr::skyatmospheredata_ptr_t>();

    scenegraph::DrawableDataKvPair::GetClassStatic();
    DrawableData::GetClassStatic();
    ModelDrawableData::GetClassStatic();
    InstancedModelDrawableData::GetClassStatic();
    BillboardStringDrawableData::GetClassStatic();
    InstancedBillboardStringDrawableData::GetClassStatic();
    OverlayStringDrawableData::GetClassStatic();

    BillboardDrawableData::GetClassStatic();
    GridDrawableData::GetClassStatic();
    GroundPlaneDrawableData::GetClassStatic();
    ProjectedGridDrawableData::GetClassStatic();
    ImposterDrawableData::GetClassStatic();

    XgmAnimChannel::GetClassStatic();
    XgmFloatAnimChannel::GetClassStatic();
    XgmVect3AnimChannel::GetClassStatic();
    XgmVect4AnimChannel::GetClassStatic();
    XgmDecompMatrixAnimChannel::GetClassStatic();

    particle::ParticleModuleData::GetClassStatic();
    particle::ParticlePoolData::GetClassStatic();
    particle::GlobalModuleData::GetClassStatic();
    particle::EntityRefModuleData::GetClassStatic();
    particle::TransformPointModuleData::GetClassStatic();
    particle::TransformDirModuleData::GetClassStatic();
    particle::Vec3AddModuleData::GetClassStatic();
    particle::Vec3CombineModuleData::GetClassStatic();
    particle::ParametersModuleData::GetClassStatic();

    terrain::TerrainModuleData::GetClassStatic();
    terrain::FbmModuleData::GetClassStatic();
    terrain::NoiseModuleData::GetClassStatic();
    terrain::ExprModuleData::GetClassStatic();
    terrain::NormalizeModuleData::GetClassStatic();
    terrain::RemapModuleData::GetClassStatic();
    terrain::ConstModuleData::GetClassStatic();
    terrain::GradientModuleData::GetClassStatic();
    terrain::CombineModuleData::GetClassStatic();
    terrain::TerraceModuleData::GetClassStatic();
    terrain::SlopeModuleData::GetClassStatic();
    terrain::CurvatureModuleData::GetClassStatic();
    terrain::RelaxUvModuleData::GetClassStatic();
    terrain::MaskBlendModuleData::GetClassStatic();
    terrain::ScatterPlaceModuleData::GetClassStatic(); // in-graph scatter placement + building pads (.ogeo export)
    terrain::ThermalErodeModuleData::GetClassStatic();
    terrain::EroxModuleData::GetClassStatic();
    terrain::PhaModuleData::GetClassStatic();
    terrain::LpfModuleData::GetClassStatic();
    terrain::BasinFillModuleData::GetClassStatic();
    terrain::Flow3DModuleData::GetClassStatic();
    terrain::FlowErodeModuleData::GetClassStatic();
    terrain::FillClosedBasinsModuleData::GetClassStatic();
    terrain::CaptureModuleData::GetClassStatic();
    // composite (subgraph / loop) terrain runtime subclasses — the core dflow schema
    // (SubGraphModuleData/LoopModuleData + bindings) is touched in reflection_init.cpp.
    terrain::TerrainSubGraphModuleData::GetClassStatic();
    terrain::TerrainLoopModuleData::GetClassStatic();
    // custom image-plug classes MUST be touched too, or JsonDeserializer can't
    // resolve "terrain::hfimg{out,inp}plug" on load (mirrors particlebuf plugs).
    terrain::hfimg_outplugdata_t::GetClassStatic();
    terrain::hfimg_inplugdata_t::GetClassStatic();

    // hypermesh (GPU mesh compute-dataflow) modules + the mesh plug type.
    hypermesh::MeshModuleData::GetClassStatic();
    hypermesh::RipplePrimitiveData::GetClassStatic();
    hypermesh::BoxData::GetClassStatic();
    hypermesh::SortTestData::GetClassStatic();
    hypermesh::EdgeTestData::GetClassStatic();
    hypermesh::VertTestData::GetClassStatic();
    hypermesh::BevelData::GetClassStatic();
    hypermesh::UvSphereData::GetClassStatic();
    hypermesh::IcoSphereData::GetClassStatic();
    hypermesh::ConeData::GetClassStatic();
    hypermesh::SubdivideModuleData::GetClassStatic();
    hypermesh::SelectData::GetClassStatic();
    hypermesh::ExtrudeFacesData::GetClassStatic();
    hypermesh::InsetData::GetClassStatic();
    hypermesh::NormalsData::GetClassStatic();
    hypermesh::TransformData::GetClassStatic();
    hypermesh::DisplaceByFieldData::GetClassStatic(); // E.1 cross-family field displace
    hypermesh::DisplaceBySdfData::GetClassStatic();   // M4b cross-family SDF displace (conform/offset/scalar)
    hypermesh::TemporalSmoothData::GetClassStatic();  // inter-frame EMA (alpha/tau) jitter damp
    hypermesh::DeleteFacesData::GetClassStatic();
    hypermesh::MirrorData::GetClassStatic();
    hypermesh::CompactData::GetClassStatic();
    hypermesh::BitOpData::GetClassStatic();
    hypermesh::GidAssignData::GetClassStatic();
    hypermesh::SectionUnwrapData::GetClassStatic();     // O3: per-section xatlas unwrap -> texture-array layers
    hypermesh::MaterialParamSinkData::GetClassStatic(); // E.6/2.12: material UBO param by name
    hypermesh::ScatterSourceData::GetClassStatic(); // E.2: the typed instance edge source
    hypermesh::LSystemModuleData::GetClassStatic(); // M1: L-system producer of the XfNodeGraph spine
    hypermesh::LSweepModuleData::GetClassStatic();  // M1/G0b: XfNodeGraph -> GpuMesh skinner (swept tube)
    hypermesh::LeafScatterModuleData::GetClassStatic(); // organ: phyllotactic leaf-card scatter on the skeleton (touch -> reflect props -> cook-hash param sensitivity)
    hypermesh::MergeMeshData::GetClassStatic();         // concat two meshes + per-source gid (bake leaves into trunk)
    hypermesh::GpuComputeModuleData::GetClassStatic(); // generic per-vertex GPU compute (shader-text deformer, no new C++)
    hypermesh::RouteSpineModuleData::GetClassStatic();    // R-family v1: least-cost spine forest (XfNodeGraph) from terrain fields
    hypermesh::RoadbedMaskModuleData::GetClassStatic();   // R-family v1: roadbed_mask + road_elev_m + road UV field (HfImage)
    hypermesh::KeepoutMaskModuleData::GetClassStatic();   // R-family v1: keepout mask (dilated roadbed) -> scatter sinks inverted
    hypermesh::ParcelizeModuleData::GetClassStatic();     // R-family v1: frontage parcels along the spine (InstanceSet)
    hypermesh::BuildingSeedsModuleData::GetClassStatic(); // R-family v1: building seeds (scatter-sink InstanceSet + freeform SoA)
    hypermesh::RoadMeshModuleData::GetClassStatic();      // R-family v2: swept road-ribbon + junction patches + gid split (XfNodeGraph -> GpuMesh)
    // GR1.a — the LRuleSet grammar-as-data schema (LSystemModuleData._grammar) is touched by
    // ork::CoreAppInit, which lev2::initModule runs first: the schema is family-neutral ork.core.
    // The mesh family's OP VOCABULARY is not reflection — it is the alphabet ork.core resolves
    // grammar op codes through, and it must exist before any mesh grammar is loaded or derived.
    hypermesh::registerMeshVocabulary();
    hypermesh::mesh_outplugdata_t::GetClassStatic();
    hypermesh::mesh_inplugdata_t::GetClassStatic();
    dflowgfx::instset_outplugdata_t::GetClassStatic(); // E.2: InstanceSet interchange plugs
    dflowgfx::instset_inplugdata_t::GetClassStatic();
    dflowgfx::xfng_outplugdata_t::GetClassStatic(); // M1: XfNodeGraph (ork::hyper) interchange plugs
    dflowgfx::xfng_inplugdata_t::GetClassStatic();
    // E.7 — the sdfgrid family + its interchange plugs
    dflowgfx::sdfgrid_outplugdata_t::GetClassStatic();
    dflowgfx::sdfgrid_inplugdata_t::GetClassStatic();
    sdf::SdfModuleData::GetClassStatic();
    sdf::SdfEvalData::GetClassStatic();
    sdf::MeshToSdfData::GetClassStatic(); // M1: GPU voxelize
    sdf::CsgData::GetClassStatic();       // M2: boolean composite
    sdf::SdfToMeshData::GetClassStatic();   // M2: marching tetrahedra
    sdf::SdfToMeshCleanData::GetClassStatic(); // M2: shape-aware clean remesh (openvdb + xatlas UV)
    sdf::RedistanceData::GetClassStatic();  // M4a: JFA eikonal redistance
    // D.3/D.4 HYPERECS host data — these serialize inside scenes, so they MUST be touched
    // (a stripped registration = `"class": ""` in the JSON + FindClass assert at load).
    hypermesh::HypermeshDrawableData::GetClassStatic();
    HypermeshGenData::GetClassStatic();
    ScatterSinkData::GetClassStatic();
    terrain::TerrainChunkDrawableData::GetClassStatic();

    particle::RingEmitterData::GetClassStatic();
    particle::EllipticalEmitterData::GetClassStatic();
    particle::LineEmitterData::GetClassStatic();
    particle::NozzleEmitterData::GetClassStatic();

    particle::GravityModuleData::GetClassStatic();
    particle::DirectionalForceModuleData::GetClassStatic();
    particle::ExprForceModuleData::GetClassStatic();   // E2.5 S8: ExprIR-driven force
    particle::SphAttractorModuleData::GetClassStatic();
    particle::EllipticalAttractorModuleData::GetClassStatic();
    particle::PointAttractorModuleData::GetClassStatic();

    particle::TurbulenceModuleData::GetClassStatic();
    particle::CurlNoiseForceModuleData::GetClassStatic();
    particle::PolyDragModuleData::GetClassStatic();
    particle::VdbLevelSetRendererData::GetClassStatic();
    particle::VortexModuleData::GetClassStatic();
    particle::DragModuleData::GetClassStatic();
    particle::PlaneColliderModuleData::GetClassStatic();
    particle::SphereColliderModuleData::GetClassStatic();
    particle::VdbColliderModuleData::GetClassStatic();

    particle::RendererModuleData::GetClassStatic();
    particle::SpriteRendererData::GetClassStatic();
    particle::StreakRendererData::GetClassStatic();
    particle::LightRendererData::GetClassStatic();

    particle::particlebuf_outplugdata_t::GetClassStatic();
    particle::particlebuf_inplugdata_t::GetClassStatic();

    particle::MaterialBase::GetClassStatic();
    particle::FlatMaterial::GetClassStatic();
    particle::GradientMaterial::GetClassStatic();
    particle::GradientAtlasMaterial::GetClassStatic();
    particle::TextureMaterial::GetClassStatic();
    particle::TexGridMaterial::GetClassStatic();
    particle::FreestyleParticleMaterial::GetClassStatic();
    particle::VolTexMaterial::GetClassStatic();

    /*

    proctex::ProcTex::GetClassStatic();
    proctex::ImgModule::GetClassStatic();
    proctex::Img32Module::GetClassStatic();
    proctex::Img64Module::GetClassStatic();
    proctex::Module::GetClassStatic();

    proctex::Periodic::GetClassStatic();
    proctex::RotSolid::GetClassStatic();
    proctex::Colorize::GetClassStatic();
    proctex::SolidColor::GetClassStatic();
    proctex::ImgOp2::GetClassStatic();
    proctex::ImgOp3::GetClassStatic();
    proctex::Transform::GetClassStatic();
    proctex::Texture::GetClassStatic();
    proctex::Gradient::GetClassStatic();
    proctex::Curve1D::GetClassStatic();
    proctex::Global::GetClassStatic();
    proctex::Group::GetClassStatic();

    proctex::Cells::GetClassStatic();
    proctex::Octaves::GetClassStatic();

    proctex::SphMap::GetClassStatic();
    proctex::SphRefract::GetClassStatic();
    proctex::H2N::GetClassStatic();
    proctex::UvMap::GetClassStatic();
    proctex::Kaled::GetClassStatic();
    */

    RegisterClassX(PointLightData);
    RegisterClassX(SpotLightData);

    RegisterClassX(OutputCompositingNode);
    RegisterClassX(VrOutputNode);
    RegisterClassX(DualMonoVrOutputNode);
    RegisterClassX(SinglePassStereoVrOutputNode);
    RegisterClassX(ScreenOutputCompositingNode);

    RegisterClassX(RenderCompositingNode);
    RegisterClassX(compositor::UnlitNode);

#if defined(ENABLE_NVMESH_SHADERS)
    RegisterClassX(pbr::deferrednode::DeferredCompositingNodeNvMs);
#endif

    RegisterClassX(CompositingScene);
    RegisterClassX(CompositingData);
    RegisterClassX(CompositingSceneItem);

    RegisterClassX(PostCompositingNode);
    RegisterClassX(ScaleBiasCompositingNode);
    // RegisterClassX(PtxCompositingNode);
    RegisterClassX(Op2CompositingNode);
    RegisterClassX(NodeCompositingTechnique);
    RegisterClassX(PBRMaterial);
    // RegisterClassX(TerrainDrawableData);
    RegisterClassX(TextureAsset);
    RegisterClassX(FxShaderAsset);
    RegisterClassX(XgmAnimAsset);
    RegisterClassX(XgmModelAsset);
    RegisterClassX(RadianceMapsAsset);

    //////////////////////////////////////////
    // register audio classes
    //////////////////////////////////////////

    RegisterClassX(audio::singularity::ProgramData);
    RegisterClassX(audio::singularity::BankData);

    RegisterClassX(audio::singularity::LayerData);
    RegisterClassX(audio::singularity::AlgData);
    RegisterClassX(audio::singularity::DspStageData);

    RegisterClassX(audio::singularity::KmRegionData);
    RegisterClassX(audio::singularity::KeyMapData);
    RegisterClassX(audio::singularity::SampleData);
    RegisterClassX(audio::singularity::MultiSampleData);

    RegisterClassX(audio::singularity::BlockModulationData);
    RegisterClassX(audio::singularity::DspParamData);

    RegisterClassX(audio::singularity::ControllerData);
    RegisterClassX(audio::singularity::AsrData);
    RegisterClassX(audio::singularity::RateLevelEnvData);
    RegisterClassX(audio::singularity::YmEnvData);
    RegisterClassX(audio::singularity::LfoData);
    RegisterClassX(audio::singularity::GradientData);
    RegisterClassX(audio::singularity::FunData);
    RegisterClassX(audio::singularity::ConstantControllerData);
    RegisterClassX(audio::singularity::CustomControllerData);

    RegisterClassX(audio::singularity::IoConfig);
    RegisterClassX(audio::singularity::DspBlockData);
    RegisterClassX(audio::singularity::PMXData);
    RegisterClassX(audio::singularity::PMXMixData);
    RegisterClassX(audio::singularity::MonoInStereoOutData);
    RegisterClassX(audio::singularity::SAMPLER_DATA);
    RegisterClassX(audio::singularity::STREAMING_OSCILLATOR_DATA);
    RegisterClassX(audio::singularity::HwInputData);

    RegisterClassX(audio::singularity::AMP_ADAPTIVE_DATA);
    RegisterClassX(audio::singularity::AMP_MONOIO_DATA);
    RegisterClassX(audio::singularity::PLUSAMP_DATA);
    RegisterClassX(audio::singularity::XAMP_DATA);
    RegisterClassX(audio::singularity::STEREO_GAIN_DATA);
    RegisterClassX(audio::singularity::GAIN_DATA);
    RegisterClassX(audio::singularity::BANGAMP_DATA);
    RegisterClassX(audio::singularity::AMPU_AMPL_DATA);
    RegisterClassX(audio::singularity::BAL_AMP_DATA);
    RegisterClassX(audio::singularity::AMP_MOD_OSC_DATA);
    RegisterClassX(audio::singularity::XGAIN_DATA);
    RegisterClassX(audio::singularity::XFADE_DATA);
    RegisterClassX(audio::singularity::PANNER_DATA);
    RegisterClassX(audio::singularity::PANNER2D_DATA);
    RegisterClassX(audio::singularity::PANNER2DU_DATA);
    RegisterClassX(audio::singularity::RingModData);
    RegisterClassX(audio::singularity::NOISEGATE_DATA);

    RegisterClassX(audio::singularity::STEEP_RESONANT_BASS_DATA);
    RegisterClassX(audio::singularity::PARABASS_DATA);
    RegisterClassX(audio::singularity::PARAMID_DATA);
    RegisterClassX(audio::singularity::PARATREBLE_DATA);
    RegisterClassX(audio::singularity::ParametricEqData);

    RegisterClassX(audio::singularity::BANDPASS_FILT_DATA);
    RegisterClassX(audio::singularity::BAND2_DATA);
    RegisterClassX(audio::singularity::NOTCH_FILT_DATA);
    RegisterClassX(audio::singularity::NOTCH2_DATA);
    RegisterClassX(audio::singularity::DOUBLE_NOTCH_W_SEP_DATA);
    RegisterClassX(audio::singularity::LOPAS2_DATA);
    RegisterClassX(audio::singularity::LP2RES_DATA);
    RegisterClassX(audio::singularity::LPGATE_DATA);
    RegisterClassX(audio::singularity::FOURPOLE_HIPASS_W_SEP_DATA);
    RegisterClassX(audio::singularity::LPCLIP_DATA);
    RegisterClassX(audio::singularity::LowPassData);
    RegisterClassX(audio::singularity::HighPassData);
    RegisterClassX(audio::singularity::AllPassData);
    RegisterClassX(audio::singularity::HighFreqStimulatorData);
    RegisterClassX(audio::singularity::TwoPoleLowPassData);
    RegisterClassX(audio::singularity::TwoPoleAllPassData);
    RegisterClassX(audio::singularity::FourPoleLowPassWithSepData);

    RegisterClassX(audio::singularity::SHAPER_DATA);
    RegisterClassX(audio::singularity::SHAPE2_DATA);
    RegisterClassX(audio::singularity::TWOPARAM_SHAPER_DATA);
    RegisterClassX(audio::singularity::WrapData);
    RegisterClassX(audio::singularity::DistortionData);

    RegisterClassX(audio::singularity::PITCH_DATA);
    RegisterClassX(audio::singularity::SWPLUSSHP_DATA);
    RegisterClassX(audio::singularity::SAWPLUS_DATA);
    RegisterClassX(audio::singularity::SINE_DATA);
    RegisterClassX(audio::singularity::SAW_DATA);
    RegisterClassX(audio::singularity::SINEPLUS_DATA);
    RegisterClassX(audio::singularity::SHAPEMODOSC_DATA);
    RegisterClassX(audio::singularity::PLUSSHAPEMODOSC_DATA);
    RegisterClassX(audio::singularity::SYNCM_DATA);
    RegisterClassX(audio::singularity::SYNCS_DATA);
    RegisterClassX(audio::singularity::PWM_DATA);
    RegisterClassX(audio::singularity::NOISE_DATA);

    RegisterClassX(audio::singularity::PitchShifterData);
    RegisterClassX(audio::singularity::RecursivePitchShifterData);

    RegisterClassX(audio::singularity::ToFrequencyDomainData);
    RegisterClassX(audio::singularity::ToTimeDomainData);
    RegisterClassX(audio::singularity::SpectralShiftData);
    RegisterClassX(audio::singularity::SpectralScaleData);
    RegisterClassX(audio::singularity::SpectralConvolveData);
    RegisterClassX(audio::singularity::SpectralConvolveTDData);
    RegisterClassX(audio::singularity::SpectralTestData);

    RegisterClassX(audio::singularity::Sum2Data);
    RegisterClassX(audio::singularity::StereoEnhancerData);
    RegisterClassX(audio::singularity::StereoDynamicEchoData);
    RegisterClassX(audio::singularity::TestReverbData);
    RegisterClassX(audio::singularity::StereoDelayData);
    RegisterClassX(audio::singularity::Fdn8ReverbData);
    RegisterClassX(audio::singularity::Fdn4ReverbXData);
    RegisterClassX(audio::singularity::Fdn4ReverbData);

    RegisterClassX(audio::singularity::SoundFieldSendData);
    RegisterClassX(audio::singularity::SpatializerData);
    RegisterClassX(audio::singularity::PannerSpatializerData);

    // HYPERECS M2b/M3 asset-gen reflected data classes.
    RegisterClassX(AssetGenData);
    RegisterClassX(ImplicitSdfGenData);
    RegisterClassX(PbrMaterialGenData);
    RegisterClassX(FreestyleMaterialGenData);
    RegisterClassX(VdbGridToDrawableGenData);
    RegisterClassX(ParticleSystemGenData);
    RegisterClassX(HeightFieldGenData);
    RegisterClassX(HdriToXirGenData);
    RegisterClassX(VdbFileSdfGenData);
    RegisterClassX(MeshSdfGenData);
    RegisterClassX(MeshGenData);
    // RigidPrimitiveDrawableData: header-only struct; its
    // reflection definition lives in rigid_primitive_drawdata.cpp.
    // Touch the class here so the registry knows about it.
    RegisterClassX(meshutil::RigidPrimitiveDrawableData);
    // ParticlesDrawableData: the polymorphic value held by
    // ParticlesComponentData's reflected DrawableData slot. Without an
    // explicit registry entry, the serializer can't resolve the
    // concrete-class name (writes class="" then deserialize asserts).
    RegisterClassX(ParticlesDrawableData);
    // PBR2 P3.D — Separable Subsurface Scattering post-fx node.
    // Class registration is needed for the polymorphic reflection on
    // SceneGraphSystemData::_postfx_nodes (directObjectMapProperty)
    // to resolve "PostFxNodeSSSS" by class name at deserialize time.
    RegisterClassX(PostFxNodeSSSS);
    // E2B item D — heat-distortion post-fx node (same polymorphic-map
    // deserialize requirement as SSSS above).
    RegisterClassX(PostFxNodeHeatDistort);
    // HSVG grade post-fx node — same polymorphic-map deserialize requirement;
    // without this touch the .ecs "class":"PostFxNodeHSVG" fails objclazz lookup.
    RegisterClassX(PostFxNodeHSVG);
    // ACES tonemap post-fx node — the player's --devkeys injects it into the SG
    // _postfx_nodes map, so the Cmd+R round-trip re-deserializes "PostFxNodeACES";
    // without this touch that FindClass fails objclazz (JsonDeserializer assert).
    RegisterClassX(PostFxNodeACES);

    //////////////////////////////////////////
  }

  ~ClassToucher() {
    logger()->defaultChannel()->log( "\nork.lev2 classes unregistered...");
  }

};

using classinit_ptr_t = std::shared_ptr<ClassToucher>;

// OPENXR X2 minimal instantiation. When ORKID_VR_DRIVER=openxr (and ENABLE_OPENXR),
// select the OpenXR device (a process singleton) as the active VR device. Idempotent
// — safe to call from multiple init sites. Returns true when OpenXR was selected.
// Must run BEFORE the backend creates its Vulkan instance so OpenXrDevice::
// preGraphicsInit participates in the X1 seam; the loader context is created inside
// the ClassToucher (non-deferred) OR in bindGfxToCurrentThread (deferred/subsystem),
// so this is invoked at both the ClassToucher graphics-init site (before that
// createLoaderContext) and GfxInit (which covers the deferred path).
bool selectVrDeviceFromEnv() {
#if defined(ENABLE_OPENXR)
  std::string drv;
  if (genviron.get("ORKID_VR_DRIVER", drv) and drv == "openxr") {
    auto xrdev = ork::lev2::orkidvr::openxr_::openxr_device();
    ork::lev2::orkidvr::setDevice(xrdev);
    return true;
  }
#endif
  return false;
}

void GfxInit(const std::string& gfxlayer) {
#if defined(ENABLE_OPENXR)
  // Pose/FOV math self-test (no runtime, no scene): prints verdict lines a repo
  // test greps. Runs regardless of the selected driver.
  {
    std::string st;
    if (genviron.get("ORKID_OPENXR_SELFTEST", st) and st == "1")
      ork::lev2::orkidvr::openxr_::runSelfTests();
  }
#endif
  // If its pre-graphics phase already failed (no runtime), the OpenXR device stays
  // inactive and downstream behaves as NoVR-equivalent (the SceneGraphSystem VR-
  // preset path re-defaults to NoVR; X4 owns making the success path stick). Full
  // driver selection/pybind is a later slice.
  if (selectVrDeviceFromEnv())
    return;
  auto def_vrdev = std::make_shared<ork::lev2::orkidvr::novr::NoVrDevice>();
  ork::lev2::orkidvr::setDevice(def_vrdev);
}
struct Lev2AppInit {

  Lev2AppInit(ork::appinitdata_ptr_t init_data) {
    ///////////////////////////////////////////////////////////////
    _ginitdata = init_data;
    _class_toucher = std::make_shared<ClassToucher>(_ginitdata); //
    meshutil::misc_init();    
    registerEnums();
    ///////////////////////////////////////////////////////////////
    //logger()->defaultChannel()->log("initialize OpenVDB....");
    /*
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    */
    openvdb::initialize();
    openvdb::ax::initialize();
    ///////////////////////////////////////////////////////////////
    if(init_data->_enable_graphics){
      init_data->enqueuePostInitOp(
          AppInitOrder::GRAPHICS_INIT,
          [] { //
            GfxInit("");
            lev2::FontMan::GetRef();
          });
    }
  }
  classinit_ptr_t _class_toucher;
};


using lev2appinit_ptr_t = std::shared_ptr<Lev2AppInit>;
static lev2appinit_ptr_t g_lev2_initializer = nullptr;
static mutex ginit_mutex("lev2init");

void initModule(appinitdata_ptr_t init_data) {
  ::ork::initModule(init_data);
  // Main-thread CHANNEL_MAIN sample sites that run before the first Context::beginFrame
  // (CtxGLFW::SlotRepaint's viewport.draw, fired from window creation) would otherwise hit
  // acquireSeries with no channel. This is the main thread for every app entry path.
  OrkProfilerChannelRegister(CHANNEL_MAIN, CpuProfilerChannel);
  ginit_mutex.Lock();
  if(g_lev2_initializer){
    ginit_mutex.UnLock();
    return;
  }
  g_lev2_initializer = std::make_shared<Lev2AppInit>(init_data);
  ginit_mutex.UnLock();
}

void exitModule(appinitdata_ptr_t init_data){
  ::ork::exitModule(init_data);
  ginit_mutex.Lock();
  g_lev2_initializer = nullptr;
  ginit_mutex.UnLock();
}

///////////////////////////////////////////////////////////////////////////////
// ensureLoaderContext - creates loader context if not already created
// Used for deferred GPU init in subsystem mode
///////////////////////////////////////////////////////////////////////////////

context_ptr_t ensureLoaderContext() {
  if (gloadercontext) {
    return gloadercontext;
  }

  if (!_ginitdata || !_ginitdata->_enable_graphics) {
    return nullptr;
  }

  switch(GRAPHICS_API){
    case "DUMMY"_crcu:{
      gloadercontext = dummy::createLoaderContext();
      OrkAssert(false);
      break;
    }
    case "VULKAN"_crcu:
    default: {
      gloadercontext = vulkan::createLoaderContext();
      // Auto-spawn loader thread on the deferred-init path too (subsystem
      // mode). Mirror of the initModule path.
      g_loader_thread.start();
      break;
    }
  }

  return gloadercontext;
}

} // namespace lev2
} // namespace ork
