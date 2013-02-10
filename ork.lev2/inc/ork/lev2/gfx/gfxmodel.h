////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _GFX_GFXMODEL_H
#define _GFX_GFXMODEL_H

#include <ork/math/cmatrix4.h>
#include <ork/math/box.h>
#include <ork/math/sphere.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/ConstString.h>
#include <ork/file/chunkfile.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxanim.h>

#define USE_XGM_FILES
#define USE_XGA_FILES

///////////////////////////////////////////////////////////////////////////////
//	Orkid Native Model File Format (XGM = XPlat Skinned Gfx Model)
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2
{

typedef CAssetHandle ModelH;

/////////////////////////////////////////////////////
//////////////////////////

class VertexBufferBase;
class GfxMaterial;
class VertexBufferBase;
class IndexBufferBase;

class XgmMesh;
class XgmModel;
class XgmCluster;
class XgmSubMesh;
struct RenderContextInstModelData;


///////////////////////////////////////////////////////////////////////////////

#define XGMMESHFLG_RIGID  0x00000001

///////////////////////////////////////////////////////////////////////////////

class XgmPrimGroup
{
public:
	int miNumIndices;

	IndexBufferBase *mpIndices;
	EPrimitiveType mePrimType;

	XgmPrimGroup();
	XgmPrimGroup( XgmPrimGroup *pgrp );

	virtual ~XgmPrimGroup();

	int							GetNumIndices( void ) const { return miNumIndices; }
	const IndexBufferBase*		GetIndexBuffer( void ) const { return mpIndices; }
	EPrimitiveType				GetPrimType( void ) const { return mePrimType; }

};

///////////////////////////////////////////////////////////////////////////////

class XgmCluster // Run Time Cluster
{
public:
	orkvector<PoolString>		mJoints;
	orkvector<int>				mJointSkelIndices;

	int							miNumPrimGroups;
	XgmPrimGroup*				mpPrimGroups;
	VertexBufferBase*			mpVertexBuffer;		// Our Models have 1 VB per cluster
	EVtxStreamFormat			meVtxStrFmt;
	DisplayList					mDisplayList;

	AABox						mBoundingBox;
	Sphere						mBoundingSphere;

	XgmCluster();
	virtual ~XgmCluster();
	void Dump( void );

	inline int				GetNumPrimGroups( void ) const { return miNumPrimGroups; }
	const XgmPrimGroup&		RefPrimGroup( int idx ) const
	{
		OrkAssert( idx >= 0 );
		OrkAssert( idx < miNumPrimGroups );
		return mpPrimGroups[idx];
	}
	XgmPrimGroup&			RefPrimGroup( int idx )
	{
		OrkAssert( idx >= 0 );
		OrkAssert( idx < miNumPrimGroups );
		return mpPrimGroups[idx];
	}
	const VertexBufferBase*	GetVertexBuffer( void ) const  { return mpVertexBuffer; }
	const PoolString&		GetJointBinding( int idx ) const  { return mJoints[ idx ]; }
	size_t					GetNumJointBindings( void ) const  { return mJoints.size(); }

	void InitDisplayList( ork::lev2::GfxTarget* pTARG );

	void dump() const;

};

///////////////////////////////////////////////////////////////////////////////

class XgmSubMesh // Run Time Cluster Set
{	public:
	GfxMaterial*		mpMaterial;
	int					miNumClusters;
	XgmCluster*			mpClusters;
	file::Path			mLightMapPath;
	Texture*			mLightMap;
	bool				mbVertexLit;

	XgmSubMesh() :	mpMaterial(0), miNumClusters(0), mpClusters(0), mLightMapPath(""), mLightMap(0), mbVertexLit(false)
	{
	}
	virtual ~XgmSubMesh();

	int					GetNumClusters( void ) const { return miNumClusters; }
	const XgmCluster&	RefCluster( int idx ) const
	{
		OrkAssert( idx >= 0 );
		OrkAssert( idx < miNumClusters );
		return mpClusters[ idx ];
	}
	XgmCluster&		RefCluster( int idx )
	{
		OrkAssert( idx >= 0 );
		OrkAssert( idx < miNumClusters );
		return mpClusters[ idx ];
	}
	GfxMaterial*		GetMaterial( void ) const { return mpMaterial; }

	void InitDisplayLists( ork::lev2::GfxTarget* pTARG );

	void dump() const;

};

///////////////////////////////////////////////////////////////////////////////

class XgmMesh
{
	public:

	/////////////////////////////////////
	XgmMesh();
	XgmMesh( XgmMesh *pMesh );
	virtual ~XgmMesh();
	/////////////////////////////////////
	void						SetBoundingBox( CVector4 &Min, CVector4 &Max ) { mvBoundingBoxMin=Min; mvBoundingBoxMax=Max;	}
	void						SetFlags( U32 flags ) { muFlags |= flags; }
	void						SetMeshIndex( int i ) { miMeshIndex=i; }
	void						SetMeshName( const PoolString& name ) { mMeshName=name; }
	int							GetNumSubMeshes( void ) const { return int(mSubMeshes.size()); }
	int							GetMeshIndex() const { return miMeshIndex; }
	U32							GetFlags( void ) const { return muFlags; }
	bool						CheckFlags( U32 flags ) const { return ((flags&muFlags)==flags); }
	const XgmSubMesh*			GetSubMesh( int idx ) const  { return mSubMeshes[idx]; }
	XgmSubMesh*					GetSubMesh( int idx ) { return mSubMeshes[idx]; }
	const PoolString&			GetMeshName() const { return mMeshName; }
	const CVector4&				RefBoundingBoxMin( void ) const { return mvBoundingBoxMin; }
	const CVector4&				RefBoundingBoxMax( void ) const { return mvBoundingBoxMax; }
	/////////////////////////////////////
	void						ReserveSubMeshes( int icount ) { mSubMeshes.reserve( icount ); }
	void						AddSubMesh( XgmSubMesh* psubmesh ) { mSubMeshes.push_back(psubmesh); }
	void						InitDisplayLists( ork::lev2::GfxTarget* pTARG );
	void						dump() const;
	/////////////////////////////////////

private:

	orkvector<XgmSubMesh*>	mSubMeshes;
	CVector4				mvBoundingBoxMin;
	CVector4				mvBoundingBoxMax;
	U32						muFlags;
	int						miNumBoneBindings;	// for ps2 truth only
	F32						mfBoundingRadius;
	CVector4				mvBoundingCenter;
	PoolString				mMeshName;
	int						miMeshIndex;
};

///////////////////////////////////////////////////////////////////////////////

class XgmModel
{

public:

	/////////////////////////////////////

	void ReserveMeshes( int icount ) { mMeshes.reserve(icount); }
	void AddMesh( const PoolString& name, XgmMesh* pmesh ) { mMeshes.AddSorted(name,pmesh); }

	/////////////////////////////////////
	
	XgmModel();
	~XgmModel();

	/////////////////////////////////////

	bool IsSkinned() const { return mbSkinned; }
	void SetSkinned( bool bv ) { mbSkinned = bv; }

	/////////////////////////////////////

	int						GetNumMeshes() const { return int(mMeshes.size()); }
	int						GetNumMaterials() const { return miNumMaterials; }

	const XgmSkeleton&		RefSkel() const { return mSkeleton; }
	XgmSkeleton&			RefSkel() { return mSkeleton; }

	const XgmMesh*			GetMesh(int idx) const { return mMeshes.GetItemAtIndex(idx).second; }
	XgmMesh*				GetMesh(int idx) { return mMeshes.GetItemAtIndex(idx).second; }

	const XgmMesh*			GetMesh(const PoolString& name) const { return mMeshes.find(name)->second; }
	XgmMesh*				GetMesh(const PoolString& name) { return mMeshes.find(name)->second; }
	int						GetMeshIndex(const PoolString& name) const;

	void*					GetUserData() { return mpUserData; }
	const GfxMaterial*		GetMaterial(int idx) const { return mvMaterials[idx]; }
	GfxMaterial*			GetMaterial(int idx) { return mvMaterials[idx]; }
	void					AddMaterial(GfxMaterial* hM);
	
	const CVector3&			GetBoundingCenter() const { return mBoundingCenter; }
	float					GetBoundingRadius() const { return mBoundingRadius; }
	const CVector3&			GetBoundingAA_XYZ() const { return mAABoundXYZ; }
	const CVector3&			GetBoundingAA_WHD() const { return mAABoundWHD; }

	int						GetBonesPerCluster() const { return miBonesPerCluster; }

	/////////////////////////////////////

	void					SetBoundingCenter(const CVector3& v) { mBoundingCenter=v; }
	void					SetBoundingRadius(float v) { mBoundingRadius=v; }
	void					SetBoundingAA_XYZ(const CVector3& v) { mAABoundXYZ=v; }
	void					SetBoundingAA_WHD(const CVector3& v) { mAABoundWHD=v; }

	void					SetBonesPerCluster( int i ) { miBonesPerCluster=i; }

	/////////////////////////////////////

	void BeginRigidBlock(		const CColor4& ModColor,
								const CMatrix4& WorldMat,
								ork::lev2::GfxTarget* pTARG,
								const RenderContextInstData & MatCtx,
								const RenderContextInstModelData& MdlCtx ) const;
	void EndRigidBlock() const;
	void RenderRigidBlockItem() const;


	void RenderRigid(		const CColor4& ModColor,
							const CMatrix4& WorldMat,
							ork::lev2::GfxTarget* pTARG,
							const RenderContextInstData & MatCtx,
							const RenderContextInstModelData& MdlCtx ) const;

	void RenderMultipleRigid(	const CColor4& ModColor,
								const CMatrix4* WorldMats, int icount,
								ork::lev2::GfxTarget* pTARG,
								const RenderContextInstData & MatCtx,
								const RenderContextInstModelData& MdlCtx ) const;

	void RenderSkinned(	
							const XgmModelInst* minst,
							const CColor4& ModColor,
							const CMatrix4& WorldMat,
							ork::lev2::GfxTarget* pTARG,
							const RenderContextInstData & MatCtx,
							const RenderContextInstModelData& MdlCtx ) const;

	void RenderMultipleSkinned(	
							const XgmModelInst* minst,
							const CColor4& ModColor,
							const CMatrix4* WorldMats, int icount,
							ork::lev2::GfxTarget* pTARG,
							const RenderContextInstData & MatCtx,
							const RenderContextInstModelData& MdlCtx ) const;

	/////////////////////////////////////

	static bool LoadUnManaged( XgmModel* mdl, const AssetPath& fname );

	/////////////////////////////////////

	void InitDisplayLists( ork::lev2::GfxTarget* pTARG );

	void dump() const;

	PoolString GetModelName() const { return msModelName; }

private:

	orklut<PoolString,XgmMesh*>	mMeshes;
	orkvector<GfxMaterial*>		mvMaterials;
	int							miBonesPerCluster;
	XgmSkeleton					mSkeleton;
	void*						mpUserData;
	int							miNumMaterials;
	PoolString					msModelName;
	CVector3					mAABoundXYZ;
	CVector3					mAABoundWHD;
	CVector3					mBoundingCenter;
	float						mBoundingRadius;
	bool						mbSkinned;

};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialFx;

class XgmModelInst
{
public:

	XgmModelInst( const XgmModel* Model );
	~XgmModelInst();

	GfxMaterialFx*				GetLayerFxMaterial() const { return mLayerFxMaterial; }
	void						SetLayerFxMaterial( GfxMaterialFx* pfx ) { mLayerFxMaterial=pfx; }

	const XgmModel*				GetXgmModel( void ) const { return mXgmModel; }
	int							GetNumChannels( void ) const;

	XgmLocalPose&				RefLocalPose() { return mLocalPose; }
	const XgmLocalPose&			RefLocalPose() const { return mLocalPose; }

	XgmMaterialStateInst&		RefMaterialInst() { return mMaterialStateInst; }
	const XgmMaterialStateInst&	RefMaterialInst() const { return mMaterialStateInst; }

	void						EnableMesh( const PoolString& ps );
	void						DisableMesh( const PoolString& ps );

	void						EnableMesh( int index );
	void						DisableMesh( int index );

	void						EnableAllMeshes();
	void						DisableAllMeshes();

	bool						IsMeshEnabled( int index );
	bool						IsMeshEnabled( const PoolString& ps );
	bool						IsAnyMeshEnabled();


private:

	static const int				knummaskbytes = 32;
	U8								mMaskBits[knummaskbytes];
	const XgmModel*					mXgmModel;
	XgmLocalPose					mLocalPose;
	XgmMaterialStateInst			mMaterialStateInst;
	GfxMaterialFx*					mLayerFxMaterial;
	int								miNumChannels;
};

///////////////////////////////////////////////////////////////////////////////

struct RenderContextInstModelData
{
	const XgmModelInst*			mpModelInst;
	const XgmMesh*				mMesh;
	const XgmSubMesh*			mSubMesh;
	const XgmCluster*			mCluster;

	bool						mbIsSkinned;
	int							miSubMeshIndex;
	const XgmWorldPose*			mpWorldPose;

	//////////////////////////////////////
	// model interface
	//////////////////////////////////////

	bool						IsSkinned( void ) const { return mbIsSkinned; }
	void						SetSkinned( bool bv ) { mbIsSkinned=bv; }

	void						SetModelInst( const XgmModelInst *pinst ) { mpModelInst=pinst; }
	const XgmModelInst*			GetModelInst( void ) const { return mpModelInst; }

	//////////////////////////////////////

	RenderContextInstModelData();

};

///////////////////////////////////////////////////////////////////////////////

bool SaveXGM( const AssetPath& Filename, const lev2::XgmModel *mdl );

///////////////////////////////////////////////////////////////////////////////

} }

#endif



