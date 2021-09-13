////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
#include <ork/math/raytracer.h>
#include <ork/math/plane.hpp>
#include "render_graph.h"
#include "shader_funcs.h"
//#include <orktool/filter/gfx/meshutil/meshutil.h>

///////////////////////////////////////////////////////////////////////////////

NoCLFromHostBuffer::NoCLFromHostBuffer()
{
}
NoCLFromHostBuffer::~NoCLFromHostBuffer()
{
}
NoCLToHostBuffer::NoCLToHostBuffer()
{
}
NoCLToHostBuffer::~NoCLToHostBuffer()
{
}

///////////////////////////////////////////////////////////////////////////////

GeoPrimTable::GeoPrimTable()
{
	mListIndex.Store(0);
}

///////////////////////////////////////////////////////////////////////////////

void GeoPrimTable::AddPrim( IGeoPrim* prim )
{
	int idx = (mListIndex.FetchAndIncrement())&kmaxdivs;

	///////////////////////
	// cycle thru multiple GeoPrimList's for finer grained locking
	///////////////////////

	GeoPrimList& list = mPrimLists[idx];
	list.AddPrim( prim );
}


///////////////////////////////////////////////////////////////////////////////

void GeoPrimList::AddPrim( IGeoPrim* prim )
{
	orkvector<IGeoPrim*>& prims = mPrimitives.LockForWrite();
	{
		prims.push_back(prim);
	}
	mPrimitives.UnLock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AABuffer::AABuffer()
	: mTriangleClBuffer(0)
	, mFragInpClBuffer(0)
	, mFragOutClBuffer(0)
{
}

AABuffer::~AABuffer()
{
	if( mTriangleClBuffer ) delete mTriangleClBuffer;
	if( mFragInpClBuffer ) delete mFragInpClBuffer;
	if( mFragOutClBuffer ) delete mFragOutClBuffer;
}

/*
void AABuffer::InitCl( const CLengine& eng )
{
	mTriangleClBuffer = new CLFromHostBuffer();
	mFragInpClBuffer = new CLFromHostBuffer();
	mFragOutClBuffer = new CLToHostBuffer();

	int iinptribufsize = (1<<16)*(sizeof(float)*18);
	mTriangleClBuffer->resize( iinptribufsize, eng.GetDevice() );

	int iinpfragbufsize = (1<<17)*(sizeof(float)*8);
	mFragInpClBuffer->resize( iinpfragbufsize, eng.GetDevice() );

	int ioutfragbufsize = (1<<18)*(sizeof(float)*8);
	mFragOutClBuffer->resize( ioutfragbufsize, eng.GetDevice() );


}*/

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RenderData::RenderData()
	: miNumTilesW(0)
	, miNumTilesH(0)
	, miImageWidth(0)
	, miImageHeight(0)
	, mpSrcMesh(0)
	, mPixelData(0)
	, miFrame(0)
	//, mClEngine( )
{
	miAADim1d = 1; //3;
	miAADim2d = miAADim1d*miAADim1d;
	mfAADim1d = float(miAADim1d);
	mfAADim2d = float(miAADim2d);
	miAATileDim = kTileDim*miAADim1d;
}

///////////////////////////////////////////////////////////////////////////////

void RenderData::operator=( const RenderData& oth )
{
}

///////////////////////////////////////////////////////////////////////////////

void RenderData::Update()
{
	float faspect = float(miImageWidth)/float(miImageHeight);
	mMatrixV = ork::fmtx4::Identity();
	mMatrixP = ork::fmtx4::Identity();
//	mMatrixV.LookAt( mEye+ork::fvec3(0.0f,750.0f,0.0f), mTarget, -ork::fvec3::Green() );
	mMatrixV.LookAt( mEye+ork::fvec3(0.0f,1.5f,0.0f), mTarget, -ork::fvec3::Green() );
	//mMatrixP.Perspective( 25.0f, faspect, 1500.0f, 10000.0f );
	mMatrixP.Perspective( 25.0f, faspect, 1500.0f, 10000.0f );
//	mMatrixP.Perspective( 25.0f, faspect, 0.1f, 10.0f );
	mFrustum.set( mMatrixV, mMatrixP );

	float fi = float(miFrame)/1800.0f;

	mMatrixM.SetToIdentity();
	mMatrixM.RotateY(fi*PI2);

	mMatrixMV = (mMatrixM*mMatrixV);
	mMatrixMVP = mMatrixMV*mMatrixP;

	ork::fvec3 root_topn = mFrustum._topPlane.GetNormal();
	ork::fvec3 root_botn = mFrustum._bottomPlane.GetNormal();
	ork::fvec3 root_lftn = mFrustum._leftPlane.GetNormal();
	ork::fvec3 root_rhtn = mFrustum._rightPlane.GetNormal();
	//ork::fvec3 root_topc = mFrustum.mFarCorners

	ork::fray3 Ray[4];
	for( int i=0; i<4; i++ )
	{
		Ray[i] = ork::fray3( mFrustum.mNearCorners[i], (mFrustum.mFarCorners[i]-mFrustum.mNearCorners[i]).Normal() );
	}

	/////////////////////////////////////////////////////////////////

	for( int iy=0; iy<miNumTilesH; iy++ )
	{
		/////////////////////////////////////////////////////
		// or maybe we want the pixel centers ?
		int ity = iy*kTileDim;
		int iby = ity+kTileDim; // or (kTileDim-1) ?
		float fity = float(ity)/float(miImageHeight);
		float fiby = float(iby)/float(miImageHeight);
		/////////////////////////////////////////////////////

		ork::fvec3 topn;
		ork::fvec3 botn;
		topn.lerp( root_topn, -root_botn, fity );
		botn.lerp( -root_topn, root_botn, fiby );
		topn.Normalize();
		botn.Normalize();

		/////////////////////////////////////////////////

		ork::fvec3 LN[2];
		ork::fvec3 RN[2];
		ork::fvec3 LF[2];
		ork::fvec3 RF[2];

		LN[0].lerp( mFrustum.mNearCorners[0], mFrustum.mNearCorners[3], fity );	// left near top
		LN[1].lerp( mFrustum.mNearCorners[0], mFrustum.mNearCorners[3], fiby );	// left near bot
		RN[0].lerp( mFrustum.mNearCorners[1], mFrustum.mNearCorners[2], fity );	// right near top
		RN[1].lerp( mFrustum.mNearCorners[1], mFrustum.mNearCorners[2], fiby );	// right near bot

		LF[0].lerp( mFrustum.mFarCorners[0], mFrustum.mFarCorners[3], fity );		// left far top
		LF[1].lerp( mFrustum.mFarCorners[0], mFrustum.mFarCorners[3], fiby );		// left far bot
		RF[0].lerp( mFrustum.mFarCorners[1], mFrustum.mFarCorners[2], fity );		// right far top
		RF[1].lerp( mFrustum.mFarCorners[1], mFrustum.mFarCorners[2], fiby );		// right far bot

		ork::fplane3 NFCenterPlane( mFrustum._nearPlane.GetNormal(), mFrustum.mCenter );

		/////////////////////////////////////////////////

		for( int ix=0; ix<miNumTilesW; ix++ )
		{
			/////////////////////////////////////////////////////
			// or maybe we want the pixel centers ?
			int ilx = ix*kTileDim;
			int irx = ilx+kTileDim; //-1);
			float filx = float(ilx)/float(miImageWidth);
			float firx = float(irx)/float(miImageWidth);
			/////////////////////////////////////////////////////

			ork::fray3 RayTL; RayTL.BiLerp( Ray[0], Ray[1], Ray[3], Ray[2], filx, fity );
			ork::fray3 RayTR; RayTR.BiLerp( Ray[0], Ray[1], Ray[3], Ray[2], firx, fity );
			ork::fray3 RayBL; RayBL.BiLerp( Ray[0], Ray[1], Ray[3], Ray[2], filx, fiby );
			ork::fray3 RayBR; RayBR.BiLerp( Ray[0], Ray[1], Ray[3], Ray[2], firx, fiby );

			float distTL, distTR, distBL, distBR;

			bool bisectTL = NFCenterPlane.Intersect( RayTL, distTL );
			bool bisectTR = NFCenterPlane.Intersect( RayTR, distTR );
			bool bisectBL = NFCenterPlane.Intersect( RayBL, distBL );
			bool bisectBR = NFCenterPlane.Intersect( RayBR, distBR );

			ork::fvec3 vCTL = RayTL.mOrigin + RayTL.mDirection*distTL;
			ork::fvec3 vCTR = RayTR.mOrigin + RayTR.mDirection*distTR;
			ork::fvec3 vCBL = RayBL.mOrigin + RayBL.mDirection*distBL;
			ork::fvec3 vCBR = RayBR.mOrigin + RayBR.mDirection*distBR;

			/////////////////////////////////////////////////////

			ork::fvec3 N0; N0.lerp( LN[0], RN[0], filx );
			ork::fvec3 N1; N1.lerp( LN[0], RN[0], firx );
			ork::fvec3 N2; N2.lerp( LN[1], RN[1], firx );
			ork::fvec3 N3; N3.lerp( LN[1], RN[1], filx );
			ork::fvec3 F0; F0.lerp( LF[0], RF[0], filx );
			ork::fvec3 F1; F1.lerp( LF[0], RF[0], firx );
			ork::fvec3 F2; F2.lerp( LF[1], RF[1], firx );
			ork::fvec3 F3; F3.lerp( LF[1], RF[1], filx );

			/////////////////////////////////////////////////////

			ork::fvec3 lftn;
			ork::fvec3 rhtn;
			lftn.lerp( root_lftn, -root_rhtn, filx );
			rhtn.lerp( -root_lftn, root_rhtn, firx );
			lftn.Normalize();
			rhtn.Normalize();

			int idx = CalcTileAddress(ix,iy);
			RasterTile& the_tile = mTiles[idx];

			/////////////////////////////////////////////////////

			the_tile.mFrustum._topPlane.CalcFromNormalAndOrigin( -topn, vCTL );
			the_tile.mFrustum._bottomPlane.CalcFromNormalAndOrigin( -botn, vCBL );
			the_tile.mFrustum._leftPlane.CalcFromNormalAndOrigin( -lftn, vCTL );
			the_tile.mFrustum._rightPlane.CalcFromNormalAndOrigin( -rhtn, vCBR );

			/////////////////////////////////////////////////
			// near and far planes on the tiles are identical to the main image
			/////////////////////////////////////////////////

			the_tile.mFrustum._nearPlane = mFrustum._nearPlane;
			the_tile.mFrustum._farPlane = mFrustum._farPlane;

			the_tile.mFrustum.CalcCorners();

			/////////////////////////////////////////////////

			the_tile.mFrustum.mNearCorners[0].lerp( LN[0], RN[0], filx );
			the_tile.mFrustum.mNearCorners[1].lerp( LN[0], RN[0], firx );
			the_tile.mFrustum.mNearCorners[2].lerp( LN[1], RN[1], firx );
			the_tile.mFrustum.mNearCorners[3].lerp( LN[1], RN[1], filx );
			the_tile.mFrustum.mFarCorners[0].lerp( LF[0], RF[0], filx );
			the_tile.mFrustum.mFarCorners[1].lerp( LF[0], RF[0], firx );
			the_tile.mFrustum.mFarCorners[2].lerp( LF[1], RF[1], firx );
			the_tile.mFrustum.mFarCorners[3].lerp( LF[1], RF[1], filx );

			ork::fvec3 C; for( int i=0; i<4; i++ ) C += the_tile.mFrustum.mNearCorners[i];
			for( int i=0; i<4; i++ ) C += the_tile.mFrustum.mFarCorners[i];

			the_tile.mFrustum.mCenter = C*0.125f;

			float dT = the_tile.mFrustum._topPlane.pointDistance( the_tile.mFrustum.mCenter );
			float dB = the_tile.mFrustum._bottomPlane.pointDistance( the_tile.mFrustum.mCenter );
			float dL = the_tile.mFrustum._leftPlane.pointDistance( the_tile.mFrustum.mCenter );
			float dR = the_tile.mFrustum._rightPlane.pointDistance( the_tile.mFrustum.mCenter );
			float dN = the_tile.mFrustum._nearPlane.pointDistance( the_tile.mFrustum.mCenter );
			float dF = the_tile.mFrustum._farPlane.pointDistance( the_tile.mFrustum.mCenter );

			OrkAssert( dT>0.0f );
			OrkAssert( dB>0.0f );
			OrkAssert( dL>0.0f );
			OrkAssert( dR>0.0f );
			OrkAssert( dN>0.0f );
			OrkAssert( dF>0.0f );

			/////////////////////////////////////////////////

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

int RenderData::GetBucketX( float fx ) const
{
	int ix = int(std::floor(fx));
	int ibx = ix/kTileDim;
	return ibx;
}
int RenderData::GetBucketY( float fy ) const
{
	int iy = int(std::floor(fy));
	int iby = iy/kTileDim;
	return iby;
}
int RenderData::GetBucketIndex( int ix, int iy ) const
{
	int idx = (iy*miNumTilesW)+ix;
	OrkAssert(idx<TriangleMetaBucket::kmaxbuckets);
	OrkAssert(idx>=0);
	return idx;
}

///////////////////////////////////////////////////////////////////////////////

void RenderData::Resize( int iw, int ih )
{
	//////////////////////////////////
	// realloc buffer
	//////////////////////////////////
	if( 0 != mPixelData )
	{
		delete[] mPixelData;
	}
	mPixelData = new u32[ (iw*ih) ];
	//////////////////////////////////

	miImageWidth = iw;
	miImageHeight = ih;
	miNumTilesW = ((iw+kTileDim-1)/kTileDim);
	miNumTilesH = ((ih+kTileDim-1)/kTileDim);
	int inumtiles = miNumTilesW*miNumTilesH;
	mTiles.resize(inumtiles);

	OrkAssert((miNumTilesW*miNumTilesH)<TriangleMetaBucket::kmaxbuckets);

	for( int iy=0; iy<miNumTilesH; iy++ )
	{
		int ity = iy*kTileDim;
		int iby = ity+(kTileDim-1);

		for( int ix=0; ix<miNumTilesW; ix++ )
		{
			int ilx = ix*kTileDim;
			int irx = ilx+(kTileDim-1);

			int idx = CalcTileAddress(ix,iy);
			mTiles[idx].miWidth = (irx<iw) ? kTileDim : (kTileDim-(1+irx-iw));
			mTiles[idx].miHeight = (iby<ih) ? kTileDim : (kTileDim-(1+iby-ih));
			mTiles[idx].miScreenXBase = ilx;
			mTiles[idx].miScreenYBase = ity;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

SlicerModule::SlicerModule()
{

}

///////////////////////////////////////////////////////////////////////////////

SlicerModule::~SlicerModule()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static rend_srcmesh* LoadRgm( const char* pfname, RenderData& rdata )
{
	rend_srcmesh* poutmesh = new rend_srcmesh;
	ork::Engine	 RayEngine;

	ShaderBuilder builder(&RayEngine,&rdata);
	ork::RgmModel* prgmmodel = ork::LoadRgmFile( pfname, builder );
	ork::fvec3 dist = (prgmmodel->_aaBox.Max()-prgmmodel->_aaBox.Min());

	poutmesh->mTarget = (prgmmodel->_aaBox.Min()+prgmmodel->_aaBox.Max())*0.5f;
	poutmesh->mEye = poutmesh->mTarget-ork::fvec3(0.0f,0.0f,dist.Mag());

	int inumsub = prgmmodel->minumsubs;

	poutmesh->miNumSubMesh = inumsub;
	poutmesh->mpSubMeshes = new rend_srcsubmesh[ inumsub ];

	for( int is=0; is<inumsub; is++ )
	{
		rend_srcsubmesh& outsub = poutmesh->mpSubMeshes[is];

		const ork::RgmSubMesh& Sub = prgmmodel->msubmeshes[is];
		const ork::BakeShader* pbakeshader = Sub.mpShader;
		rend_shader* pshader = pbakeshader->mPlatformShader.get<rend_shader*>();

		int inumtri = Sub.minumtris;

		outsub.miNumTriangles = inumtri;
		outsub.mpTriangles = new rend_srctri[ inumtri ];
		outsub.mpShader = pshader;

		for( int it=0; it<inumtri; it++ )
		{
			rend_srctri& outtri = outsub.mpTriangles[ it ];
			const ork::RgmTri& rgmtri = Sub.mtriangles[it];

			outtri.mFaceNormal = rgmtri.mFacePlane.GetNormal();
			outtri.mSurfaceArea = rgmtri.mArea;
			outtri.mpVertices[0].mPos = rgmtri.mpv0->pos;
			outtri.mpVertices[0].mVertexNormal = rgmtri.mpv0->nrm;
			outtri.mpVertices[0].mUv = rgmtri.mpv0->uv;
			outtri.mpVertices[1].mPos = rgmtri.mpv1->pos;
			outtri.mpVertices[1].mVertexNormal = rgmtri.mpv1->nrm;
			outtri.mpVertices[1].mUv = rgmtri.mpv1->uv;
			outtri.mpVertices[2].mPos = rgmtri.mpv2->pos;
			outtri.mpVertices[2].mVertexNormal = rgmtri.mpv2->nrm;
			outtri.mpVertices[2].mUv = rgmtri.mpv2->uv;

		}
	}

	return poutmesh;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if 0
static rend_srcmesh* LoadObj( const char* pfname, RenderData& rdata )
{
	ork::MeshUtil::Mesh tmesh;
	tmesh.ReadFromWavefrontObj( pfname );

	int inumsubs = tmesh.numSubMeshes();
	const ork::orklut<std::string, ork::MeshUtil::submesh*>& sublut = tmesh.RefSubMeshLut();

	rend_srcmesh* poutmesh = new rend_srcmesh;

	ork::AABox aabox;
	poutmesh->miNumSubMesh = inumsubs;
	poutmesh->mpSubMeshes = new rend_srcsubmesh[ inumsubs ];

	aabox.BeginGrow();
	for( int is=0; is<inumsubs; is++ )
	{
		ork::MeshUtil::submesh* psrcsub = sublut.GetItemAtIndex(is).second;
		rend_srcsubmesh& outsub = poutmesh->mpSubMeshes[is];

		aabox.Grow( psrcsub->GetAABox().Min() );
		aabox.Grow( psrcsub->GetAABox().Max() );

		//const ork::RgmSubMesh& Sub = prgmmodel->msubmeshes[is];
		//const ork::BakeShader* pbakeshader = Sub.mpShader;
		rend_shader* pshader = new Shader1(/*rdata.mClEngine*/);
		//rend_shader* pshader = new Shader2();
		pshader->mRenderData = & rdata;

		int inumtri = psrcsub->GetNumPolys(3);

		outsub.miNumTriangles = inumtri;
		outsub.mpTriangles = new rend_srctri[ inumtri ];
		outsub.mpShader = pshader;

		for( int it=0; it<inumtri; it++ )
		{
			rend_srctri& outtri = outsub.mpTriangles[ it ];
			const ork::MeshUtil::poly& inpoly = psrcsub->RefPoly(it);

			int inumv = inpoly.GetNumSides();
			OrkAssert(inumv==3);
			outtri.mFaceNormal = inpoly.ComputeNormal( psrcsub->RefVertexPool() );
			outtri.mSurfaceArea = inpoly.ComputeArea(psrcsub->RefVertexPool(), ork::fmtx4::Identity() );
			int iv0 = inpoly.GetVertexID(0);
			int iv1 = inpoly.GetVertexID(1);
			int iv2 = inpoly.GetVertexID(2);
			const ork::MeshUtil::vertex& v0 = psrcsub->RefVertexPool().GetVertex(iv0);
			const ork::MeshUtil::vertex& v1 = psrcsub->RefVertexPool().GetVertex(iv1);
			const ork::MeshUtil::vertex& v2 = psrcsub->RefVertexPool().GetVertex(iv2);

			outtri.mpVertices[0].mPos = v0.mPos;
			outtri.mpVertices[0].mVertexNormal = v0.mNrm;
			outtri.mpVertices[0].mUv = v0.mUV[0].mMapTexCoord;
			outtri.mpVertices[1].mPos = v1.mPos;
			outtri.mpVertices[1].mVertexNormal = v1.mNrm;
			outtri.mpVertices[1].mUv = v1.mUV[0].mMapTexCoord;
			outtri.mpVertices[2].mPos = v2.mPos;
			outtri.mpVertices[2].mVertexNormal = v2.mNrm;
			outtri.mpVertices[2].mUv = v2.mUV[0].mMapTexCoord;

		}
	}
	aabox.EndGrow();

	ork::fvec3 dist = (aabox.Max()-aabox.Min());
	poutmesh->mTarget = (aabox.Min()+aabox.Max())*0.5f;
	poutmesh->mEye = poutmesh->mTarget-ork::fvec3(0.0f,0.0f,dist.Mag());

	return poutmesh;
}

#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

render_graph::render_graph()
	: mTransformAndClip(mRenderData)
	, mBoundAndSplit(mRenderData,mTransformAndClip)
{
	mRenderData.mpSrcMesh = LoadRgm("final_1_lit.rgm",mRenderData);
	//mRenderData.mpSrcMesh = LoadObj("data/obj/dactyl.obj",mRenderData);
	//mRenderData.mpSrcMesh = LoadObj("data/obj/rhino_2.obj",mRenderData);
	//mRenderData.mpSrcMesh = LoadObj("data/obj/cube.obj",mRenderData);
	//mRenderData.mpSrcMesh = LoadObj("data/obj/sphere.obj",mRenderData);
	//mRenderData.mTarget = ork::fvec3(0.0f,0.0f,0.0f); //mRenderData.mpSrcMesh->mTarget;
	//mRenderData.mEye = ork::fvec3(0.0f,5.0f,30.0f)*.7f; //mRenderData.mpSrcMesh->mEye;
	mRenderData.mTarget = mRenderData.mpSrcMesh->mTarget;
	mRenderData.mEye = mRenderData.mpSrcMesh->mEye;

	int inumsub = mRenderData.mpSrcMesh->miNumSubMesh;

	for( int is=0; is<inumsub; is++ )
	{
		rend_srcsubmesh& outsub = mRenderData.mpSrcMesh->mpSubMeshes[is];

		rend_shader* pshader = outsub.mpShader;

		orkmap<const rend_shader*,TriangleMetaBucket*>::const_iterator it=mTransformAndClip.GetMetaBuckets().find(pshader);
		if( it == mTransformAndClip.GetMetaBuckets().end() )
		{
			TriangleMetaBucket* pmeta = new TriangleMetaBucket;
			mTransformAndClip.GetMetaBuckets()[pshader] = pmeta;
		}
	}



}

///////////////////////////////////////////////////////////////////////////////

void render_graph::Resize( int iw, int ih )
{
	mRenderData.Resize(iw,ih);
}

///////////////////////////////////////////////////////////////////////////////

const u32* render_graph::GetPixels() const
{
	return mRenderData.mPixelData;
}

///////////////////////////////////////////////////////////////////////////////

class task1 : public ork::threadpool::task
{
public:
	task1(RenderData&rdata)
		: mRenderData(rdata)
	{
	}
private:
	RenderData&			mRenderData;
	////////////////////////////////////////////////
	struct MySubTaskData
	{
		int miTileX;
		int miTileY;
	};
	////////////////////////////////////////////////
	void do_divide(ork::threadpool::thread_pool* tpool) // virtual
	{
		for( int ih=0; ih<mRenderData.miNumTilesH; ih++ )
		{
			for( int iw=0; iw<mRenderData.miNumTilesW; iw++ )
			{
				MySubTaskData std;
				std.miTileX = iw;
				std.miTileY = ih;
				ork::threadpool::sub_task* psub = new ork::threadpool::sub_task(this);
				psub->SetData<MySubTaskData>(std);
				tpool->AddSubTask(psub);
				IncNumTasks();
			}
		}
	}
	////////////////////////////////////////////////
	void do_onstarted() // virtual
	{
	}
	////////////////////////////////////////////////
	void do_subtask_finished( const ork::threadpool::sub_task* tsk )
	{
		delete tsk;
	}
	////////////////////////////////////////////////
	void do_onfinished()
	{
	}
	////////////////////////////////////////////////
	void do_process( const ork::threadpool::sub_task* tsk, const ork::threadpool::thread_pool_worker* ptpw )
	{
		if(1) return;

		const MySubTaskData& subtaskdata = tsk->GetData<MySubTaskData>();

		const RasterTile& tile = mRenderData.GetTile(subtaskdata.miTileX,subtaskdata.miTileY);
		int itw = tile.miWidth;
		int ith = tile.miHeight;
		int itx = tile.miScreenXBase;
		int ity = tile.miScreenYBase;

		int imgW = mRenderData.miImageWidth;

		int irand = rand()%0xff;

		int iframe = mRenderData.miFrame;

		float ftime = float(iframe)/30.0f;
		float fb = 0.5f+0.5f*cosf( ftime);
		u8 ucb = u8(fb*255.0f);

		for( int iy=0; iy<ith; iy++ )
		{
			int iabsy = (ity+iy);
			float fy = float(iabsy)/float(mRenderData.miImageHeight);
			float fsy = 0.5f+0.5f*sinf( ftime+fy*10.0f );
			float fcy = 0.5f+0.5f*cosf( ftime+fy*10.0f );

			for( int ix=0; ix<itw; ix++ )
			{
				int iabsx = (itx+ix);
				float fx = float(iabsx)/float(mRenderData.miImageWidth);

				float fsx = 0.5f+0.5f*sinf( ftime+fx*10.0f );
				float fcx = 0.5f+0.5f*cosf( ftime+fx*10.0f );

				float fr = fx;
				float fg = fy;

				u8 ucr = u8(fsx*255.0f);
				u8 ucg = u8(fsy*255.0f);

				u32 upix = (ucb<<0)|u32(ucg<<8)|u32(ucr<<16);
				///////////////////////////////////////////////////////////////////
				int ipixidx = mRenderData.CalcPixelAddress(iabsx,iabsy);
				//mRenderData.mPixelData[ipixidx]=upix;
				///////////////////////////////////////////////////////////////////
			}
		}
	}
	////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

void render_graph::Compute(ork::threadpool::thread_pool*pool)
{
	mRenderData.Update();
	////////////////////////////////////////
	pool->AddTask(&mTransformAndClip);
	mTransformAndClip.wait();
	////////////////////////////////////////
	pool->AddTask(&mBoundAndSplit);
	mBoundAndSplit.wait();
	////////////////////////////////////////
	mRenderData.miFrame++;
	//task1 tk1(mRenderData);
	//pool->AddTask(&tk1);
	//tk1.wait();
	////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
