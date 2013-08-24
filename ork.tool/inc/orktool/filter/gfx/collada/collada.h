////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/filter/filter.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
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

	inline void Add( ork::lev2::EVtxStreamFormat efmt );

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
	bool						mbIsSkinned;
	EUnits						mUnits;
	bool						mDDSInputOnly;

	ColladaExportPolicy()
		: miNumBonesPerCluster(0), mbIsSkinned(false), mUnits(UNITS_ANY), mDDSInputOnly(false) {}
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

class FCDocument;
class FCDGeometryMesh;
class FCDEffectStandard;
class FCDEffectProfileFX;
class FCDAsset;

namespace ork { namespace tool {
	
CMatrix4 FCDMatrixToCMatrix4( const FMMatrix44 & inmat );

///////////////////////////////////////////////////////////////////////////////

struct SColladaMaterialChannel
{
	std::string						mTextureName;
	std::string						mPlacementNodeName;
	float							mRepeatU;
	float							mRepeatV;

	SColladaMaterialChannel()
		: mRepeatU( 1.0f )
		, mRepeatV( 1.0f )
	{
	}

};

///////////////////////////////////////////////////////////////////////////////

struct SColladaMaterial
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
	SColladaMaterialChannel				mDiffuseMapChannel;
	SColladaMaterialChannel				mSpecularMapChannel;
	SColladaMaterialChannel				mNormalMapChannel;
	SColladaMaterialChannel				mAmbientMapChannel;

	ork::lev2::GfxMaterial*				mpOrkMaterial;
	CVector4							mEmissiveColor;
	CVector4							mTransparencyColor;
	FCDEffect*							mFx;
	FCDEffectProfileFX*					mFxProfile;
	FCDEffectStandard*					mStdProfile;
	FCDEffectStandard::TransparencyMode	mTransparencyMode;
	orkmap<std::string,std::string>		mAnnotations;
	
	SColladaMaterial();

	void ParseFxMaterial( FCDEffectProfileFX *FxProf );
	void ParseStdMaterial( FCDEffectStandard *StdProf );
	void ParseMaterial( FCDocument* doc, const std::string & ShadingGroupName, const std::string& MaterialName );

};

///////////////////////////////////////////////////////////////////////////////

struct SColladaVertexWeightingInfo
{
	static const int kmaxweights = 4;

	ork::lev2::XgmSkelNode*		mpSkelNodes[kmaxweights];
	CReal						mWeighting[kmaxweights];
	int							miNumWeights;

	SColladaVertexWeightingInfo()
	{
		miNumWeights = 0;

		for( int i=0; i<kmaxweights; i++ )
		{
			mpSkelNodes[i] = 0;
			mWeighting[i] = CReal(0.0f);
		}
	}
	
};

///////////////////////////////////////////////////////////////////////////////

struct SColladaMeshConfigurationFlags
{
	bool	mbSkinned;

	SColladaMeshConfigurationFlags()
		: mbSkinned( false )
	{

	}

};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterTri
{
	ork::MeshUtil::vertex Vertex[3];
};

///////////////////////////////////////////////////////////////////////////////

struct SColladaMatGroup;

///////////////////////////////////////////////////////////////////////////////

class XgmClusterBuilder : public ork::Object
{	
	RttiDeclareAbstract(XgmClusterBuilder,ork::Object);
public:
	ork::MeshUtil::submesh			mSubMesh;
	lev2::VertexBufferBase*			mpVertexBuffer;
	//////////////////////////////////////////////////
	XgmClusterBuilder();
	virtual ~XgmClusterBuilder();
	//////////////////////////////////////////////////
	virtual bool AddTriangle( const XgmClusterTri& Triangle ) = 0;
	virtual void BuildVertexBuffer( const SColladaMatGroup& matgroup ) = 0;
	//////////////////////////////////////////////////
	void Dump( void );
	///////////////////////////////////////////////////////////////////
	// Build Vertex Buffers
	///////////////////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

class XgmSkinnedClusterBuilder : public XgmClusterBuilder
{
	RttiDeclareAbstract(XgmSkinnedClusterBuilder,XgmClusterBuilder);
	/////////////////////////////////////////////////
public:
	const orkmap<std::string,int>& RefBoneRegMap() const { return mmBoneRegMap; }
private:
	bool AddTriangle( const XgmClusterTri& Triangle );
	int FindNewBoneIndex( const std::string& BoneName );
	void BuildVertexBuffer_V12N12T8I4W4();
	void BuildVertexBuffer_V12N12B12T8I4W4();
	void BuildVertexBuffer_V12N6I1T4();
	void BuildVertexBuffer( const SColladaMatGroup& matgroup ); // virtual

