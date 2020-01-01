///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
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
#include <orktool/filter/gfx/meshutil/clusterizer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::tool {

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

	ork::lev2::TextureAsset* get( const ColladaMaterialChannel & MatChanIn, const std::string & model_directory, std::string &ActualFileName )
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
					lev2::ContextDummy DummyTarget;
					ork::lev2::TextureAsset *pl2tex = new ork::lev2::TextureAsset;
					ork::lev2::Texture* ptex = pl2tex->GetTexture();
					bool bOK = DummyTarget.TXI()->LoadTexture( PathToTexture, ptex );
					if( bOK ){
            printf( "loaded texture<%s>\n", PathToTexture.c_str() );
						pl2tex->SetName( ork::AddPooledString(PathToTexture.c_str()) );
						ptex->_varmap.makeValueForKey<std::string>("abspath")=PathToTexture.c_str();
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

void ConfigureFxMaterial( CColladaModel *ColModel, MeshUtil::ToolMaterialGroup *ColMatGroup, lev2::XgmSubMesh & XgmClusSet )
{
	const bool bskinned = ColMatGroup->mMeshConfigurationFlags.mbSkinned;

	const std::string & ShadingGroupName = ColMatGroup->mShadingGroupName;
	const ColladaMaterial &ColladaMaterial = ColModel->GetMaterialFromShadingGroup( ShadingGroupName );

	// TODO: The dependency on the "data://" URL prefix should be removed so any URL can work.
	const file::Path::NameType mdlname = FileEnv::filespec_strip_base(ColModel->mFileName.c_str(), "data://");
	const file::Path::NameType model_directory = FileEnv::FilespecToContainingDirectory(mdlname);

	///////////////////////////////////////////////

	ork::lev2::GfxMaterial* pmat = ColMatGroup->_orkMaterial;

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

			ColladaMaterialChannel matchan;
			matchan.mTextureName = initstr;
			matchan.mRepeatU = 1.0f;
			matchan.mRepeatV = 1.0f;

			std::string foundtexname;

			ork::lev2::TextureAsset* ptexture = gtex_setter.get( matchan, model_directory.c_str(), foundtexname );

			if( ptexture )
			{
				ptexture->GetTexture()->_varmap.makeValueForKey<std::string>( "usage")=param->GetRecord()._name;
				ColModel->AddTexture( ptexture );
				paramf->mValue = ptexture->GetTexture();
			}

		}
	}


	///////////////////////////////////////////////

	OrkAssert( ColMatGroup->_orkMaterial != 0 );

	///////////////////////////////////////////////

	XgmClusSet.mpMaterial = ColMatGroup->_orkMaterial;
}

///////////////////////////////////////////////////////////////////////////////

void ConfigureStdMaterial( CColladaModel *ColModel, MeshUtil::ToolMaterialGroup *ColMatGroup, lev2::XgmSubMesh & XgmClusSet )
{
	const ColladaExportPolicy* policy = ColladaExportPolicy::context();

	const bool bskinned = ColMatGroup->mMeshConfigurationFlags.mbSkinned;

	const std::string & ShadingGroupName = ColMatGroup->mShadingGroupName;
	const ColladaMaterial &ColladaMaterial = ColModel->GetMaterialFromShadingGroup( ShadingGroupName );
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
		DiffuseTex->GetTexture()->_varmap.makeValueForKey<std::string>( "usage")="diffusemap";
		ColModel->AddTexture( DiffuseTex );
	}
	if( NormalTex )
	{
		NormalTex->GetTexture()->_varmap.makeValueForKey<std::string>( "usage")="normalmap";
		ColModel->AddTexture( NormalTex );
	}
	if( SpecularTex )
	{
		SpecularTex->GetTexture()->_varmap.makeValueForKey<std::string>( "usage")="specularmap";
		ColModel->AddTexture( SpecularTex );
	}
	if( AmbientTex )
	{
		AmbientTex->GetTexture()->_varmap.makeValueForKey<std::string>( "usage")="ambientmap";
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
		case ColladaMaterial::ELIGHTING_LAMBERT:
			stdtechname +=		bNormalMapPresent
							? 	"/lambert/tex/bump"
							:	bDiffuseMapPresent
								?	"/lambert/tex"
								:	"/modvtx";
			break;
		case ColladaMaterial::ELIGHTING_BLINN:
			stdtechname += bNormalMapPresent ? "/blinn/tex/bump" : "/lambert/tex";
			break;
		case ColladaMaterial::ELIGHTING_PHONG:
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
		MatVct->_rasterstate.SetBlending( ork::lev2::EBLENDING_ALPHA );
	}
}

///////////////////////////////////////////////////////////////////////////////

void CColladaModel::BuildXgmTriStripMesh( lev2::XgmMesh& XgmMesh, SColladaMesh* ColMesh )
{
	int inummatgroups = ColMesh->RefMatGroups().size();

	orkvector<MeshUtil::ToolMaterialGroup*> clustersets;

	for( int imat=0; imat<inummatgroups; imat++ )
	{
		MeshUtil::ToolMaterialGroup *ColMatGroup = ColMesh->RefMatGroups()[ imat ];
		clustersets.push_back( ColMatGroup );
	}

	/////////////////////////////////////////////////////////////////////////////////////

	int inumclusset = int( clustersets.size() );

	XgmMesh.ReserveSubMeshes( inumclusset );

	for( int imat=0; imat<inumclusset; imat++ )
	{
		lev2::XgmSubMesh & XgmClusSet = * new lev2::XgmSubMesh;
		XgmMesh.AddSubMesh( & XgmClusSet );

		MeshUtil::ToolMaterialGroup *ColMatGroup = clustersets[ imat ];

		XgmClusSet.mLightMapPath = ColMatGroup->mLightMapPath;
		XgmClusSet.mbVertexLit = ColMatGroup->mbVertexLit;
		///////////////////////////////////////////////

		if( ColMatGroup->meMaterialClass == MeshUtil::ToolMaterialGroup::EMATCLASS_FX )
		{
			ConfigureFxMaterial( this, ColMatGroup, XgmClusSet );
		}
		else if( ColMatGroup->meMaterialClass == MeshUtil::ToolMaterialGroup::EMATCLASS_STANDARD )
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
			MeshUtil::XgmClusterBuilder *clusterbuilder = ColMatGroup->GetClusterizer()->GetCluster( ic );

			auto format = ColMatGroup->GetVtxStreamFormat();
			clusterbuilder->buildVertexBuffer( format );

			lev2::XgmCluster & XgmClus = XgmClusSet.mpClusters[ ic ];
			buildTriStripXgmCluster( XgmClus, clusterbuilder );

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

} // namespace ork::tool

///////////////////////////////////////////////////////////////////////////////

#endif // USE_FCOLLADA
