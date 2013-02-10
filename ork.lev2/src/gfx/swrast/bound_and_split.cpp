////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include "lev3_test.h"
#include <math.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/Array.hpp>
#include <ork/math/collision_test.h>
#include <ork/math/sphere.h>
#include <ork/math/plane.hpp>
#include <ork/math/line.h>
#include "render_graph.h"
#include <IL/il.h>
#include <IL/ilut.h>

bool boxisect(	float fax0, float fay0, float fax1, float fay1,
				float fbx0, float fby0, float fbx1, float fby1 );

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BoundAndSplitModule::BoundAndSplitModule(const RenderData& rdata,const TransformAndClipModule&tac)
	: mSourceHash(0)
	, mRenderData(rdata)
	, mTAC(tac)
{

	//OrkAssert( kAABUFTILES == ork::threadpool::NumTHreads );

	int inumpixpertile = RenderData::kTileDim*RenderData::kTileDim*rdata.miAADim2d;

	for( int i=0; i<kAABUFTILES; i++ )
	{
		mAABufTiles[i].mColorBuffer = new u32[ inumpixpertile ];
		mAABufTiles[i].mDepthBuffer = new f32[ inumpixpertile ];
		mAABufTiles[i].mFragmentBuffer = new rend_fraglist[ inumpixpertile ];

//		mAABufTiles[i].InitCl( rdata.mClEngine );
	}
}
///////////////////////////////////////////////////////////////////////////////
struct BoundAndSplitModuleWuData
{
	int	miTileX;
	int	miTileY;
};
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::do_divide(ork::threadpool::thread_pool* tpool)
{

	int inumtiles = (mRenderData.miNumTilesH*mRenderData.miNumTilesW);
	for( int ih=0; ih<mRenderData.miNumTilesH; ih++ )
	{
		for( int iw=0; iw<mRenderData.miNumTilesW; iw++ )
		{
			BoundAndSplitModuleWuData std;
			std.miTileX = iw;
			std.miTileY = ih;
			ork::threadpool::sub_task* psub = new ork::threadpool::sub_task(this);
			psub->SetData<BoundAndSplitModuleWuData>(std);
			tpool->AddSubTask(psub);
		}
	}
	IncNumTasks(inumtiles);

}
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::do_onstarted()
{
}
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::do_subtask_finished( const ork::threadpool::sub_task* tsk )
{
	delete tsk;
}
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::do_onfinished()
{
//	const ork::RgmModel* pmodel = mRenderData.mpModel;
//	void* hash = (void*) pmodel;
//	mSourceHash = hash;
	static int iframe = 0;
	printf( "iframe<%d>\n", iframe );
	iframe++;
}
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::do_process( const ork::threadpool::sub_task* tsk, const ork::threadpool::thread_pool_worker* ptpw )
{
	const BoundAndSplitModuleWuData& subtaskdata = tsk->GetData<BoundAndSplitModuleWuData>();

	const RasterTile& tile = mRenderData.GetTile(subtaskdata.miTileX,subtaskdata.miTileY);
	int itw = tile.miWidth;
	int ith = tile.miHeight;
	int itx = tile.miScreenXBase;
	int ity = tile.miScreenYBase;
	const ork::CPlane& topplane = tile.mFrustum.mTopPlane;
	const ork::CPlane& bottomplane = tile.mFrustum.mBottomPlane;
	const ork::CPlane& nearplane = tile.mFrustum.mNearPlane;
	const ork::CPlane& farplane = tile.mFrustum.mFarPlane;
	const ork::CPlane& leftplane = tile.mFrustum.mLeftPlane;
	const ork::CPlane& rightplane = tile.mFrustum.mRightPlane;

	int imgW = mRenderData.miImageWidth;

	/////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////
	// debug clear the tile
	/////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////

	const int gicounter = ptpw->GetIndex();

	int ishift = (0==((gicounter&8)>>3));
	u8 ucr = (gicounter&1)<<(ishift+5);
	u8 ucg = (gicounter&2)<<(ishift+4);
	u8 ucb = (gicounter&4)<<(ishift+3);
	u32 upix = (ucb<<0)|u32(ucg<<8)|u32(ucr<<16);
	ork::CVector3 clear_color;//( float(ucr)/255.0f, float(ucg)/255.0f, float(ucb)/255.0f );
	int tilaabufidx = gicounter%kAABUFTILES;
	AABuffer& aabuf = mAABufTiles[ tilaabufidx ];

	for( int iy=0; iy<mRenderData.miAATileDim; iy++ )
	{
		for( int ix=0; ix<mRenderData.miAATileDim; ix++ )
		{
			int ipixidxAA = mRenderData.CalcAAPixelAddress(ix,iy);
			aabuf.mColorBuffer[ipixidxAA] = upix;
			aabuf.mDepthBuffer[ipixidxAA] = 1.0e30f;
			aabuf.mFragmentBuffer[ipixidxAA].Reset();
		}
	}

	for( int iy=0; iy<ith; iy++ )
	{
		int iabsy = (ity+iy);

		for( int ix=0; ix<itw; ix++ )
		{
			int iabsx = (itx+ix);
			int ipixidx = mRenderData.CalcPixelAddress(iabsx,iabsy);
			mRenderData.mPixelData[ipixidx] = upix;
		}
	}

	/////////////////////////////////////////////////////////
	///////////////////////////////////
	// BLOCKING WRITE TO OPENCL BUFFER
	//mRenderData.mClEngine.GetDevice()->Lock();
	//aabuf.mTriangleClBuffer->EnqueueWrite(mRenderData.mClEngine.GetDevice(), itribufferidx*sizeof(float));
	//mRenderData.mClEngine.GetDevice()->UnLock();
	/////////////////////////////////////////////////////////
	// RASTERIZE
	/////////////////////////////////////////////////////////
	rendtri_context ctx( aabuf );
	FragmentPool& fpool = aabuf.mFragmentPool;
	fpool.Reset();
	rend_fraglist* pFBUFFER = aabuf.mFragmentBuffer;
	const rend_prefrags& pfgs = aabuf.mPreFrags;
	/////////////////////////////////////////////////////////

	int ibucketX = (itx/RenderData::kTileDim);
	int ibucketY = (ity/RenderData::kTileDim);
	int ibucket = mRenderData.GetBucketIndex(ibucketX,ibucketY);

	const orkmap<const rend_shader*,TriangleMetaBucket*>& metabucketmap = mTAC.GetMetaBuckets();

	for( orkmap<const rend_shader*,TriangleMetaBucket*>::const_iterator itshader=metabucketmap.begin(); itshader!=metabucketmap.end(); itshader++ )
	{
		const rend_shader* pshader = itshader->first;
		const TriangleMetaBucket* pmetabucket = itshader->second;
		const TriangleBucket& pucket = pmetabucket->mBuckets[ ibucket ];	

		const orkvector<rend_triangle*>& tris = pucket.mPostTransformTriangles;
		int inumtri = pucket.mTriangleIndex.Fetch();

		/////////////////////////////////////////////////////////
		// rasterize/shade
		/////////////////////////////////////////////////////////
		int itribase = 0;
		const int knumtri = inumtri;
		int ifragmentindex = 0;
		while( inumtri )
		{	const int ktriblocksize = 0x8000;
			int itx = (inumtri>ktriblocksize) ? ktriblocksize : inumtri;
			int itri = itribase;

			////////////////////////////////////////////////
			// CL put all source triangles into CL tri-buffer here
			// screen space positions, obj and wld space normals, uv's (anything interpolated)
			// probably if possible should use the shared vertex paradigm
			////////////////////////////////////////////////

			float* ptribuffer = (float*) aabuf.mTriangleClBuffer->GetBufferRegion();
		
			/////////////////////////////////
			// rasterize (scan convert triangles)	
			/////////////////////////////////
			aabuf.mPreFrags.Reset();
			int itribufferidx = 0;
			for( int it=0; it<itx; it++ )
			{
				const rend_triangle& tri = *tris[itri++];
				RasterizeTri( ctx, tri, tile, it );
				const rend_ivtx& v0 = tri.mSVerts[0];
				const rend_ivtx& v1 = tri.mSVerts[1];
				const rend_ivtx& v2 = tri.mSVerts[2];
				ptribuffer[itribufferidx++] = v0.mObjSpaceNrm.GetX();
				ptribuffer[itribufferidx++] = v0.mObjSpaceNrm.GetY();
				ptribuffer[itribufferidx++] = v0.mObjSpaceNrm.GetZ();
				ptribuffer[itribufferidx++] = v1.mObjSpaceNrm.GetX();
				ptribuffer[itribufferidx++] = v1.mObjSpaceNrm.GetY();
				ptribuffer[itribufferidx++] = v1.mObjSpaceNrm.GetZ();
				ptribuffer[itribufferidx++] = v2.mObjSpaceNrm.GetX();
				ptribuffer[itribufferidx++] = v2.mObjSpaceNrm.GetY();
				ptribuffer[itribufferidx++] = v2.mObjSpaceNrm.GetZ();

				ptribuffer[itribufferidx++] = v0.mWldSpaceNrm.GetX();
				ptribuffer[itribufferidx++] = v0.mWldSpaceNrm.GetY();
				ptribuffer[itribufferidx++] = v0.mWldSpaceNrm.GetZ();
				ptribuffer[itribufferidx++] = v1.mWldSpaceNrm.GetX();
				ptribuffer[itribufferidx++] = v1.mWldSpaceNrm.GetY();
				ptribuffer[itribufferidx++] = v1.mWldSpaceNrm.GetZ();
				ptribuffer[itribufferidx++] = v2.mWldSpaceNrm.GetX();
				ptribuffer[itribufferidx++] = v2.mWldSpaceNrm.GetY();
				ptribuffer[itribufferidx++] = v2.mWldSpaceNrm.GetZ();

				ptribuffer[itribufferidx++] = v0.mWldSpacePos.GetX();
				ptribuffer[itribufferidx++] = v0.mWldSpacePos.GetY();
				ptribuffer[itribufferidx++] = v0.mWldSpacePos.GetZ();
				ptribuffer[itribufferidx++] = v1.mWldSpacePos.GetX();
				ptribuffer[itribufferidx++] = v1.mWldSpacePos.GetY();
				ptribuffer[itribufferidx++] = v1.mWldSpacePos.GetZ();
				ptribuffer[itribufferidx++] = v2.mWldSpacePos.GetX();
				ptribuffer[itribufferidx++] = v2.mWldSpacePos.GetY();
				ptribuffer[itribufferidx++] = v2.mWldSpacePos.GetZ();

				ptribuffer[itribufferidx++] = v0.mObjSpacePos.GetX();
				ptribuffer[itribufferidx++] = v0.mObjSpacePos.GetY();
				ptribuffer[itribufferidx++] = v0.mObjSpacePos.GetZ();
				ptribuffer[itribufferidx++] = v1.mObjSpacePos.GetX();
				ptribuffer[itribufferidx++] = v1.mObjSpacePos.GetY();
				ptribuffer[itribufferidx++] = v1.mObjSpacePos.GetZ();
				ptribuffer[itribufferidx++] = v2.mObjSpacePos.GetX();
				ptribuffer[itribufferidx++] = v2.mObjSpacePos.GetY();
				ptribuffer[itribufferidx++] = v2.mObjSpacePos.GetZ();
			}
			itribase += itx;
			inumtri -= itx;
			/////////////////////////////////////////////////////////
			// shade fragments in per material blocks
			//  the block method will allow shading by GP/GPU
			/////////////////////////////////////////////////////////
			const int knumfrag = pfgs.miNumPreFrags;
			int i=0;
			int ipfragbase = 0;
			while( i<knumfrag )
			{	
				int iremaining = knumfrag-i;
				int icount = (iremaining>=AABuffer::kfragallocsize) 
								? AABuffer::kfragallocsize
								: iremaining;

				OrkAssert( ifragmentindex+icount < AABuffer::kfragallocsize );
				bool bOK = fpool.AllocFragments( aabuf.mpFragments+ifragmentindex, icount );
				///////////////////////////////////////////////
				// CL shadeblock will write a CL fragment buffer, call a shader kernel, and read the resulting colorz fragments
				pshader->ShadeBlock( aabuf, ipfragbase, icount, itx );
				///////////
				for( int j=0; j<icount; j++ )
				{	const rend_prefragment& pfrag = pfgs.mPreFrags[ipfragbase++];
					rend_fragment* frag = aabuf.mpFragments[ifragmentindex++];
					pFBUFFER[pfrag.miPixIdxAA].AddFragment( frag );
				}
				i += icount;
				ipfragbase += icount;
			}
		}
	}
	/////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////
	// AA resolve
	/////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////

	int imul = mRenderData.miAADim1d;
	int idiv = mRenderData.miAADim2d;

	float fdepthcomplexitysum = 0.0f;
	float fdepthcomplexityctr = 0.0f;

	//FragmentCompositorREYES& sorter = ctx.mAABuffer.mCompositorREYES;
	FragmentCompositorZBuffer& sorter = ctx.mAABuffer.mCompositorZB;
	//

	//sorter.miThreadID = gicounter;

	//FragmentCompositorZBuffer& sorter = ctx.mAABuffer.mCompositorZB;
	sorter.Reset();
	for( int iy=0; iy<ith; iy++ )
	{
		int iabsy = (ity+iy);
		int iy2 = iy*imul;

		for( int ix=0; ix<itw; ix++ )
		{
			int ix2 = ix*imul;
			u32 uR=0;
			u32 uG=0;
			u32 uB=0;
			u32 uA=0;
			for( int iaay=0; iaay<imul; iaay++ )
			for( int iaax=0; iaax<imul; iaax++ )
			{	int iaddress = mRenderData.CalcAAPixelAddress(ix2+iaax,iy2+iaay);
				////////////////////////////////////////////////////////////
				sorter.Reset();
				ctx.mAABuffer.mFragmentBuffer[ iaddress ].Visit( sorter );
				u32 upA = sorter.Composite(clear_color).GetRGBAU32();
				////////////////////////////////////////////////////////////
				uA += ((upA)&0xff);
				uB += ((upA>>8)&0xff)>>1;
				uG += ((upA>>16)&0xff)>>1;
				uR += ((upA>>24)&0xff)>>1;
			}
			////////////////////////////////////////////////////////////
			// avg samples
			////////////////////////////////////////////////////////////
			uR = uR/idiv;
			uG = uG/idiv;
			uB = uB/idiv;
			uA = uA/idiv;
			U32 upix = (uR)|(uG<<8)|(uB<<16)|(uA<<24);
			////////////////////////////////////////////////////////////
			// store resolved pixel to framebuffer
			////////////////////////////////////////////////////////////
			int iabsx = (itx+ix);
			int ipixidx = mRenderData.CalcPixelAddress(iabsx,iabsy);
			mRenderData.mPixelData[ipixidx] = upix;
			////////////////////////////////////////////////////////////
		}
	}
	/////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::RasterizeTri( const rendtri_context& ctx, const rend_triangle& tri, const RasterTile& tile, int itri )
{
	int itw = tile.miWidth;
	int ith = tile.miHeight;
	int itx0 = tile.miScreenXBase;
	int ity0 = tile.miScreenYBase;
	int itx1 = itx0+itw;
	int ity1 = ity0+ith;
	////////////////////////////////
	const rend_ivtx& rvtx0 = tri.mSVerts[0];
	const rend_ivtx& rvtx1 = tri.mSVerts[1];
	const rend_ivtx& rvtx2 = tri.mSVerts[2];
	////////////////////////////////
	const ork::CVector3& n = tri.mFaceNormal;
	ork::CVector3 un = (n*0.5f)+ork::CVector3(0.5f,0.5f,0.5f);
	U32 unc = un.GetBGRAU32();
	////////////////////////////////
	int order[3];
	////////////////////////////////
	order[0] =	(rvtx0.mSY<rvtx1.mSY)	// min Y	
					?	((rvtx0.mSY<rvtx2.mSY)	? 0 : 2)
					:	((rvtx1.mSY<rvtx2.mSY)	? 1 : 2);
	////////////////////////////////
	order[2] =	(rvtx0.mSY>rvtx1.mSY)	// max Y
					?	((rvtx0.mSY>rvtx2.mSY)	? 0 : 2)
					:	((rvtx1.mSY>rvtx2.mSY)	? 1 : 2);
	////////////////////////////////
	order[1] = 3 - (order[0] + order[2]); 
	////////////////////////////////
	const rend_ivtx& vtxA = tri.mSVerts[order[0]];
	const rend_ivtx& vtxB = tri.mSVerts[order[1]];
	const rend_ivtx& vtxC = tri.mSVerts[order[2]];
	////////////////////////////////
	// trivially reject 
	if( int(vtxA.mSY - vtxC.mSY) == 0 ) return; 
	////////////////////////////////
	rend_subtri stri;
	stri.mpSourcePrim = & tri;
	stri.vtxA = vtxA;
	stri.vtxB = vtxB;
	stri.vtxC = vtxC;
	stri.miIA = order[0];
	stri.miIB = order[1];
	stri.miIC = order[2];
	stri.miTri = itri;
	RasterizeSubTri( ctx, stri, tile, unc );
	//////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void BoundAndSplitModule::RasterizeSubTri( const rendtri_context& ctx, const rend_subtri& tri, const RasterTile& tile, u32 unc )
{	AABuffer& aabuf = ctx.mAABuffer;
	////////////////////////////////
	const rend_triangle* psrctri = tri.mpSourcePrim;
	const rend_shader* pshader = psrctri->mpShader;
	////////////////////////////////
	rend_prefrags& pfgs = aabuf.mPreFrags;
	//rend_prefraggroup& pfgroup = aabuf.mPreFragGroup;
	static int gisleeper = 0;
	if( gisleeper%(1<<13)==0 ) ork::msleep(1);
	gisleeper++;
	////////////////////////////////
	const rend_ivtx& srcvtxR = tri.mpSourcePrim->mSVerts[0];
	const rend_ivtx& srcvtxS = tri.mpSourcePrim->mSVerts[1];
	const rend_ivtx& srcvtxT = tri.mpSourcePrim->mSVerts[2];
	////////////////////////////////
	const rend_ivtx& vtxA = tri.vtxA;
	const rend_ivtx& vtxB = tri.vtxB;
	const rend_ivtx& vtxC = tri.vtxC;
	float dim1 = mRenderData.mfAADim1d;
	int idim1 = mRenderData.miAADim1d;
	int imod = mRenderData.miAATileDim;
	float fAAXA = vtxA.mSX*dim1;
	float fAAYA = vtxA.mSY*dim1;
	float fAAXB = vtxB.mSX*dim1;
	float fAAYB = vtxB.mSY*dim1;
	float fAAXC = vtxC.mSX*dim1;
	float fAAYC = vtxC.mSY*dim1;
	////////////////////////////////
	// guarantees
	////////////////////////////////
	// vtxA always guaranteed to be the top
	// vtxB.mSY >= vtxA.mSY
	// vtxC.mSY >= vtxB.mSY
	// vtxC.mSY > vtxA.mSY
	// no guarantees on left/right ordering
	////////////////////////////////
	int iYA = int(std::floor(fAAYA+0.5f)); // iYA, iYC, iy are in pixel coordinate
	int iYC = int(std::floor(fAAYC+0.5f)); // iYA, iYC, iy are in pixel coordinate
	OrkAssert(iYC>=iYA);
	////////////////////////////////
	// gradient set up
	////////////////////////////////
	float dyAB = (fAAYB - fAAYA); // vertical distance between A and B in fp pixel coordinates
	float dyAC = (fAAYC - fAAYA); // vertical distance between A and C in fp pixel coordinates
	float dyBC = (fAAYC - fAAYB); // vertical distance between B and C in fp pixel coordinates
	////////////////////////////////
	float dxAB = (fAAXB - fAAXA); // horizontal distance between A and B in fp pixel coordinates
	float dxAC = (fAAXC - fAAXA); // horizontal distance between A and C in fp pixel coordinates
	float dxBC = (fAAXC - fAAXB); // horizontal distance between B and C in fp pixel coordinates
	////////////////////////////////
	float dzAB = (vtxB.mfInvDepth - vtxA.mfInvDepth); // depth distance between A and B in fp pixel coordinates
	float dzAC = (vtxC.mfInvDepth - vtxA.mfInvDepth); // depth distance between A and C in fp pixel coordinates
	float dzBC = (vtxC.mfInvDepth - vtxB.mfInvDepth); // depth distance between B and C in fp pixel coordinates
	////////////////////////////////
	float drAB = (vtxB.mRoZ - vtxA.mRoZ); // depth distance between A and B in fp pixel coordinates
	float drAC = (vtxC.mRoZ - vtxA.mRoZ); // depth distance between A and C in fp pixel coordinates
	float drBC = (vtxC.mRoZ - vtxB.mRoZ); // depth distance between B and C in fp pixel coordinates
	////////////////////////////////
	float dsAB = (vtxB.mSoZ - vtxA.mSoZ); // depth distance between A and B in fp pixel coordinates
	float dsAC = (vtxC.mSoZ - vtxA.mSoZ); // depth distance between A and C in fp pixel coordinates
	float dsBC = (vtxC.mSoZ - vtxB.mSoZ); // depth distance between B and C in fp pixel coordinates
	////////////////////////////////
	float dtAB = (vtxB.mToZ - vtxA.mToZ); // depth distance between A and B in fp pixel coordinates
	float dtAC = (vtxC.mToZ - vtxA.mToZ); // depth distance between A and C in fp pixel coordinates
	float dtBC = (vtxC.mToZ - vtxB.mToZ); // depth distance between B and C in fp pixel coordinates
	////////////////////////////////
	bool bABZERO = (dyAB==0.0f); // prevent division by zero
	bool bACZERO = (dyAC==0.0f); // prevent division by zero
	bool bBCZERO = (dyBC==0.0f); // prevent division by zero
	////////////////////////////////
	float dxABdy = bABZERO ? 0.0f : dxAB / dyAB;
	float dxACdy = bACZERO ? 0.0f : dxAC / dyAC;
	float dxBCdy = bBCZERO ? 0.0f : dxBC / dyBC;
	//////////////////////////////////
	float dzABdy = bABZERO ? 0.0f : dzAB / dyAB;
	float dzACdy = bACZERO ? 0.0f : dzAC / dyAC;
	float dzBCdy = bBCZERO ? 0.0f : dzBC / dyBC;
	//////////////////////////////////
	float drABdy = bABZERO ? 0.0f : drAB / dyAB;
	float drACdy = bACZERO ? 0.0f : drAC / dyAC;
	float drBCdy = bBCZERO ? 0.0f : drBC / dyBC;
	//////////////////////////////////
	float dsABdy = bABZERO ? 0.0f : dsAB / dyAB;
	float dsACdy = bACZERO ? 0.0f : dsAC / dyAC;
	float dsBCdy = bBCZERO ? 0.0f : dsBC / dyBC;
	//////////////////////////////////
	float dtABdy = bABZERO ? 0.0f : dtAB / dyAB;
	float dtACdy = bACZERO ? 0.0f : dtAC / dyAC;
	float dtBCdy = bBCZERO ? 0.0f : dtBC / dyBC;
	//////////////////////////////////
	// raster loop
	////////////////////////////////
	int tileY0 = tile.miScreenYBase*idim1;
	int tileY1 = ((tile.miScreenYBase+tile.miHeight)*idim1);
	if( iYA<tileY0 ) iYA = tileY0;
	if( iYA>tileY1 ) iYA = tileY1;
	if( iYC<tileY0 ) iYC = tileY0;
	if( iYC>tileY1 ) iYC = tileY1;
	int tileX0 = tile.miScreenXBase*idim1;
	int tileX1 = ((tile.miScreenXBase+tile.miWidth)*idim1);
	///////////////////////////////////
	// y loop
	///////////////////////////////////
	for( int iY=iYA; iY<iYC; iY++ )
	{	int iAAY = iY%imod;
		float pixel_center_Y = float(iY)+0.5f;
		///////////////////////////////////
		// edge selection (AC always active, AB or BC depending upon y)
		///////////////////////////////////
		bool bAB = pixel_center_Y<=fAAYB;
		float yB = bAB ? fAAYA : fAAYB;
		float xB = bAB ? fAAXA : fAAXB;
		float zB = bAB ? vtxA.mfInvDepth : vtxB.mfInvDepth;
		float rB = bAB ? vtxA.mRoZ : vtxB.mRoZ;
		float sB = bAB ? vtxA.mSoZ : vtxB.mSoZ;
		float tB = bAB ? vtxA.mToZ : vtxB.mToZ;
		float dXdyB = bAB ? dxABdy : dxBCdy;
		float dZdyB = bAB ? dzABdy : dzBCdy;
		float dRdyB = bAB ? drABdy : drBCdy;
		float dSdyB = bAB ? dsABdy : dsBCdy;
		float dTdyB = bAB ? dtABdy : dtBCdy;
		///////////////////////////////////
		float dyA = pixel_center_Y-fAAYA;
		float dyB = pixel_center_Y-yB;
		///////////////////////////////////
		// calc left and right boundaries
		float fxLEFT = fAAXA + dxACdy*dyA;
		float fxRIGHT = xB + dXdyB*dyB;
		float fzLEFT = vtxA.mfInvDepth + dzACdy*dyA;
		float fzRIGHT = zB + dZdyB*dyB;
		float frLEFT = vtxA.mRoZ + drACdy*dyA;
		float frRIGHT = rB + dRdyB*dyB;
		float fsLEFT = vtxA.mSoZ + dsACdy*dyA;
		float fsRIGHT = sB + dSdyB*dyB;
		float ftLEFT = vtxA.mToZ + dtACdy*dyA;
		float ftRIGHT = tB + dTdyB*dyB;
		///////////////////////////////////
		// enforce left to right
		///////////////////////////////////
		if( fxLEFT>fxRIGHT ) 
		{	std::swap(fxLEFT,fxRIGHT);
			std::swap(fzLEFT,fzRIGHT);
			std::swap(frLEFT,frRIGHT);
			std::swap(fsLEFT,fsRIGHT);
			std::swap(ftLEFT,ftRIGHT);
		}
		int ixLEFT = int(std::floor(fxLEFT+0.5f));
		int ixRIGHT = int(std::floor(fxRIGHT+0.5f));
		///////////////////////////////////
		float fdZdX = (fzRIGHT-fzLEFT)/float(ixRIGHT-ixLEFT);
		float fdRdX = (frRIGHT-frLEFT)/float(ixRIGHT-ixLEFT);
		float fdSdX = (fsRIGHT-fsLEFT)/float(ixRIGHT-ixLEFT);
		float fdTdX = (ftRIGHT-ftLEFT)/float(ixRIGHT-ixLEFT);
		float fZ = fzLEFT; 
		float fR = frLEFT; 
		float fS = fsLEFT; 
		float fT = ftLEFT; 
		///////////////////////////////////
		ixRIGHT = ( ixRIGHT>tileX1 ) ? tileX1 : ixRIGHT;
		///////////////////////////////////
		// prestep X
		///////////////////////////////////
		float fxprestep = 0.0f;
		if( ixLEFT<tileX0 )
		{
			float fxprestep = float(tileX0-ixLEFT);
			ixLEFT = tileX0;
			fZ += fdZdX*fxprestep;
			fR += fdRdX*fxprestep;
			fS += fdSdX*fxprestep;
			fT += fdTdX*fxprestep;
		}
		///////////////////////////////////
		// X loop
		///////////////////////////////////
		for( int iX=ixLEFT; iX<ixRIGHT; iX++ )
		{	int iAAX = iX%imod;
			///////////////////////////////////////////////
			float rz = 1.0f/fZ;
			///////////////////////////////////////////////
			rend_prefragment& prefrag = pfgs.AllocPreFrag();
			prefrag.mfR = fR*rz;
			prefrag.mfS = fS*rz;
			prefrag.mfT = fT*rz;
			prefrag.mfZ = rz;
			prefrag.srcvtxR = & srcvtxR;
			prefrag.srcvtxS = & srcvtxS;
			prefrag.srcvtxT = & srcvtxT;
			prefrag.miPixIdxAA = mRenderData.CalcAAPixelAddress(iAAX,iAAY);
			prefrag.mpSrcPrimitive = psrctri;
			prefrag.miTri = tri.miTri;
			///////////////////////////////////////////////
			fZ += fdZdX;
			fR += fdRdX;
			fS += fdSdX;
			fT += fdTdX;
		}
		///////////////////////////////////
	}
}