	orkmap<std::string,int>			mmBoneRegMap;
};

///////////////////////////////////////////////////////////////////////////////

class XgmRigidClusterBuilder : public XgmClusterBuilder
{
	RttiDeclareAbstract(XgmRigidClusterBuilder,XgmClusterBuilder);
	/////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle );
	void BuildVertexBuffer_V12N6C2T4();
	void BuildVertexBuffer_V12N12B12T8C4();
	void BuildVertexBuffer_V12N12T16C4();
	void BuildVertexBuffer_V12N12B12T16();
	void BuildVertexBuffer( const SColladaMatGroup& matgroup ); // virtual
};

///////////////////////////////////////////////////////////////////////////////

class XgmClusterizer
{
public:
	///////////////////////////////////////////////////////
	XgmClusterizer();
	virtual ~XgmClusterizer();
	///////////////////////////////////////////////////////
	virtual bool AddTriangle( const XgmClusterTri& Triangle, const SColladaMatGroup* cmg ) = 0;
	virtual void Begin() {}
	virtual void End() {}
	///////////////////////////////////////////////////////
	size_t GetNumClusters() const { return ClusterVect.size(); }
	XgmClusterBuilder* GetCluster(int idx) const { return ClusterVect[idx]; }
protected:
	///////////////////////////////////////////////////////
	orkvector< XgmClusterBuilder* > ClusterVect;
	///////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class XgmClusterizerDiced : public XgmClusterizer
{
public:
	///////////////////////////////////////////////////////
	XgmClusterizerDiced();
	virtual ~XgmClusterizerDiced();
	///////////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle, const SColladaMatGroup* cmg );
	void Begin(); // virtual
	void End(); // virtual
	///////////////////////////////////////////////////////
private:
	ork::MeshUtil::submesh mPreDicedMesh;
};

///////////////////////////////////////////////////////////////////////////////

class XgmClusterizerStd : public XgmClusterizer
{
public:
	///////////////////////////////////////////////////////
	XgmClusterizerStd();
	virtual ~XgmClusterizerStd();
	///////////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle, const SColladaMatGroup* cmg );
	///////////////////////////////////////////////////////
private:
};

///////////////////////////////////////////////////////////////////////////////

class SColladaMatGroup
{
public:

	enum EMatClass
	{
		EMATCLASS_STANDARD = 0,
		EMATCLASS_FX,
	};

	void Parse( const SColladaMaterial& colladamat );
	
	std::string									mShadingGroupName;
	SColladaMeshConfigurationFlags				mMeshConfigurationFlags;
	EMatClass									meMaterialClass;
	ork::lev2::GfxMaterial*						mpOrkMaterial;
	orkvector<ork::lev2::VertexConfig>			mVertexConfigData;
	orkvector<ork::lev2::VertexConfig>			mAvailVertexConfigData;
	lev2::EVtxStreamFormat						meVtxFormat;
	ork::file::Path								mLightMapPath;
	bool										mbVertexLit;

	///////////////////////////////////////////////////////////////////
	// Build Clusters
	///////////////////////////////////////////////////////////////////

	void BuildTriStripXgmCluster( lev2::XgmCluster & XgmCluster, const XgmClusterBuilder *pclusbuilder );

	lev2::EVtxStreamFormat GetVtxStreamFormat() const { return meVtxFormat; }

	void ComputeVtxStreamFormat();

	XgmClusterizer* GetClusterizer() const { return mClusterizer; }
	void SetClusterizer( XgmClusterizer* pcl ) { mClusterizer=pcl; }

	///////////////////////////////////////////////////////////////////

	SColladaMatGroup()
		: meMaterialClass( EMATCLASS_STANDARD )
		, mpOrkMaterial( 0 )
		, mClusterizer( 0 )
		, mbVertexLit(false)
	{
	}

private:
	XgmClusterizer*								mClusterizer;

};

///////////////////////////////////////////////////////////////////////////////

class SColladaMesh
{
	orkvector<SColladaMatGroup*>			mMatGroups;
	orkvector<SColladaVertexWeightingInfo>	mVertexWeighting;
	bool									mbSkinned;
	std::string								mMeshName;

public:

	SColladaMesh() : mbSkinned(false) {}

	void SetSkinned( bool bv ) { mbSkinned=bv; }
	bool IsSkinned( void ) const { return mbSkinned; }
	orkvector<SColladaMatGroup*>& RefMatGroups( void ) { return mMatGroups; }
	orkvector<SColladaVertexWeightingInfo>& RefWeightingInfo( void ) { return mVertexWeighting; }
	void SetMeshName( const std::string& name ) { mMeshName=name; }
	const std::string& GetMeshName() const { return mMeshName; }
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
	orkmap<std::string,SColladaMaterial>		mMaterialMap;
	orkmap<std::string,std::string>				mMaterialSemanticBindingMap;
	orkmap<std::string,ork::lev2::XgmSkelNode*>	mSkeleton;
	ork::lev2::XgmSkelNode*						mSkeletonRoot;
	CVector3									mAABoundXYZ;
	CVector3									mAABoundWHD;
	CMatrix4									mBindShapeMatrix;
	CMatrix4									mTopNodesMatrix;
	DaeReadOpts									mReadOpts;

