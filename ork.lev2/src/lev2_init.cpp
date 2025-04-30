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
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPtx.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/vr/vr.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
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
#include <ork/math/plane.hpp>

#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_test_objects.h>
#include <ork/lev2/ui/ged/ged_factory.h>

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

namespace ork::lev2::editor::imgui {
void initModule(appinitdata_ptr_t initdata) {
  initdata->_imgui = true;
}
void exitModule(appinitdata_ptr_t initdata) {
}
} // namespace ork::imgui

namespace ork {
void initModule(appinitdata_ptr_t init_data);
void exitModule(appinitdata_ptr_t init_data);
namespace lev2 {

appinitdata_ptr_t _ginitdata;

uint64_t GRAPHICS_API = "OPENGL"_crcu;
//uint64_t GRAPHICS_API = "VULKAN"_crcu;

namespace vulkan{
  lev2::context_ptr_t createLoaderContext();
  void touchClasses();
};
namespace dummy{
  lev2::context_ptr_t createLoaderContext();
  void touchClasses();
}
namespace opengl{
  lev2::context_ptr_t createLoaderContext();
  void touchClasses();
}

void registerEnums();

ork::lev2::context_ptr_t gloadercontext;

struct ClassToucher {
  ClassToucher(appinitdata_ptr_t aid) {

    printf( "ork.lev2 classes registered...\n");

    AllocationLabel label("ork::lev2::Init");

    Context::GetClassStatic();
    if(aid->_enable_graphics){
      vulkan::touchClasses();
      dummy::touchClasses();
      opengl::touchClasses();

      ////////////////////////////////////////

      std::string gfx_api_str;
      if( genviron.get("ORKID_GRAPHICS_API",gfx_api_str) ){
        if(gfx_api_str=="VULKAN"){
          GRAPHICS_API  = "VULKAN"_crcu;
        }     
        else if(gfx_api_str=="OPENGL"){
          GRAPHICS_API  = "OPENGL"_crcu;
        }     
        else if(gfx_api_str=="DUMMY"){
          GRAPHICS_API  = "DUMMY"_crcu;
        }     
      }

      ////////////////////////////////////////

      switch(GRAPHICS_API){
        case "DUMMY"_crcu:{
          gloadercontext = dummy::createLoaderContext();
          //GfxEnv::setContextClass(clazz);
          OrkAssert(false);
          break;
        }
        case "OPENGL"_crcu:{
          gloadercontext = opengl::createLoaderContext();
          break;
        }
        case "VULKAN"_crcu:
        default: {
          gloadercontext = vulkan::createLoaderContext();
          break;
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

    GridDrawableData::GetClassStatic();
    GroundPlaneDrawableData::GetClassStatic();

    XgmAnimChannel::GetClassStatic();
    XgmFloatAnimChannel::GetClassStatic();
    XgmVect3AnimChannel::GetClassStatic();
    XgmVect4AnimChannel::GetClassStatic();
    XgmDecompMatrixAnimChannel::GetClassStatic();

    particle::ParticleModuleData::GetClassStatic();
    particle::ParticlePoolData::GetClassStatic();
    particle::GlobalModuleData::GetClassStatic();

    particle::RingEmitterData::GetClassStatic();
    particle::EllipticalEmitterData::GetClassStatic();
    particle::LineEmitterData::GetClassStatic();
    particle::NozzleEmitterData::GetClassStatic();

    particle::GravityModuleData::GetClassStatic();
    particle::SphAttractorModuleData::GetClassStatic();
    particle::EllipticalAttractorModuleData::GetClassStatic();
    particle::PointAttractorModuleData::GetClassStatic();

    particle::TurbulenceModuleData::GetClassStatic();
    particle::VortexModuleData::GetClassStatic();
    particle::DragModuleData::GetClassStatic();

    particle::RendererModuleData::GetClassStatic();
    particle::SpriteRendererData::GetClassStatic();
    particle::StreakRendererData::GetClassStatic();
    particle::LightRendererData::GetClassStatic();

    particle::particlebuf_outplugdata_t::GetClassStatic();
    particle::particlebuf_inplugdata_t::GetClassStatic();

    particle::MaterialBase::GetClassStatic();
    particle::FlatMaterial::GetClassStatic();
    particle::GradientMaterial::GetClassStatic();
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

    RegisterClassX(OutputCompositingNode);
    RegisterClassX(VrOutputNode);
    RegisterClassX(DualMonoVrOutputNode);
    RegisterClassX(ScreenOutputCompositingNode);

    RegisterClassX(RenderCompositingNode);
    RegisterClassX(compositor::UnlitNode);
    RegisterClassX(pbr::deferrednode::DeferredCompositingNode);
    RegisterClassX(pbr::deferrednode::DeferredCompositingNodePbr);

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

    //////////////////////////////////////////
  }

  ~ClassToucher() {
    printf( "ork.lev2 classes unregistered...\n");
  }

};

using classinit_ptr_t = std::shared_ptr<ClassToucher>;

void GfxInit(const std::string& gfxlayer) {
  opq::init();

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
    printf("initialize OpenVDB....\n");
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


} // namespace lev2
} // namespace ork
