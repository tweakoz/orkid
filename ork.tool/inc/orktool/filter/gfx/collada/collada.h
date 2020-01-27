////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/filter/filter.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>

///////////////////////////////////////////////////////////////////////////////

#if defined(USE_FCOLLADA)
#include <FCollada.h>
#include <FCDocument/FCDocument.h>
#include <FCDocument/FCDMaterial.h>
#include <FCDocument/FCDImage.h>
#include <FCDocument/FCDTexture.h>
#include <FCDocument/FCDGeometrySource.h>
#include <FCDocument/FCDGeometryPolygons.h>
#include <FCDocument/FCDGeometryPolygonsTools.h> // For Triagulate
#include <FCDocument/FCDGeometryPolygonsInput.h>
#include <FCDocument/FCDEffectProfile.h>
#include <FCDocument/FCDEffect.h>
#include <FCDocument/FCDSceneNode.h>
#include <FCDocument/FCDGeometryInstance.h>
#include <FCDocument/FCDLibrary.h>
#include <FCDocument/FCDMaterialInstance.h>
#include <FCDocument/FCDEffectStandard.h>
#include <FCDocument/FCDEffectProfileFX.h>
#include <FCDocument/FCDEffectTechnique.h>
#include <FCDocument/FCDExtra.h>
#include <FCDocument/FCDEffectTools.h>
#include <FCDocument/FCDEffectParameterSampler.h>
#include <FCDocument/FCDEffectParameterSurface.h>

class FCDocument;
class FCDGeometryMesh;
class FCDEffectStandard;
class FCDEffectProfileFX;
class FCDAsset;

namespace ork::MeshUtil {
  struct ToolMaterialGroup;
}

namespace ork::tool {

///////////////////////////////////////////////////////////////////////////////

struct ColladaVertexFormat
{
	ork::lev2::EVtxStreamFormat	meVertexStreamFormat;

	int							miNumJoints;
	int							miNumColors;
	bool						mbNormals;
	int							miNumUvs;
	int							miNumBinormals;
	int							miNumTangents;
	int							miVertexSize;

	ColladaVertexFormat();
	void SetFormat( ork::lev2::EVtxStreamFormat efmt );
};

struct ColladaAvailVertexFormats
{
	typedef orkmap<ork::lev2::EVtxStreamFormat,ColladaVertexFormat> FormatMap;

	FormatMap	mFormats;

	void add( ork::lev2::EVtxStreamFormat efmt );

	const FormatMap& GetFormats() const { return mFormats; }
};

///////////////////////////////////////////////////////////////////////////////

struct ColladaSkinPolicy
{
	enum Weighting
	{
		EPOLICY_MATRIXPALETTESKIN_W1 = 0,
		EPOLICY_MATRIXPALETTESKIN_W4,
	};

	Weighting	mWeighting;
	ColladaSkinPolicy( Weighting epcy=EPOLICY_MATRIXPALETTESKIN_W1 ) : mWeighting(epcy) {}

};
struct ColladaNormalPolicy
{
	enum ENormalPolicy
	{
		ENP_JOIN_NORMALS = 0,
		ENP_ALLOW_SPLIT_NORMALS,
	};

	ENormalPolicy meNormalPolicy;

	ColladaNormalPolicy( ENormalPolicy epcy=ENP_ALLOW_SPLIT_NORMALS ) : meNormalPolicy(epcy) {}
};
struct ColladaPrimGroupPolicy
{
	enum MaxIndices
	{
		EPOLICY_MAXINDICES_NONE = 0,
		EPOLICY_MAXINDICES_WII,
	};

	MaxIndices	mMaxIndices;
	ColladaPrimGroupPolicy( MaxIndices epcy=EPOLICY_MAXINDICES_NONE ) : mMaxIndices(epcy) {}
};

struct ColladaReadComponentsPolicy
{
	enum ReadComponents
	{
		EPOLICY_READCOMPONENTS_NONE = 0,
		EPOLICY_READCOMPONENTS_POSITION = (1<<0),
		EPOLICY_READCOMPONENTS_NORMAL = (1<<1),
		EPOLICY_READCOMPONENTS_UV0 = (1<<2),
		EPOLICY_READCOMPONENTS_UV1 = (1<<3),
		EPOLICY_READCOMPONENTS_BIN0 = (1<<4),
		EPOLICY_READCOMPONENTS_BIN1 = (1<<5),
		EPOLICY_READCOMPONENTS_COLOR0 = (1<<6),
		EPOLICY_READCOMPONENTS_COLOR1 = (1<<7),
		EPOLICY_READCOMPONENTS_ALL = 0xFFFFFFFF
	};