	orkvector<std::string>						mVertexColorSources;
	orkvector<std::string>						mVertexUvSources;
	orkset<lev2::TextureAsset*>					mTextures;

	static CColladaModel* Load( const AssetPath & fname );
	bool FindDaeMeshes();
	bool ParseGeometries();
	void ParseMaterial( SColladaMatGroup * MatGroup );

	bool ParseMaterialBindings();
	bool ParseControllers( );
	
	bool BuildXgmTriStripModel();
	void BuildXgmTriStripMesh( lev2::XgmMesh& XgmMesh, SColladaMesh* ColMesh );
	bool BuildXgmSkeleton();

	bool ConvertTextures(const file::Path& outmdlpth, ork::tool::FilterOptMap& options );

	const SColladaMaterial & GetMaterialFromShadingGroup( const std::string & ShadingGroupName ) const;

	bool IsSkinned() const { return mSkeleton.size()>0; }

	typedef orkvector<ork::CMatrix4> MatrixVector;

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

	orkvector<CMatrix4>				mSampledFrames;

public:

	void SetParam( int iframe, int irow, int icol, CReal fval );

	ColladaMatrixAnimChannel( const std::string & Name )
		: ColladaAnimChannel(Name)
		, miSettingFrame(0)
	{
	}

	int GetNumFrames() const { return int(mSampledFrames.size()); } // virtual

	const CMatrix4& GetFrame( int idx ) const { return mSampledFrames[idx]; }
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

	//void SetParam( int iframe, int irow, int icol, CReal fval );

	ColladaUvAnimChannel( const std::string & Name )
		: ColladaAnimChannel(Name)
		, miSettingFrame(0)
	{
	}

	int GetNumFrames() const { return int(mSampledFrames.size()); } // virtual

	void SetMaterialName( const char* pname ) { mMaterialName=pname; }
	const std::string& GetMaterialName() const { return mMaterialName; }

	const Place2dData& GetFrame( int idx ) const { return mSampledFrames[idx]; }

	void SetData( int iframe, const std::string& itemname, float fval );
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class ColladaFxAnimChannel : public ColladaAnimChannel
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(ColladaUvAnimChannel,ColladaAnimChannel);

	orkvector<T>								mSampledFrames;

public:

	ork::lev2::GfxMaterialFxParamArtist<T>*		mTargetParameter;
	std::string									mTargetMaterialName;
	std::string									mTargetPropertyName;

	ColladaFxAnimChannel( const std::string & ChannelName, const std::string & TargetMaterialName, const std::string & TargetPropertyName )
		: ColladaAnimChannel( ChannelName )
		, mTargetParameter(0)
		, mTargetMaterialName( TargetMaterialName )
		, mTargetPropertyName( TargetPropertyName )
	{
	}
	const T& GetFrame( int idx ) const { return mSampledFrames[idx]; }
	int GetNumFrames() const { return int(mSampledFrames.size()); } // virtual
	void AddFrame( const T& val ) { mSampledFrames.push_back(val); }

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
	orkmap<std::string,std::string>							mShadingGroupMap;
	orkmap<std::string,SColladaMaterial>					mMaterialMap;
	orkmap<std::string,ork::lev2::GfxMaterialFxParamBase*>	mFxAnimatables;
	orkmap<std::string,ColladaUvAnimChannel*>				mUvAnimatables;

	///////////////////////////////////////////////////////////////////

	orkmap<std::string,CMatrix4> mPose;

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

class DAEXGMFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(DAEXGMFilter,CAssetFilterBase);
	bool ConvertTextures( CColladaModel* mdl, const file::Path& outmdlpth );
public: //
	DAEXGMFilter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};
class DAEGGMFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(DAEGGMFilter,CAssetFilterBase);
	bool ConvertTextures( CColladaModel* mdl, const file::Path& outmdlpth );
public: //
	DAEGGMFilter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};
class DAEXGAFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(DAEXGAFilter,CAssetFilterBase);
public: //
	DAEXGAFilter(  );
	virtual bool ConvertAsset( const tokenlist& toklist );
};

///////////////////////////////////////////////////////////////////////////////

} }

///////////////////////////////////////////////////////////////////////////////
#endif // USE_FCOLLADA
