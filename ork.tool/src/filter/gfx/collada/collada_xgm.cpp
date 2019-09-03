///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/application/application.h>
#if defined(USE_FCOLLADA)
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <ork/lev2/lev2_asset.h>
#include "../meshutil/meshutil_stripper.h"

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::XgmClusterBuilder, "XgmClusterBuilder");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::XgmSkinnedClusterBuilder, "XgmSkinnedClusterBuilder");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::XgmRigidClusterBuilder, "XgmRigidClusterBuilder");

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::BuildXgmSkeleton( void )
{
	int inumjoints = mSkeleton.size();

	ork::lev2::XgmSkeleton & XgmSkel = mXgmModel.RefSkel();

	mXgmModel.SetSkinned( IsSkinned() );

	if( inumjoints>1 )
	{
		XgmSkel.mBindShapeMatrix = mBindShapeMatrix;

		XgmSkel.SetNumJoints( inumjoints );

		////////////////////////////////////////////////////////
		// assign linearized joint indices
		////////////////////////////////////////////////////////

		int ijntindex = 0;

		for( orkmap<std::string,ork::lev2::XgmSkelNode*>::iterator it=mSkeleton.begin(); it!=mSkeleton.end(); it++ )
		{
			ork::lev2::XgmSkelNode* SkelNode = (*it).second;
			SkelNode->miSkelIndex = ijntindex++;
		}

		////////////////////////////////////////////////////////
		// setup hierarchy
		////////////////////////////////////////////////////////

		for( orkmap<std::string,ork::lev2::XgmSkelNode*>::iterator it=mSkeleton.begin(); it!=mSkeleton.end(); it++ )
		{
			const std::string & JointName = (*it).first;
			ork::lev2::XgmSkelNode* SkelNode = (*it).second;
			const ork::lev2::XgmSkelNode* ParNode = SkelNode->mpParent;
			int idx = SkelNode->miSkelIndex;
			int pidx = ParNode ? ParNode->miSkelIndex : -1;
			PoolString JointNameSidx = AddPooledString( JointName.c_str() );
			XgmSkel.AddJoint( idx, pidx, JointNameSidx );
			XgmSkel.RefInverseBindMatrix(idx) = SkelNode->mBindMatrixInverse;
			XgmSkel.RefJointMatrix( idx ) = SkelNode->mJointMatrix;
		}

		////////////////////////////////////////////////////////
	}
	else
	{
		return true;
	}

	XgmSkel.mTopNodesMatrix = mTopNodesMatrix;

	/////////////////////////////////////
	// flatten the skeleton
	/////////////////////////////////////

	XgmSkel.miRootNode = mSkeletonRoot ? mSkeletonRoot->miSkelIndex : -1;

	if( mSkeletonRoot )
	{
		orkstack<lev2::XgmSkelNode *> NodeStack;
		NodeStack.push( mSkeletonRoot );
		while( false == NodeStack.empty() )
		{	lev2::XgmSkelNode *ParNode = NodeStack.top();
			int iparentindex = ParNode->miSkelIndex;
			NodeStack.pop();
			int inumchildren = ParNode->mChildren.size();
			for( int ic=0; ic<inumchildren; ic++ )
			{	lev2::XgmSkelNode *Child = ParNode->mChildren[ ic ];
				int ichildindex = Child->miSkelIndex;

				lev2::XgmBone Bone = { iparentindex, ichildindex };

				XgmSkel.AddFlatBone( Bone );
				NodeStack.push( Child );
			}
		}
	}
	XgmSkel.mpRootNode = mSkeletonRoot;
	//XgmSkel.dump();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

struct TexSetter
{
	orkmap<file::Path, ork::lev2::TextureAsset* > mTextureMap;

	ork::lev2::TextureAsset* get( const SColladaMaterialChannel & MatChanIn, const std::string & model_directory, std::string &ActualFileName )
	{
		ork::lev2::TextureAsset* htexture = 0;

		if( MatChanIn.mTextureName.length() )
		{
			file::Path::NameType texname = MatChanIn.mTextureName.c_str();
			file::Path::NameType texext = FileEnv::filespec_to_extension(MatChanIn.mTextureName.c_str());

			texname.replace_in_place("\\","/");

			printf( "model_directory<%s>\n", model_directory.c_str() );
			printf( "texname1<%s>\n", texname.c_str() );



			auto it_slash = texname.find("/");
			auto it_src_slash = texname.find("src/");
			const auto NPOS = file::Path::NameType::npos;

			printf( "texname2<%s>\n", texname.c_str() );
			printf( "it_slash<%zx>\n", it_slash );
			printf( "it_src_slash<%zx>\n", it_src_slash );

			///////////////////////////////////////////////////////////////
			// find data/src or data\\src in texture path
			///////////////////////////////////////////////////////////////

			if( NPOS == it_slash ){
				ActualFileName = texname.c_str();
			}
			else if( NPOS != it_src_slash )
			{
				it_src_slash+=4;
				texname = texname.substr( it_src_slash, texname.length()-it_src_slash );
				ActualFileName = CreateFormattedString("data/src/%s", texname.c_str() );
			}
			else
			{
				std::string mdir = model_directory;

				if( mdir.find( "data/pc/" ) == 0 )
				{
					mdir = CreateFormattedString( "data/src/%s", mdir.substr( 8, mdir.length()-8 ).c_str() );
				}
				ActualFileName = CreateFormattedString("%s/%s", mdir.c_str(), texname.c_str() );
			}

			///////////////////////////////////////////////////////////////
			// if texture exists, assign it..
			///////////////////////////////////////////////////////////////

			file::Path PathToTexture( ActualFileName.c_str() );

			auto itt = mTextureMap.find( PathToTexture );

			if( mTextureMap.end() == itt ){
				if( FileEnv::DoesFileExist( PathToTexture ) ){
					lev2::GfxTargetDummy DummyTarget;
					ork::lev2::TextureAsset *pl2tex = new ork::lev2::TextureAsset;
					ork::lev2::Texture* ptex = pl2tex->GetTexture();
					bool bOK = DummyTarget.TXI()->LoadTexture( PathToTexture, ptex );
					if( bOK ){
            printf( "loaded texture<%s>\n", PathToTexture.c_str() );
						ptex->SetTexClass( ork::lev2::Texture::ETEXCLASS_STATIC );
						pl2tex->SetName( ork::AddPooledString(PathToTexture.c_str()) );
						ptex->setProperty<std::string>( "abspath", PathToTexture.c_str() );
						htexture = 	pl2tex;
						mTextureMap[ PathToTexture ] = pl2tex;
					}
				}
			}
			else
			{
				htexture = itt->second;
			}
		}
		return htexture;
	}
};

///////////////////////////////////////////////////////////////////////////////

static TexSetter gtex_setter;

void ConfigureFxMaterial( CColladaModel *ColModel, SColladaMatGroup *ColMatGroup, lev2::XgmSubMesh & XgmClusSet )
{
	const bool bskinned = ColMatGroup->mMeshConfigurationFlags.mbSkinned;

	const std::string & ShadingGroupName = ColMatGroup->mShadingGroupName;
	const SColladaMaterial &ColladaMaterial = ColModel->GetMaterialFromShadingGroup( ShadingGroupName );

	// TODO: The dependency on the "data://" URL prefix should be removed so any URL can work.
	const file::Path::NameType mdlname = FileEnv::filespec_strip_base(ColModel->mFileName.c_str(), "data://");
	const file::Path::NameType model_directory = FileEnv::FilespecToContainingDirectory(mdlname);

	///////////////////////////////////////////////

	ork::lev2::GfxMaterial* pmat = ColMatGroup->mpOrkMaterial;

	ork::lev2::GfxMaterialFx* pmatfx = rtti::autocast( pmat );

	const ork::lev2::GfxMaterialFxEffectInstance& fxinst = pmatfx->GetEffectInstance();

	const orklut<std::string,ork::lev2::GfxMaterialFxParamBase*>& parms = fxinst.mParameterInstances;

	int inump = parms.size();

	for( int ip=0; ip<inump; ip++ )
	{
		lev2::GfxMaterialFxParamBase* param = fxinst.mParameterInstances.GetItemAtIndex(ip).second;

		if( param->GetRecord().meParameterType == ork::EPROPTYPE_SAMPLER )
		{
			lev2::GfxMaterialFxParamArtist<lev2::Texture*> *paramf = (lev2::GfxMaterialFxParamArtist<lev2::Texture*>*) param;

			const std::string initstr = paramf->GetInitString();

			SColladaMaterialChannel matchan;
			matchan.mTextureName = initstr;
			matchan.mRepeatU = 1.0f;
			matchan.mRepeatV = 1.0f;

			std::string foundtexname;

			ork::lev2::TextureAsset* ptexture = gtex_setter.get( matchan, model_directory.c_str(), foundtexname );

			if( ptexture )
			{
				ptexture->GetTexture()->setProperty<std::string>( "usage", param->GetRecord().mParameterName );
				ColModel->AddTexture( ptexture );
				paramf->mValue = ptexture->GetTexture();
			}

		}
	}


	///////////////////////////////////////////////

	OrkAssert( ColMatGroup->mpOrkMaterial != 0 );

	///////////////////////////////////////////////

	XgmClusSet.mpMaterial = ColMatGroup->mpOrkMaterial;
}

///////////////////////////////////////////////////////////////////////////////

void ConfigureStdMaterial( CColladaModel *ColModel, SColladaMatGroup *ColMatGroup, lev2::XgmSubMesh & XgmClusSet )
{
	const ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();

	const bool bskinned = ColMatGroup->mMeshConfigurationFlags.mbSkinned;

	const std::string & ShadingGroupName = ColMatGroup->mShadingGroupName;
	const SColladaMaterial &ColladaMaterial = ColModel->GetMaterialFromShadingGroup( ShadingGroupName );
	const std::string& MaterialName = ColladaMaterial.mMaterialName;

	const file::Path mdlname = FileEnv::GetPathFromUrlExt(ColModel->mFileName.c_str());
	const file::Path::NameType model_directory = FileEnv::FilespecToContainingDirectory(ColModel->mFileName.c_str());

	///////////////////////////////////////////////

	std::string diftexfname, nrmtexfname, spctexfname, ambtexfname;

	ork::lev2::TextureAsset* DiffuseTex = gtex_setter.get( ColladaMaterial.mDiffuseMapChannel, model_directory.c_str(), diftexfname );
	ork::lev2::TextureAsset* NormalTex = gtex_setter.get( ColladaMaterial.mNormalMapChannel, model_directory.c_str(), nrmtexfname );
	ork::lev2::TextureAsset* SpecularTex = gtex_setter.get( ColladaMaterial.mSpecularMapChannel, model_directory.c_str(), spctexfname );
	ork::lev2::TextureAsset* AmbientTex = gtex_setter.get( ColladaMaterial.mAmbientMapChannel, model_directory.c_str(), ambtexfname );

	if( DiffuseTex )
	{
		DiffuseTex->GetTexture()->setProperty<std::string>( "usage", "diffusemap" );
		ColModel->AddTexture( DiffuseTex );
	}
	if( NormalTex )
	{
		NormalTex->GetTexture()->setProperty<std::string>( "usage", "normalmap" );
		ColModel->AddTexture( NormalTex );
	}
	if( SpecularTex )
	{
		SpecularTex->GetTexture()->setProperty<std::string>( "usage", "specularmap" );
		ColModel->AddTexture( SpecularTex );
	}
	if( AmbientTex )
	{
		AmbientTex->GetTexture()->setProperty<std::string>( "usage", "ambientmap" );
		ColModel->AddTexture( AmbientTex );
	}

	///////////////////////////////////////////////

	bool bNormalMapPresent = (0!=NormalTex);
	bool bDiffuseMapPresent = (0!=DiffuseTex);

	if( 0 == DiffuseTex )
	{
		orkerrorlog( "WARNING:  <ColladaModel %s> <ShadingGroup %s> <Texture %s> UhOh, There is no diffuse texture (or it can't be found). Dont be suprised when your model is fubar!\n", ColModel->mFileName.c_str(), ShadingGroupName.c_str(), diftexfname.c_str() );
	}
	if( ShadingGroupName == "initialShadingGroup" )
	{
		orkerrorlog( "WARNING: <ColladaModel %s> Why the fuck are you using initialShadingGroup?!\n", ColModel->mFileName.c_str() );
	}
	///////////////////////////////////////////////

	std::string stdtechname;

	switch( ColladaMaterial.mLightingType )
	{
		case SColladaMaterial::ELIGHTING_LAMBERT:
			stdtechname +=		bNormalMapPresent
							? 	"/lambert/tex/bump"
							:	bDiffuseMapPresent
								?	"/lambert/tex"
								:	"/modvtx";
			break;
		case SColladaMaterial::ELIGHTING_BLINN:
			stdtechname += bNormalMapPresent ? "/blinn/tex/bump" : "/lambert/tex";
			break;
		case SColladaMaterial::ELIGHTING_PHONG:
			stdtechname += bNormalMapPresent ? "/phong/tex/bump" : "/lambert/tex";
			break;
		default:
			stdtechname += "/modvtx";
			orkerrorlog( "WARNING: <ColladaModel %s> you have a mesh with no material on it!\n", ColModel->mFileName.c_str() );
			//OrkAssert( false );
			break;
	}

	if( bskinned )
	{
		stdtechname += "/skinned";
	}

	printf( "StdMaterial shgrp<%s> shnam<%s> using technique<%s>\n", ShadingGroupName.c_str(), MaterialName.c_str(), stdtechname.c_str() );

	FCDEffectStandard::TransparencyMode transmode = ColladaMaterial.mTransparencyMode;

	///////////////////////////////////////////////

	ork::lev2::GfxMaterialWiiBasic *MatVct = new ork::lev2::GfxMaterialWiiBasic( stdtechname.c_str() );

	ork::lev2::Texture* ptexD = DiffuseTex ? DiffuseTex->GetTexture() : 0;
	ork::lev2::Texture* ptexN = NormalTex ? NormalTex->GetTexture() : 0;
	ork::lev2::Texture* ptexS = SpecularTex ? SpecularTex->GetTexture() : 0;
	ork::lev2::Texture* ptexA = AmbientTex ? AmbientTex->GetTexture() : 0;

	ork::lev2::TextureContext difctx(ptexD,ColladaMaterial.mDiffuseMapChannel.mRepeatU,ColladaMaterial.mDiffuseMapChannel.mRepeatV);
	ork::lev2::TextureContext bmpctx(ptexN,ColladaMaterial.mNormalMapChannel.mRepeatU,ColladaMaterial.mNormalMapChannel.mRepeatV);
	ork::lev2::TextureContext spcctx(ptexS,ColladaMaterial.mSpecularMapChannel.mRepeatU,ColladaMaterial.mSpecularMapChannel.mRepeatV);
	ork::lev2::TextureContext ambctx(ptexA,ColladaMaterial.mAmbientMapChannel.mRepeatU,ColladaMaterial.mAmbientMapChannel.mRepeatV);

	MatVct->SetTexture( ork::lev2::ETEXDEST_DIFFUSE, difctx );
	MatVct->SetTexture( ork::lev2::ETEXDEST_BUMP, bmpctx );
	MatVct->SetTexture( ork::lev2::ETEXDEST_SPECULAR, spcctx );
	MatVct->SetTexture( ork::lev2::ETEXDEST_AMBIENT, ambctx );

	MatVct->mSpecularPower = ColladaMaterial.mSpecularPower;
	MatVct->mEmissiveColor = ColladaMaterial.mEmissiveColor;

	XgmClusSet.mpMaterial = MatVct;

	MatVct->SetName( AddPooledString( ColladaMaterial.mMaterialName.c_str() ) );

	if( transmode == FCDEffectStandard::A_ONE )
	{
		MatVct->mRasterState.SetBlending( ork::lev2::EBLENDING_ALPHA );
	}
}

///////////////////////////////////////////////////////////////////////////////

void CColladaModel::BuildXgmTriStripMesh( lev2::XgmMesh& XgmMesh, SColladaMesh* ColMesh )
{
	int inummatgroups = ColMesh->RefMatGroups().size();

	orkvector<SColladaMatGroup*> clustersets;

	for( int imat=0; imat<inummatgroups; imat++ )
	{
		SColladaMatGroup *ColMatGroup = ColMesh->RefMatGroups()[ imat ];
		clustersets.push_back( ColMatGroup );
	}

	/////////////////////////////////////////////////////////////////////////////////////

	int inumclusset = int( clustersets.size() );

	XgmMesh.ReserveSubMeshes( inumclusset );

	for( int imat=0; imat<inumclusset; imat++ )
	{
		lev2::XgmSubMesh & XgmClusSet = * new lev2::XgmSubMesh;
		XgmMesh.AddSubMesh( & XgmClusSet );

		SColladaMatGroup *ColMatGroup = clustersets[ imat ];

		XgmClusSet.mLightMapPath = ColMatGroup->mLightMapPath;
		XgmClusSet.mbVertexLit = ColMatGroup->mbVertexLit;
		///////////////////////////////////////////////

		if( ColMatGroup->meMaterialClass == SColladaMatGroup::EMATCLASS_FX )
		{
			ConfigureFxMaterial( this, ColMatGroup, XgmClusSet );
		}
		else if( ColMatGroup->meMaterialClass == SColladaMatGroup::EMATCLASS_STANDARD )
		{
			ConfigureStdMaterial( this, ColMatGroup, XgmClusSet );
		}
		else
		{
			OrkAssert( false );
		}

		ColMatGroup->ComputeVtxStreamFormat();

		mXgmModel.AddMaterial( XgmClusSet.GetMaterial() );

		///////////////////////////////////////////////

		int inumclus = ColMatGroup->GetClusterizer()->GetNumClusters();

		XgmClusSet.miNumClusters = inumclus;

		XgmClusSet.mpClusters = new lev2::XgmCluster[ inumclus ];

		for( int ic=0; ic<inumclus; ic++ )
		{
			XgmClusterBuilder *XgmClusBuilder = ColMatGroup->GetClusterizer()->GetCluster( ic );

			XgmClusBuilder->BuildVertexBuffer( *ColMatGroup );

			lev2::XgmCluster & XgmClus = XgmClusSet.mpClusters[ ic ];
			ColMatGroup->BuildTriStripXgmCluster( XgmClus, XgmClusBuilder );

			int inumclusjoints = XgmClus.mJoints.size();
			for( int ib=0; ib<inumclusjoints; ib++ )
			{
				const PoolString JointName = XgmClus.mJoints[ ib ];
				orklut<PoolString,int>::const_iterator itfind = mXgmModel.RefSkel().mmJointNameMap.find( JointName );
				int iskelindex = (*itfind).second;
				XgmClus.mJointSkelIndices.push_back(iskelindex);
			}

		}

	}
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::BuildXgmTriStripModel( void )
{
	int inummeshes = mMeshIdMap.size();

	mXgmModel.ReserveMeshes( inummeshes );

	std::string FindMesh  = "fg_2_1_3_ground_SG_ground_GeoDaeId";
	int imesh = 0;
	for( orkmap<std::string,ColMeshRec*>::iterator it =mMeshIdMap.begin(); it!=mMeshIdMap.end(); it++ )
	{
		if( it->first == FindMesh )
		{
			orkprintf( "found <%s>\n", FindMesh.c_str() );

		}
		lev2::XgmMesh&		XgmMesh = * new lev2::XgmMesh;
		SColladaMesh*		ColMesh = it->second->mcolmesh;
		PoolString MeshName = AddPooledString(ColMesh->GetMeshName().c_str());

		mXgmModel.AddMesh( MeshName, & XgmMesh );

		XgmMesh.SetMeshName( MeshName );

		BuildXgmTriStripMesh( XgmMesh, ColMesh );

		imesh++;

	}
	mXgmModel.SetBoundingAA_XYZ( mAABoundXYZ );
	mXgmModel.SetBoundingAA_WHD( mAABoundWHD );
	mXgmModel.SetBoundingCenter( mAABoundXYZ + (mAABoundWHD*float(0.5f)) );
	mXgmModel.SetBoundingRadius( mAABoundWHD.Mag()*float(0.5f) );

	return (inummeshes>0);

}

///////////////////////////////////////////////////////////////////////////////
void SColladaMatGroup::ComputeVtxStreamFormat()
{
	ork::lev2::GfxMaterialFx* MatFx = 0;

	if( mpOrkMaterial )
	{
		MatFx = rtti::downcast<ork::lev2::GfxMaterialFx*>( mpOrkMaterial );
	}

	meVtxFormat = lev2::EVTXSTREAMFMT_END;

	const orkvector<ork::lev2::VertexConfig>& VertexConfigDataAvl = mAvailVertexConfigData;
	int inumvtxcfgavailable = VertexConfigDataAvl.size();

	const ColladaAvailVertexFormats::FormatMap& TargetFormatsAvailable = ColladaExportPolicy::GetContext()->mAvailableVertexFormats.GetFormats();

	//////////////////////////////////////////
	// get vertex configuration data
	//////////////////////////////////////////

	int imin_jnt = mMeshConfigurationFlags.mbSkinned ? 1 : 0;
	int	imin_clr = 0;
	int	imin_tex = 0;
	int	imin_nrm = 0;
	int	imin_bin = 0;
	int	imin_tan = 0;

	if( MatFx ) // fx material
	{	const orkvector<ork::lev2::VertexConfig>& VertexConfigDataMtl = MatFx->RefVertexConfig();
		int inumvtxcfgmaterial = VertexConfigDataMtl.size();
		for( int iv=0; iv<inumvtxcfgmaterial; iv++ )
		{	const ork::lev2::VertexConfig& vcfg = VertexConfigDataMtl[iv];
			if( vcfg.Semantic.find("COLOR") != std::string::npos ) imin_clr++;
			if( vcfg.Semantic.find("TEXCOORD") != std::string::npos ) imin_tex++;
			if( vcfg.Semantic.find("BINORMAL") != std::string::npos ) imin_bin++;
			if( vcfg.Semantic.find("TANGENT") != std::string::npos ) imin_tan++;
			if( vcfg.Semantic == "NORMAL") imin_nrm++;
		}
	}
	else // basic materials
	{
		imin_clr = mMeshConfigurationFlags.mbSkinned ? 0 : 1;
		imin_tex = 1;
		imin_nrm = 1;
	}

	if( mLightMapPath.length() )
	{
		imin_tex++;
		//orkprintf( "lightmap<%s> found\n", LightMapPath.c_str() );
	}
	//////////////////////////////////////////
	// find lowest cost match
	//////////////////////////////////////////

	static const int kbad_score = 0x10000;
	int inv_score = kbad_score;

	for( ColladaAvailVertexFormats::FormatMap::const_iterator itf=TargetFormatsAvailable.begin(); itf!=TargetFormatsAvailable.end(); itf++ )
	{
		const ColladaVertexFormat& format = itf->second;

		bool bok_jnt = mMeshConfigurationFlags.mbSkinned ? (format.miNumJoints >= imin_jnt) : (format.miNumJoints==0);
		bool bok_clr = (format.miNumColors >= imin_clr);
		bool bok_tex = (format.miNumUvs >= imin_tex);
		bool bok_nrm = (imin_nrm==1) ? format.mbNormals : true;
		bool bok_bin = (format.miNumBinormals >= imin_bin);

		bool bok = (bok_jnt&bok_clr&bok_tex&bok_nrm&bok_bin);

		/////////////////////////////////

		int iscore = bok	?	// build weighted score (lower score is better)
								format.miVertexSize
							:	kbad_score;

		/////////////////////////////////

		if( iscore<inv_score )
		{
			meVtxFormat = format.meVertexStreamFormat;
			inv_score = iscore;
		}
	}

	//////////////////////////////////////////


}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void XgmClusterBuilder::Describe( )
{
}
void XgmSkinnedClusterBuilder::Describe( )
{
}
void XgmRigidClusterBuilder::Describe( )
{
}

void XgmSkinnedClusterBuilder::BuildVertexBuffer( const SColladaMatGroup& matgroup )
{
	switch( matgroup.GetVtxStreamFormat() )
	{
		case lev2::EVTXSTREAMFMT_V12N12T8I4W4: // PC skinned format
		{	BuildVertexBuffer_V12N12T8I4W4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8I4W4: // PC binormal skinned format
		{	BuildVertexBuffer_V12N12B12T8I4W4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N6I1T4: // WII skinned format
		{	BuildVertexBuffer_V12N6I1T4();
			break;
		}
		default:
		{
			orkerrorlog("ERROR: Unknown or unsupported vertex stream format (%s : %s)\n"
				, matgroup.mShadingGroupName.c_str()
				, matgroup.mpOrkMaterial ? matgroup.mpOrkMaterial->GetName().c_str() : "null");
			break;
		}
	}
}

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N12B12T8I4W4() // binormal pc skinned
{
	lev2::GfxTargetDummy DummyTarget;
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T8I4W4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T8I4W4> vwriter;
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );

	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T8I4W4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;

		const std::string& jn0 = InVtx.mJointNames[0];
		const std::string& jn1 = InVtx.mJointNames[1];
		const std::string& jn2 = InVtx.mJointNames[2];
		const std::string& jn3 = InVtx.mJointNames[3];

		int index0 = FindNewBoneIndex( jn0 );
		int index1 = FindNewBoneIndex( jn1 );
		int index2 = FindNewBoneIndex( jn2 );
		int index3 = FindNewBoneIndex( jn3 );

		index0 = (index0==-1) ? 0 : index0;
		index1 = (index1==-1) ? 0 : index1;
		index2 = (index2==-1) ? 0 : index2;
		index3 = (index3==-1) ? 0 : index3;

		OutVtx.mBoneIndices = (index0) | (index1<<8) | (index2<<16) | (index3<<24);

		fvec4 vw;
		vw.SetX(InVtx.mJointWeights[3]);
		vw.SetY(InVtx.mJointWeights[2]);
		vw.SetZ(InVtx.mJointWeights[1]);
		vw.SetW(InVtx.mJointWeights[0]);

		OutVtx.mBoneWeights = vw.GetRGBAU32();

		vwriter.AddVertex( OutVtx );

	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N12T8I4W4() // basic pc skinned
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();

	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12T8I4W4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12T8I4W4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{
		ork::lev2::SVtxV12N12T8I4W4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;

		const std::string& jn0 = InVtx.mJointNames[0];
		const std::string& jn1 = InVtx.mJointNames[1];
		const std::string& jn2 = InVtx.mJointNames[2];
		const std::string& jn3 = InVtx.mJointNames[3];

		int index0 = FindNewBoneIndex( jn0 );
		int index1 = FindNewBoneIndex( jn1 );
		int index2 = FindNewBoneIndex( jn2 );
		int index3 = FindNewBoneIndex( jn3 );

		index0 = (index0==-1) ? 0 : index0;
		index1 = (index1==-1) ? 0 : index1;
		index2 = (index2==-1) ? 0 : index2;
		index3 = (index3==-1) ? 0 : index3;

		OutVtx.mBoneIndices = (index0) | (index1<<8) | (index2<<16) | (index3<<24);

		fvec4 vw;
		vw.SetX(InVtx.mJointWeights[3]);
		vw.SetY(InVtx.mJointWeights[2]);
		vw.SetZ(InVtx.mJointWeights[1]);
		vw.SetW(InVtx.mJointWeights[0]);

		OutVtx.mBoneWeights = vw.GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N6I1T4() // basic wii skinned
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N6I1T4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N6I1T4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N6I1T4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);

		OutVtx.mX = InVtx.mPos.GetX()*kVertexScale;
		OutVtx.mY = InVtx.mPos.GetY()*kVertexScale;
		OutVtx.mZ = InVtx.mPos.GetZ()*kVertexScale;

		OutVtx.mNX = s16( InVtx.mNrm.GetX() * float(32767.0f) );
		OutVtx.mNY = s16( InVtx.mNrm.GetY() * float(32767.0f) );
		OutVtx.mNZ = s16( InVtx.mNrm.GetZ() * float(32767.0f) );

		OutVtx.mU = s16( InVtx.mUV[0].mMapTexCoord.GetX() * float(1024.0f) );
		OutVtx.mV = s16( InVtx.mUV[0].mMapTexCoord.GetY() * float(1024.0f) );

		///////////////////////////////////////

		const std::string& jn0 = InVtx.mJointNames[0];
		const std::string& jn1 = InVtx.mJointNames[1];
		const std::string& jn2 = InVtx.mJointNames[2];
		const std::string& jn3 = InVtx.mJointNames[3];

		int index0 = FindNewBoneIndex( jn0 );
		int index1 = FindNewBoneIndex( jn1 );
		int index2 = FindNewBoneIndex( jn2 );
		int index3 = FindNewBoneIndex( jn3 );

		index0 = (index0==-1) ? 0 : index0;
		index1 = (index1==-1) ? 0 : index1;
		index2 = (index2==-1) ? 0 : index2;
		index3 = (index3==-1) ? 0 : index3;

		orkset<int> BoneSet;
		BoneSet.insert(index0);
		BoneSet.insert(index1);
		BoneSet.insert(index2);
		BoneSet.insert(index3);

		OrkAssertI(BoneSet.size()==1, "Sorry, wii does not support hardware weighting!!!" );
		OrkAssertI(index0<8, "Sorry, wii only has 8 matrix registers!!!" );

		OutVtx.mBone = u8(index0);
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer( const SColladaMatGroup& matgroup )
{
	switch( matgroup.GetVtxStreamFormat() )
	{
		case lev2::EVTXSTREAMFMT_V12N6C2T4: // basic wii environmen
		{	BuildVertexBuffer_V12N6C2T4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8C4: // basic pc environment
		{	BuildVertexBuffer_V12N12B12T8C4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T16: // basic pc environment
		{	BuildVertexBuffer_V12N12B12T16();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T16C4: // basic pc environment
		{	BuildVertexBuffer_V12N12T16C4();
			break;
		}
		default:
		{	OrkAssert(false);
			break;
		}
	}
}

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N6C2T4() // basic wii environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N6C2T4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N6C2T4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N6C2T4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);

		OutVtx.mX = InVtx.mPos.GetX()*kVertexScale;
		OutVtx.mY = InVtx.mPos.GetY()*kVertexScale;
		OutVtx.mZ = InVtx.mPos.GetZ()*kVertexScale;

		OutVtx.mNX = s16( InVtx.mNrm.GetX() * float(32767.0f) );
		OutVtx.mNY = s16( InVtx.mNrm.GetY() * float(32767.0f) );
		OutVtx.mNZ = s16( InVtx.mNrm.GetZ() * float(32767.0f) );

		OutVtx.mU = s16( InVtx.mUV[0].mMapTexCoord.GetX() * float(1024.0f) );
		OutVtx.mV = s16( InVtx.mUV[0].mMapTexCoord.GetY() * float(1024.0f) );

		int ir = int(InVtx.mCol[0].GetY()*255.0f);
		int ig = int(InVtx.mCol[0].GetZ()*255.0f);
		int ib = int(InVtx.mCol[0].GetW()*255.0f);

		OutVtx.mColor = U16(((ir>>3)<<11)|((ig>>2)<<5)|((ib>>3)<<0));
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12B12T8C4() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T8C4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T8C4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T8C4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;
		OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}
void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12T16C4() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12T16C4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12T16C4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12T16C4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mUV1 = InVtx.mUV[1].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12B12T16() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T16> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T16>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T16 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mUV1 = InVtx.mUV[1].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;
		//OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static void BuildXgmClusterPrimGroups( lev2::XgmCluster & XgmCluster, const std::vector<unsigned int> & TriangleIndices )
{
	lev2::GfxTargetDummy DummyTarget;

	const int imaxvtx = XgmCluster.mpVertexBuffer->GetNumVertices();

	const ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();
	// TODO: Is this correct? Why?
	static const int WII_PRIM_GROUP_MAX_INDICES = 0xFFFF;

	////////////////////////////////////////////////////////////
	// Build TriStrips

	MeshUtil::TriStripper MyStripper( TriangleIndices, 16, 4 );

	bool bhastris = (MyStripper.GetTriIndices().size()>0);

	int inumstripgroups = MyStripper.GetStripGroups().size();

	bool bhasstrips = (inumstripgroups>0);

	int inumpg = inumstripgroups + int(bhastris);

	////////////////////////////////////////////////////////////
	// Create PrimGroups

	XgmCluster.mpPrimGroups = new ork::lev2::XgmPrimGroup[ inumpg ];
	XgmCluster.miNumPrimGroups = inumpg;

	////////////////////////////////////////////////////////////

	int ipg = 0;

	////////////////////////////////////////////////////////////
	if( bhasstrips )
	////////////////////////////////////////////////////////////
	{
		const orkvector<MeshUtil::TriStripperPrimGroup>& StripGroups = MyStripper.GetStripGroups();
		for( int i=0; i<inumstripgroups; i++ )
		{
			const orkvector<unsigned int>& StripIndices = MyStripper.GetStripIndices(i);
			int inumidx = StripIndices.size();

			/////////////////////////////////
			// check index buffer size policy
			//  (some platforms do not have 32bit indices)
			/////////////////////////////////

			if(ColladaExportPolicy::GetContext()->mPrimGroupPolicy.mMaxIndices == ColladaPrimGroupPolicy::EPOLICY_MAXINDICES_WII)
			{
				if(inumidx > WII_PRIM_GROUP_MAX_INDICES)
				{
					orkerrorlog("ERROR: <%s> Wii prim group max indices exceeded: %d\n", policy->mColladaOutName.c_str(), inumidx);
					throw std::exception();
				}
			}

			/////////////////////////////////

			ork::lev2::StaticIndexBuffer<U16> *pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
			U16 *pidx = (U16*) DummyTarget.GBI()->LockIB( *pidxbuf );
			OrkAssert(pidx!=0);
			{
				for( int ii=0; ii<inumidx; ii++ )
				{
					int index = StripIndices[ii];
					OrkAssert(index<imaxvtx);
					pidx[ii] = U16(index);
				}
			}
			DummyTarget.GBI()->UnLockIB( *pidxbuf );

			/////////////////////////////////

			ork::lev2::XgmPrimGroup & StripGroup = XgmCluster.mpPrimGroups[ ipg++ ];

			StripGroup.miNumIndices = inumidx;
			StripGroup.mpIndices = pidxbuf;
			StripGroup.mePrimType = lev2::EPRIM_TRIANGLESTRIP;
		}
	}

	////////////////////////////////////////////////////////////
	if( bhastris )
	////////////////////////////////////////////////////////////
	{
		int inumidx = MyStripper.GetTriIndices().size();

		/////////////////////////////////////////////////////
		ork::lev2::StaticIndexBuffer<U16> *pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
		U16 *pidx = (U16*) DummyTarget.GBI()->LockIB( *pidxbuf );
		OrkAssert(pidx!=0);
		for( int ii=0; ii<inumidx; ii++ )
		{
			pidx[ii] = U16(MyStripper.GetTriIndices()[ii]);
		}
		DummyTarget.GBI()->UnLockIB( *pidxbuf );
		/////////////////////////////////////////////////////

		ork::lev2::XgmPrimGroup & StripGroup = XgmCluster.mpPrimGroups[ ipg++ ];

		if(ColladaExportPolicy::GetContext()->mPrimGroupPolicy.mMaxIndices == ColladaPrimGroupPolicy::EPOLICY_MAXINDICES_WII)
			if(inumidx > WII_PRIM_GROUP_MAX_INDICES)
			{
				orkerrorlog("ERROR: <%s> Wii prim group max indices exceeded: %d\n", policy->mColladaOutName.c_str(), inumidx);
				throw std::exception();
			}

		StripGroup.miNumIndices = inumidx;
		StripGroup.mpIndices = pidxbuf;
		StripGroup.mePrimType = lev2::EPRIM_TRIANGLES;

	}
}

///////////////////////////////////////////////////////////////////////////////

void SColladaMatGroup::BuildTriStripXgmCluster( lev2::XgmCluster & XgmCluster, const XgmClusterBuilder *XgmClusterBuilder )
{
	if(!XgmClusterBuilder->mpVertexBuffer)
		return;

	XgmCluster.mpVertexBuffer = XgmClusterBuilder->mpVertexBuffer;

	const int imaxvtx = XgmCluster.mpVertexBuffer->GetNumVertices();

	/////////////////////////////////////////////////////////////
	// triangle indices come from the ClusterBuilder

	std::vector<unsigned int> TriangleIndices;
	std::vector<int> ToolMeshTriangles;

	XgmClusterBuilder->mSubMesh.FindNSidedPolys( ToolMeshTriangles, 3 );

	int inumtriangles = int( ToolMeshTriangles.size() );

	for( int i=0; i<inumtriangles; i++ )
	{
		int itri_i = ToolMeshTriangles[ i ];

		const ork::MeshUtil::poly& ClusTri = XgmClusterBuilder->mSubMesh.RefPoly( itri_i );

		TriangleIndices.push_back( ClusTri.GetVertexID(0) );
		TriangleIndices.push_back( ClusTri.GetVertexID(1) );
		TriangleIndices.push_back( ClusTri.GetVertexID(2) );
	}

	/////////////////////////////////////////////////////////////

	BuildXgmClusterPrimGroups( XgmCluster, TriangleIndices );

	XgmCluster.mBoundingBox = XgmClusterBuilder->mSubMesh.GetAABox();
	XgmCluster.mBoundingSphere = Sphere(XgmCluster.mBoundingBox.Min(),XgmCluster.mBoundingBox.Max());

	/////////////////////////////////////////////////////////////
	// bone -> matrix register mapping

	const XgmSkinnedClusterBuilder* skinner = ork::rtti::autocast(XgmClusterBuilder);

	if( skinner )
	{
		const orkmap<std::string,int> & BoneMap = skinner->RefBoneRegMap();

		int inumjointsmapped = BoneMap.size();

		XgmCluster.mJoints.resize( inumjointsmapped );

		for( orkmap<std::string,int>::const_iterator it=BoneMap.begin(); it!=BoneMap.end(); it++ )
		{
			const std::string & JointName = it->first;	// the index of the bone in the skeleton
			int JointRegister = it->second;				// the shader register index the bone goes into
			XgmCluster.mJoints[ JointRegister ] = AddPooledString(JointName.c_str());
		}
	}

	/////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

} }
#endif // USE_FCOLLADA