	ReadComponents mReadComponents;
	ColladaReadComponentsPolicy(ReadComponents readcomp = EPOLICY_READCOMPONENTS_ALL ) : mReadComponents(readcomp) {}
};

struct ColladaTriangulationPolicy
{
public:
	enum EPolicy
	{
		ECTP_DONT_TRIANGULATE = 0,
		ECTP_TRIANGULATE,
	};


	ColladaTriangulationPolicy(EPolicy epcy = ECTP_DONT_TRIANGULATE ) : mePolicy(epcy) {}

	EPolicy GetPolicy() const { return mePolicy; }
	void SetPolicy( EPolicy epcy ) { mePolicy = epcy; }

private:

	EPolicy mePolicy;
};

///////////////////////////////////////////////////////////
// fixed grid dicer policy
///////////////////////////////////////////////////////////

struct ColladaDicingPolicy
{
public:

	enum EPolicy
	{
		ECTP_DONT_DICE = 0,
		ECTP_DICE,
	};

	ColladaDicingPolicy(EPolicy epcy = ECTP_DONT_DICE ) : mePolicy(epcy) {}

	EPolicy GetPolicy() const { return mePolicy; }
	void SetPolicy( EPolicy epcy ) { mePolicy = epcy; }

private:

	EPolicy			mePolicy;
};

typedef enum
{
	UNITS_ANY = -1,

	UNITS_CENTIMETER,
	UNITS_METER
} EUnits;

struct ColladaExportPolicy : public ork::util::Context<ColladaExportPolicy>
{
	ColladaSkinPolicy			mSkinPolicy;
	ColladaNormalPolicy			mNormalPolicy;
	ColladaPrimGroupPolicy		mPrimGroupPolicy;
	ColladaReadComponentsPolicy	mReadComponentsPolicy;
	ColladaAvailVertexFormats	mAvailableVertexFormats;
	ColladaTriangulationPolicy	mTriangulation;
	ColladaDicingPolicy			mDicingPolicy;
	int							miNumBonesPerCluster;
	std::string					mColladaInpName;
	std::string					mColladaOutName;
	bool						mbisSkinned;
	EUnits						mUnits;
	bool						mDDSInputOnly;

	ColladaExportPolicy()
		: miNumBonesPerCluster(0), mbisSkinned(false), mUnits(UNITS_ANY), mDDSInputOnly(false) {}
};

///////////////////////////////////////////////////////////////////////////////

struct DaeReadOpts
{
	std::set<std::string>	mReadLayers;
	std::set<std::string>	mExcludeLayers;
	bool					mbMergeMeshShGrpName;
	int						miNumThreads;
	bool					mbEmptyLayers;
	DaeReadOpts()
		: mbMergeMeshShGrpName(false)
		, mbEmptyLayers(false)
		, miNumThreads(1) {}
};

///////////////////////////////////////////////////////////////////////////////

struct DaeWriteOpts
{
	enum EMaterialSetup
	{
		EMS_NOMATERIALS = 0,
		EMS_DEFTEXMATERIALS,
		EMS_PRESERVEMATERIALS,
	};


	ork::file::Path				mTextureBase;
	EMaterialSetup				meMaterialSetup;
	DaeWriteOpts() : meMaterialSetup(EMS_NOMATERIALS) {}
};

///////////////////////////////////////////////////////////////////////////////

struct DaeExtraNode
{
	std::map<std::string,DaeExtraNode*>	mChildren;
	std::string							mValue;
};

///////////////////////////////////////////////////////////////////////////////


fmtx4 FCDMatrixTofmtx4( const FMMatrix44 & inmat );

///////////////////////////////////////////////////////////////////////////////

struct ColladaMaterialChannel
{
	std::string						mTextureName;
	std::string						mPlacementNodeName;
	float							mRepeatU;
	float							mRepeatV;

	ColladaMaterialChannel()
		: mRepeatU( 1.0f )
		, mRepeatV( 1.0f )
	{
	}

};

///////////////////////////////////////////////////////////////////////////////

struct ColladaMaterial
{
	enum ELightingType
	{
		ELIGHTING_LAMBERT = 0,
		ELIGHTING_BLINN ,
		ELIGHTING_PHONG ,
		ELIGHTING_NONE
	};

	std::string							mShadingGroupName;
	std::string							mMaterialName;
	ELightingType						mLightingType;
	float								mSpecularPower;
	ColladaMaterialChannel				mDiffuseMapChannel;
	ColladaMaterialChannel				mSpecularMapChannel;
	ColladaMaterialChannel				mNormalMapChannel;
	ColladaMaterialChannel				mAmbientMapChannel;

