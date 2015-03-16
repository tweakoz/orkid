////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// Synthetic Fluvial Erosion
//	based on "erode" code from John P. Beale 6/10/95-7/7/95
//
///////////////////////////////////////////////////////////////////////////////
//
//  notes from original erode:
//   
//	1. the algorithm may "bomb" on perfectly flat surfaces
//
//	2  sometimes there's some kind of instability where there is suddently
//		a spike pushing up out of the surface like a new volcano or something; this
//		seems to happen at the higher erosion or smoothing rates. It might not
//		appear with the default values. Then again it might.
//
///////////////////////////////////////////////////////////////////////////////
#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>
#include <ork/dataflow/scheduler.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include "terrain_synth.h"
#include "terrain_erosion.h"
///////////////////////////////////////////////////////////////////////////////
#if 0
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static int gindex = -1;
static heightfield_compute_buffer& GetComputeWrite()
{
	gindex++;
	return HeightMapGPGPUComputeBuffer(gindex&1);
}
static heightfield_compute_buffer& GetComputeRead()
{
	return HeightMapGPGPUComputeBuffer((gindex+1)&1);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuErodeBegin(	ErosionContext& ec, const Map2D<float>& hfin )
{
	const int kSIZE = ec.miGridSize;
	heightfield_compute_buffer& ComputeBuffer = GetComputeWrite();
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	//////////////////////////////////////////////////////////
	{	CVector4* pv4 = const_cast<CVector4*>( static_cast<const CVector4*>( ComputeBuffer.tex1buffer.GetData()  ));
		for( int iz=0; iz<kSIZE; iz++ )
		{	int iaddr = ComputeBuffer.tex1buffer.CalcDataIndex(0,iz);
			const float* floatbase = & hfin.Read(0,iz);
			for( int ix=0; ix<kSIZE; ix++ )
			{	float fheight = floatbase[ix];
				pv4[iaddr++] = CVector4( fheight, 0.0f, 0.0f, 0.0f ); 
			}
		}
		//////////////////////////////////////////////////////////
		lev2::GfxMaterial3DSolid matsolid( ComputeBuffer.GetContext(), "miniorkshader://heightmap_erode", "begin" );
		matsolid.SetTexture( & ComputeBuffer.tex1 );
		float texw = float(ComputeBuffer.tex1.GetWidth());
		float itexw = 1.0f / float( ComputeBuffer.tex1.GetWidth() );
		CColor4 mc( float(kSIZE), float(kSIZE), texw, itexw );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBuffer );
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuFindFlow2(	ErosionContext& ec )
{	
	#if 0
	const int kSIZE = ec.miGridSize;
	heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
	heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	//////////////////////////////////////////////////////////
	{	
		lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "find_flow2" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		float texw = float(ComputeBufferRD.mpTexture->GetWidth());
		float itexw = 1.0f / texw;
		CColor4 mc( float(kSIZE), float(kSIZE), texw, itexw );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuFindUpFlow(	ErosionContext& ec )
{
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	{
		heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
		heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
		lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "find_upflow" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		float texw = float(ComputeBufferRD.mpTexture->GetWidth());
		float itexw = 1.0f / texw;
		CColor4 mc( float(kSIZE), float(kSIZE), texw, itexw );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
	}
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuFindUphillArea1( const ErosionContext& ec, Map2D<float>& hfout_ua )
{
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	{	
		heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
		heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
		CVector4* pv4 = const_cast<CVector4*>( static_cast<const CVector4*>( ComputeBufferWR.tex1buffer.GetData()  ));
		for( int iz=0; iz<kSIZE; iz++ )
		{	int iaddr = ComputeBufferWR.tex1buffer.CalcDataIndex(0,iz);
			const float* uabase = & hfout_ua.Read(0,iz);
			for( int ix=0; ix<kSIZE; ix++ )
			{	float uain = uabase[ix];
				pv4[iaddr++] = CVector4( uain, 0.0f,0.0f, 0.0f ); 
			}
		}
		//////////////////////////////////////////////////////////
		lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "find_uphillarea1" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		matsolid.SetTexture2( & ComputeBufferWR.tex1 );
		float texw = float(ComputeBufferWR.tex1.GetWidth());
		float itexw = 1.0f / texw;
		CColor4 mc( float(kSIZE), float(kSIZE), texw, itexw );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
	}
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////
	// 2nd through n passes: cumulative add areas in a downhill-flow hierarchy 
	///////////////////////////////////////////////////////////
	for( int i=0; i<3; i++ )
	{
		heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
		heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
		lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
		{	lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "find_uphillarea2" );
			matsolid.SetTexture( ComputeBufferRD.mpTexture );
			float texw = float(ComputeBufferRD.tex1.GetWidth());
			float itexw = 1.0f / texw;
			CColor4 mc( float(kSIZE), float(kSIZE), texw, itexw );
			GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
		}
		lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
		if( i==2 )
		{
			ComputeBufferWR.GetContext()->Capture( ComputeBufferWR.MyCaptureBuffer );
			const CVector4* VecBuffer = (const CVector4*) ComputeBufferWR.MyCaptureBuffer.GetData();
			for( int iZ=0; iZ<kSIZE; iZ++ )
			{	for( int iX=0; iX<kSIZE; iX++ )
				{	int index_n = (iZ*kSIZE)+iX;
					int index = ComputeBufferWR.MyCaptureBuffer.CalcDataIndex( iX, iZ );
					const CVector4& v = VecBuffer[ index ];
					float uaout = v.GetW();
					hfout_ua.Write(iX,iZ) = uaout;
				}
			}
		}
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuErode1(	ErosionContext& ec )
{	
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
	heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
	//////////////////////////////////////////////////////////
	{	lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "erode1" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		matsolid.SetTexture2( & ComputeBufferWR.tex1 );
		float slopefactor = ec.mfTerrainHeight / (ec.mfTerrainSize / ec.xsize);  // slope of 0.0 next to 1.0 
		CColor4 mc( float(kSIZE), slopefactor, ec.mErosionRate, 0.0f );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuErodeCorrect( ErosionContext& ec )
{
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
	heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
	//////////////////////////////////////////////////////////
	{	lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "erode_correct" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		CColor4 mc( float(kSIZE), 0.0f, 0.0f, 0.0f );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuSlump(	ErosionContext& ec )
{
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
	heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
	//////////////////////////////////////////////////////////
	{	lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "slump" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		CColor4 mc( float(kSIZE), ec.mSmoothingRate, ec.mSlumpScale, 0.0f );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuSmooth(	ErosionContext& ec )
{
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
	heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
	//////////////////////////////////////////////////////////
	{	lev2::GfxMaterial3DSolid matsolid( ComputeBufferWR.GetContext(), "miniorkshader://heightmap_erode", "smooth" );
		matsolid.SetTexture( ComputeBufferRD.mpTexture );
		CColor4 mc( float(kSIZE), ec.mSmoothingRate, 0.0f, 0.0f );
		GpGpuTask( CColor4::Black(), mc, matsolid, kSIZE, kSIZE, ComputeBufferWR );
		//ComputeBufferWR.GetContext()->Capture( ComputeBufferWR.MyCaptureBuffer );
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GpuErodeEnd(	ErosionContext& ec,	Map2D<float>& hfout	)
{
	#if 0
	const int kSIZE = ec.miGridSize;
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	heightfield_compute_buffer& ComputeBufferWR = GetComputeWrite();
	heightfield_compute_buffer& ComputeBufferRD = GetComputeRead();
	//ComputeBufferRD.GetContext()->Capture( ComputeBufferRD.MyCaptureBuffer );
	//////////////////////////////////////////////////////////
	{	const CVector4* VecBuffer = (const CVector4*) ComputeBufferRD.MyCaptureBuffer.GetData();
		for( int iZ=0; iZ<kSIZE; iZ++ )
		{	for( int iX=0; iX<kSIZE; iX++ )
			{	int index_n = (iZ*kSIZE)+iX;
				int index = ComputeBufferRD.MyCaptureBuffer.CalcDataIndex( iX, iZ );
				const CVector4& v = VecBuffer[ index ];
				hfout.Write(iX,iZ) = v.GetX();
	}
		}
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}}
#endif