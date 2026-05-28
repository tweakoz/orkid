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
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/scenegraph/sgnode_projectedgrid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_imposter.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/vr/vr.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
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

    particle::RingEmitterData::GetClassStatic();
    particle::EllipticalEmitterData::GetClassStatic();
    particle::LineEmitterData::GetClassStatic();
    particle::NozzleEmitterData::GetClassStatic();

    particle::GravityModuleData::GetClassStatic();
    particle::DirectionalForceModuleData::GetClassStatic();
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

    RegisterClassX(audio::singularity::SpatializerData);
    RegisterClassX(audio::singularity::PannerSpatializerData);

    // HYPERECS M2b/M3 asset-gen reflected data classes.
    RegisterClassX(AssetGenData);
    RegisterClassX(ImplicitSdfGenData);
    RegisterClassX(PbrMaterialGenData);
    RegisterClassX(FreestyleMaterialGenData);
    RegisterClassX(VdbGridToDrawableGenData);
    RegisterClassX(ParticleSystemGenData);
    RegisterClassX(HdriToXirGenData);
    RegisterClassX(VdbFileSdfGenData);
    RegisterClassX(MeshSdfGenData);
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

    //////////////////////////////////////////
  }

  ~ClassToucher() {
    logger()->defaultChannel()->log( "\nork.lev2 classes unregistered...");
  }

};

using classinit_ptr_t = std::shared_ptr<ClassToucher>;

void GfxInit(const std::string& gfxlayer) {
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
    logger()->defaultChannel()->log("initialize OpenVDB....");
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
