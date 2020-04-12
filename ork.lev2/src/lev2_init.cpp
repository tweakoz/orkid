////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/timer.h>
#include <ork/dataflow/dataflow.h>
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
#include <ork/lev2/gfx/particle/modular_particles.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorFx3.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPtx.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
///////////////////////////////////////////////////////////////////////////////
//#define WIIEMU
///////////////////////////////////////////////////////////////////////////////

namespace ork {

namespace lev2 {

namespace vk {
void init();
}
static FileDevContext LocPlatformLevel2FileContext;
const FileDevContext& PlatformLevel2FileContext = LocPlatformLevel2FileContext;

#if defined(_WIN32)
static bool gbPREFEROPENGL = false;
#else
static bool gbPREFEROPENGL = true;
#endif

void Direct3dContextInit();
void WiiContextInit();
void OpenGlContextInit();
void DummyContextInit();

void PreferOpenGL() {
  ork::lev2::OpenGlContextInit();
  gbPREFEROPENGL = true;
}

void ClassInit() {
  AllocationLabel label("ork::lev2::Init");

  GfxEnv::GetRef();
  GfxPrimitives::GetRef();

  //////////////////////////////////////////
  // touch of class

  particle::ParticleModule::GetClassStatic();
  particle::ParticlePool::GetClassStatic();
  particle::ExtConnector::GetClassStatic();

  particle::Constants::GetClassStatic();
  particle::FloatOp2Module::GetClassStatic();
  particle::Vec3Op2Module::GetClassStatic();
  particle::Vec3SplitModule::GetClassStatic();

  particle::Global::GetClassStatic();

  particle::RingEmitter::GetClassStatic();
  particle::ReEmitter::GetClassStatic();

  particle::GravityModule::GetClassStatic();
  particle::TurbulenceModule::GetClassStatic();
  particle::VortexModule::GetClassStatic();

  particle::RendererModule::GetClassStatic();
  particle::SpriteRenderer::GetClassStatic();
  particle::SpriteRenderer::GetClassStatic();

  ork::dataflow::floatxfitembase::GetClassStatic();
  ork::dataflow::floatxfmsbcurve::GetClassStatic();

  dataflow::outplug<proctex::ImgBase>::GetClassStatic();
  dataflow::inplug<proctex::ImgBase>::GetClassStatic();

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

  RegisterClassX(PointLightData);

  RegisterClassX(OutputCompositingNode);
  RegisterClassX(VrCompositingNode);
  RegisterClassX(ScreenOutputCompositingNode);

  RegisterClassX(RenderCompositingNode);
  RegisterClassX(ForwardCompositingNode);
  RegisterClassX(deferrednode::DeferredCompositingNode);
  RegisterClassX(deferrednode::DeferredCompositingNodePbr);

#if defined(ENABLE_NVMESH_SHADERS)
  RegisterClassX(deferrednode::DeferredCompositingNodeNvMs);
#endif

  RegisterClassX(CompositingScene);
  RegisterClassX(CompositingData);
  RegisterClassX(CompositingGroupEffect);
  RegisterClassX(CompositingGroup);
  RegisterClassX(CompositingSceneItem);

  RegisterClassX(PostCompositingNode);
  RegisterClassX(Fx3CompositingTechnique);
  RegisterClassX(Fx3CompositingNode);
  RegisterClassX(ScaleBiasCompositingNode);
  RegisterClassX(SeriesCompositingNode);
  RegisterClassX(PtxCompositingNode);
  RegisterClassX(Op2CompositingNode);
  RegisterClassX(NodeCompositingTechnique);
  RegisterClassX(PBRMaterial);

  RegisterClassX(TerrainDrawableData);

  //////////////////////////////////////////
  // register lev2 graphics target classes

  DummyContextInit();

  //////////////////////////////////////////
}

void GfxInit(const std::string& gfxlayer) {
  vk::init();
  if (gfxlayer != "dummy") {
#if defined(ORK_CONFIG_OPENGL)
    OpenGlContextInit();
#endif
  }
  DrawableBuffer::gbInsideClearAndSync = false;
}

StdFileSystemInitalizer::StdFileSystemInitalizer(int argc, char** argv) {
  // printf("CPA\n");
  OldSchool::SetGlobalStringVariable("lev2://", std::string("ork.data/platform_lev2/"));
  OldSchool::SetGlobalStringVariable("miniorkdata://", CreateFormattedString("ork.data/"));
  OldSchool::SetGlobalStringVariable("src://", CreateFormattedString("ork.data/src/"));
  OldSchool::SetGlobalStringVariable("temp://", CreateFormattedString("ork.data/temp/"));

  // printf("CPB\n");
  //////////////////////////////////////////
  // Register data:// urlbase

  // todo - hold somewhere not static
  static auto WorkingDirContext = std::make_shared<FileDevContext>();

  auto base_dir = ork::file::GetStartupDirectory();

  if (getenv("ORKID_WORKSPACE_DIR") != nullptr)
    base_dir = getenv("ORKID_WORKSPACE_DIR");

  OldSchool::SetGlobalStringVariable("data://", base_dir.c_str());

  printf("ORKID_WORKSPACE_DIR<%s>\n", base_dir.c_str());

  // printf("CPB2\n");
  //////////////////////////////////////////
  // Register lev2:// data urlbase

  static auto LocPlatformLevel2FileContext = std::make_shared<FileDevContext>();
  LocPlatformLevel2FileContext->SetFilesystemBaseAbs(OldSchool::GetGlobalStringVariable("lev2://").c_str());
  LocPlatformLevel2FileContext->SetPrependFilesystemBase(true);

  FileEnv::registerUrlBase("lev2://", LocPlatformLevel2FileContext);

  // printf("CPB3\n");

  //////////////////////////////////////////
  // Register src:// data urlbase

  static auto SrcPlatformLevel2FileContext = std::make_shared<FileDevContext>();
  SrcPlatformLevel2FileContext->SetFilesystemBaseAbs(OldSchool::GetGlobalStringVariable("src://").c_str());
  SrcPlatformLevel2FileContext->SetPrependFilesystemBase(true);

  FileEnv::registerUrlBase("src://", SrcPlatformLevel2FileContext);

  // printf("CPC\n");

  //////////////////////////////////////////
  // Register temp:// data urlbase

  static auto TempPlatformLevel2FileContext = std::make_shared<FileDevContext>();
  TempPlatformLevel2FileContext->SetFilesystemBaseAbs(OldSchool::GetGlobalStringVariable("temp://").c_str());
  TempPlatformLevel2FileContext->SetPrependFilesystemBase(true);

  FileEnv::registerUrlBase("temp://", TempPlatformLevel2FileContext);

  //////////////////////////////////////////
  // Register miniork:// data urlbase

  static auto LocPlatformMorkDataFileContext = std::make_shared<FileDevContext>();
  LocPlatformMorkDataFileContext->SetFilesystemBaseAbs(OldSchool::GetGlobalStringVariable("miniorkdata://").c_str());
  LocPlatformMorkDataFileContext->SetPrependFilesystemBase(true);

  FileEnv::registerUrlBase("miniorkdata://", LocPlatformMorkDataFileContext);

  //////////////////////////////////////////

  static auto DataDirContext = std::make_shared<FileDevContext>();

  auto data_dir = file::Path::orkroot_dir() / "ork.data" / "pc";

  DataDirContext->SetFilesystemBaseAbs(data_dir.c_str());
  DataDirContext->SetPrependFilesystemBase(true);

  static auto MiniorkDirContext = std::make_shared<FileDevContext>();
  MiniorkDirContext->SetFilesystemBaseAbs(OldSchool::GetGlobalStringVariable("lev2://").c_str());
  MiniorkDirContext->SetPrependFilesystemBase(true);

  // printf("CPM\n");

  for (int iarg = 1; iarg < argc; iarg++) {
    const char* parg = argv[iarg];

    if (strcmp(parg, "--datafolder") == 0) {
      file::Path pth(argv[iarg + 1]);

      file::Path::NameType dirname = pth.ToAbsolute().c_str();
      std::transform(dirname.begin(), dirname.end(), dirname.begin(), ork::dos2unixpathsep());

      FileEnvDir* TheDir = FileEnv::GetRef().OpenDir(dirname.c_str());

      if (TheDir) {
        OldSchool::SetGlobalStringVariable("data://", dirname.c_str());
        FileEnv::GetRef().CloseDir(TheDir);
        DataDirContext->SetFilesystemBaseAbs(dirname);
      } else {
        OrkNonFatalAssertI(false, "specified Data Folder Does Not Exist!!\n");
      }
      iarg++;
    } else if (strcmp(parg, "--lev2folder") == 0) {
      file::Path pth(argv[iarg + 1]);

      file::Path::NameType dirname = pth.ToAbsolute().c_str();
      std::transform(dirname.begin(), dirname.end(), dirname.begin(), ork::dos2unixpathsep());

      FileEnvDir* TheDir = FileEnv::GetRef().OpenDir(dirname.c_str());

      if (TheDir) {
        OldSchool::SetGlobalStringVariable("lev2://", dirname.c_str());
        FileEnv::GetRef().CloseDir(TheDir);
        MiniorkDirContext->SetFilesystemBaseAbs(dirname);
      } else {
        OrkNonFatalAssertI(false, "specified MiniorkFolder Does Not Exist!!\n");
      }
      iarg++;
    }
  }

  FileEnv::registerUrlBase("data://", DataDirContext);
  FileEnv::registerUrlBase("lev2://", MiniorkDirContext);
}
StdFileSystemInitalizer::~StdFileSystemInitalizer() {
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

void PerformanceTracker::Draw(ork::lev2::Context* pTARG) {
  // return; //
  // orklist<PerformanceItem*>* PerfItemList = PerformanceTracker::GetItemList();
  /*s64 PerfTotal = PerformanceTracker::GetRef().mpRoot->miAvgCycle;

  int itX = pTARG->GetX();
  int itY = pTARG->GetY();
  int itW = pTARG->GetW();
  int itH = pTARG->GetH();

  int iih = 16;

  int ipY2 = itH-8;
  int ipY = ipY2-iih;
  int iSX = 8;
  int iSW = itW-iSX;

  //////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////
  ork::lev2::GfxMaterial3DSolid Material(pTARG);
  Material._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
  Material._rasterstate.SetBlending( ork::lev2::EBLENDING_ADDITIVE );
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

          pTARG->IMI()->DrawPrim( Vertices, 6, ork::lev2::EPrimitiveType::TRIANGLES );

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

} // namespace ork
