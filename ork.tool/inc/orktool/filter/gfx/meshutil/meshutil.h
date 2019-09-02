////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/util/crc.h>
#include <ork/util/crc64.h>
#include <orktool/filter/filter.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/box.h>
#include <algorithm>
#include <ork/kernel/Array.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <unordered_map>

struct DaeReadOpts;
struct DaeWriteOpts;

namespace ork { namespace tool {

struct SColladaMaterial; 

}}

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace MeshUtil {

struct MaterialBindingItem
{
	std::string mMaterialName;
	std::string mMaterialDaeId;
	//std::vector<FCDMaterialInstanceBind*> mBindings;
};

typedef orkmap<std::string,MaterialBindingItem> material_semanticmap_t;

class Light : public ork::Object
{
	RttiDeclareAbstract(Light, ork::Object);

public:

	std::string		mName;
	fmtx4		mWorldMatrix;
	fvec3		mColor;
	float			mIntensity;
	float			mShadowSamples;
	float			mShadowBias;
	float			mShadowBlur;
	bool			mbSpecular;
	bool			mbIsShadowCaster;

	virtual bool AffectsSphere( const fvec3& center, float radius ) const { return false; }
	virtual bool AffectsAABox( const AABox& aab ) const { return false; }

	Light() 
		: mColor(1.0f,1.0f,1.0f)
		, mIntensity(1.0f) 
		, mbSpecular(false)
		, mShadowSamples(1.0f)
		, mShadowBlur(0.0f)
		, mShadowBias(0.2f)
		, mbIsShadowCaster(false)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

class LightContainer : public ork::Object
{
	RttiDeclareConcrete(LightContainer, ork::Object);
public:

	orklut<PoolString,Light*>	mLights;
};

///////////////////////////////////////////////////////////////////////////////

struct HashU6432
    : public std::unary_function<U64, std::size_t>
{
    std::size_t operator()(U64 v) const
    {
		U64 sh = v>>32;
		size_t h = size_t(sh);
		return h;
    }
	bool operator() (U64 s1, U64 s2) const
	{
		return s1 < s2;
	}
};
	
struct Hash3232
    : public std::unary_function<int, std::size_t>
{
    std::size_t operator()(int v) const
    {
		size_t h = size_t(v);
		return h;
    }
	bool operator() (int s1, int s2) const
	{
		return s1 < s2;
	}
};
///////////////////////////////////////////////////////////////////////////////

typedef std::unordered_map<U64,int,HashU6432>	HashU64IntMap;
typedef std::unordered_map<int,int,Hash3232>	HashIntIntMap;

static const int kmaxpolysperedge = 4;

class edge
{
	int	miVertexA;
	int	miVertexB;
	int miNumConnectedPolys;
	int miConnectedPolys[kmaxpolysperedge];

public:

	int GetVertexID( int iv ) const 
	{
		int id = -1;

		switch( iv )
		{
			case 0: id = miVertexA; break;
			case 1: id = miVertexB; break;
			default:
				OrkAssert( false ); 
				break;
		}

		return id;
	}

	U64 GetHashKey( void ) const;
	bool Matches( const edge & other ) const;

	edge() 
		: miVertexA(-1)
		, miVertexB(-1) 
		, miNumConnectedPolys( 0 )
	{
		for( int i=0; i<kmaxpolysperedge; i++ )  miConnectedPolys[i]=-1;
	}

	edge( int iva, int ivb )
		: miNumConnectedPolys( 0 )
		, miVertexA(iva)
		, miVertexB(ivb)
	{
		for( int i=0; i<kmaxpolysperedge; i++ )  miConnectedPolys[i]=-1;
	}

	void ConnectToPoly( int ipoly );

	int GetNumConnectedPolys( void ) const { return miNumConnectedPolys; }

	int GetConnectedPoly( int ip ) const 
	{
		OrkAssert( ip<miNumConnectedPolys );
		return miConnectedPolys[ ip ];
	}

};

///////////////////////////////////////////////////////////////////////////////

struct uvmapcoord
{
	fvec3 mMapBiNormal;
	fvec3 mMapTangent;
	fvec2 mMapTexCoord;

	void Lerp( const uvmapcoord & ina, const uvmapcoord &inb, float flerp );

	uvmapcoord operator+ ( const uvmapcoord & ina ) const;
	uvmapcoord operator* ( const float Scalar ) const;

	uvmapcoord()
	{
	}

	void Clear( void )
	{
		mMapBiNormal = fvec3();
		mMapTangent = fvec3();
		mMapTexCoord = fvec2();
	}
};

///////////////////////////////////////////////////////////////////////////////

struct vertex
{
	static const int kmaxinfluences = 4;
	static const int kmaxcolors = 2;
	static const int kmaxuvs = 2;
	static const int kmaxconpoly = 8;

	fvec3	mPos;
	fvec3	mNrm;

	int				miNumWeights;
	int				miNumColors;
	int				miNumUvs;

	std::string	mJointNames[kmaxinfluences];

	fvec4	mCol[kmaxcolors];
	uvmapcoord	mUV[kmaxuvs];
	float		mJointWeights[kmaxinfluences];

	vertex()
		: miNumWeights( 0 )
		, miNumColors( 0 )
		, miNumUvs( 0 )
	{
		for( int i=0; i<kmaxcolors; i++ )
		{
			mCol[i] = fvec4::White();
		}
		for( int i=0; i<kmaxinfluences; i++ )
		{
			mJointNames[i] = "";
			mJointWeights[i] = float(0.0f);
		}
	}

	vertex Lerp( const vertex & vtx, float flerp ) const;
	void Lerp( const vertex& a, const vertex & b, float flerp );

	const fvec3& Pos() const { return mPos; }

	void Center( const vertex** pverts, int icnt );

	U64 Hash() const;

};

///////////////////////////////////////////////////////////////////////////////

struct vertexpool
{
	static const vertexpool	EmptyPool;

	HashU64IntMap			VertexPoolMap;
	orkvector<vertex>		VertexPool;

	int MergeVertex( const vertex & vtx, int idx=-1 );

	const vertex & GetVertex( int ivid ) const
	{
		OrkAssert( orkvector<vertex>::size_type(ivid)<VertexPool.size() );
		return VertexPool[ ivid ];
	}
	vertex & GetVertex( int ivid )
	{
		OrkAssert( orkvector<vertex>::size_type(ivid)<VertexPool.size() );
		return VertexPool[ ivid ];
	}

	size_t GetNumVertices( void ) const
	{
		return VertexPool.size();
	}

	vertexpool();
};

///////////////////////////////////////////////////////////////////////////////

struct AnnoMap
{
	orkmap<std::string,std::string>	mAnnotations;
	AnnoMap* Fork() const;
	static orkset<AnnoMap*>	gAllAnnoSets;
	void SetAnnotation( const std::string& key, const std::string& val );
	const std::string& GetAnnotation( const std::string& annoname ) const;

	AnnoMap();
	~AnnoMap();
};

///////////////////////////////////////////////////////////////////////////////

static const int kmaxsidesperpoly = 5;

class poly
{
	const AnnoMap*	mAnnotationSet;

public:

	static const U64 Inv = 0xffffffffffffffffL;
	const AnnoMap* GetAnnoMap() const { return mAnnotationSet; }
	void SetAnnoMap( const AnnoMap* pmap ) { mAnnotationSet=pmap; }

	const std::string& GetAnnotation( const std::string& annoname ) const;

	int				miVertices[kmaxsidesperpoly];
	U64				mEdges[kmaxsidesperpoly];
	int				miNumSides;

	int GetNumSides( void ) const { return miNumSides; }

	int GetVertexID( int i ) const
	{
		OrkAssert( i<miNumSides );
		return miVertices[ i ];
	}

	poly()
		: miNumSides( 0 )
		, mAnnotationSet(0)
	{
		for( int i=0; i<kmaxsidesperpoly; i++ )
		{
			miVertices[i] = -1;
			mEdges[i] = Inv;
		}
	}

	poly( int ia, int ib, int ic )
		: miNumSides( 3 )
		, mAnnotationSet(0)
	{
		miVertices[0] = ia;
		miVertices[1] = ib;
		miVertices[2] = ic;
		mEdges[0] = Inv;
		mEdges[1] = Inv;
		mEdges[2] = Inv;
		for(int i=3 ; i<kmaxsidesperpoly ; i++) {
			miVertices[i] = -1;
			mEdges[i] = Inv;
		}
	}

	poly( int ia, int ib, int ic, int id )
		: miNumSides( 4 )
		, mAnnotationSet(0)
	{
		miVertices[0] = ia;
		miVertices[1] = ib;
		miVertices[2] = ic;
		miVertices[3] = id;
		mEdges[0] = Inv;
		mEdges[1] = Inv;
		mEdges[2] = Inv;
		mEdges[3] = Inv;
		for(int i=4 ; i<kmaxsidesperpoly ; i++) {
			miVertices[i] = -1;
			mEdges[i] = Inv;
		}
	}

	poly( const int verts[], int numSides )
		: miNumSides(numSides)
		, mAnnotationSet(0)
	{
		for(int i=0 ; i<numSides ; i++) {
			miVertices[i] = verts[i];
			mEdges[i] = Inv;
		}
		for(int i=numSides ; i<kmaxsidesperpoly ; i++) {
			miVertices[i] = -1;
			mEdges[i] = Inv;
		}
	}

	// vertex clockwise around the poly from the given one
	int VertexCW(int vert) const;
	// vertex counter-clockwise around the poly from the given one
	int VertexCCW(int vert) const;

	vertex ComputeCenter( const vertexpool &vpool ) const;
	float ComputeEdgeLength( const vertexpool &vpool, const fmtx4 & MatRange, int iedge ) const;
	float ComputeArea( const vertexpool &vpool, const fmtx4 & MatRange ) const;
	fvec3 ComputeNormal( const vertexpool& vpool) const;

	U64 HashIndices( void ) const;

};

///////////////////////////////////////////////////////////////////////////////

struct IndexTestContext
{
	int					iset;
	int					itest;
	orkset<int>			PairedIndices[2];
	orkset<int>			PairedIndicesCombined;
	orkset<int>			CornerIndices;
};

///////////////////////////////////////////////////////////////////////////////

class toolmesh;
struct submesh;

struct annopolylut
{
	orkmap<U64,const AnnoMap*>	mAnnoMap;
	virtual U64 HashItem( const submesh& tmesh, const poly& item ) const = 0;
	const AnnoMap* Find( const submesh& tmesh, const poly& item ) const;
};
struct annopolyposlut : public annopolylut
{
	virtual U64 HashItem( const submesh& tmesh, const poly& item ) const;
};


///////////////////////////////////////////////////////////////////////////////

struct submesh
{
	typedef orkmap<std::string,std::string> AnnotationMap;

	std::string						name;
	AnnotationMap					mAnnotations;
	float							mfSurfaceArea;
	vertexpool						mvpool;
	HashU64IntMap					mpolyhashmap;
	orkvector<edge>					mEdges;
	HashU64IntMap					mEdgeMap;
	orkvector<poly>					mMergedPolys;
	int								mPolyTypeCounter[kmaxsidesperpoly];
	bool							mbMergeEdges;

	/////////////////////////////////////
	// these are mutable so we can get bounding boxes faster with const refs to toolmesh's
	mutable AABox					mAABox;
	mutable bool					mAABoxDirty;
	/////////////////////////////////////

	void SplitOnAnno( toolmesh& out, const std::string& annokey ) const;
	void SplitOnAnno( toolmesh& out, const std::string& prefix, const std::string& annokey ) const;
	void SplitOnAnno( toolmesh& out, const std::string& annokey, const AnnotationMap& MergeAnnos ) const;
	void ImportPolyAnnotations( const annopolylut& apl );
	void ExportPolyAnnotations( annopolylut& apl ) const;

	void SetAnnotation( const char* annokey, const char* annoval );
	const char* GetAnnotation( const char* annokey ) const;
	AnnotationMap& RefAnnotations() { return mAnnotations; }
	const AnnotationMap& RefAnnotations() const { return mAnnotations; }

	void MergeAnnos( const AnnotationMap& mrgannos, bool boverwrite );

	//////////////////////////////////////////////////////////////////////////////

	const vertexpool&		RefVertexPool() const { return mvpool; }
	const edge&				RefEdge( U64 edgekey ) const;
	poly&					RefPoly( int i );
	const poly&				RefPoly( int i ) const;
	const orkvector<poly>&	RefPolys() const;

	//////////////////////////////////////////////////////////////////////////////

	int MergeVertex( const vertex& vtx, int idx=-1 );
	U64 MergeEdge( const edge& ed, int ipolyindex=-1 );
	void MergePoly( const poly& ply );
	void MergeSubMesh( const submesh& oth );

	//////////////////////////////////////////////////////////////////////////////

	int	GetNumPolys( int inumsides=0 ) const;
	void FindNSidedPolys( orkvector<int>& output, int inumsides ) const;
	void GetConnectedPolys( const edge& ed, orkset<int>& output ) const;
	void GetEdges( const poly& ply, orkvector<edge>& Edges ) const;
	void GetAdjacentPolys( int ply, orkset<int>& output ) const;
	const U64 GetEdgeBetween( int a, int b ) const;

	const AABox& GetAABox() const;

	/////////////////////////////////////////////////////////////////////////

	void SetVertexPool( const vertexpool & vpool ) { mvpool = vpool; }

	/////////////////////////////////////////////////////////////////////////

	void Triangulate( submesh *poutsmesh ) const;
	void TrianglesToQuads( submesh *poutsmesh ) const;
	void SubDivQuads( submesh *poutsmesh ) const;
	void SubDivTriangles( submesh *poutsmesh ) const;
	void SubDiv( submesh *poutsmesh ) const;

	/////////////////////////////////////////////////////////////////////////

	submesh(const vertexpool& vpool = vertexpool::EmptyPool);
	~submesh();
	
};

///////////////////////////////////////////////////////////////////////////////

class toolmesh
{
	orkmap<std::string,ork::tool::SColladaMaterial*>	mMaterialsByShadingGroup;
	orkmap<std::string,ork::tool::SColladaMaterial*>	mMaterialsByName;

	fvec4							mRangeScale;
	fvec4							mRangeTranslate;
	fmtx4							mMatRange;
	orkmap<std::string,std::string>		mAnnotations;
	orklut<std::string, submesh*>		mPolyGroupLut;
	material_semanticmap_t				mShadingGroupToMaterialMap;
	LightContainer						mLights;
	bool								mbMergeEdges;
	ork::lev2::MaterialMap				mFxmMaterialMap;

public:

	void SetMergeEdges( bool bflg ) { mbMergeEdges=bflg; }

	/////////////////////////////////////////////////////////////////////////
	
	void Dump(const std::string& comment) const;

	/////////////////////////////////////////////////////////////////////////

	const ork::lev2::MaterialMap& RefFxmMaterialMap() const { return mFxmMaterialMap; }
	const orkmap<std::string,ork::tool::SColladaMaterial*>& RefMaterialsBySG() const { return mMaterialsByShadingGroup; }
	const orkmap<std::string,ork::tool::SColladaMaterial*>& RefMaterialsByName() const { return mMaterialsByName; }
	const LightContainer& RefLightContainer() const { return mLights; }
	LightContainer& RefLightContainer() { return mLights; }
	
	void CopyMaterialsFromToolMesh( const toolmesh& from );
	void MergeMaterialsFromToolMesh( const toolmesh& from );

	void RemoveSubMesh( const std::string& pgroup );
	void Prune();

	/////////////////////////////////////////////////////////////////////////

	void SetAnnotation( const char* annokey, const char* annoval );
	const char* GetAnnotation( const char* annokey ) const;

	/////////////////////////////////////////////////////////////////////////
	void WriteToWavefrontObj( const file::Path& outpath ) const;
	void WriteToDaeFile( const file::Path& outpath, const DaeWriteOpts& writeopts ) const;
	void WriteToRgmFile( const file::Path& outpath ) const;
	void WriteToXgmFile( const file::Path& outpath ) const;
	void WriteAuto( const file::Path& outpath ) const;
	/////////////////////////////////////////////////////////////////////////
	void ReadFromXGM( const file::Path& inpath );
	void ReadFromWavefrontObj( const file::Path& inpath );
	void ReadFromDaeFile( const file::Path& inpath, DaeReadOpts& readopts );

	/////////////////////////////////////////////////////////////////////////
#if defined(_USE_D3DX)
	void WriteToD3DXFile( const file::Path& outpath ) const;
	void ReadFromD3DXFile( const file::Path& inpath );
#endif
	/////////////////////////////////////////////////////////////////////////

	void ReadAuto( const file::Path& outpath );

	/////////////////////////////////////////////////////////////////////////

	AABox GetAABox() const;

	/////////////////////////////////////////////////////////////////////////

	void SetRangeTransform( const fvec4 &VScale, const fvec4 & VTrans );

	/////////////////////////////////////////////////////////////////////////
	
	void MergeToolMeshAs( const toolmesh & sr, const char* pgroupname );
	void MergeToolMeshThreadedExcluding( const toolmesh & sr, int inumthreads, const std::set<std::string>& ExcludeSet );
	void MergeToolMeshThreaded( const toolmesh & sr, int inumthreads );
	void MergeSubMesh( const toolmesh& src, const submesh* pgrp, const char* newname );
	void MergeSubMesh( const submesh& pgrp, const char* newname );
	void MergeSubMesh( const submesh& pgrp );
	submesh& MergeSubMesh( const char* pname );
	submesh& MergeSubMesh( const char* pname, const submesh::AnnotationMap& merge_annos );

	/////////////////////////////////////////////////////////////////////////

	const orklut<std::string, submesh*>& RefSubMeshLut() const;
	const material_semanticmap_t& RefShadingGroupToMaterialMap() const { return mShadingGroupToMaterialMap; }
	material_semanticmap_t& RefShadingGroupToMaterialMap() { return mShadingGroupToMaterialMap; }
	
	int GetNumSubMeshes() const { return int(mPolyGroupLut.size()); }

	const submesh* FindSubMeshFromMaterialName(const std::string& materialname ) const;
	submesh* FindSubMeshFromMaterialName(const std::string& materialname );
	const submesh* FindSubMesh(const std::string& grpname ) const;
	submesh* FindSubMesh(const std::string& grpname );

	/////////////////////////////////////////////////////////////////////////

	toolmesh();
	~toolmesh();

private:

	toolmesh(const toolmesh&oth) 
	{
		OrkAssert(false);
	}

};

///////////////////////////////////////////////////////////////////////////////
/*
struct TriStripperPrimGroup
{
	orkvector<unsigned int> mIndices;
};

class TriStripper
{
	triangle_stripper::tri_stripper tristripper;
	orkvector<TriStripperPrimGroup> mStripGroups;
	TriStripperPrimGroup			mTriGroup;

public:

	TriStripper( const std::vector<unsigned int> &InTriIndices, int icachesize, int iminstripsize );

	const orkvector<TriStripperPrimGroup> & GetStripGroups( void ) const
	{
		return mStripGroups;
	}

	const orkvector<unsigned int> & GetStripIndices( int igroup ) const
	{
		return mStripGroups[igroup].mIndices;
	}

	const orkvector<unsigned int> & GetTriIndices( void ) const { return mTriGroup.mIndices; }

};
*/

///////////////////////////////////////////////////////////////////////////////

class OBJ_OBJ_Filter : public ork::tool::CAssetFilterBase
{
	RttiDeclareConcrete(OBJ_OBJ_Filter,ork::tool::CAssetFilterBase);
public: //
	OBJ_OBJ_Filter(  );
	bool ConvertAsset( const tokenlist& toklist ) final;
};
class D3DX_OBJ_Filter : public ork::tool::CAssetFilterBase
{
	RttiDeclareConcrete(D3DX_OBJ_Filter,ork::tool::CAssetFilterBase);
public: //
	D3DX_OBJ_Filter(  );
	bool ConvertAsset( const tokenlist& toklist ) final;
};
class XGM_OBJ_Filter : public ork::tool::CAssetFilterBase
{
	RttiDeclareConcrete(XGM_OBJ_Filter,ork::tool::CAssetFilterBase);
public: //
	XGM_OBJ_Filter(  );
	bool ConvertAsset( const tokenlist& toklist ) final;
};
class OBJ_XGM_Filter : public ork::tool::CAssetFilterBase
{
	RttiDeclareConcrete(OBJ_XGM_Filter,ork::tool::CAssetFilterBase);
public: //
	OBJ_XGM_Filter(  );
	bool ConvertAsset( const tokenlist& toklist ) final;
};

///////////////////////////////////////////////////////////////////////////////

class AmbientLight : public Light
{
	RttiDeclareConcrete(AmbientLight, Light);
public:
	AmbientLight(){}
	bool AffectsSphere( const fvec3& center, float radius ) const final { return true; }
	bool AffectsAABox( const AABox& aab ) const final { return true; }
};

///////////////////////////////////////////////////////////////////////////////

struct DirLight : public Light
{
	RttiDeclareConcrete(DirLight, Light);
public:
	fvec3	mFrom;
	fvec3	mTo;

	DirLight() {}

	bool AffectsSphere( const fvec3& center, float radius ) const final { return true; }
	bool AffectsAABox( const AABox& aab ) const final { return true; }

};

///////////////////////////////////////////////////////////////////////////////

class PointLight : public Light
{
	RttiDeclareConcrete(PointLight, Light);
public:
	fvec3	mPoint;
	float		mFalloff;
	float		mRadius;

	PointLight() : mFalloff(1.0f), mRadius(0.0f) {}

	bool AffectsSphere( const fvec3& center, float radius ) const final ;
	bool AffectsAABox( const AABox& aab ) const final ;
};

///////////////////////////////////////////////////////////////////////////////

struct FlatSubMesh
{
	lev2::EVtxStreamFormat				evtxformat;
	orkvector<int>						TrianglePolyIndices;
	orkvector<int>						QuadPolyIndices;
	orkvector<int>						MergeTriIndices;
	orkvector<lev2::SVtxV12N12B12T16>	MergeVertsT16;
	orkvector<lev2::SVtxV12N12B12T8C4>	MergeVertsT8;

	int inumverts;
	int ivtxsize;
	void* poutvtxdata;

	FlatSubMesh( const submesh& mesh );

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if defined(_USE_D3DX)
class XGM_D3DX_Filter : public ork::tool::CAssetFilterBase
{
	RttiDeclareConcrete(XGM_D3DX_Filter,ork::tool::CAssetFilterBase);
public: //
	XGM_D3DX_Filter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};
class OBJ_D3DX_Filter : public ork::tool::CAssetFilterBase
{
	RttiDeclareConcrete(OBJ_D3DX_Filter,ork::tool::CAssetFilterBase);
public: //
	OBJ_D3DX_Filter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};
struct UvAtlasContext
{
	const submesh&						mInpMesh;
	submesh&							mOutMesh;
	float								mfGutter;
	float								mfStretching;
	int									miTexRes;
	bool								mbDoIMT;
	float								mfAreaUnification;
	ork::file::Path						mIMTTexturePath;
	std::string							mGroupName;

	UvAtlasContext( const submesh& inmesh, submesh& outmesh );
};


bool UvAtlasSubMesh( const UvAtlasContext& Ctx );
bool UvAtlasSubMesh2( const UvAtlasContext& Ctx );
void GenerateUVAtlas( const tokenlist& options );
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} } // namespace MeshUtil


