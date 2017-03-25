////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxenv_targetinterfaces.h>
#include <ork/lev2/gfx/particle/modular_particles.h>
#include <ork/dataflow/dataflow.h>
///////////////////////////////////////////////////////////////////////////////
//#define WIIEMU
///////////////////////////////////////////////////////////////////////////////

namespace ork {


namespace lev2 {

static SFileDevContext LocPlatformLevel2FileContext;
const SFileDevContext & PlatformLevel2FileContext = LocPlatformLevel2FileContext;

#if defined(_WIN32)
static bool gbPREFEROPENGL = false;
#else
static bool gbPREFEROPENGL = true;
#endif

void Direct3dGfxTargetInit();
void WiiGfxTargetInit();
void OpenGlGfxTargetInit();
void DummyGfxTargetInit();

void PreferOpenGL()
{
	ork::lev2::OpenGlGfxTargetInit();
	gbPREFEROPENGL=true;
}

void Init(const std::string& gfxlayer)
{
	AllocationLabel label("ork::lev2::Init");

	GfxEnv::GetRef();
	CGfxPrimitives::GetRef();

	GfxMaterialWiiBasic::StaticInit();

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


	//////////////////////////////////////////
	// register lev2 graphics target classes

	DummyGfxTargetInit();

	if( gfxlayer != "dummy" )
	{
		#if defined(WII) || defined(WIIEMU)
		WiiGfxTargetInit();
		#endif
		#if defined(ORK_CONFIG_OPENGL)
		OpenGlGfxTargetInit();
		#endif
		#if defined(ORK_CONFIG_DIRECT3D)
		if( false == gbPREFEROPENGL )
		{
			Direct3dGfxTargetInit();
		}
		#endif
	}
	
	//////////////////////////////////////////
}

} // namespace lev2


///////////////////////////////////////////////////////////////////////////////

class sortperfpred
{
public:

	bool operator()( const CPerformanceItem* t1, const CPerformanceItem* t2 ) const // comparison predicate for map sorting
	{
		bool bval = true;
			
		const CPerformanceItem* RootU = CPerformanceTracker::GetRef().mRoots[ CPerformanceTracker::EPS_UPDTHREAD];
		const CPerformanceItem* RootG = CPerformanceTracker::GetRef().mRoots[ CPerformanceTracker::EPS_GFXTHREAD];


		if( (t1==RootU)||(t1==RootG) )
		{
			return true;
		}
		else if( (t2==RootU)||(t2==RootG) )
		{
			return false;
		}

		if( t1->miAvgCycle <= t2->miAvgCycle ) 
		{
			bval = false;
		}

		return bval;
	}
};

void CPerformanceTracker::Draw( ork::lev2::GfxTarget *pTARG )
{
	//return; //
	//orklist<CPerformanceItem*>* PerfItemList = CPerformanceTracker::GetItemList();
	/*s64 PerfTotal = CPerformanceTracker::GetRef().mpRoot->miAvgCycle;

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
	Material.mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
	Material.mRasterState.SetBlending( ork::lev2::EBLENDING_ADDITIVE );
	Material.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
	Material.mRasterState.SetZWriteMask( false );
	pTARG->BindMaterial( & Material );

	pTARG->PushModColor( CColor4(0.0f,0.5f,0.0f) );

	//////////////////////////////////////////////////////////////////////
	orkstack<CPerformanceItem*> PerfItemStack;
	PerfItemStack.push(CPerformanceTracker::GetRef().mpRoot);
	orkvector<CPerformanceItem*> SortedPerfVect;
	while( false == PerfItemStack.empty() )
	{
		CPerformanceItem* pItem = PerfItemStack.top();
		PerfItemStack.pop();

		SortedPerfVect.push_back( pItem );
		orklist<CPerformanceItem*>* ChildList = pItem->GetChildrenList();
		for( orklist<CPerformanceItem*>::iterator itc=ChildList->begin(); itc!=ChildList->end(); itc++ )
		{
			PerfItemStack.push(*itc);
		}
	}
		
	std::sort( SortedPerfVect.begin(), SortedPerfVect.end(), sortperfpred() );

	//////////////////////////////////////////////////////////////////////

	for( int i=0; i<int(SortedPerfVect.size()); i++ )
	{
		CPerformanceItem* pItem = SortedPerfVect[i];

		s64 fvalue = pItem->miAvgCycle;

		if( fvalue )
		{
			std::string name = pItem->GetName();

			f64 fpercent = 0.0;
			if(PerfTotal)
				fpercent = f64(fvalue) / f64(PerfTotal);

			f32 fx = (f32) iSX+1;
			f32 fx2 = (f32) (iSX + (fpercent*iSW))-1;
			CVector4 Vertices[6];
			Vertices[0].SetXYZ( fx, (f32) ipY+1, 0.5f );
			Vertices[1].SetXYZ( fx, (f32) ipY2-1, 0.5f );
			Vertices[2].SetXYZ( fx2, (f32) ipY2-1, 0.5f );
			Vertices[3].SetXYZ( fx, (f32) ipY+1, 0.5f );
			Vertices[4].SetXYZ( fx2, (f32) ipY+1, 0.5f );
			Vertices[5].SetXYZ( fx2, (f32) ipY2-1, 0.5f );

			pTARG->IMI()->DrawPrim( Vertices, 6, ork::lev2::EPRIM_TRIANGLES );

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

	pTARG->PushModColor( CColor4::White() );
	for( int i=0; i<int(SortedPerfVect.size()); i++ )
	{
		CPerformanceItem* pItem = SortedPerfVect[i];

		s64 fvalue = pItem->miAvgCycle;

		if( fvalue )
		{
			std::string name = pItem->GetName();

			f64 fpercent = 0.0;
			if(PerfTotal)
				fpercent = f64(fvalue) / f64(PerfTotal);

			f64 ftime = fvalue/CSystem::GetRef().mfClockRate;
			int ix = iSX+4;

			int fps = int(1.0f/ftime);
#ifndef WII
			ork::lev2::CFontMan::DrawText( pTARG, ix, ipY, (char*) CreateFormattedString( "%s <%02d fps> <%2.2f msec>", (char *) name.c_str(), fps, ftime*1000.0f ).c_str() );
#endif

			ipY -= iih;
			ipY2 -= iih;
		}
	}

	pTARG->IMI()->QueFlush();
	pTARG->PopModColor();*/
}

} // namespace ork
