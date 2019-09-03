////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/dataflow/dataflow.h>
//#include <ork/dataflow/dataflow.hpp>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>
#include <ork/dataflow/scheduler.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/gfx/camera.h>

#include "terrain_synth.h"
#if 0

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
template<> int MaxFanout<ork::ent::HeightMap>() { return 1; }
}}
namespace ork { namespace terrain {

void ComputeNormalsGpu(hmap_hfield_module& mod, HeightMap_datablock& db, orkvector<fvec3>& outputnormals );
void ComputeColorsGpu(HeightMap_datablock& db);
void ComputeColorsGpu(HeightMap_datablock& db, const orkvector<fvec3>& normals , orkvector<U32>& outputcolors, lev2::Texture* lightenvtex );
lev2::CaptureBuffer& HeightMapGPGPUCaptureBuffer()
{
	static lev2::CaptureBuffer capbuf;
	return capbuf;
}
heightfield_compute_buffer& HeightMapGPGPUComputeBuffer(int idb)
{
	OrkAssert(idb>=0);
	OrkAssert(idb<2);
	static heightfield_compute_buffer compbuf[2];
	return compbuf[idb];
}

const int heightfield_compute_buffer::kw = 1024;
const int heightfield_compute_buffer::kh = 1024;

heightfield_compute_buffer::heightfield_compute_buffer()
	: lev2::GfxBuffer(
		0,
		kx, ky,
		kw, kh, 
		lev2::EBUFFMT_RGBA128,
		lev2::ETGTTYPE_EXTBUFFER,
		"HeightFieldComputeBuffer" )
{
	CreateContext();
	tex1buffer.SetWidth( kw );
	tex1buffer.SetHeight( kh );
	tex1buffer.SetFormat( lev2::EBUFFMT_RGBA128 );
//	tex2buffer.SetWidth( kw );
//	tex2buffer.SetHeight( kh );
//	tex2buffer.SetFormat( lev2::EBUFFMT_RGBA128 );
	
	assert(false);
	//bool bOK = GetContext()->SetTexture( tex1buffer, tex1 );
	//OrkAssert( bOK );

//	bOK = GetContext()->SetTexture( tex2buffer, tex2 );
//	OrkAssert( bOK );
}
//////////////////////////////////////////////////////////
void GpGpuTask	(	const fvec4& ClearColor,
					const fvec4& ModColor,
					lev2::GfxMaterial& material,
					int iwidth, int iheight,
					heightfield_compute_buffer& MyComputeBuffer
				)
{
	#if 0
	//////////////////////////////////////////////////////////
	MyComputeBuffer.GetContext()->SetClearColor( ClearColor );
	MyComputeBuffer.GetContext()->SetAutoClear(true);
	MyComputeBuffer.BeginFrame();
	//////////////////////////////////////////////////////////
	{	fmtx4 MatP = MyComputeBuffer.GetContext()->Ortho( 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f );		
		OrkAssert( isize <= heightfield_compute_buffer::kw );
		OrkAssert( isize <= heightfield_compute_buffer::kh );
		SRect VPRECT( 0, 0, iwidth, iheight );
		//////////////////////////////////////////////////////////
		MyComputeBuffer.GetContext()->PushPMatrix( MatP );
		MyComputeBuffer.GetContext()->PushVMatrix( fmtx4::Identity );
		MyComputeBuffer.GetContext()->PushMMatrix( fmtx4::Identity );
		MyComputeBuffer.GetContext()->BindMaterial( & material );
		MyComputeBuffer.GetContext()->PushModColor( ModColor );
		MyComputeBuffer.GetContext()->PushViewport(VPRECT);
		MyComputeBuffer.GetContext()->PushScissor(VPRECT);
		//////////////////////////////////////////////////////////
		{	static lev2::StaticVertexBuffer<lev2::SVtxV12C4T8> MyVtxBuf( 8, 0, lev2::EPRIM_TRIANGLES );
			MyComputeBuffer.GetContext()->VtxBuf_Lock( MyVtxBuf );
			const float fZed = 0.5f;
			const float kfmax = 1.0f; //0.25f;
			MyVtxBuf.AddVertex( lev2::SVtxV12C4T8( 0.0f, 0.0f, fZed, 0.0f, 0.0f, 0xffffffff ) );
			MyVtxBuf.AddVertex( lev2::SVtxV12C4T8( kfmax, 0.0f, fZed, 1.0f, 0.0f, 0xffffffff ) );
			MyVtxBuf.AddVertex( lev2::SVtxV12C4T8( kfmax, kfmax, fZed, 1.0f, 1.0f, 0xffffffff ) );
			MyVtxBuf.AddVertex( lev2::SVtxV12C4T8( 0.0f, 0.0f, fZed, 0.0f, 0.0f, 0xffffffff ) );
			MyVtxBuf.AddVertex( lev2::SVtxV12C4T8( kfmax, kfmax, fZed, 1.0f, 1.0f, 0xffffffff ) );
			MyVtxBuf.AddVertex( lev2::SVtxV12C4T8( 0.0f, kfmax, fZed, 0.0f, 1.0f, 0xffffffff ) );
			MyComputeBuffer.GetContext()->VtxBuf_UnLock( MyVtxBuf );

			MyComputeBuffer.GetContext()->SetTexture( MyComputeBuffer.tex1buffer, MyComputeBuffer.tex1 );
			//MyComputeBuffer.GetContext()->SetTexture( MyComputeBuffer.tex2buffer, MyComputeBuffer.tex2 );

			MyComputeBuffer.GetContext()->DrawPrimitive( MyVtxBuf, lev2::EPRIM_TRIANGLES );
		}
		//////////////////////////////////////////////////////////
		MyComputeBuffer.GetContext()->PopScissor();
		MyComputeBuffer.GetContext()->PopViewport();
		MyComputeBuffer.GetContext()->PopModColor();
		MyComputeBuffer.GetContext()->PopMMatrix();
		MyComputeBuffer.GetContext()->PopVMatrix();
		MyComputeBuffer.GetContext()->PopPMatrix();
		//////////////////////////////////////////////////////////
	}
	//////////////////////////////////////////////////////////
	MyComputeBuffer.EndFrame();
	//MyComputeBuffer.GetContext()->Capture( MyComputeBuffer.MyCaptureBuffer );
	//////////////////////////////////////////////////////////
	#endif
}

void heightfield_compute_buffer::OutputToTexture( lev2::Texture* ptex )
{
	//GetContext()->CaptureToTexture( ptex );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
HeightMap_datablock::HeightMap_datablock()
	: mHeightMap(1)
{
}
void HeightMap_datablock::Copy( const HeightMap_datablock& oth )
{
	mHeightMap.SetGridSize( oth.mHeightMap.GetGridSize() );
	mHeightMap.SetWorldSize( oth.mHeightMap.GetWorldSize() );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int hmap_hfield_module::GetNumInputs() const { return 1; }
int hmap_hfield_module::GetNumOutputs() const {	return 1; }
dataflow::inplugbase* hmap_hfield_module::GetInput(int idx)
{
	OrkAssert( idx==0 );
	return &mInputPlug;
}
const dataflow::outplugbase* hmap_hfield_module::GetOutput(int idx) const
{	OrkAssert( idx==0 );
	return &mHeightOutputPlug;
}
///////////////////////////////////////////////////////////////////////////////
hmap_hfield_module::hmap_hfield_module( ent::GradientSet& gset, int isize )
	: mHeightOutputPlug(this,dataflow::EPR_UNIFORM, &mDefDataBlock.mHeightMap, "Output" )
	, mInputPlug( this, dataflow::EPR_UNIFORM, mDefDataBlock.mHeightMap, "Input" )
	, mIndexToWorld(1.0f)
	, mInverseGridSize(1.0f)
	, mVisMutex( "VisMutex" )
	//, mpLightEnvTexture( HeightMapGPGPUComputeBuffer().GetContext()->LoadTexture( "lev2://textures/skymap_earthy.tga" ) )
	, miSize( isize )
	, mpColorMapTexture( 0 )

{
	SetSize(isize);
	AddDependency( mHeightOutputPlug, mInputPlug );
}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::SetSize( int i )
{	miSize = i;
	size_t isq = size_t(i*i);
	float fwsize = mDefDataBlock.mHeightMap.GetWorldSize();
	if( mDefDataBlock.mHeightMap.GetGridSize() != i )
	{	mDefDataBlock.mHeightMap.SetGridSize( i );
	}
	mVisMutex.Lock();
	{	if( mrgb.size() != isq )
		{	mrgb.resize(isq);
		}
		int imaxsize = mnormals.max_size();
		if( mnormals.size() != isq )
		{	mnormals.resize(isq);
		}
		float fii( 1.0f/ float(i) );
		mInverseGridSize = fii;
		mIndexToWorld = fwsize*fii;
	}
	mVisMutex.UnLock();
	mHeightOutputPlug.SetDirty(true);
}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::Compute(dataflow::workunit* wu)
{
	#if 0
	if( mInputPlug.IsConnected() )
	{	HeightMap_datablock* hcw = (HeightMap_datablock*) wu->GetContextData();
		const HeightMap& hmin = mInputPlug.GetValue();
		int iw = hmin.GetGridSize();
		hcw->mHeightMap.SetGridSize( iw );
		hcw->mHeightMap.SetWorldSize( hmin.GetWorldSize() );
		hcw->mHeightMap.GetLock().Lock();
		{	for( int iz=0; iz<iw; iz++ )
			{	for( int ix=0; ix<iw; ix++ )
				{	float fh = hmin.GetHeight( ix, iz );
					hcw->mHeightMap.SetHeight( ix, iz, fh );
				}
			}
		}
		hcw->mHeightMap.GetLock().UnLock();
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus ) const
{	
	#if 0
	int inumcpu_processors = sch.GetNumProcessors( dataflow::scheduler::CpuAffinity );
	int inumgpu_processors = sch.GetNumProcessors( dataflow::scheduler::GpuAffinity );
	if( mInputPlug.IsConnected() )
	{	const HeightMap& hmin = mInputPlug.GetValue();
		int isiz = hmin.GetGridSize();
		HeightMap_datablock* hcw = OrkNew HeightMap_datablock;
		hcw->mHeightMap.SetGridSize( isiz );
		hcw->mHeightMap.SetWorldSize( hmin.GetWorldSize() );
		hcw->miX1 = 0;
		hcw->miZ1 = 0;
		hcw->miX2 = isiz-1;
		hcw->miZ2 = isiz-1;
		dataflow::workunit* wu = OrkNew dataflow::workunit(this,clus,0);
		wu->SetContextData(hcw);
		wu->SetAffinity( dataflow::scheduler::CpuAffinity );
		clus->AddWorkUnit(wu);
	}
	#endif

}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::CombineWork( const dataflow::cluster* clus )
{	
	#if 0
	const LockedResource< orkvector<dataflow::workunit*> >& WorkUnits = clus->GetWorkUnits();
	const orkvector<dataflow::workunit*>& wuvect = WorkUnits.LockForRead();
	int inumwu = wuvect.size();
	for( int i=0; i<inumwu; i++ )
	{	dataflow::workunit* wu = wuvect[i];
		if( wu->GetModule() == this )
		{	int wuidx = wu->GetModuleWuIndex();
			HeightMap_datablock* hcw = (HeightMap_datablock*) wu->GetContextData();
			if( 0 == wuidx ) // set my size from this
			{	SetSize(hcw->mHeightMap.GetGridSize());
				mDefDataBlock.mHeightMap.SetWorldSize( hcw->mHeightMap.GetWorldSize() );
				mDefDataBlock.mHeightMap.ResetMinMax();
			}
			mDefDataBlock.mHeightMap.GetLock().Lock();
			{	for( int iz=hcw->miZ1; iz<=hcw->miZ2; iz++ )
				{	for( int ix=hcw->miX1; ix<=hcw->miX2; ix++ )
					{	float fh = hcw->mHeightMap.GetHeight( ix, iz );
						mDefDataBlock.mHeightMap.SetHeight( ix, iz, fh );
					}
				}
			}
			mDefDataBlock.mHeightMap.GetLock().UnLock();
		}
	}
	WorkUnits.UnLock();
	ComputeNormals();
	ComputeColors();
	ComputeGeometry();
	mHeightOutputPlug.SetDirty(false);
	#endif
}
///////////////////////////////////////////////////////////////////////////////
fvec3 hmap_hfield_module::XYZ( int iX, int iZ ) const
{	return mDefDataBlock.mHeightMap.XYZ(iX,iZ);
}
///////////////////////////////////////////////////////////////////////////////
fvec3 hmap_hfield_module::ComputeNormal(int ix1,int iz1) const
{	return mDefDataBlock.mHeightMap.ComputeNormal(ix1,iz1);
}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::ComputeNormals()
{	
	#if 0
	orkprintf( "ComputingNormals\n" );
	int iw = mDefDataBlock.mHeightMap.GetGridSize();
	if( 0 == iw )
	{
		return;
		//mDefDataBlock.mHeightMap.SetGridSize( );
	}
	if( 1 )
	{	ComputeNormalsGpu( *this, mDefDataBlock, mnormals );
	}
	else
	{	for( int iZ1=0; iZ1<(iw-1); iZ1++ )
		{	int iZ2 = (iZ1+1)%iw;
			for( int iX1=0; iX1<(iw-1); iX1++ )
			{	int iX2 = (iX1+1)%iw;
				Normal(iX1,iZ1) = ComputeNormal(iX1,iZ1);
				Normal(iX2,iZ1) = ComputeNormal(iX2,iZ1);
				Normal(iX2,iZ2) = ComputeNormal(iX2,iZ2);
				Normal(iX1,iZ2) = ComputeNormal(iX1,iZ2);
			}
		}
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::ComputeColors()
{	orkprintf( "ComputingColors\n" );
	ComputeColorsGpu( mDefDataBlock, mnormals, mrgb, GetLightEnvTexture() );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
fvec3& hmap_hfield_module::Normal(int ix,int iz)
{	int idx = mDefDataBlock.mHeightMap.CalcAddress(ix,iz);
	return mnormals[idx];
}
const fvec3& hmap_hfield_module::Normal(int ix,int iz) const
{	int idx = mDefDataBlock.mHeightMap.CalcAddress(ix,iz);
	return mnormals[idx];
}
U32 hmap_hfield_module::Color(int ix, int iz) const
{	int idx = mDefDataBlock.mHeightMap.CalcAddress(ix,iz);
	return mrgb[idx];
}
U32& hmap_hfield_module::Color(int ix, int iz)
{	int idx = mDefDataBlock.mHeightMap.CalcAddress(ix,iz);
	return mrgb[idx];
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ComputeNormalsGpu(hmap_hfield_module& mod, HeightMap_datablock& db, orkvector<fvec3>& outputnormals )
{	
	#if 0
	HeightMap& hm = db.mHeightMap;
	int isize = hm.GetGridSize();
	outputnormals.resize( isize*isize );
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	heightfield_compute_buffer& ComputeBuffer = HeightMapGPGPUComputeBuffer();
	//////////////////////////////////////////////////////////
	lev2::CaptureBuffer& heightbuf = ComputeBuffer.tex1buffer;
	lev2::Texture& heighttex = ComputeBuffer.tex1;
	fvec4* pv4 = const_cast<fvec4*>( static_cast<const fvec4*>( heightbuf.GetData()  ));
	float* pf = (float*) db.mHeightMap.GetHeightData();
	for( int iz=0; iz<isize; iz++ )
	{	int iaddr = heightbuf.CalcDataIndex(0,iz);
		for( int ix=0; ix<isize; ix++ )
		{	float fx = float(ix)/float(isize);
			fvec3 xyz = mod.XYZ( ix, iz );
			pv4[iaddr++] = xyz; 
		}
	}
	//////////////////////////////////////////////////////////
	lev2::GfxMaterial3DSolid matsolid( ComputeBuffer.GetContext(), "miniorkshader://heightmap_edit", "computenormals" );
	matsolid.SetTexture( & heighttex );
	float texw = float(heighttex.GetWidth());
	float itexw = 1.0f / float( heighttex.GetWidth() );
	fcolor4 mc( float(isize), float(isize), texw, itexw );
	lev2::CaptureBuffer& MyCaptureBuffer = ComputeBuffer.MyCaptureBuffer;
	GpGpuTask( fcolor4::Blue(), mc, matsolid, isize, isize, ComputeBuffer );
	ComputeBuffer.GetContext()->Capture( MyCaptureBuffer );
	//////////////////////////////////////////////////////////
	const fvec4* VecBuffer = (const fvec4*) MyCaptureBuffer.GetData();
	for( int iZ=0; iZ<isize; iZ++ )
	{	for( int iX=0; iX<isize; iX++ )
		{	int index = MyCaptureBuffer.CalcDataIndex( iX, iZ );
			int index_n = (iZ*isize)+iX;
			const fvec4& v = VecBuffer[ index ];
			outputnormals[index_n] = v.xyz();
		}
	}
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
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
void ComputeColorsGpu(HeightMap_datablock& db, const orkvector<fvec3>& normals , orkvector<U32>& outputcolors, lev2::Texture* lightenvtex )
{
	#if 0
	int isize = db.mHeightMap.GetGridSize();
	outputcolors.resize( isize*isize );
	const int kWUW = isize;
	const int kWUH = isize;
	const int BUFW = isize;
	const int BUFH = isize;

	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	{
	
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		
		#if 1
		ork::lev2::GfxBuffer ShadowBuffer( 0, 0, 0, 1024, 1024, ork::lev2::EBUFFMT_RGBA128, lev2::ETGTTYPE_EXTBUFFER );
		ShadowBuffer.CreateContext();

		if( 0 == (BUFW*BUFH) )
		{
			lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
			return;
		}
		////////////////////////
		// fill in vertex buffer
		////////////////////////

		ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12C4T16> HFVBuf( BUFW*BUFH, 0, ork::lev2::EPRIM_TRIANGLES );

		ShadowBuffer.GetContext()->VtxBuf_Lock( HFVBuf );

		for( int iz=0; iz<BUFH; iz++ )
		{	for( int ix=0; ix<BUFW; ix++ )
			{	int index_n = (iz*isize)+ix;
				fvec3 xyz = db.mHeightMap.XYZ(ix,iz);
				const fvec3& normal = normals[index_n];
				fvec2 nxz( normal.GetX(), normal.GetZ() );
				HFVBuf.AddVertex(ork::lev2::SVtxV12C4T8(xyz, nxz));
			}
		}

		ShadowBuffer.GetContext()->VtxBuf_UnLock( HFVBuf );

		////////////////////////
		// fill in index buffer
		////////////////////////

		ork::lev2::StaticIndexBuffer<U32> Indices( BUFW*BUFH*6 );
		U32* pu32 = (U32*) ShadowBuffer.GetContext()->IdxBuf_Lock( Indices );

		int icounter = 0;
		for( int iz0=0; iz0<(BUFH-1); iz0++ )
		{	int iz1 = iz0+1;
			for( int ix0=0; ix0<(BUFW-1); ix0++ )
			{	int ix1 = ix0+1;
				int index0 = (iz0*BUFW)+ix0;
				int index1 = (iz0*BUFW)+ix1;
				int index2 = (iz1*BUFW)+ix1;
				int index3 = (iz1*BUFW)+ix0;

				pu32[icounter++] = index0;
				pu32[icounter++] = index1;
				pu32[icounter++] = index2;

				pu32[icounter++] = index0;
				pu32[icounter++] = index2;
				pu32[icounter++] = index3;

			}
		}
		ShadowBuffer.GetContext()->IdxBuf_UnLock( Indices );

		const int knumpasses = 96;

		heightfield_compute_buffer* WriteBuffer = & GetComputeWrite();
		heightfield_compute_buffer* ReadBuffer = & GetComputeRead();
		
		///////////////////////////////////////////////////////////////////
		// clear read buffer
		///////////////////////////////////////////////////////////////////
		
		ReadBuffer->GetContext()->SetClearColor( ork::fcolor4::Black() );
		ReadBuffer->GetContext()->BeginFrame();
		ReadBuffer->GetContext()->PushPMatrix( fmtx4::Identity );
		ReadBuffer->GetContext()->PushVMatrix( fmtx4::Identity );
		ReadBuffer->GetContext()->PushMMatrix( fmtx4::Identity );
		{
		}
		ReadBuffer->GetContext()->PopPMatrix();
		ReadBuffer->GetContext()->PopVMatrix();
		ReadBuffer->GetContext()->PopMMatrix();
		ReadBuffer->GetContext()->EndFrame();

		///////////////////////////////////////////////////////////////////

		fvec4* pv4 = const_cast<fvec4*>( static_cast<const fvec4*>( WriteBuffer->tex1buffer.GetData()  ));
		for( int iz=0; iz<BUFH; iz++ )
		{	int iaddr = WriteBuffer->tex1buffer.CalcDataIndex(0,iz);
			for( int ix=0; ix<BUFW; ix++ )
			{	int index_n = (iz*isize)+ix;
				const fvec3& normal = normals[index_n];
				fvec3 xyz = db.mHeightMap.XYZ(ix,iz);
				pv4[iaddr++] = xyz;
			}
		}

		for( int ipass=0; ipass<knumpasses; ipass++ )
		{
			orkprintf( "AOPASS<%d> of <%d>\n", ipass, knumpasses );
			///////////////////////////////////////
			// generate shadow (depthmap) buffer 
			///////////////////////////////////////

			int ix = (rand()%1024)-512;
			int iy = (rand()%512);
			int iz = (rand()%1024)-512;

			if( ix==0 ) ix=1;
			if( iz==0 ) iz=1;

			float fx = float(ix)/512.0f;
			float fy = float(iy)/512.0f;
			float fz = float(iz)/512.0f;

			fvec3 vdir = fvec3( fx, fy, fz ).Normal();
			fvec3 vsid = vdir.Cross(fvec3::Green());
			fvec3 vup = vsid.Cross(vdir);

			fvec3 veye = vdir*2000.0f;

			////////////
			ork::CameraData cdata;
			cdata.BindGfxTarget( ShadowBuffer.GetContext() );
			cdata.Lookat( veye, ork::fvec3::Black(), vup, 1.0f, 3000.0f, 70.0f );
			cdata.CalcCameraData();

			ShadowBuffer.GetContext()->SetClearColor( ork::fcolor4::Black() );
			ShadowBuffer.GetContext()->BeginFrame();
			ShadowBuffer.GetContext()->PushPMatrix( cdata.GetPMatrix() );
			ShadowBuffer.GetContext()->PushVMatrix( cdata.GetVMatrix() );
			ShadowBuffer.GetContext()->PushMMatrix( fmtx4::Identity );
			{
				lev2::GfxMaterial3DSolid ShadowGenMaterial( ShadowBuffer.GetContext(), "miniorkshader://heightmap_edit", "depthcolor" );
				ShadowGenMaterial.mRasterState.SetCullTest( ork::lev2::ECULLTEST_OFF );
				ShadowGenMaterial.mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_LEQUALS );
				ShadowBuffer.GetContext()->BindMaterial( & ShadowGenMaterial );
				ShadowBuffer.GetContext()->DrawIndexedPrimitive( HFVBuf, Indices, ork::lev2::EPRIM_TRIANGLES );
			}
			ShadowBuffer.GetContext()->PopPMatrix();
			ShadowBuffer.GetContext()->PopVMatrix();
			ShadowBuffer.GetContext()->PopMMatrix();
			ShadowBuffer.GetContext()->EndFrame();

			//ShadowBuffer.GetContext()->Capture( "shadowout.dds" );

			//////////////////////////////////////////////////////////

			lev2::GfxMaterial3DSolid matsolid( WriteBuffer->GetContext(), "miniorkshader://heightmap_edit", "shadowrecv" );
			matsolid.SetTexture( & WriteBuffer->tex1 );
			matsolid.SetTexture2( ShadowBuffer.mpTexture );
			matsolid.SetTexture3( lightenvtex );
			matsolid.SetTexture4( ReadBuffer->mpTexture );
			matsolid.SetAuxMatrix( cdata.GetVMatrix()*cdata.GetPMatrix() );
			float texw = float(WriteBuffer->tex1.GetWidth());
			float itexw = 1.0f / float( WriteBuffer->tex1.GetWidth() );
			fcolor4 mc( float(isize), float(isize), texw, itexw );
			GpGpuTask( fcolor4::Black(), mc, matsolid, isize, isize, *WriteBuffer );
			//WriteBuffer->GetContext()->Capture( "yoout.dds" );

			WriteBuffer = & GetComputeWrite();
			ReadBuffer = & GetComputeRead();

		}
		ReadBuffer->GetContext()->Capture( ReadBuffer->MyCaptureBuffer );
		//////////////////////////////////////////////////////////
		const fvec4* VecBuffer = (const fvec4*) ReadBuffer->MyCaptureBuffer.GetData();
		for( int iZ=0; iZ<BUFW; iZ++ )
		{	for( int iX=0; iX<BUFH; iX++ )
			{	int index_n = (iZ*isize)+iX;
				int index = ReadBuffer->MyCaptureBuffer.CalcDataIndex( iX, iZ );
				const fvec4& v = VecBuffer[ index ];
				outputcolors[index_n] = (v.xyz()*1.0/float(knumpasses)).GetRGBAU32();
			}
		}
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
	
		#else

		fvec4* pv4 = const_cast<fvec4*>( static_cast<const fvec4*>( ComputeBuffer.tex1buffer.GetData()  ));
		for( int iz=0; iz<BUFH; iz++ )
		{	int iaddr = ComputeBuffer.tex1buffer.CalcDataIndex(0,iz);
			for( int ix=0; ix<BUFW; ix++ )
			{	int index_n = (iz*isize)+ix;
				const fvec3& normal = normals[index_n];
				fvec3 xyz = db.mHeightMap.XYZ(ix,iz);
				pv4[iaddr++] = xyz; //normal; 
			}
		}
		//////////////////////////////////////////////////////////
		lev2::GfxMaterial3DSolid matsolid( ComputeBuffer.GetContext(), "miniorkshader://heightmap_edit", "aohmshade" );
		matsolid.SetTexture( & ComputeBuffer.tex1 );
		matsolid.SetTexture2( lightenvtex );
		float texw = float(ComputeBuffer.tex1.GetWidth());
		float itexw = 1.0f / float( ComputeBuffer.tex1.GetWidth() );
		fcolor4 mc( float(isize), float(isize), texw, itexw );
		GpGpuTask( fcolor4::Black(), mc, matsolid, isize, isize, ComputeBuffer );
		ComputeBuffer.GetContext()->Capture( ComputeBuffer.MyCaptureBuffer );
		//////////////////////////////////////////////////////////
		const fvec4* VecBuffer = (const fvec4*) ComputeBuffer.MyCaptureBuffer.GetData();
		for( int iZ=0; iZ<BUFW; iZ++ )
		{	for( int iX=0; iX<BUFH; iX++ )
			{	int index_n = (iZ*isize)+iX;
				int index = ComputeBuffer.MyCaptureBuffer.CalcDataIndex( iX, iZ );
				const fvec4& v = VecBuffer[ index ];
				outputcolors[index_n] = v.GetRGBAU32();
			}
		}

		#endif

		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////
	}
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
	#endif
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::ReadSurface( const fvec3& xyz, fvec3& pos, fvec3& nrm ) const
{
	float fiterX, fiterZ;
	bool bOK = HeightMapData().CalcClosestAddress( xyz, fiterX, fiterZ );
	int iterX = int(fiterX);
	int iterZ = int(fiterZ);

	int igsiz = HeightMapData().GetGridSize()-2;

	//////////////////////////////////////////
	nrm = fvec3(0.0f,1.0f,0.0f);
	pos = fvec3(xyz.GetX(),xyz.GetY()+50.0f,xyz.GetZ());
	//////////////////////////////////////////
	if( false == bOK )
	{
		if( fiterX==-1.0f )
		{
			nrm = fvec3(1.0f,0.0f,0.0f);
		}
		else if( fiterX==-2.0f)
		{
			nrm = fvec3(-1.0f,0.0f,0.0f);
		}
		//////////////////////////////////////////
		if( fiterZ==-1.0f )
		{
			nrm = fvec3(0.0f,0.0f,1.0f);
		}
		else if( fiterZ==-2.0f)
		{
			nrm = fvec3(0.0f,0.0f,-1.0f);
		}
	}
	//////////////////////////////////////////
	else if( iterX >= 2 && iterZ >= 2 && iterX < igsiz && iterZ < igsiz )
	//////////////////////////////////////////
	{
		fvec3 terp_xyz[4];
		fvec3 terp_nrm[4];
		for( int is=0; is<4; is++ )
		{
			int isx = is&1;
			int isz = is>>1;
			terp_xyz[is] = XYZ( iterX+isx, iterZ+isz );
			terp_nrm[is] = Normal( iterX+isx, iterZ+isz );
		}
		
		float flerpx = fiterX-float(iterX);
		float flerpz = fiterZ-float(iterZ);

		fvec3 pza;	pza.Lerp( terp_xyz[0],terp_xyz[2], flerpz );
		fvec3 pzb;	pzb.Lerp( terp_xyz[1],terp_xyz[3], flerpz );

		pos.Lerp( pza, pzb, flerpx );

		fvec3 nra;	nra.Lerp( terp_nrm[0],terp_nrm[2], flerpz );
		fvec3 nrb;	nrb.Lerp( terp_nrm[1],terp_nrm[3], flerpz );

		nrm.Lerp( nra, nrb, flerpx );

		nrm.Normalize();
	}

	nrm = nrm*-1.0f;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::SaveNormalsToTexture( const file::Path& filename ) const
{
	#if 0
	const float* pdata = 0; //hm.GetData();
	// D3DFMT_A32B32G32R32F D3DFMT_R32F
	LPDIRECT3DTEXTURE9 dxtex = 0;
	HRESULT hr = D3DXCreateTexture( ork::MeshUtil::GetNullD3dDevice(),
									miSize, miSize, 
									1, 0,
									D3DFMT_R8G8B8,
									D3DPOOL_SYSTEMMEM,
									& dxtex );
	D3DLOCKED_RECT d3dlr;
	hr = dxtex->LockRect( 0, &d3dlr, 0, 0 );
	OrkAssert( SUCCEEDED( hr ) );
	U8* pDst = (U8*) d3dlr.pBits;
	for( int iz=0; iz<miSize; iz++ )
	{
		for( int ix=0; ix<miSize; ix++ )
		{
			const fvec3& Normal = this->Normal( ix, iz );
			int ipix = (iz*miSize)+ix;

			U8* ppix = pDst+(ipix*3);

			float fx = (Normal.GetX()*1.0f)*255.0f;
			float fy = (Normal.GetY()*1.0f)*255.0f;
			float fz = (Normal.GetZ()*1.0f)*255.0f;

			ppix[0] = u8(fx);
			ppix[1] = u8(fy);
			ppix[2] = u8(fz);

		}
	}
	hr = dxtex->UnlockRect (0);
	OrkAssert( SUCCEEDED( hr ) );
	hr = D3DXSaveTextureToFile( filename.ToAbsolute().c_str(), D3DXIFF_TGA, dxtex, 0 ); // D3DXIFF_HDR
	OrkAssert( SUCCEEDED( hr ) );
	dxtex->Release();
	#endif

}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::SaveColorsToTexture( const file::Path& filename ) const
{
#if 0
	heightfield_compute_buffer& ComputeBuffer = HeightMapGPGPUComputeBuffer();
	if( mpColorMapTexture )
	{
		ComputeBuffer.GetContext()->SaveTexture( filename, mpColorMapTexture );
	}
#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::SaveLightingToTexture( const file::Path& filename ) const
{
	#if 0
	const float* pdata = 0; //hm.GetData();
	// D3DFMT_A32B32G32R32F D3DFMT_R32F
	LPDIRECT3DTEXTURE9 dxtex = 0;
	HRESULT hr = D3DXCreateTexture( ork::MeshUtil::GetNullD3dDevice(),
									miSize, miSize, 
									1, 0,
									D3DFMT_R8G8B8,
									D3DPOOL_SYSTEMMEM,
									& dxtex );
	D3DLOCKED_RECT d3dlr;
	hr = dxtex->LockRect( 0, &d3dlr, 0, 0 );
	OrkAssert( SUCCEEDED( hr ) );
	U8* pDst = (U8*) d3dlr.pBits;
	for( int iz=0; iz<miSize; iz++ )
	{
		for( int ix=0; ix<miSize; ix++ )
		{
			const fvec3& Normal = this->Color( ix, iz );
			int ipix = (iz*miSize)+ix;

			U8* ppix = pDst+(ipix*3);

			float fx = (Normal.GetX()*1.0f)*255.0f;
			float fy = (Normal.GetY()*1.0f)*255.0f;
			float fz = (Normal.GetZ()*1.0f)*255.0f;

			ppix[0] = u8(fz);
			ppix[1] = u8(fy);
			ppix[2] = u8(fx);

		}
	}
	hr = dxtex->UnlockRect (0);
	OrkAssert( SUCCEEDED( hr ) );
	hr = D3DXSaveTextureToFile( filename.ToAbsolute().c_str(), D3DXIFF_TGA, dxtex, 0 ); // D3DXIFF_HDR
	OrkAssert( SUCCEEDED( hr ) );
	dxtex->Release();
	#endif

}
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::SaveHeightToTexture( const file::Path& filename ) const
{
	#if 0
	const float* pdata = 0; //hm.GetData();
	// D3DFMT_A32B32G32R32F D3DFMT_R32F
	LPDIRECT3DTEXTURE9 dxtex = 0;
	HRESULT hr = D3DXCreateTexture( ork::MeshUtil::GetNullD3dDevice(),
									miSize, miSize, 
									1, 0,
									D3DFMT_R32F,
									D3DPOOL_SYSTEMMEM,
									& dxtex );
	D3DLOCKED_RECT d3dlr;
	hr = dxtex->LockRect( 0, &d3dlr, 0, 0 );
	OrkAssert( SUCCEEDED( hr ) );
	float* pDst = (float*) d3dlr.pBits;
	float fmin = Float::TypeMax();
	float fmax = -Float::TypeMax();
	for( int iz=0; iz<miSize; iz++ )
	{
		for( int ix=0; ix<miSize; ix++ )
		{
			const fvec3& XYZ = this->XYZ( ix, iz );
			float fy = XYZ.GetY();
			if( fy > fmax ) fmax = fy;
			if( fy < fmin ) fmin = fy;
		}
	}
	float frange = (fmax-fmin);

	for( int iz=0; iz<miSize; iz++ )
	{
		for( int ix=0; ix<miSize; ix++ )
		{
			const fvec3& XYZ = this->XYZ( ix, iz );
			int ipix = (iz*miSize)+ix;
			float fy = XYZ.GetY();
			*(pDst+ipix) = (fy-fmin)/frange;
		}
	}
	hr = dxtex->UnlockRect (0);
	OrkAssert( SUCCEEDED( hr ) );
	hr = D3DXSaveTextureToFile( filename.ToAbsolute().c_str(), D3DXIFF_DDS, dxtex, 0 ); // D3DXIFF_HDR
	OrkAssert( SUCCEEDED( hr ) );
	dxtex->Release();
	#endif

}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void hmap_hfield_module::SetLightEnvTexture(lev2::Texture*ptex)
{
	mpLightEnvTexture=ptex;
	mHeightOutputPlug.SetDirty(true);
}
void hmap_hfield_module::SetColorMapTexture(lev2::Texture*ptex)
{
	mpColorMapTexture=ptex;
	mHeightOutputPlug.SetDirty(true);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}}

#endif