	ork::lev2::GfxMaterial*				_orkMaterial;
	fvec4							    mEmissiveColor;
	fvec4							    mTransparencyColor;
	FCDEffect*							mFx;
	FCDMaterial*					    mFxProfile;
	FCDEffectStandard*					mStdProfile;
	FCDEffectStandard::TransparencyMode	mTransparencyMode;
	orkmap<std::string,std::string>		mAnnotations;

	ColladaMaterial();

	void ParseStdMaterial( FCDEffectStandard *StdProf );
	void ParseMaterial( FCDocument* doc, const std::string & ShadingGroupName, const std::string& MaterialName );

};

///////////////////////////////////////////////////////////////////////////////

struct SColladaVertexWeightingInfo
{
	static const int kmaxweights = 4;

	ork::lev2::XgmSkelNode*		mpSkelNodes[kmaxweights];
	float						mWeighting[kmaxweights];
	int							miNumWeights;

	SColladaVertexWeightingInfo()
	{
		miNumWeights = 0;

		for( int i=0; i<kmaxweights; i++ )
		{
			mpSkelNodes[i] = 0;
			mWeighting[i] = float(0.0f);
		}
	}

};

///////////////////////////////////////////////////////////////////////////////

class SColladaMesh
{
	orkvector<MeshUtil::ToolMaterialGroup*>			mMatGroups;
	orkvector<SColladaVertexWeightingInfo>	mVertexWeighting;
	bool									mbSkinned;
	std::string								mMeshName;

public:

	SColladaMesh() : mbSkinned(false) {}

	void SetSkinned( bool bv ) { mbSkinned=bv; }
	bool isSkinned( void ) const { return mbSkinned; }
	orkvector<MeshUtil::ToolMaterialGroup*>& RefMatGroups( void ) { return mMatGroups; }
	orkvector<SColladaVertexWeightingInfo>& RefWeightingInfo( void ) { return mVertexWeighting; }
	void SetMeshName( const std::string& name ) { mMeshName=name; }
	const std::string& meshName() const { return mMeshName; }
};

///////////////////////////////////////////////////////////////////////////////

class CColladaAsset
{
public:

	enum EAssetType
	{
		ECOLLADA_MODEL = 0,
		ECOLLADA_ANIM,
		ECOLLADA_END,
	};

	FCDocument*									mDocument;
	const std::string							mFileName;
	float										mUnitsPerMeter;

	CColladaAsset( const std::string & fname, EAssetType etype )
		: mFileName( fname )
		, meAssetType( etype )
	{
	}

	~CColladaAsset();

	static EAssetType GetAssetType( const AssetPath & fname );

	bool LoadDocument( const AssetPath & fname );

protected:

	EAssetType									meAssetType;
	FCDAsset*									mpColladaAsset;

};

///////////////////////////////////////////////////////////////////////////////

struct ColMeshRec
{
	FCDGeometryMesh* mdaemesh;
	SColladaMesh*	mcolmesh;
};

///////////////////////////////////////////////////////////////////////////////

class CColladaModel : public CColladaAsset
{
public:
	enum EUsage
	{
		EUSAGE_MODEL = 0,
		EUSAGE_CHARACTER,
		EUSAGE_COLLISION,
	};

	orkmap<std::string,ColMeshRec*>				mMeshIdMap;
	lev2::XgmModel 								mXgmModel;
	orkmap<std::string,ColladaMaterial>		mMaterialMap;
	MeshUtil::material_semanticmap_t			mMaterialSemanticBindingMap;
	orkmap<std::string,ork::lev2::XgmSkelNode*>	mSkeleton;
	ork::lev2::XgmSkelNode*						mSkeletonRoot;
	fvec3									mAABoundXYZ;
	fvec3									mAABoundWHD;
	fmtx4									mBindShapeMatrix;
	fmtx4									mTopNodesMatrix;
	DaeReadOpts									mReadOpts;

	orkvector<std::string>						mVertexColorSources;
	orkvector<std::string>						mVertexUvSources;
	orkset<lev2::TextureAsset*>					mTextures;

	static CColladaModel* Load( const AssetPath & fname );
	bool FindDaeMeshes();
	bool ParseGeometries();
	void ParseMaterial( MeshUtil::ToolMaterialGroup * MatGroup );

	bool ParseMaterialBindings();
	bool ParseControllers( );

	bool BuildXgmTriStripModel();
	void BuildXgmTriStripMesh( lev2::XgmMesh& XgmMesh, SColladaMesh* ColMesh );
	bool BuildXgmSkeleton();

	bool ConvertTextures(const file::Path& outmdlpth, ork::tool::FilterOptMap& options );

	const ColladaMaterial & GetMaterialFromShadingGroup( const std::string & ShadingGroupName ) const;

	bool isSkinned() const { return mSkeleton.size()>0; }

	typedef orkvector<ork::fmtx4> MatrixVector;

	void AddTexture( lev2::TextureAsset*ptex ) { mTextures.insert(ptex); }

