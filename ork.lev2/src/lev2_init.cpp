////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
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
#include <ork/math/plane.hpp>

#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_test_objects.h>
#include <ork/lev2/ui/ged/ged_factory.h>

///////////////////////////////////////////////////////////////////////////////
//#define WIIEMU
///////////////////////////////////////////////////////////////////////////////

namespace ork {

namespace lev2 {

appinitdata_ptr_t _ginitdata;

#if !defined(ORK_OSX)
namespace vk {
void init();
}
#endif

#if defined(_WIN32)
static bool gbPREFEROPENGL = false;
#else
static bool gbPREFEROPENGL = true;
#endif

context_ptr_t OpenGlContextInit();
void DummyContextInit();

void PreferOpenGL() {
  static auto ctx = ork::lev2::OpenGlContextInit();
  gbPREFEROPENGL  = true;
}

void registerEnums();

struct ClassToucher {
  ClassToucher() {
    AllocationLabel label("ork::lev2::Init");

    Context::GetClassStatic();

    PreferOpenGL();

    GfxEnv::GetRef();
    GfxPrimitives::GetRef();

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
    particle::NozzleEmitterData::GetClassStatic();
    particle::GravityModuleData::GetClassStatic();
    particle::TurbulenceModuleData::GetClassStatic();
    particle::VortexModuleData::GetClassStatic();
    particle::RendererModuleData::GetClassStatic();
    particle::SpriteRendererData::GetClassStatic();
    particle::StreakRendererData::GetClassStatic();

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
    RegisterClassX(VrCompositingNode);
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

    RegisterClassX(audio::singularity::BlockModulationData);
    RegisterClassX(audio::singularity::DspParamData);

    RegisterClassX(audio::singularity::ControllerData);
    RegisterClassX(audio::singularity::AsrData);
    RegisterClassX(audio::singularity::RateLevelEnvData);
    RegisterClassX(audio::singularity::YmEnvData);
    RegisterClassX(audio::singularity::LfoData);
    RegisterClassX(audio::singularity::FunData);
    RegisterClassX(audio::singularity::ConstantControllerData);
    RegisterClassX(audio::singularity::CustomControllerData);

    RegisterClassX(audio::singularity::IoConfig);
    RegisterClassX(audio::singularity::DspBlockData);
    RegisterClassX(audio::singularity::PMXData);
    RegisterClassX(audio::singularity::PMXMixData);
    RegisterClassX(audio::singularity::MonoInStereoOutData);

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
    RegisterClassX(audio::singularity::RingModData);

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

    RegisterClassX(audio::singularity::Sum2Data);
    RegisterClassX(audio::singularity::StereoEnhancerData);
    RegisterClassX(audio::singularity::StereoDynamicEchoData);
    RegisterClassX(audio::singularity::TestReverbData);
    RegisterClassX(audio::singularity::Fdn8ReverbData);
    RegisterClassX(audio::singularity::Fdn4ReverbXData);
    RegisterClassX(audio::singularity::Fdn4ReverbData);

    //////////////////////////////////////////
    // register lev2 graphics target classes
    //////////////////////////////////////////

    DummyContextInit();

    //////////////////////////////////////////
  }
};

void ClassInit() {
  static ClassToucher toucher;
}

ork::lev2::context_ptr_t gloadercontext;

void GfxInit(const std::string& gfxlayer) {

#if defined(ENABLE_VULKAN)
  // vk::init();
#endif

  if (gfxlayer != "dummy") {
#if defined(ORK_CONFIG_OPENGL)
    gloadercontext = OpenGlContextInit();
    // FontMan::gpuInit(gloadercontext.get());
#endif
  }
  opq::init();

  auto def_vrdev = std::make_shared<ork::lev2::orkidvr::novr::NoVrDevice>();
  ork::lev2::orkidvr::setDevice(def_vrdev);
}
struct ModuleInit {

