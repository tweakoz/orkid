///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/path.h>
#if defined(USE_FCOLLADA)
#include <FCollada.h>
#include <FCDocument/FCDocument.h>
#include <FCDocument/FCDExtra.h>
#include <FCDocument/FCDGeometry.h>
#include <FCDocument/FCDGeometryMesh.h>
#include <FCDocument/FCDGeometryPolygons.h>
#include <FCDocument/FCDGeometryPolygonsInput.h>
#include <FCDocument/FCDGeometryPolygonsTools.h> // For Triagulate
#include <FCDocument/FCDGeometrySource.h>
#include <FCDocument/FCDGeometryInstance.h>
#include <FCDocument/FCDLibrary.h>
#include <FCDocument/FCDAsset.h>

#include <FCDocument/FCDAnimation.h>
#include <FCDocument/FCDAnimationChannel.h>
#include <FCDocument/FCDAnimationCurve.h>

#include <FCDocument/FCDController.h>
#include <FCDocument/FCDSkinController.h>

#include <FCDocument/FCDSceneNode.h>

#include <FUtils/FUString.h> // For fm::StingList

#include <FCDocument/FCDAnimationKey.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/collada/daeutil.h>

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2;
namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::FindDaeMeshes( void )
{
	const ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////
	// Read Layers

	orkset<std::string> IgnoreMeshSet;

	FCDLayerList& layers = mDocument->GetLayers();

	int inumlayers = int(layers.size());

	for( int il=0; il<inumlayers; il++ )
	{
		const FCDLayer* layer = layers[il];

		const fm::string& layername = layer->name;

		printf( "layer<%d:%s>\n", il, layername.c_str() );

		std::string lname = layername.c_str();
		std::transform( lname.begin(), lname.end(), lname.begin(), lower() );

		////////////////////////////////////////////
		bool bignore = false;
		////////////////////////////////////////////
		// this is a reference layer, do not export its meshes
		// or it is a collision layer

		if( lname.find("ref_") != std::string::npos )
		{
			bignore = true;
		}
		if( lname.find( "collision" ) != std::string::npos )
		{
			bignore = true;
		}

		////////////////////////////////////////////
		if( bignore )
		{
			int inumobjects = int(layer->objects.size());

			for( int io=0; io<inumobjects; io++ )
			{
				const fm::string& objname = layer->objects[io];

				IgnoreMeshSet.insert( std::string(objname.c_str()) );

				FCDEntity *pent = mDocument->FindEntity( objname );

				if( pent->GetType() == FCDEntity::SCENE_NODE )
				{
					orkstack<FCDSceneNode*> NodeStack;

					FCDSceneNode *parnode = static_cast<FCDSceneNode*>(pent);

					NodeStack.push(parnode);

					while(NodeStack.empty()==false)
					{
						FCDSceneNode *node = NodeStack.top();
						NodeStack.pop();

						size_t inumchild = node->GetChildrenCount();

						for( size_t ichild=0; ichild<inumchild; ichild++ )
						{
							FCDSceneNode* Child = node->GetChild(ichild);
							NodeStack.push(Child);
						}

						fm::string NodeName = node->GetName();

						size_t inuminst = node->GetInstanceCount();

						for( size_t i=0; i<inuminst; i++ )
						{
							FCDEntityInstance * pinst = node->GetInstance( 0 );

							//if( FCDEntityInstance::GEOMETRY == pinst->GetType() )
							if(pinst->HasType(FCDGeometryInstance::GetClassType()))
							{
								FCDGeometryInstance *pgeoinst = static_cast<FCDGeometryInstance*>(pinst);

								std::string instname = pgeoinst->GetEntity()->GetName().c_str();

								IgnoreMeshSet.insert( instname );
							}
						}
					}
				}
			}

		}

	}

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	FCDVisualSceneNodeLibrary* VizSceneLib = mDocument->GetVisualSceneLibrary();

	size_t inumscenenodes = VizSceneLib->GetEntityCount();

	int inumtotalmatbindings = 0;

	for( size_t inode=0; inode<inumscenenodes; inode++ )
	{
		FCDSceneNode* Node = VizSceneLib->GetEntity(inode);

		std::string NodeName = Node->GetName().c_str();

		size_t inumchild = Node->GetChildrenCount();

		for( size_t ichild=0; ichild<inumchild; ichild++ )
		{
			FCDSceneNode* Child = Node->GetChild(ichild);

			std::string ChildName = Child->GetName().c_str();

			size_t inuminst = Child->GetInstanceCount();

			if( inuminst )
			{
				inumtotalmatbindings ++;

				FCDEntityInstance * pinst = Child->GetInstance( 0 );

				//if( FCDEntityInstance::GEOMETRY == pinst->GetType() )
				if(pinst->HasType(FCDGeometryInstance::GetClassType()))
				{
					FCDGeometryInstance *pgeoinst = static_cast<FCDGeometryInstance*>(pinst);

					std::string instname = pgeoinst->GetEntity()->GetName().c_str();

					if( IgnoreMeshSet.find( ChildName ) != IgnoreMeshSet.end() )
					{
						IgnoreMeshSet.insert( instname );

						printf( "ignoring mesh<%s>\n", instname.c_str() );
					}
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	mTopNodesMatrix = fmtx4::Identity;

	bool rval = false;

	FCDGeometryLibrary *GeoLib =  mDocument->GetGeometryLibrary();

	float fminX = 1.0E10f, fmaxX = -1.0E10f;
	float fminY = 1.0E10f, fmaxY = -1.0E10f;
	float fminZ = 1.0E10f, fmaxZ = -1.0E10f;

	if( GeoLib )
	{
		size_t inument =  GeoLib->GetEntityCount ();
	
		orkvector<FCDGeometryMesh*> DaeMeshes;

		for( size_t ient=0; ient<inument; ient++ )
		{
			FCDGeometry *GeoObj = GeoLib->GetEntity(ient);
	
			bool is_mesh = GeoObj->IsMesh();
			printf( "collada_ent<%d> is_mesh<%d>\n", ient, int(is_mesh) );

			if( is_mesh )
			{
				FCDGeometryMesh* mesh = GeoObj->GetMesh();
				DaeMeshes.push_back(mesh);
				
				if( policy->mTriangulation.GetPolicy() == ColladaTriangulationPolicy::ECTP_TRIANGULATE )
				{
					FCDGeometryPolygonsTools::Triangulate(mesh);
				}
			}
		}

		int inummeshes = DaeMeshes.size();

		for( int imesh=0; imesh<inummeshes; imesh++ )
		{
			FCDGeometryMesh *Mesh = DaeMeshes[ imesh ];
			std::string MeshDaeID = Mesh->GetDaeId().c_str();
			//const fm::string & MeshRefID = Mesh->GetReference();

			if( IgnoreMeshSet.find( MeshDaeID ) == IgnoreMeshSet.end() )
			{
				ColMeshRec* MeshRec = new ColMeshRec;

				SColladaMesh *ColMesh = new SColladaMesh;

				MeshRec->mcolmesh = ColMesh;
				MeshRec->mdaemesh = Mesh;

				ColMesh->SetMeshName( MeshDaeID.c_str() );

				mMeshIdMap[ MeshDaeID.c_str() ] = MeshRec;
			}

		}
	
		if( mMeshIdMap.size()>0 ) rval=true;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

typedef orkmap<std::string,ork::lev2::XgmSkelNode*> SkelMap;

bool CColladaModel::ParseControllers( )
{
	const FCDControllerLibrary * ConLib = mDocument->GetControllerLibrary();
	const ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();

	if( 0 == ConLib ) return true;

	orkvector<const FCDSkinController*>	SkinControllers;

	size_t inument =  ConLib->GetEntityCount ();

	for( size_t ient=0; ient<inument; ient++ )
	{
		const FCDController* ConObj = ConLib->GetEntity(ient);
		if(ConObj->IsSkin())
		{
			const FCDSkinController *skinController = ConObj->GetSkinController();
			SkinControllers.push_back( skinController );
		}
	}

	printf( "NumSkinControllers<%d>\n", int(SkinControllers.size()));

	const FCDVisualSceneNodeLibrary *VizLib = mDocument->GetVisualSceneLibrary();

	int inumvizent( VizLib->GetEntityCount() );

	////////////////////////////////////////////////////////////////////

	mTopNodesMatrix = fmtx4::Identity;

	orkmap<std::string,std::string> NodeSubIdMap;

	if( 1 == inumvizent )
	{
		const FCDSceneNode *prootnode = VizLib->GetEntity(0);
		fmtx4 RootMtx = FCDMatrixTofmtx4( prootnode->ToMatrix() );

		orkstack<const FCDSceneNode *> NodeStack;

		///////////////////////////////////////////////////
		// create skelnodes

		NodeStack.push( prootnode );

		//mSkeleton[ prootnode->GetName().c_str() ] = mSkeletonRoot;

		while( false == NodeStack.empty() )
		{
			const FCDSceneNode *pnode = NodeStack.top();
			NodeStack.pop();
			std::string NodeName = pnode->GetName().c_str();
			int inumchildren = int(pnode->GetChildrenCount());		
	
			printf( "Node<%s> numchild<%d>\n", NodeName.c_str(), inumchildren );

			for( int i=0; i<inumchildren; i++ )
			{	
				const FCDSceneNode *pchild = pnode->GetChild(i);

				FCDEntity::Type typ = pchild->GetType();
				bool bjoint = pchild->GetJointFlag();

				if( bjoint )
				{
					std::string ChildName = pchild->GetName().c_str();
					std::string ChildSid = pchild->GetSubId().c_str();
					XgmSkelNode *SkelNode = new XgmSkelNode( pchild->GetName().c_str() );
					mSkeleton[ ChildName ] = SkelNode;
					NodeSubIdMap[ ChildSid ] = ChildName;
				}
				NodeStack.push(pchild);
			}
		}

		///////////////////////////////////////////////////
		// fill in skelnode data

		orkstack<fmtx4> MtxStack;

		NodeStack.push( prootnode );
		MtxStack.push( RootMtx );

		while( false == NodeStack.empty() )
		{
			const FCDSceneNode *pnode = NodeStack.top();
			const fmtx4 ParentMtx = MtxStack.top();
			NodeStack.pop();
			MtxStack.pop();

			///////////////////////////////////////

			fm::string NodeName = pnode->GetName();

			SkelMap::const_iterator it = mSkeleton.find( NodeName.c_str() );

			if( it != mSkeleton.end() )
			{
				XgmSkelNode *SkelNode = it->second;
				SkelNode->mJointMatrix = ParentMtx;
			}

			///////////////////////////////////////
			// recurse children

			int inumchildren = int(pnode->GetChildrenCount());		
			for( int i=0; i<inumchildren; i++ )
			{	
				const FCDSceneNode *pchild = pnode->GetChild(i);
				std::string ChildName = pchild->GetName().c_str();
				std::string ChildSid = pchild->GetSubId().c_str();
				fmtx4 ChildMtx = FCDMatrixTofmtx4( pchild->ToMatrix() );
				NodeStack.push(pchild);
				MtxStack.push(ChildMtx);

				bool bjoint = pchild->GetJointFlag();

				if( bjoint )
				{
					it = mSkeleton.find( ChildName );
					OrkAssert( it != mSkeleton.end() );
					std::string pname = pnode->GetName().c_str();
					SkelMap::const_iterator it2 = mSkeleton.find( pname );
					if( it2 == mSkeleton.end() )
					{
						it->second->mpParent = 0;
					}
					else
					{
						it->second->mpParent = it2->second;
						it2->second->mChildren.push_back( it->second ); 
					}
				}
			}
		}
	}

	////////////////////////////////
	// Find The Root
	////////////////////////////////

	for( SkelMap::const_iterator it=mSkeleton.begin(); it!=mSkeleton.end(); it++ )
	{
		if( it->second->mpParent == 0 )
		{
			mSkeletonRoot = it->second;
			//new XgmSkelNode( prootnode->GetName().c_str() );
		}
	}

	////////////////////////////////

	orkvector<const FCDSkinController*>::size_type inumskincontrollers = SkinControllers.size();
	for(orkvector<const FCDSkinController*>::size_type iskincon = 0; iskincon < inumskincontrollers; ++iskincon)
	{
		const FCDSkinController* skinController = SkinControllers[iskincon];

		mBindShapeMatrix = FCDMatrixTofmtx4(skinController->GetBindShapeTransform());

		const FCDEntity *ptarget = skinController->GetTarget();
		const std::string targetDaeId = ptarget->GetDaeId().c_str();

		OrkAssert( mMeshIdMap.find(targetDaeId) != mMeshIdMap.end() );

		SColladaMesh* targetMesh = mMeshIdMap[targetDaeId]->mcolmesh;

		size_t inumjoints(skinController->GetJointCount());

		////////////////////////////////////////////////

		for(size_t ij = 0; ij < inumjoints; ++ij)
		{
			const FMMatrix44& invertedBindPoseMat =  skinController->GetJoint(ij)->GetBindPoseInverse();
			fm::string jid = skinController->GetJoint(ij)->GetId();
			std::string jname = NodeSubIdMap[ jid.c_str() ];
			SkelMap::const_iterator it = mSkeleton.find( jname );
			OrkAssert( it != mSkeleton.end() );
			ork::lev2::XgmSkelNode* skelnode = it->second;
			if( skelnode )
			{
				skelnode->mBindMatrixInverse = FCDMatrixTofmtx4(invertedBindPoseMat);
			}
		}

		////////////////////////////////////////////////
		// Get Vertex Weighting

		if(inumjoints > 0)
		{
			size_t inumvtxinfs = skinController->GetInfluenceCount();

			for(size_t im = 0; im < inumvtxinfs; ++im)
			{
				const FCDSkinControllerVertex* Vertex = skinController->GetVertexInfluence(im);
				size_t inumpairs = Vertex->GetPairCount();

				switch( policy->mSkinPolicy.mWeighting )
				{
					case ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W4:
					{
						if(inumpairs > 4)
						{
							orkerrorlog("ERROR: <%s> vtxinf<%d> NumWeights<%d> < skinning (no more than 4 allowed), I will attempt to prune automagically >\n", policy->mColladaOutName.c_str(), im, inumpairs);
						}
						break;
					}
					case ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W1:
					{
						if(inumpairs > 1)
						{
							orkprintf("WARNING: <%s> vtxinf<%d> NumWeights<%d> < skinning (no more than 1 allowed), I will attempt to prune automagically >: ", policy->mColladaOutName.c_str(), im, inumpairs);
						}
						break;
					}
				}

				SColladaVertexWeightingInfo winfo;

				typedef std::map<float, int> LargestWeightMap;
				LargestWeightMap largestWeightMap;
				std::map<int,float> RawWeightMap;

				const float kfprune = 16.0f;
				
				for(size_t ip = 0; ip < inumpairs; ++ip)
				{
					const FCDJointWeightPair* jointWeightPair = Vertex->GetPair(ip);
					
					float fw = jointWeightPair->weight;

					RawWeightMap[jointWeightPair->jointIndex] = fw;

					switch( policy->mSkinPolicy.mWeighting )
					{
						case ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W4:
						{
							largestWeightMap[1.0f-fw] = ip;
							break;
						}
						case ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W1:
						{
							int iw = int(floor(fw*kfprune+0.5f));
							fw = float(iw)/kfprune;

							if( fw != 0.0f )
							{
								largestWeightMap[1.0f-fw] = ip;
							}
							break;
						}
					}
				}

				int icount = 0;
				float totweight = 0.0f;
				for(LargestWeightMap::const_iterator it=largestWeightMap.begin(); it!=largestWeightMap.end(); it++ )
				{
					if(icount < 4)
					{
						totweight += (1.0f - it->first);
					}
					icount++;
				}

				if( policy->mSkinPolicy.mWeighting == ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W1 )
				{
					if( largestWeightMap.size() == 1 )
					{
						if( inumpairs!=1 )
						{
							orkprintf( "OK!\n", im );
						}
					}
					else
					{
						orkprintf( "\n" );
						orkerrorlog( "ERROR: <%s> vertex<%d> numpairs<%d> largestWeightMap<%d>\n", policy->mColladaOutName.c_str(), im, inumpairs, largestWeightMap.size() );
						orkerrorlog( "ERROR: <%s> cannot prune weight, out of tolerance. You must prune it manually\n", policy->mColladaOutName.c_str() );
						return false;
					}
				}
				icount = 0;
				winfo.miNumWeights = 0;
				float newtotweight = 0.0f;

				for(LargestWeightMap::const_iterator it = largestWeightMap.begin(); it != largestWeightMap.end(); ++it)
				{
					if(icount < 4)
					{
						const FCDJointWeightPair* jointWeightPair = Vertex->GetPair( it->second );

						float fjointweight = jointWeightPair->weight / totweight;
						newtotweight += fjointweight;

						fm::string jid = skinController->GetJoint(jointWeightPair->jointIndex)->GetId();
						std::string jname = NodeSubIdMap[ jid.c_str() ];
						SkelMap::const_iterator it = mSkeleton.find( jname );
						OrkAssert( it != mSkeleton.end() );

						winfo.mWeighting[winfo.miNumWeights] = fjointweight;
						winfo.mpSkelNodes[winfo.miNumWeights] = it->second;
						++winfo.miNumWeights;
					}
					++icount;
				}

				float fwtest = fabs(1.0f - newtotweight);

				if( fwtest >= 0.02f )
				{
					orkerrorlog( "ERROR: <%s> vertex<%d> fwtest<%f> numpairs<%d> largestWeightMap<%d>\n", policy->mColladaOutName.c_str(), im, fwtest, inumpairs, largestWeightMap.size() );
					orkerrorlog( "ERROR: <%s> cannot prune weight, out of tolerance. You must prune it manually\n", policy->mColladaOutName.c_str() );
					return false;
				}

				targetMesh->RefWeightingInfo().push_back(winfo);
			}
		}

		orkset<XgmSkelNode*> boneSet;

		orkvector<SColladaVertexWeightingInfo>::size_type inumwrecs = targetMesh->RefWeightingInfo().size();
		for(orkvector<SColladaVertexWeightingInfo>::size_type iwr = 0; iwr < inumwrecs; ++iwr)
		{
			const SColladaVertexWeightingInfo& wi = targetMesh->RefWeightingInfo()[iwr];

			switch( policy->mSkinPolicy.mWeighting )
			{
				case ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W4:
				{
					OrkAssert(wi.miNumWeights<=4);
					break;
				}
				case ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W1:
				{
					OrkAssert(wi.miNumWeights==1);
					break;
				}
			}

			for(int iw = 0; iw < wi.miNumWeights; ++iw)
			{
				boneSet.insert(wi.mpSkelNodes[iw]);
			}
		}

		targetMesh->SetSkinned(boneSet.size() > 0);

		if( targetMesh->IsSkinned() )
		{
			ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();
			policy->mbIsSkinned = true;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::ParseGeometries()
{
	const ColladaExportPolicy* policy = ColladaExportPolicy::GetContext();

	bool brval = false;

	FCDGeometryLibrary *GeoLib =  mDocument->GetGeometryLibrary();

	float fminX = 1.0E10f, fmaxX = -1.0E10f;
	float fminY = 1.0E10f, fmaxY = -1.0E10f;
	float fminZ = 1.0E10f, fmaxZ = -1.0E10f;

	std::string FindMesh  = "fg_2_1_3_ground_SG_ground_GeoDaeId";

	if( GeoLib )
	{
		int inummeshes = mMeshIdMap.size();

		int imesh = 0;
		for( orkmap<std::string,ColMeshRec*>::iterator it =mMeshIdMap.begin(); it!=mMeshIdMap.end(); it++ )
		{
			if( it->first == FindMesh )
			{
				orkprintf( "FoundMesh<%s>\n", FindMesh.c_str() );
			}
			SColladaMesh *ColMesh = it->second->mcolmesh;
			FCDGeometryMesh *Mesh = it->second->mdaemesh;

			size_t inumfaces = Mesh->GetFaceCount();

			if( 0 == inumfaces )
			{
				orkprintf( "skipping mesh with 0 faces in it\n" );
				continue;
			}

			size_t inummatgroups = Mesh->GetPolygonsCount();

			orkprintf( "Mesh id<%s> NumFaces %d NumMatGroups %d\n", it->first.c_str(), inumfaces, inummatgroups );

			///////////////////////////////////////////////////////////////////////////////
			// fill in SourceMap
			///////////////////////////////////////////////////////////////////////////////

			std::multimap<FUDaeGeometryInput::Semantic,FCDGeometrySource*> SourceMap;

			size_t inumsources = Mesh->GetSourceCount();
			for( size_t isrc=0; isrc<inumsources; isrc++ )
			{
				FCDGeometrySource *Source = Mesh->GetSource( isrc );
				FUDaeGeometryInput::Semantic src_type = Source->GetType();
				const fstring& src_name = Source->GetName();
				SourceMap.insert( std::make_pair(src_type,Source) );
			}

			int inumpos = SourceMap.count(FUDaeGeometryInput::POSITION);
			int inumnrm = SourceMap.count(FUDaeGeometryInput::NORMAL);
			int inumclr = SourceMap.count(FUDaeGeometryInput::COLOR);
			int inumtex = SourceMap.count(FUDaeGeometryInput::TEXCOORD);
			int inumbin = SourceMap.count(FUDaeGeometryInput::TEXBINORMAL);
			int inumtan = SourceMap.count(FUDaeGeometryInput::TEXTANGENT);

			OrkAssert( inumpos == 1 );
			OrkAssert( inumnrm == 1 );


			for( size_t imatgroup=0; imatgroup<inummatgroups; imatgroup++ )
			{
				FCDGeometryPolygons * MatGroup = Mesh->GetPolygons(imatgroup);

				////////////////////////////////////////////////
				size_t inumpolys = MatGroup->GetFaceCount();

				if( 0 == inumpolys ) continue;

				SColladaMatGroup * ColMatGroup = new SColladaMatGroup;
				ColMatGroup->mMeshConfigurationFlags.mbSkinned = ColMesh->IsSkinned();
				ColMesh->RefMatGroups().push_back( ColMatGroup );

				////////////////////////////////////////////////
				// find lightmapper info 
				////////////////////////////////////////////////
				bool buselightmap = false;
				FCDExtra* pextra = MatGroup->GetExtra();
				if( pextra )
				{	FCDETechnique* pminiorktek = pextra->GetDefaultType()->FindTechnique("MiniOrk");
					if( pminiorktek )
					{	FCDENode* prootnode = pminiorktek->FindChildNode( "Root" );
						if( prootnode )
						{	FCDENode* plmappernode = prootnode->FindChildNode( "LightMapper" );
							FCDENode* pvtxlitnode = prootnode->FindChildNode( "VtxLit" );
							if( pvtxlitnode )
							{
								ColMatGroup->mbVertexLit = true;
							}
							else if( plmappernode )
							{
								FCDENode* plmapnode = plmappernode->FindChildNode( "LightMap" );
								std::string LightmapBase = plmapnode->GetContent();
								std::string LightMapName = CreateFormattedString( "/../lightmaps/%s.dds", LightmapBase.c_str() );
								file::Path LightMapPath( policy->mColladaInpName.c_str() );
								LightMapPath.SetExtension("");
								LightMapPath.SetFile("");
								if( strstr(LightMapPath.c_str(),"hiend")!=0 )
								{
									LightMapPath.AppendFolder("/../../lightmaps/");
								}
								else
								{
									LightMapPath.AppendFolder("/../lightmaps/");
								}
								LightMapPath.SetFile(LightmapBase.c_str());
								LightMapPath.SetExtension("dds");
								ork::file::Path PngPath = LightMapPath; PngPath.SetExtension("png");

								if( CFileEnv::GetRef().DoesFileExist( LightMapPath ) || CFileEnv::GetRef().DoesFileExist( PngPath ) )
								{
									FixedString<1024> pth = LightMapPath.c_str();
									pth.replace_in_place("data/src/", "data://" );
									pth.replace_in_place("ref/hiend/../../", "" );
									pth.replace_in_place("ref/../", "" );
									file::Path relative_pth( pth.c_str() );
									ColMatGroup->mLightMapPath = relative_pth;
									buselightmap = true;
								}

							}
						}
					}
				}
				////////////////////////////////////////////////
				
				if( buselightmap )
				{
					ColMatGroup->SetClusterizer( new XgmClusterizerStd );
				}
				else
				switch( policy->mDicingPolicy.GetPolicy() )
				{
					case ColladaDicingPolicy::ECTP_DICE:
						ColMatGroup->SetClusterizer( new XgmClusterizerDiced );
						//ColMatGroup->SetClusterizer( new XgmClusterizerStd );
						break;
					case ColladaDicingPolicy::ECTP_DONT_DICE:
						ColMatGroup->SetClusterizer( new XgmClusterizerStd );
						break;
					default:
						OrkAssert(false);
						break;
				}

				ColMatGroup->GetClusterizer()->Begin();

				/////////////////////////////////////////////////////

				orkvector<const DaeDataSource*>	UvSetSources;
				orkvector<const DaeDataSource*>	BinSetSources;
				orkvector<const DaeDataSource*>	TanSetSources;
				orkvector<const DaeDataSource*>	ColorSources;
				DaeDataSource*					PosSource = 0;
				DaeDataSource*					NrmSource = 0;

				int icounter_clr = 0;
				int icounter_tex = 0;
				int icounter_bin = 0;
				int icounter_tan = 0;

				for( size_t isrc=0; isrc<inumsources; isrc++ )
				{
					FCDGeometrySource *Source = Mesh->GetSource( isrc );
					FUDaeGeometryInput::Semantic src_type = Source->GetType();
					const fstring& src_name = Source->GetName();

					VertexConfig vcfg;
					vcfg.Name = Source->GetName();
					vcfg.Source = Source->GetName();

					DaeDataSource* DaeSrc = new DaeDataSource( Source, MatGroup );

					switch( src_type )
					{
						case FUDaeGeometryInput::POSITION:
							PosSource = DaeSrc;
							vcfg.Type = "float3";
							vcfg.Semantic = "POSITION";
							break;
						case FUDaeGeometryInput::NORMAL:
							NrmSource = DaeSrc;
							vcfg.Type = "float3";
							vcfg.Semantic = "NORMAL";
							break;
						case FUDaeGeometryInput::COLOR:
							ColorSources.push_back(DaeSrc);
							vcfg.Type = "float4";
							vcfg.Semantic = "COLOR"+ork::CreateFormattedString("%d",icounter_clr);;
							icounter_clr++;
							break;
						case FUDaeGeometryInput::TEXCOORD:
							UvSetSources.push_back(DaeSrc);
							vcfg.Type = "float2";
							vcfg.Semantic = "TEXCOORD"+ork::CreateFormattedString("%d",icounter_tex);
							icounter_tex++;
							break;
						case FUDaeGeometryInput::TEXBINORMAL:
							BinSetSources.push_back(DaeSrc);
							vcfg.Type = "float3";
							vcfg.Semantic = "BINORMAL"+ork::CreateFormattedString("%d",icounter_bin);;
							icounter_bin++;
							break;
						case FUDaeGeometryInput::TEXTANGENT:
							TanSetSources.push_back(DaeSrc);
							vcfg.Type = "float3";
							vcfg.Semantic = "TANGENT"+ork::CreateFormattedString("%d",icounter_tan);;
							icounter_tan++;
							break;

					}

					ColMatGroup->mAvailVertexConfigData.push_back( vcfg );

				}

				/////////////////////////////////////////////////////
				// parse material
				/////////////////////////////////////////////////////

				std::string ShadingGroupName = MatGroup->GetMaterialSemantic ().c_str();
				auto binding = mMaterialSemanticBindingMap[ ShadingGroupName ];

				auto MaterialName = binding.mMaterialDaeId;
				
				ColMatGroup->mShadingGroupName = ShadingGroupName;

				orkmap<std::string,SColladaMaterial>::iterator itmat = mMaterialMap.find( ShadingGroupName );

				if( mMaterialMap.end() == itmat )
				{
					SColladaMaterial colladamaterial;
					colladamaterial.ParseMaterial( mDocument, ShadingGroupName, MaterialName );
					std::pair<std::string,SColladaMaterial> item(ShadingGroupName,colladamaterial);
					mMaterialMap.insert( item);
					itmat = mMaterialMap.find( ShadingGroupName );
				}

				ColMatGroup->Parse( itmat->second );

				if( icounter_clr > MeshUtil::vertex::kmaxcolors )
				{
					orkerrorlog( "ERROR: <Model %s> UhOh, There are too many colorsets [%d]\n", mFileName.c_str(), icounter_clr );
					return false;
				}
				if( icounter_tex > MeshUtil::vertex::kmaxuvs )
				{
					orkerrorlog( "ERROR: <Model %s> UhOh, There are too many uvsets [%d]\n", mFileName.c_str(), icounter_tex );
					return false;
				}

				if( policy->mbIsSkinned && (false==ColMatGroup->mMeshConfigurationFlags.mbSkinned) )
				{
					orkerrorlog( "ERROR: <Model %s> ShadingGroup<%s> UhOh, Skinned Model with RigidMesh in it\n", mFileName.c_str(), ShadingGroupName.c_str() );
				}

				/////////////////////////////////////////////////////
				size_t imatnumfaces = MatGroup->GetFaceCount();
				FCDGeometryPolygons::PrimitiveType eprimtype = MatGroup->GetPrimitiveType();
				orkvector<unsigned int> TriangleIndices;

				const auto& facvtx_counts = MatGroup->RefFaceVertexCounts();
				const auto& hole_faces = MatGroup->RefHoleFaces();

				// Calculates the number of holes within the polygon set that appear before the given face index.
				auto GetHoleCountBefore = [&](size_t index) -> size_t
				{
					size_t holeCount = 0;
					for (UInt32List::const_iterator it = hole_faces.begin(); it != hole_faces.end(); ++it)
					{
						if ((*it) <= index) { ++holeCount; ++index; }
					}
					return holeCount;
				};

				// Retrieves the number of holes within a given face.
				auto GetHoleCount = [&](size_t index) -> size_t
				{
					size_t holeCount = 0;
					for (size_t i = index + GetHoleCountBefore(index) + 1; i < facvtx_counts.size(); ++i)
					{
						bool isHoled = hole_faces.find((uint32) i) != hole_faces.end();
						if (!isHoled) break;
						else ++holeCount;
					}
					return holeCount;
				};
				auto GetFaceCount = [&]() -> size_t
				{
					return facvtx_counts.size()-hole_faces.size();
				};
				// The number of face-vertex pairs for a given face.
				auto GetFaceVertexCount = [&](size_t index) -> size_t
				{
					size_t count = 0;
					if (index < GetFaceCount())
					{
						size_t holeCount = GetHoleCount(index);
						UInt32List::const_iterator it = facvtx_counts.begin() + index + GetHoleCountBefore(index);
						UInt32List::const_iterator end = it + holeCount + 1; // +1 in order to sum the face-vertex pairs of the polygon as its holes.
						for (; it != end; ++it) count += (*it);
					}
					return count;
				};
				auto GetFaceVertexOffset = [&](size_t index) -> size_t
				{
					size_t offset = 0;

					// We'll need to skip over the holes
					size_t holeCount = GetHoleCountBefore(index);
					if (index + holeCount < facvtx_counts.size())
					{
						// Sum up the wanted offset
						UInt32List::const_iterator end = facvtx_counts.begin() + index + holeCount;
						for (UInt32List::const_iterator it = facvtx_counts.begin(); it != end; ++it)
						{
							offset += (*it);
						}
					}
					return offset;
				};

				for( size_t iface=0; iface<imatnumfaces; iface++ )
				{	


					size_t iface_numfverts = GetFaceVertexCount(iface);
					size_t iface_fvertbase = GetFaceVertexOffset(iface);
					OrkAssert( 3 == iface_numfverts );
					XgmClusterTri ClusTri;
					if( iface%1000 == 0 )
						printf( "iface<%d> of %d\n", iface, imatnumfaces );
					for( size_t iface_v=0; iface_v<iface_numfverts; iface_v++ )
					{	MeshUtil::vertex& MuVtx = ClusTri.Vertex[ iface_v ];
						/////////////////////////////////
						// position
						/////////////////////////////////
						uint32 iposidx = PosSource->GetSourceIndex( iface_fvertbase, iface_v );
						float fX = PosSource->GetData( iposidx+0 );
						float fY = PosSource->GetData( iposidx+1 );
						float fZ = PosSource->GetData( iposidx+2 );
						if(fX > fmaxX)	fmaxX = fX;
						if(fY > fmaxY)	fmaxY = fY;
						if(fZ > fmaxZ)	fmaxZ = fZ;
						if(fX < fminX)	fminX = fX;
						if(fY < fminY)	fminY = fY;
						if(fZ < fminZ)	fminZ = fZ;
						MuVtx.mPos = fvec3( fX, fY, fZ );								
						/////////////////////////////////
						// normal
						/////////////////////////////////
						uint32 inrmidx = NrmSource->GetSourceIndex( iface_fvertbase, iface_v );
						if( 0 ) // inrmidx >= NrmSource.GetDataSize() )
						{
							orkerrorlog( "ERROR: <ColladaModel %s> (Normal Index out of range [nidx %d] [numnormals %d])\n", this->mFileName.c_str(), (inrmidx/3), (NrmSource->GetDataSize()/3) );
							return false;
						}
						float fNX = NrmSource->GetData( inrmidx+0 );
						float fNY = NrmSource->GetData( inrmidx+1 );
						float fNZ = NrmSource->GetData( inrmidx+2 );
						MuVtx.mNrm = fvec3( fNX, fNY, fNZ );								
						/////////////////////////////////
						// texcoords
						/////////////////////////////////
						for( int ic=0; ic<int(UvSetSources.size()); ic++ )
						{	//////////////////////////
							const DaeDataSource * UvSource = UvSetSources[ic];
							uint32 iuvidx = UvSource->GetSourceIndex( iface_fvertbase, iface_v );
							float fS = UvSource->GetData( iuvidx+0 );
							float fT = UvSource->GetData( iuvidx+1 );
							MuVtx.mUV[ic].mMapTexCoord = fvec2( fS, fT );	
							//////////////////////////
							if( ic<int(BinSetSources.size()) )
							{	const DaeDataSource * BinSource = BinSetSources[ic];
								uint32 ibinidx = BinSource->GetSourceIndex( iface_fvertbase, iface_v );
								float fBX = BinSource->GetData( ibinidx+0 );
								float fBY = BinSource->GetData( ibinidx+1 );
								float fBZ = BinSource->GetData( ibinidx+2 );
								MuVtx.mUV[ic].mMapBiNormal = fvec3( fBX, fBY, fBZ );	
							}
							//////////////////////////
							if( ic<int(TanSetSources.size()) )
							{	const DaeDataSource * TanSource = TanSetSources[ic];
								uint32 itanidx = TanSource->GetSourceIndex( iface_fvertbase, iface_v );
								float fTX = TanSource->GetData( itanidx+0 );
								float fTY = TanSource->GetData( itanidx+1 );
								float fTZ = TanSource->GetData( itanidx+2 );
								MuVtx.mUV[ic].mMapTangent = fvec3( fTX, fTY, fTZ );	
							}	
							//////////////////////////
						}
						/////////////////////////////////////////
						// vertex colors
						/////////////////////////////////////////
						for(int ic = 0; ic < int(ColorSources.size()); ++ic)
						{
							const DaeDataSource* colorSource = ColorSources[ic];
							uint32 colorIndex = colorSource->GetSourceIndex(iface_fvertbase, iface_v);
							float fr = colorSource->GetData(colorIndex + 0);
							float fg = colorSource->GetData(colorIndex + 1);
							float fb = colorSource->GetData(colorIndex + 2);
							float fa = colorSource->GetData(colorIndex + 3);
							MuVtx.mCol[ic].Set(fa, fr, fg, fb);
						}
						/////////////////////////////////////////
						// skin weights
						/////////////////////////////////////////
						if( ColMesh->IsSkinned() )
						{
							const int ivtxidx = iposidx/PosSource->GetStride();
							const SColladaVertexWeightingInfo & Weighting = ColMesh->RefWeightingInfo()[ ivtxidx ];
							int inumweights = Weighting.miNumWeights;
							MuVtx.miNumWeights = inumweights;
							int ilargestweightindex = -1;
							float fmaxw(0.0f);
							for( int iw=0; iw<inumweights; iw++ )
							{
								MuVtx.mJointNames[ iw ] = Weighting.mpSkelNodes[ iw ]->mNodeName;
								MuVtx.mJointWeights[ iw ] = Weighting.mWeighting[ iw ];
								if( MuVtx.mJointWeights[ iw ] > fmaxw )
								{
									ilargestweightindex = iw;
									fmaxw = MuVtx.mJointWeights[ iw ];
								}
								//orkprintf(	"SKIN <stage: read> <face %d of %d> <vtx %d of %d> <wght %d of %d> <joint %s> <value %f>\n",
								//			iface,		imatnumfaces,
								//			iface_v,	iface_numfverts,
								//			iw,			inumweights,
								//			MuVtx.mJointNames[ iw ].c_str(),
								//			float(Weighting.mWeighting[ iw ]) );
							}
							/////////////////////////////////
							// fill the tail (unused jonints) with initialized data
							/////////////////////////////////
							for( int iw=inumweights; iw<4; iw++ )
							{
								MuVtx.mJointNames[ iw ] = Weighting.mpSkelNodes[ 0 ]->mNodeName;
								MuVtx.mJointWeights[ iw ] = 0.0f;
							}
						}
					}
					///////////////////////////////////////////////////////////////////////
					// Add Triangle
					///////////////////////////////////////////////////////////////////////
					ColMatGroup->GetClusterizer()->AddTriangle( ClusTri, ColMatGroup );
					///////////////////////////////////////////////////////////////////////
					// end Add Triangle
					///////////////////////////////////////////////////////////////////////
				} // for( size_t iface=0; iface<imatnumfaces; iface++ )

				ColMatGroup->GetClusterizer()->End();

			} // for( size_t imatgroup=0; imatgroup<inummatgroups; imatgroup++ )
			imesh++;
		} // for( orkmap<std::string,ColMeshRec*>::iterator it =mMeshIdMap.begin(); it!=mMeshIdMap.end(); it++ )
		if( inummeshes )
		{
			brval = true;
		}
	}
	mAABoundXYZ.SetX(float(fminX));
	mAABoundXYZ.SetY(float(fminY));
	mAABoundXYZ.SetZ(float(fminZ));
	mAABoundWHD.SetX(float(fmaxX - fminX));
	mAABoundWHD.SetY(float(fmaxY - fminY));
	mAABoundWHD.SetZ(float(fmaxZ - fminZ));
	return brval;
}

///////////////////////////////////////////////////////////////////////////////

void CColladaModel::GetNodeMatricesByName(MatrixVector& nodeMatrices, const char* nodeName)
{
	// Preconditions
	OrkAssert(nodeMatrices.empty());
	OrkAssert(nodeName != NULL);

	size_t nodeNameLength = strlen(nodeName);

	const FCDVisualSceneNodeLibrary* visualScenesLibrary = mDocument->GetVisualSceneLibrary();
	for(size_t sceneIndex = 0; sceneIndex < visualScenesLibrary->GetEntityCount(); ++sceneIndex)
	{
		// Check if this node's name attribute matches nodeName
		const FCDSceneNode* sceneNode = visualScenesLibrary->GetEntity(sceneIndex);
		for(size_t nodeIndex = 0; nodeIndex < sceneNode->GetChildrenCount(); ++nodeIndex)
		{
			const FCDSceneNode* childNode = sceneNode->GetChild(nodeIndex);
			if(strncmp(childNode->GetName().c_str(), nodeName, nodeNameLength) == 0)
			{
				nodeMatrices.push_back(FCDMatrixTofmtx4(childNode->ToMatrix()));
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

}}
#endif