	void GetNodeMatricesByName(MatrixVector& nodeMatrices, const char* nodeName);

	CColladaModel( const std::string & fname, const DaeReadOpts& opts )
		: CColladaAsset( fname, ECOLLADA_MODEL )
		, mSkeletonRoot( 0 )
		, mReadOpts( opts )
	{

	}

private:

};

///////////////////////////////////////////////////////////////////////////////

class ColladaAnimChannel : public ork::Object
{
	RttiDeclareAbstract(ColladaAnimChannel,ork::Object);

	std::string ChannelName;

public:

	ColladaAnimChannel( const std::string & name )
		: ChannelName( name )
	{
	}

	const std::string& GetName() const { return ChannelName; }
	virtual int GetNumFrames() const = 0;

};

///////////////////////////////////////////////////////////////////////////////

class ColladaMatrixAnimChannel : public ColladaAnimChannel
{
	RttiDeclareAbstract(ColladaMatrixAnimChannel,ColladaAnimChannel);

	int								miSettingFrame;

	orkvector<fmtx4>				mSampledFrames;

public:

	void SetParam( int iframe, int irow, int icol, float fval );

	ColladaMatrixAnimChannel( const std::string & Name )
		: ColladaAnimChannel(Name)
		, miSettingFrame(0)
	{
	}

	int GetNumFrames() const final { return int(mSampledFrames.size()); } // virtual

	const fmtx4& GetFrame( int idx ) const { return mSampledFrames[idx]; }
};

///////////////////////////////////////////////////////////////////////////////

struct Place2dData
{
	float	rotateUV;
	float	repeatU;
	float	repeatV;
	float	offsetU;
	float	offsetV;

	Place2dData();
};

class ColladaUvAnimChannel : public ColladaAnimChannel
{
	RttiDeclareAbstract(ColladaUvAnimChannel,ColladaAnimChannel);

	int								miSettingFrame;
	std::string						mMaterialName;

	orkvector<Place2dData>			mSampledFrames;

public:

	//void SetParam( int iframe, int irow, int icol, float fval );

	ColladaUvAnimChannel( const std::string & Name )
		: ColladaAnimChannel(Name)
		, miSettingFrame(0)
	{
	}

	int GetNumFrames() const final { return int(mSampledFrames.size()); }

	void SetMaterialName( const char* pname ) { mMaterialName=pname; }
	const std::string& GetMaterialName() const { return mMaterialName; }

	const Place2dData& GetFrame( int idx ) const { return mSampledFrames[idx]; }

	void SetData( int iframe, const std::string& itemname, float fval );
};

///////////////////////////////////////////////////////////////////////////////

class CColladaAnim : public CColladaAsset
{
public:

	typedef orkmap<std::string,ork::tool::ColladaAnimChannel*> ChannelsMap;

	ork::lev2::XgmAnim	mXgmAnim;

	float		mfSampleRate;
	int			miNumFrames;
	ChannelsMap	mAnimationChannels;

	///////////////////////////////////////////////////////////////////
	MeshUtil::material_semanticmap_t						mShadingGroupMap;
	orkmap<std::string,ColladaMaterial>					mMaterialMap;
	orkmap<std::string,ColladaUvAnimChannel*>				mUvAnimatables;

	///////////////////////////////////////////////////////////////////

	orkmap<std::string,fmtx4> mPose;

	static CColladaAnim* Load( const AssetPath & fname );

	void ParseMaterials();
	void ParseTextures();

	bool Parse();
	bool GetPose();

	CColladaAnim( const std::string & fname, const float ksamplerate=30.0f )
		: CColladaAsset( fname, ECOLLADA_ANIM )
		, mfSampleRate( ksamplerate )
		, miNumFrames( 0 )
	{
	}

	int GetNumFrames() const { return miNumFrames; }

	size_t GetNumChannels() const { return mAnimationChannels.size(); }

	const ChannelsMap & RefChannels() const { return mAnimationChannels; }

};

///////////////////////////////////////////////////////////////////////////////

class DAEXGMFilter : public AssetFilterBase
{
	RttiDeclareConcrete(DAEXGMFilter,AssetFilterBase);
	bool ConvertTextures( CColladaModel* mdl, const file::Path& outmdlpth );
public: //
	DAEXGMFilter(  );
	bool ConvertAsset( const tokenlist& toklist ) final;
};
class DAEXGAFilter : public AssetFilterBase
{
	RttiDeclareConcrete(DAEXGAFilter,AssetFilterBase);
public: //
	DAEXGAFilter(  );
	bool ConvertAsset( const tokenlist& toklist ) final;
};

}

///////////////////////////////////////////////////////////////////////////////
#endif // USE_FCOLLADA
