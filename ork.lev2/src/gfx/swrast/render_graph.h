////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

//#include <dxgi.h>
//#include <D2d1.h>
#include <ork/math/frustum.h>
#include <unordered_map>
#include <ork/kernel/thread_pool.h>
#include <ork/gfx/radixsort.h>
#include <ork/math/misc_math.h>
#include <ork/math/box.h>
//#include "cl.h"

///////////////////////////////////////////////////////////////////////////////

struct IGeoPrim;		// 
class RenderData;
struct rend_prefragment;
struct rend_fragment;
struct rend_shader;
struct rend_volume_shader;
struct AABuffer;
struct rend_prefragsubgroup;

///////////////////////////////////////////////////////////////////////////////

struct rend_srcvtx
{
	ork::fvec3	mPos;
	ork::fvec3	mVertexNormal;
	ork::fvec3	mUv;
};

struct rend_srctri
{
	rend_srcvtx		mpVertices[3];
	ork::fvec3	mFaceNormal;
	float			mSurfaceArea;
};

struct rend_srcsubmesh
{
	int				miNumTriangles;
	rend_srctri*	mpTriangles;
	rend_shader*	mpShader;

	rend_srcsubmesh() : miNumTriangles(0), mpTriangles(0), mpShader(0) {}
};

struct rend_srcmesh
{
	int					miNumSubMesh;
	rend_srcsubmesh*	mpSubMeshes;
	ork::AABox			mAABox;
	ork::fvec3		mTarget;
	ork::fvec3		mEye;