  ModuleInit(ork::appinitdata_ptr_t init_data) {
    _ginitdata = init_data;

    auto it = init_data->_miscvars.find("lev2_init");

    if (it == init_data->_miscvars.end()) {
      init_data->enqueuePreInitOp([] { ClassInit(); });
      init_data->_miscvars["lev2_init"] = nullptr;
    }

    ///////////////////////////////////////////////////////////////

    meshutil::misc_init();
    registerEnums();

    ///////////////////////////////////////////////////////////////
  }
};

void initModule(ork::appinitdata_ptr_t init_data) {
  static ModuleInit initer(init_data);
}

} // namespace lev2

///////////////////////////////////////////////////////////////////////////////

class sortperfpred {
public:
  bool operator()(const PerformanceItem* t1, const PerformanceItem* t2) const // comparison predicate for map sorting
  {
    bool bval = true;

    const PerformanceItem* RootU = PerformanceTracker::GetRef().mRoots[PerformanceTracker::EPS_UPDTHREAD];
    const PerformanceItem* RootG = PerformanceTracker::GetRef().mRoots[PerformanceTracker::EPS_GFXTHREAD];

    if ((t1 == RootU) || (t1 == RootG)) {
      return true;
    } else if ((t2 == RootU) || (t2 == RootG)) {
      return false;
    }

    if (t1->miAvgCycle <= t2->miAvgCycle) {
      bval = false;
    }

    return bval;
  }
};

#if 0
void PerformanceTracker::Draw(ork::lev2::Context* pTARG) {
  // return; //
  // orklist<PerformanceItem*>* PerfItemList = PerformanceTracker::GetItemList();
  /*s64 PerfTotal = PerformanceTracker::GetRef().mpRoot->miAvgCycle;

  int itX = pTARG->x;
  int itY = pTARG->y;
  int itW = pTARG->width();
  int itH = pTARG->height();

  int iih = 16;

  int ipY2 = itH-8;
  int ipY = ipY2-iih;
  int iSX = 8;
  int iSW = itW-iSX;

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  ork::lev2::GfxMaterial3DSolid Material(pTARG);
  Material._rasterstate.SetDepthTest( ork::lev2::EDepthTest::ALWAYS );
  Material._rasterstate.SetBlending( ork::lev2::Blending::ADDITIVE );
  Material.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
  Material._rasterstate.SetZWriteMask( false );
  pTARG->BindMaterial( & Material );

  pTARG->PushModColor( fcolor4(0.0f,0.5f,0.0f) );

  //////////////////////////////////////////////////////////////////////
  orkstack<PerformanceItem*> PerfItemStack;
  PerfItemStack.push(PerformanceTracker::GetRef().mpRoot);
  orkvector<PerformanceItem*> SortedPerfVect;
  while( false == PerfItemStack.empty() )
  {
      PerformanceItem* pItem = PerfItemStack.top();
      PerfItemStack.pop();

      SortedPerfVect.push_back( pItem );
      orklist<PerformanceItem*>* ChildList = pItem->GetChildrenList();
      for( orklist<PerformanceItem*>::iterator itc=ChildList->begin(); itc!=ChildList->end(); itc++ )
      {
          PerfItemStack.push(*itc);
      }
  }

  std::sort( SortedPerfVect.begin(), SortedPerfVect.end(), sortperfpred() );

  //////////////////////////////////////////////////////////////////////

  for( int i=0; i<int(SortedPerfVect.size()); i++ )
  {
      PerformanceItem* pItem = SortedPerfVect[i];

      s64 fvalue = pItem->miAvgCycle;

      if( fvalue )
      {
          std::string name = pItem->GetName();

          f64 fpercent = 0.0;
          if(PerfTotal)
              fpercent = f64(fvalue) / f64(PerfTotal);

          f32 fx = (f32) iSX+1;
          f32 fx2 = (f32) (iSX + (fpercent*iSW))-1;
          fvec4 Vertices[6];
          Vertices[0].SetXYZ( fx, (f32) ipY+1, 0.5f );
          Vertices[1].SetXYZ( fx, (f32) ipY2-1, 0.5f );
          Vertices[2].SetXYZ( fx2, (f32) ipY2-1, 0.5f );
          Vertices[3].SetXYZ( fx, (f32) ipY+1, 0.5f );
          Vertices[4].SetXYZ( fx2, (f32) ipY+1, 0.5f );
          Vertices[5].SetXYZ( fx2, (f32) ipY2-1, 0.5f );

          pTARG->IMI()->DrawPrim( Vertices, 6, ork::lev2::PrimitiveType::TRIANGLES );

          ipY -= iih;
          ipY2 -= iih;
      }

  }
  pTARG->IMI()->QueFlush();
  pTARG->PopModColor();

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  ipY2 = (itH-8)+2;
  ipY = ipY2-iih;

  pTARG->PushModColor( fcolor4::White() );
  for( int i=0; i<int(SortedPerfVect.size()); i++ )
  {
      PerformanceItem* pItem = SortedPerfVect[i];

      s64 fvalue = pItem->miAvgCycle;

      if( fvalue )
      {
          std::string name = pItem->GetName();

          f64 fpercent = 0.0;
          if(PerfTotal)
              fpercent = f64(fvalue) / f64(PerfTotal);

          f64 ftime = fvalue/OldSchool::GetRef().mfClockRate;
          int ix = iSX+4;

          int fps = int(1.0f/ftime);
#ifndef WII
          ork::lev2::FontMan::DrawText( pTARG, ix, ipY, (char*) CreateFormattedString( "%s <%02d fps> <%2.2f msec>", (char *)
name.c_str(), fps, ftime*1000.0f ).c_str() ); #endif

          ipY -= iih;
          ipY2 -= iih;
      }
  }

  pTARG->IMI()->QueFlush();
  pTARG->PopModColor();*/
}
#endif

} // namespace ork

template class ork::Plane<float>; // explicit template instantiation