	rend_srcmesh() : miNumSubMesh(0), mpSubMeshes(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct rend_ivtx
{
	float	mSX;
	float	mSY;
	float	mRoZ;
	float	mSoZ;
	float	mToZ;
	float	mfDepth;
	float	mfInvDepth;
	ork::fvec3 mWldSpacePos;
	ork::fvec3 mObjSpacePos;
	ork::fvec3 mWldSpaceNrm;
	ork::fvec3 mObjSpaceNrm;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_triangle
{
	rend_ivtx			mSVerts[3];
	ork::fvec3		mFaceNormal;
	float				mfArea;
	const rend_shader*	mpShader;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_subtri
{
	rend_ivtx vtxA;
	rend_ivtx vtxB;
	rend_ivtx vtxC;
	const rend_triangle* mpSourcePrim;
	int miIA;
	int miIB;
	int miIC;
	int miTri;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_shader
{
	enum eType
	{
		EShaderTypeSurface = 0,
		EShaderTypeAtmosphere,
		EShaderTypeDisplacement,
	};

	const RenderData*			 mRenderData;
	const rend_volume_shader	*mpVolumeShader;

	////////////////////////////////////////////

	rend_shader() : mRenderData(0), mpVolumeShader(0) {}
	virtual eType GetType() const = 0;
	virtual void Shade( const rend_prefragment& prefrag, rend_fragment* pdstfrag ) const = 0;
	virtual void ShadeBlock( AABuffer& aabuf, int ifragbase, int icount, int inumtri ) const;

};
///////////////////////////////////////////////////////////////////////////////
struct rend_prefragment
{
	const rend_triangle*	mpSrcPrimitive;
	const rend_ivtx*		srcvtxR;
	const rend_ivtx*		srcvtxS;
	const rend_ivtx*		srcvtxT;
	float					mfR;
	float					mfS;
	float					mfT;
	float					mfZ;
	int						miPixIdxAA;
	int						miTri;
};

struct rend_prefrags
{
	orkvector<rend_prefragment>	mPreFrags;
	int							miNumPreFrags;
	int							miMaxPreFrags;
	rend_prefragment& AllocPreFrag();
	void Reset();
	rend_prefrags();
};
///////////////////////////////////////////////////////////////////////////////
struct rend_fragment
{
	ork::fvec4			mRGBA;
	ork::fvec3			mWorldPos;	// needed for volume shaders
	ork::fvec3			mWldSpaceNrm;
	float					mZ;
	const rend_triangle*	mpPrimitive;
	const rend_fragment*	mpNext;
	const rend_shader*		mpShader;

	rend_fragment()
		: mpPrimitive(0)
		, mpNext(0)
		, mZ(0.0f)
		, mRGBA(0.0f,0.0f,0.0f,1.0f)
		, mpShader(0)
	{
	}
};
///////////////////////////////////////////////////////////////////////////////
struct rend_texture2D
{
	ork::fvec4*	mpData;
	int				miWidth;
	int				miHeight;
	//mutable cl_mem	mCLhandle;
	rend_texture2D();
	~rend_texture2D();
	ork::fvec4 sample_point( float u, float v, bool wrapu, bool wrapv ) const;

	void Load( const std::string& pth );
	void Init( /*const CLDevice* pdev*/ ) const;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_surface_shader : public rend_shader
{
	virtual eType GetType() const { return EShaderTypeSurface; }
	virtual void Compute( rend_fragment* pfrag ) = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_volume_shader 
{
//	virtual void Compute( rend_fragment* pfrag ) = 0;
	u32	mBooleanKey;
	rend_volume_shader() : mBooleanKey(0) {}
	virtual ork::fvec4 ShadeVolume( const ork::fvec3& entrywpos, const ork::fvec3& exitwpos ) const = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_fraglist_visitor
{
	virtual void Visit( const rend_fragment* pnode ) = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct rend_fraglist
{
	rend_fragment* mpHead;

	void AddFragment( rend_fragment* pfrag );

	int DepthComplexity() const;
	void Visit( rend_fraglist_visitor& visitor ) const;
	
	rend_fraglist();
	void Reset();
};
///////////////////////////////////////////////////////////////////////////////
struct FragmentPoolNode
{
	static const int kNumFragments = 1<<14;
	rend_fragment						mFragments[kNumFragments];
	int									mFragmentIndex;
	///////////////////////////////////////////////////////////
	rend_fragment* AllocFragment();
	void AllocFragments(rend_fragment** ppfrag, int icount);
	void Reset();
	int GetNumAvailable() const { return (kNumFragments-mFragmentIndex); }
	FragmentPoolNode();
};
///////////////////////////////////////////////////////////////////////////////
struct FragmentPool
{
	orkvector<FragmentPoolNode*>		mFragmentPoolNodes;
	orkvector<rend_fragment*>			mFreeFragments;
	int									miPoolNodeIndex;
	FragmentPoolNode*					mpCurNode;
	///////////////////////////////////////////////////////////
	rend_fragment* AllocFragment();
	bool AllocFragments( rend_fragment**, int icount );
	void FreeFragment(rend_fragment*);
	void Reset();
	FragmentPool();
};
///////////////////////////////////////////////////////////////////////////////
struct FragmentCompositorREYES : public rend_fraglist_visitor
{
	static const int kmaxfrags=256;
	int miNumFragments;
	const rend_fragment*	mpFragments[kmaxfrags];
	const rend_fragment*	mpSortedFragments[kmaxfrags];
	float					mFragmentZ[kmaxfrags];
	float					opaqueZ;
	const rend_fragment*	mpOpaqueFragment;
	ork::RadixSort			mRadixSorter;
	int						miThreadID;
	FragmentCompositorREYES() : miThreadID(0), opaqueZ(1.0e30f), miNumFragments(0), mpOpaqueFragment(0) {}
	void Visit( const rend_fragment* pfrag ); // virtual
	void SortAndHide(); // Sort and Hide occluded
	void Reset();
	ork::fvec3 Composite(const ork::fvec3&clrcolor);
	////////////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
class NoCLFromHostBuffer
{
public:
	void resize(int isize);
	char* GetBufferRegion() const { return GetBufferRegionPriv(); }
	NoCLFromHostBuffer();
	~NoCLFromHostBuffer();
	int GetSize() const { return miSize; }
protected:
	char* GetBufferRegionPriv() const { return (miWriteIndex&1)?mpBuffer1:mpBuffer0; }
	int		miSize;
	char*	mpBuffer0;
	char*	mpBuffer1;
	int		miWriteIndex;
};
class NoCLToHostBuffer
{
public:
	void resize(int isize);
	//cl_mem& GetHandle() { return mCLhandle; }
	const char* GetBufferRegion() const { return GetBufferRegionPriv(); }
	NoCLToHostBuffer();
	~NoCLToHostBuffer();
	//void SetArg(cl_kernel k, int iarg);
	//void Transfer( const CLDevice* pdev, int isize, bool bblock );
	int GetSize() const { return miSize; }
protected:
	char* GetBufferRegionPriv() const { return (miReadIndex&1)?mpBuffer1:mpBuffer0; }
	//cl_mem	mCLhandle;
	int		miSize;
	U32		mBufferflags;
	char*	mpBuffer0;
	char*	mpBuffer1;
	int		miReadIndex;
};
///////////////////////////////////////////////////////////////////////////////
struct FragmentCompositorZBuffer : public rend_fraglist_visitor
{
	float					opaqueZ;
	const rend_fragment*	mpOpaqueFragment;
	FragmentCompositorZBuffer() : opaqueZ(1.0e30f), mpOpaqueFragment(0) {}
	void Visit( const rend_fragment* pfrag ); // virtual
	void Reset();
	ork::fvec3 Composite(const ork::fvec3&clrcolor);
	////////////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
struct AABuffer
{
	u32*						mColorBuffer;
	f32*						mDepthBuffer;
	rend_fraglist*				mFragmentBuffer;
	FragmentPool				mFragmentPool;
	FragmentCompositorREYES		mCompositorREYES;
	FragmentCompositorZBuffer	mCompositorZB;
	NoCLFromHostBuffer*			mTriangleClBuffer;
	NoCLFromHostBuffer*			mFragInpClBuffer;
	NoCLToHostBuffer*			mFragOutClBuffer;

	rend_prefrags				mPreFrags;
	static const int kfragallocsize = 1<<19;
	rend_fragment*				mpFragments[kfragallocsize];

	AABuffer();
	~AABuffer();

	//void InitCl( const CLengine& eng );
};
///////////////////////////////////////////////////////////////////////////////
// context information for rasterizing a triangle (which color and z buffer, etc...)
struct rendtri_context
{
	rendtri_context( AABuffer& aabuf ) : mAABuffer( aabuf ) {}
	AABuffer& mAABuffer;
};
///////////////////////////////////////////////////////////////////////////////
// BOUNDING/SPLITTING STAGE DATA STRUCTURES
///////////////////////////////////////////////////////////////////////////////
struct GeoPrimList // reyes primitive
{
	// 
	ork::LockedResource<orkvector<IGeoPrim*>>	mPrimitives;

	void AddPrim( IGeoPrim* prim ); // thread safe function

	GeoPrimList( const GeoPrimList& oth ) {}
	GeoPrimList() {}
};
///////////////////////////////////////////////////////////////////////////////
struct GeoPrimTable // complete table of all primitives for the bounder/splitter module
{	
	////////////////////////////////////////////
	// will have many threads reading and writing to it simultaneously
	// => use granular locking
	////////////////////////////////////////////

	GeoPrimTable();
	void operator=(const GeoPrimTable& oth) {}

	void AddPrim( IGeoPrim* prim ); // thread safe function

	////////////////////////////////////////////
	ork::fmtx4		mProjectionMatrix;
	ork::Frustum 		mProjectionFrustum;
	////////////////////////////////////////////
	static const int kmaxdivs = 16;
	GeoPrimList				mPrimLists[kmaxdivs];
	ork::threadpool::MyAtomicNum<int>		mListIndex;
	////////////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
struct IGeoPrim // reyes course primitive (pre-diced)
{
	IGeoPrim() {}

	ork::AABox	mBoundingBoxWorldSpace;
	ork::AABox	mBoundingBoxScreenSpace;
};
///////////////////////////////////////////////////////////////////////////////
struct GeoPrimPolyMesh : public IGeoPrim
{
	//const ork::RgmModel*		mpModel;
};
///////////////////////////////////////////////////////////////////////////////
struct RasterTile
{
	int						miScreenXBase, miScreenYBase;
	int						miWidth, miHeight;
	ork::Frustum			mFrustum;
	//mutable float			MinZ;
};
///////////////////////////////////////////////////////////////////////////////
class RenderData 
{
public:

	RenderData();

	static const int 		kTileDim=32;
	int						miNumTilesW;
	int						miNumTilesH;
	int						miImageWidth;
	int						miImageHeight;
	int						miFrame;
	orkvector<RasterTile>	mTiles;

	rend_srcmesh*			mpSrcMesh;
	//ork::RgmModel*			mpModel;
	//ork::Engine				mRayEngine;
	ork::fvec3			mEye;
	ork::fvec3			mTarget;
	ork::fmtx4			mMatrixP;
	ork::fmtx4			mMatrixV;
	ork::fmtx4			mMatrixM;
	ork::fmtx4			mMatrixMV;
	ork::fmtx4			mMatrixMVP;
	ork::Frustum			mFrustum;
	u32*					mPixelData;
	
	int						miAADim1d;
	int						miAADim2d;
	int						miAATileDim;
	float					mfAADim1d;
	float					mfAADim2d;
	//CLengine				mClEngine;

	int GetBucketX( float fx ) const;
	int GetBucketY( float fy ) const;
	int GetBucketIndex( int ix, int iy ) const;

	void Resize( int iw, int ih );
	void Update();

	void operator=( const RenderData& oth );

	inline const RasterTile& GetTile( int itx, int ity ) const;
	inline int CalcTileAddress( int itx, int ity ) const;
	inline int CalcPixelAddress( int ix, int iy ) const;
	inline int CalcAAPixelAddress( int ix, int iy ) const;
};

///////////////////////////////////////////////////////////////////////////////

inline const RasterTile& RenderData::GetTile( int itx, int ity ) const
{
	OrkAssert( itx<miNumTilesW );
	OrkAssert( ity<miNumTilesH );
	OrkAssert( itx>=0 );
	OrkAssert( ity>=0 );

	int idx = CalcTileAddress(itx,ity);

	return mTiles[idx];
}

///////////////////////////////////////////////////////////////////////////////

inline int RenderData::CalcTileAddress( int itx, int ity ) const
{
	int idx = ity*miNumTilesW+itx;
	return idx;
}

///////////////////////////////////////////////////////////////////////////////

inline int RenderData::CalcPixelAddress( int ix, int iy ) const
{
	OrkAssert( ix<miImageWidth );
	OrkAssert( iy<miImageHeight );
	OrkAssert( ix>=0 );
	OrkAssert( iy>=0 );
	return (iy*miImageWidth)+ix;
}

///////////////////////////////////////////////////////////////////////////////

inline int RenderData::CalcAAPixelAddress( int ix, int iy ) const
{
	OrkAssert( ix<miAATileDim );
	OrkAssert( iy<miAATileDim );
	OrkAssert( ix>=0 );
	OrkAssert( iy>=0 );
	return (iy*miAATileDim)+ix;
}

///////////////////////////////////////////////////////////////////////////////

struct ClipVert
{	ork::fvec3 pos;
	void Lerp( const ClipVert& va, const ClipVert& vb, float flerp )
	{	pos.Lerp( va.pos, vb.pos, flerp );
	}
	const ork::fvec3& Pos() const { return pos; }
	ClipVert(const ork::fvec3&tp) : pos(tp) {}
	ClipVert() : pos() {}
};

///////////////////////////////////////////////////////////////////////////////

class ClipPoly
{
	static const int kmaxverts = 32;

	ClipVert	mverts[kmaxverts];
	int			minumverts;
public:
	typedef ClipVert VertexType;
	void AddVertex( const ClipVert& v ) { mverts[minumverts++]=v; }
	int GetNumVertices() const { return minumverts; }
	const ClipVert& GetVertex( int idx ) const { return mverts[idx]; }
	ClipPoly() : minumverts(0) {}
	void SetDefault()
	{	minumverts = 0;
	}
};


///////////////////////////////////////////////////////////////////////////////

struct TriangleBucket
{
	orkvector<rend_triangle*>						mPostTransformTriangles;
	ork::threadpool::MyAtomicNum<int>				mTriangleIndex;

	rend_triangle* GetTriangle()
	{
		int idx = mTriangleIndex.FetchAndIncrement();
		return mPostTransformTriangles[idx];
	}

	TriangleBucket()
	{
		Reset();
	}
	void Reset();
};

///////////////////////////////////////////////////////////////////////////////

struct TriangleMetaBucket
{
	static const int kmaxbuckets = 4096;
	TriangleBucket	mBuckets[kmaxbuckets];

/*	const rend_shader* mpLastShader;
	TriangleBucket* mpLastBucket;

	TriangleMetaBucket() : mpLastShader(0), mpLastBucket(0) {}

	TriangleBucket* GetBucket( const rend_shader* pshader )
	{
		if( mpLastShader==pshader )
		{
			OrkAssert( mpLastBucket != 0 );		
			return mpLastBucket;
		}

		mpLastShader = pshader;
		orkmap<const rend_shader*,TriangleBucket*>::iterator it=mSubBuckets.find(pshader);
		if( it == mSubBuckets.end() )
		{
			TriangleBucket* pucket = new TriangleBucket;
			mSubBuckets.insert( std::make_pair( pshader, pucket ) );
			mpLastBucket = pucket;
		}
		else
		{
			mpLastBucket = it->second;
		}
		OrkAssert( mpLastBucket != 0 );
		return mpLastBucket;
	}
	const TriangleBucket* FindBucket( const rend_shader* pshader ) const
	{
		orkmap<const rend_shader*,TriangleBucket*>::const_iterator it=mSubBuckets.find(pshader);
		if( it == mSubBuckets.end() )
		{
			return 0;	
		}
		return it->second;
	}
*/
	void Reset();
};

///////////////////////////////////////////////////////////////////////////////

class TransformAndClipModule : public ork::threadpool::task
{
public:

	TransformAndClipModule(const RenderData& rdata);
	//const orkvector<rend_triangle*>& GetPostTransformTriangles(int ibucket) const { return mBuckets[ibucket].mPostTransformTriangles; }
	//int GetNumTriangles(int ibucket) const { return mBuckets[ibucket].mTriangleIndex.Fetch(); }
	const orkmap<const rend_shader*,TriangleMetaBucket*>& GetMetaBuckets( ) const { return mMetaBuckets; }
	orkmap<const rend_shader*,TriangleMetaBucket*>& GetMetaBuckets( ) { return mMetaBuckets; }

private:
	void do_divide(ork::threadpool::thread_pool* tpool); // virtual
	void do_onstarted(); // virtual
	void do_subtask_finished( const ork::threadpool::sub_task* tsk ); // virtual
	void do_onfinished(); // virtual
	void do_process( const ork::threadpool::sub_task* tsk, const ork::threadpool::thread_pool_worker* ptpw ); // virtual
	////////////////////////////////////////////////////////////
	GeoPrimTable 									mPrimTable;
	const RenderData& 								mRenderData;
	void*											mSourceHash;
	int												miNumWorkUnits;
	orkmap<const rend_shader*,TriangleMetaBucket*>	mMetaBuckets;
	orkvector<rend_triangle>						mPostTransformTriangles;
	ork::threadpool::MyAtomicNum<int>				mTriangleIndex;
};

///////////////////////////////////////////////////////////////////////////////

class BoundAndSplitModule : public ork::threadpool::task
{
public:
	BoundAndSplitModule(const RenderData& rdata,const TransformAndClipModule&tac);
private:
	void do_divide(ork::threadpool::thread_pool* tpool); // virtual
	void do_onstarted(); // virtual
	void do_subtask_finished( const ork::threadpool::sub_task* tsk ); // virtual
	void do_onfinished(); // virtual
	void do_process( const ork::threadpool::sub_task* tsk, const ork::threadpool::thread_pool_worker* ptpw ); // virtual
	////////////////////////////////////////////////////////////
	void RasterizeTri( const rendtri_context& ctx, const rend_triangle& tri, const RasterTile& tile, int it );
	void RasterizeSubTri( const rendtri_context& ctx, const rend_subtri& tri, const RasterTile& tile, u32 unc );
	////////////////////////////////////////////////////////////
	const RenderData& 					mRenderData;
	const TransformAndClipModule&		mTAC;
	void*								mSourceHash;
	static const int					kAABUFTILES = 16;
	AABuffer							mAABufTiles[kAABUFTILES];
};

///////////////////////////////////////////////////////////////////////////////

struct SlicerData
{};

///////////////////////////////////////////////////////////////////////////////

class SlicerModule // : public ork::threadpool::task
{
public:
	SlicerModule();
	~SlicerModule();
private:
	////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class render_graph 
{
public:
	render_graph();
	void Compute(ork::threadpool::thread_pool*pool);

	void Resize( int iw, int ih );
	const u32* GetPixels() const;

private:
	RenderData				mRenderData;
	BoundAndSplitModule		mBoundAndSplit;
	TransformAndClipModule	mTransformAndClip;
};
///////////////////////////////////////////////////////////////////////////////

