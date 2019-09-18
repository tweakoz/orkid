////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/gfx/camera.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/Lighting.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <orktool/qtui/qtui_tool.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/reflect/enum_serializer.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/math/audiomath.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/FileOutputStream.h>
///////////////////////////////////////////////////////////////////////////////
#if 0
#include <FCollada.h>
#include <FCDocument/FCDocument.h>
#include <FCDocument/FCDLibrary.h>
#include <FCDocument/FCDExtra.h>
#include <FCDocument/FCDGeometry.h>
#include <FCDocument/FCDMaterial.h>
#include <FCDocument/FCDEffect.h>
#include <FCDocument/FCDEffectStandard.h>
#include <FCDocument/FCDEffectProfile.h>
#include <FCDocument/FCDGeometrySource.h>
#include <FCDocument/FCDGeometryMesh.h>
#include <FCDocument/FCDGeometryInstance.h>
#include <FCDocument/FCDGeometryPolygons.h>
#include <FCDocument/FCDGeometryPolygonsInput.h>
//#include <FCDocument/FCDGeometryPolygonsTools.h> // For Triagulate
#include <FCDocument/FCDEntityInstance.h>
#include <FCDocument/FCDSceneNode.h>
#include <FCDocument/FCDAsset.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/filter/gfx/collada/daeutil.h>
//#include <orktool/filter/gfx/collada/collada.h>
///////////////////////////////////////////////////////////////////////////////
#include "SurfaceBaker.h"
#include <pthread/pthread.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void WriteAtlasedDae( const MeshUtil::toolmesh& tmesh, const file::Path& BasePath );
void CollectLights( MeshUtil::LightContainer& lc, const SceneData* psd, const std::string& match );

///////////////////////////////////////////////////////////////////////////////

struct AtlasSubMeshQ;
struct AtlasSubMeshQItem
{
	std::string name;
	std::string outname;
	std::string vtxfmt;
	std::string merged_pgname;
	std::string pgname;
	std::string str_casters;
	std::string str_lights;
	std::string bgroupname;

	ork::MeshUtil::toolmesh*		OutputMesh;
	ork::MeshUtil::toolmesh*		DicedMesh;
	const ork::MeshUtil::submesh*	sub_group;
	ork::MeshUtil::submesh*			out_sub_group;
	float							filterwidth;
	const BakingGroup*				baking_group;
	const AtlasMapperOps*			pOPS;
	float							fbaseprogress;
	float							fnextprogresspart;
	float							fpgs;

	AtlasSubMeshQ*					mpQ;

	AtlasSubMeshQItem() 
		: OutputMesh(0)
		, DicedMesh(0)
		, sub_group(0)
		, out_sub_group(0)
		, baking_group(0)
		, pOPS(0)
		, mpQ(0)
		, filterwidth(0.0f)
		, fbaseprogress(0.0f)
		, fnextprogresspart(0.0f)
		, fpgs(0.0f)
	{
	}

	bool AtlasSubMesh() const;
};

struct AtlasSubMeshQ
{
	int												miNumFinished;
	int												miNumQueued;
	LockedResource< orkvector<AtlasSubMeshQItem> >	mJobSet;
	ork::mutex::standard_lock						mSourceLock;
	ork::mutex										mSourceMutex;

	AtlasSubMeshQ() : miNumFinished(0), miNumQueued(0), mSourceMutex("srcmutex"), mSourceLock(mSourceMutex) {}
};

///////////////////////////////////////////////////////////////////////////////

bool AtlasSubMeshQItem::AtlasSubMesh() const
{
	int inumpolys = sub_group->GetNumPolys();
	ork::MeshUtil::submesh::AnnotationMap anno_merge_map;
	anno_merge_map["subgroup"] = pgname;
	/////////////////////////////////////////////////////////
	// BEGIN Uv Atlas Generation
	/////////////////////////////////////////////////////////
	ork::MeshUtil::submesh UvAtlasedMesh;
	ork::MeshUtil::UvAtlasContext UvaCtx( *sub_group, UvAtlasedMesh );
	UvaCtx.mGroupName = CreateFormattedString( "bgroup<%s>:pgroup<%s>",bgroupname.c_str(),merged_pgname.c_str());
	//////////////////////////////////
	UvAtlasedMesh.SetAnnotation( "OutVtxFormat", vtxfmt.c_str() );
	//////////////////////////////////
	UvaCtx.mfGutter = filterwidth;
	UvaCtx.miTexRes = baking_group->GetResolution();
	UvaCtx.mbDoIMT = pOPS->mbIMT;
	UvaCtx.mfStretching = baking_group->GetAtlasStretching();
	UvaCtx.mfAreaUnification = baking_group->GetAtlasUnification();
	//////////////////////////////////
	if( UvaCtx.mbDoIMT )
	{	EBAKEMAP_TYPE etype = baking_group->GetBakeType();
		std::string outtypestr = "other";
		switch( etype )
		{	case EBMT_AMBOCC:
				outtypestr = "ambocc";
				break;
			case EBMT_DIFFUSE:
				outtypestr = "diffuse";
				break;
			case EBMT_USER:
			{	outtypestr = "user";
				break;
			}
		}
		file::Path::NameType OutTgaName;
		OutTgaName.format( "data/temp/uvatlasdbg/%s/%s_%s.tga", outtypestr.c_str(), outname.c_str(), merged_pgname.c_str() );
		ork::file::Path OutTgaPath(OutTgaName.c_str());
		UvaCtx.mIMTTexturePath = OutTgaPath;
	}
	//////////////////////////////////
	//
	//////////////////////////////////
	bool bOK = UvAtlasSubMesh( UvaCtx );
	//bool bOK = UvAtlasSubMesh2( UvaCtx );
	if( false == bOK ) return false;
	//////////////////////////////////
	//
	//////////////////////////////////
	//ctx.pOPS->SetProgress( ctx.fbaseprogress+(ctx.fnextprogresspart*ctx.fpgs*0.5f) );
	/////////////////////////////////////////////////////////
	// END Uv Atlas Generation
	/////////////////////////////////////////////////////////
	inumpolys = UvAtlasedMesh.GetNumPolys();
	MeshUtil::submesh::AnnotationMap merge_annos;
	merge_annos["SplitPrefix"] = pgname;
	merge_annos["LightMapGroup"] = merged_pgname;
	merge_annos["Casters"] = str_casters.c_str();
	merge_annos["Lights"] = str_lights.c_str();
	merge_annos["BakeGroup"] = bgroupname.c_str();
	merge_annos["FixedGridNode"] = pgname.c_str();
	for( int ip=0; ip<inumpolys; ip++ )
	{	const MeshUtil::poly& ply = UvAtlasedMesh.RefPoly(ip);
		std::string pgroup = ply.GetAnnotation( "shadinggroup" );
		std::string merged_name = pgname+std::string("_")+pgroup;
		if( pgroup!="" )
		{	mpQ->mSourceLock.Lock();
			MeshUtil::submesh& sub = OutputMesh->MergeSubMesh(merged_name.c_str(),merge_annos);
			mpQ->mSourceLock.UnLock();
			sub.SetAnnotation( "ShadingGroup", pgroup.c_str() );
			int inumpv = ply.GetNumSides();
			MeshUtil::poly NewPoly;
			NewPoly.miNumSides = inumpv;
			for( int iv=0; iv<inumpv; iv++ )
			{	int ivi = ply.GetVertexID(iv);
				const MeshUtil::vertex& vtx = UvAtlasedMesh.RefVertexPool().GetVertex( ivi );
				int inewvi = sub.MergeVertex( vtx );
				NewPoly.miVertices[iv] = inewvi;
			}
			NewPoly.SetAnnoMap(ply.GetAnnoMap());
			sub.MergePoly(NewPoly);
		}
	}
	/////////////////////////////////////////////////////////
	//ctx.pOPS->SetProgress( ctx.fbaseprogress+(ctx.fnextprogresspart*ctx.fpgs) );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

static void ProcessSubMeshLightmap()
{
}


///////////////////////////////////////////////////////////////////////////////

static void* AtlasSubMeshJobThread( void* pval );

bool PerformAtlas( AtlasMapperOps* pOPS, const BakerSettings* psetting )
{	bool brv = true;
	const file::Path& daepath = psetting->GetDaeInput();
	const SceneData* psd = pOPS->mpARCH->GetSceneData();
	//////////////////////////////////////////////////////////////
	if( false == FileEnv::DoesFileExist(daepath) ) return false;
	//////////////////////////////////////////////////////////////
	file::Path ColFile( daepath.c_str() );
	const file::Path::NameType outname = ColFile.GetName();
	ork::file::Path::NameType outfile;
	outfile.format( "data/temp/uvatlasdbg/%s", outname.c_str() );
	ork::MeshUtil::toolmesh OutputMesh;
	{
		MeshUtil::toolmesh InpMesh;
		ColladaExportPolicy policy;
		policy.mNormalPolicy.meNormalPolicy = ColladaNormalPolicy::ENP_ALLOW_SPLIT_NORMALS;
		policy.mColladaInpName = daepath.c_str();
		policy.mColladaOutName = daepath.c_str();
		policy.mDicingPolicy.SetPolicy( ColladaDicingPolicy::ECTP_DICE );
		policy.mTriangulation.SetPolicy( ColladaTriangulationPolicy::ECTP_TRIANGULATE );
		////////////////////////////////////////////////////////////////
		// PC vertex formats supported
		////////////////////////////////////////////////////////////////
		policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12B12T8C4 );	// PC 1 tanspace unskinned
		policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12T16C4 );	// PC 1 tanspace unskinned
		////////////////////////////////////////////////////////////////
		DaeReadOpts opts;
		opts.mExcludeLayers.insert( "sectors" );
		opts.mExcludeLayers.insert( "ref_sectors" );
		opts.mExcludeLayers.insert( "collision" );
		opts.mExcludeLayers.insert( "ref_collision" );
		opts.mbMergeMeshShGrpName = true;
		opts.miNumThreads = ork::OldSchool::GetNumCores();
		opts.mbEmptyLayers = true;
//		InpMesh.ReadFromDaeFile( daepath, opts );
		pOPS->SetProgress( 0.02f );
		const orklut<std::string, ork::MeshUtil::submesh* >& material_pgmap = InpMesh.RefSubMeshLut();
		InpMesh.Dump( "InpMesh::PostLoad" );
		//ork::MeshUtil::annopolyposlut AnnoPolyPosLut;
		//InpMesh.ExportPolyAnnotations( AnnoPolyPosLut );
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		// divide material polygroups into baking groups
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		struct BakingGroupItem
		{	const ork::MeshUtil::submesh*	mPolyGroup;
			std::string						mMaterialGroupName;
			std::string						mBakingGroupName;
		};
		/////////////////////////////////////////////////////////////////
		// create a unique set of baking items to test against matches
		/////////////////////////////////////////////////////////////////
		orkset<BakingGroupItem*> ItemSet;
		for( orklut<std::string, ork::MeshUtil::submesh* >::const_iterator it=material_pgmap.begin(); it!=material_pgmap.end(); it++ )
		{	const std::string& material_pgname = it->first;
			const ork::MeshUtil::submesh& material_pgrp = *it->second;
			BakingGroupItem item;
			item.mPolyGroup = & material_pgrp;
			item.mMaterialGroupName = material_pgname;
			ItemSet.insert( new BakingGroupItem(item));
		}
		/////////////////////////////////////////////////////////////////
		// match the unique item set to baking groups
		// some items will be discarded if there are no matches
		// but.. no items will appear in multiple baking groups (no duplications allowed)
		/////////////////////////////////////////////////////////////////
		struct BakingGroupItemList
		{	orkvector<BakingGroupItem>	mItemList;
		};
		orkmap<BakingGroup*,BakingGroupItemList> Groups;
		bool bdone = false;
		while( false == bdone )
		{	orkset<BakingGroupItem*>::iterator it = ItemSet.begin();
			if( it != ItemSet.end() )
			{	BakingGroupItem* item = *it;
				const std::string& material_pgname = item->mMaterialGroupName;
				const ork::MeshUtil::submesh* material_pgrp = item->mPolyGroup;
				BakingGroupMatchItem bgmi = psetting->Match( material_pgname );
				BakingGroup* pgroup = bgmi.mMatchGroup;
				if( pgroup )
				{	orkmap<BakingGroup*,BakingGroupItemList>::iterator GrpIt = Groups.find( pgroup );
					if( GrpIt == Groups.end() )
					{	std::pair< BakingGroup*,BakingGroupItemList > NewPair( pgroup, BakingGroupItemList() );
						Groups.insert( NewPair );
						GrpIt = Groups.find( pgroup );
					}
					BakingGroupItemList& item_list = GrpIt->second;
					item->mBakingGroupName = bgmi.mMatchName.c_str();
					item_list.mItemList.push_back( *item );
				}
				delete (*it);
				ItemSet.erase( it );
			}
			//////////////
			// we are done when there are no items left that match
			//////////////
			if( 0 == int(ItemSet.size()) ) bdone = true;
		}
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		//bool UseVertexColors = psetting->UseVertexColors();
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		// iterate thru the matched items
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		//ork::MeshUtil::toolmesh LightMapMesh;
		CollectLights( OutputMesh.RefLightContainer(), psd, "" );
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////

		//OutputMesh.MergeToolMeshThreaded(InpMesh,ork::OldSchool::GetNumCores());
		int inumpgs = material_pgmap.size();
		int ipg = 0;
			OutputMesh.Dump( "OutputMesh::MergeToolMesh(InpMesh)" );
		
		{
			ork::MeshUtil::submesh& VtxColSub = OutputMesh.MergeSubMesh("vtxlit");
			VtxColSub.SetAnnotation("vtxlit","true");
		}

		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////

		orkset<std::string>	ExludeWhenMerging;

//		for( orkmap<BakingGroup*,BakingGroupItemList>::const_iterator
//			it=Groups.begin();
//			it!=Groups.end();
//			it++ )
		for( auto it : Groups )
		{	BakingGroup* baking_group = it->first;
			bool UseVertexColors = baking_group->UseVertexColors();

			const orkvector<BakingGroupItem>& itemlist = it->second.mItemList;
			const PoolString& ps_casters = baking_group->GetShadowCasters();
			const PoolString& ps_lights = baking_group->GetLights();
			std::string str_casters = (ps_casters.c_str()==0) ? "" : ps_casters.c_str();
			std::string str_lights = (ps_lights.c_str()==0) ? "" : ps_lights.c_str();
			////////////////////////////////
			// collect subset lights for this group
			////////////////////////////////

			ork::MeshUtil::LightContainer group_lightset;
			CollectLights( group_lightset, psd, str_lights );
			const ork::orklut<ork::PoolString,ork::MeshUtil::Light*>& GroupLights = group_lightset.mLights;

			////////////////////////////////
			///////////////////////////////////////////
			// merge items in itemslist to one mesh for dicing and atlasing
			///////////////////////////////////////////
			float fbaseprogress = 0.0f;
			float fnextprogresspart = 0.0f;
			std::string bgroupname;
			PropTypeString vtxfmt; 
			float filterwidth = baking_group->GetFilterWidth();

			///////////////////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////////////////
			if( UseVertexColors )
			{
				ork::MeshUtil::submesh& DefSubMesh = OutputMesh.MergeSubMesh("vtxlit");
				//ork::MeshUtil::submesh bakinggroup_submesh;
				int inumitems = int(itemlist.size());
				for( int iti=0; iti<inumitems; iti++ )
				{	const BakingGroupItem& item = itemlist[iti];
					const ork::MeshUtil::submesh* sub_mesh = item.mPolyGroup;
					const std::string& material_pgname = item.mMaterialGroupName;
					const std::string& bakinggrpname = item.mBakingGroupName;
					const MeshUtil::submesh::AnnotationMap& annomap =  sub_mesh->mAnnotations;
					MeshUtil::submesh::AnnotationMap::const_iterator ita = annomap.find("shadinggroup");
					bgroupname = bakinggrpname;

					int inump=sub_mesh->GetNumPolys();

					int inuml = group_lightset.mLights.size();
					//bakinggroup_submesh.MergeSubMesh( *sub_mesh );
					for( int ip=0; ip<inump; ip++ )
					{
						const ork::MeshUtil::poly& in_ply = sub_mesh->GetPoly(ip);
						int inumv = in_ply.GetNumSides();
						ork::MeshUtil::poly NewPoly;
						NewPoly.miNumSides = inumv;
						for( int iv=0; iv<inumv; iv++ )
						{
							int ivi = in_ply.GetVertexID(iv);
							const ork::MeshUtil::vertex& in_vtx = sub_mesh->RefVertexPool().GetVertex(ivi);
							const fvec4& OrigColor = in_vtx.mCol[0];
							const fvec3 Normal = in_vtx.mNrm.Normal();
							fvec3 LightAccumColor(0.0f,0.0f,0.0f);
							for( orklut<PoolString,ork::MeshUtil::Light*>::const_iterator itl=GroupLights.begin(); itl!=GroupLights.end(); itl++ )
							{	const ork::MeshUtil::Light* plight = itl->second;
								const ork::MeshUtil::DirLight* pdlight = ork::rtti::autocast(plight);
								const ork::MeshUtil::PointLight* pplight = ork::rtti::autocast(plight);
								if( pdlight )
								{
									float fdot = Normal.Dot( -pdlight->mWorldMatrix.GetZNormal().Normal() );
									if( fdot<0.0f ) fdot=0.0f;
									if( fdot>1.0f ) fdot=1.0f;
									LightAccumColor += pdlight->mColor*fdot;
								}
								else if( pplight )
								{
									fvec3 wtol = (in_vtx.mPos-pplight->mWorldMatrix.GetTranslation());
									float fdot = Normal.Normal().Dot( -wtol.Normal() );
									
									
									if( fdot<0.0f ) fdot=0.0f;
									if( fdot>1.0f ) fdot=1.0f;
									float fdist = wtol.Mag();
									float falloff = pplight->mFalloff;
									float atten = fdot/(1.0f+(fdist*fdist)*falloff);
									LightAccumColor += pplight->mColor*atten;
								}
							}
							fvec3 NewColor = LightAccumColor; //(OrigColor*LightAccumColor);
							//pow( lmpCol.x*1.0f, 1.5f )*2.0f;
							////////////////////////////////
							NewColor.SetX( powf( double(NewColor.GetX()), 0.5 )*0.5f );
							NewColor.SetY( powf( double(NewColor.GetY()), 0.5 )*0.5f );
							NewColor.SetZ( powf( double(NewColor.GetZ()), 0.5 )*0.5f );
							if( NewColor.GetX() > 1.0f ) NewColor.SetX(1.0f);
							if( NewColor.GetY() > 1.0f ) NewColor.SetY(1.0f);
							if( NewColor.GetZ() > 1.0f ) NewColor.SetZ(1.0f);
							////////////////////////////////
							ork::MeshUtil::vertex NewVertex = in_vtx;
							NewVertex.mCol[0] = fvec4( NewColor, OrigColor.GetW() );
							int inewvi = DefSubMesh.MergeVertex( NewVertex );
							NewPoly.miVertices[iv] = inewvi;
						}
						NewPoly.SetAnnoMap(in_ply.GetAnnoMap());
						DefSubMesh.MergePoly(NewPoly);
					}
					//OutputMesh.RemoveSubMesh( material_pgname );
					//OutputMesh.MergeSubMesh(bakinggroup_submesh);
					ExludeWhenMerging.insert(material_pgname);
				}
				
			}
			///////////////////////////////////////////////////////////////////////////
			else // LightMaps
			///////////////////////////////////////////////////////////////////////////
			{	ork::MeshUtil::toolmesh DicedMesh;
				{
					ork::MeshUtil::submesh bakinggroup_submesh;
					int inumitems = int(itemlist.size());
					for( int iti=0; iti<inumitems; iti++ )
					{	const BakingGroupItem& item = itemlist[iti];
						const ork::MeshUtil::submesh* sub_mesh = item.mPolyGroup;
						const std::string& material_pgname = item.mMaterialGroupName;
						const std::string& bakinggrpname = item.mBakingGroupName;
						const MeshUtil::submesh::AnnotationMap& annomap =  sub_mesh->mAnnotations;
						MeshUtil::submesh::AnnotationMap::const_iterator ita = annomap.find("shadinggroup");
						bgroupname = bakinggrpname;
						bakinggroup_submesh.MergeSubMesh( *sub_mesh );
						//OutputMesh.RemoveSubMesh( material_pgname );
						ExludeWhenMerging.insert(material_pgname);
					}
					///////////////////////////////////////////
					// determine dicing size for baking group mesh
					///////////////////////////////////////////
					//ork::MeshUtil::submesh* submesh = bakinggroup_mesh.FindSubMesh(bgroupname);
					int inumpolys = bakinggroup_submesh.GetNumPolys();
					float fsurface_area = bakinggroup_submesh.mfSurfaceArea;
					float fnumpolys = float(inumpolys);
					float favgareaperpoly = fsurface_area/fnumpolys;
					int inumtexels = baking_group->GetResolution()*baking_group->GetResolution();
					float ftexperpoly = float(inumtexels)/fnumpolys;
					float flog = log_base( 2.0f, ftexperpoly );
					orkprintf( "PreDice<%s> TotalArea<%f> NumPolys<%f> AvgAreaPerPoly<%f> ftexperpoly<%f> log2<%f>\n",
								bgroupname.c_str(),
								fsurface_area,
								fnumpolys,
								favgareaperpoly,
								ftexperpoly, 
								flog );
					int itexperpoly = int(ftexperpoly);
					int idicesize = baking_group->GetDiceSize();
					/////////////////////////////////////////////////////////
					// Dice Mesh
					/////////////////////////////////////////////////////////
					fbaseprogress = float(ipg)/float(inumpgs);
					fnextprogresspart = 1.0f/float(inumpgs);
					PropType<lev2::EVtxStreamFormat>::ToString(lev2::EVTXSTREAMFMT_V12N12B12T16,vtxfmt);
					DicedMesh.SetAnnotation( "OutVtxFormat", vtxfmt.c_str() );
					bakinggroup_submesh.SetAnnotation( "OutVtxFormat", vtxfmt.c_str() );
					ork::MeshUtil::GridGraph thegraph(idicesize);
					thegraph.BeginPreMerge();
						thegraph.PreMergeMesh( bakinggroup_submesh );
					thegraph.EndPreMerge();
					thegraph.MergeMesh( bakinggroup_submesh, DicedMesh );
					DicedMesh.Dump( "DicedMesh::MergeDice" );
				}	// Kill bakinggroup_submesh

				pOPS->SetProgress( fbaseprogress+(0.1f*fnextprogresspart) );
				int inumpacc = 0;
				const orklut<std::string, ork::MeshUtil::submesh* >& pgmap = DicedMesh.RefSubMeshLut();
				int inumgroups = pgmap.size();
				static int igroup = 0;
				int ipgs = 0;
				///////////////////////////////////////////////////////////////////////////
				AtlasSubMeshQ Q;
				///////////////////////////////////////////////////////////////////////////
				for( orklut<std::string, ork::MeshUtil::submesh* >::const_iterator
						it=pgmap.begin();
						it!=pgmap.end();
						it++ )
				{	ipgs++;
					float fpgs = float(ipgs)/float(inumgroups);
					const std::string& pgname = it->first;
					const ork::MeshUtil::submesh& sub_group = *it->second;
					std::string merged_pgname = bgroupname+std::string("_")+pgname;
					ork::MeshUtil::submesh& out_sub_group = OutputMesh.MergeSubMesh(merged_pgname.c_str());
					out_sub_group.SetAnnotation( "bakegroup", bgroupname.c_str() );
					out_sub_group.SetAnnotation( "subgroup", pgname.c_str() );
					/////////////////////////////
					AtlasSubMeshQItem asmctx;
					/////////////////////////////
					asmctx.name = pgname;
					asmctx.pgname = pgname.c_str();
					asmctx.outname = outname.c_str();
					asmctx.merged_pgname = merged_pgname.c_str();
					asmctx.str_casters = str_casters.c_str();
					asmctx.str_lights = str_lights.c_str();
					asmctx.bgroupname = bgroupname.c_str();
					asmctx.vtxfmt = vtxfmt.c_str();
					/////////////////////////////
					asmctx.sub_group = & sub_group;
					asmctx.out_sub_group = & out_sub_group;
					asmctx.OutputMesh = & OutputMesh;
					asmctx.DicedMesh = & DicedMesh;
					asmctx.baking_group = baking_group;
					asmctx.pOPS = pOPS;
					/////////////////////////////
					asmctx.filterwidth = filterwidth;
					asmctx.fbaseprogress = fbaseprogress;
					asmctx.fnextprogresspart = fnextprogresspart;
					asmctx.fpgs = fpgs;
					asmctx.mpQ = & Q;
					/////////////////////////////
					orkvector<AtlasSubMeshQItem>& QV = Q.mJobSet.LockForWrite();
					QV.push_back(asmctx);
					Q.mJobSet.UnLock();
					Q.miNumQueued++;
					//AtlasSubMesh( asmctx );
				}
				/////////////////////////////////////////////////////////
				// start threads
				/////////////////////////////////////////////////////////
				int inumcores = ork::OldSchool::GetNumCores();
				orkvector<pthread_t>	ThreadVect;
				for( int ic=0; ic<inumcores; ic++ )
				{
					pthread_t job_thread;
					if (pthread_create(&job_thread, NULL, AtlasSubMeshJobThread, (void*)&Q) != 0)
					{
						OrkAssert(false);
					}
					ThreadVect.push_back(job_thread);
				}
				/////////////////////////////////////////////////////////
				// wait for threads
				/////////////////////////////////////////////////////////
				for( orkvector<pthread_t>::iterator it=ThreadVect.begin(); it!=ThreadVect.end(); it++ )
				{
					pthread_t job = (*it);
					void* pret = 0;
					int iret = pthread_join(job, & pret);
					if( pret != ((void*) 0) )
					{
						return false;
					}
				}
				//mpQ->mSourceLock.Lock();
				//DicedMesh.Dump( "DicedMesh::PostAtlas" );
				//DicedMesh.RemoveSubMesh(pgname);
				//mpQ->mSourceLock.Lock();
			} // else LightMaps
			///////////////////////////////////////////////////////////////////////////
		}
		///////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////
		//orkset<std::string>	ExludeWhenMerging;
		OutputMesh.MergeToolMeshThreadedExcluding(InpMesh,ork::OldSchool::GetNumCores(),ExludeWhenMerging);

		///////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////

		/////////////////////////////////////////////////////////
		ipg++;
		pOPS->SetProgress( float(ipg)/float(inumpgs) );
		OutputMesh.CopyMaterialsFromToolMesh(InpMesh);
		OutputMesh.Prune();

	} // Kill InpMesh


	ork::file::Path final_out_path = daepath;
	final_out_path.AppendFile("_lit");
	OutputMesh.SetAnnotation( "BaseName", outname.c_str() );

	final_out_path.SetExtension("rgm");
	OutputMesh.WriteToRgmFile( final_out_path );

	OutputMesh.Dump( "OutputMesh::Output" );

	final_out_path.SetExtension("dae");
	WriteAtlasedDae( OutputMesh, final_out_path );
	return brv;
}


///////////////////////////////////////////////////////////////////////////////

static void* AtlasSubMeshJobThread( void* pval )
{
	AtlasSubMeshQ* q = (AtlasSubMeshQ*) pval;

	bool bdone = false;
	while( ! bdone )
	{
		orkvector<AtlasSubMeshQItem>& qq = q->mJobSet.LockForWrite();
		if( qq.size() )
		{
			orkvector<AtlasSubMeshQItem>::iterator it = (qq.end()-1);
			AtlasSubMeshQItem qitem = *it;
			qq.erase(it);
			q->mJobSet.UnLock();

			////////////////////////////////
			// Do Work Here
			////////////////////////////////

			bool bOK = qitem.AtlasSubMesh();

			if( false == bOK )
			{
				orkvector<AtlasSubMeshQItem>& qq = q->mJobSet.LockForWrite();
				while( qq.size() )
				{
					qq.erase(qq.begin());
				}
				q->mJobSet.UnLock();
				return (void*) 1;
			}

			////////////////////////////////
			// post work
			////////////////////////////////

			q->mSourceLock.Lock();
			{
				q->miNumFinished++;
				bdone = (q->miNumFinished==q->miNumQueued);
				//qitem.mpSourceToolMesh->RemoveSubMesh(qitem.mSourceSubName);
			}
			q->mSourceLock.UnLock();
			Sleep(0);
		}
		else
		{
			q->mJobSet.UnLock();
			bdone=true;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct DaeSplitQueueItem
{
	const MeshUtil::toolmesh*			mpSourceToolMesh;
	const MeshUtil::submesh*			mpSourceSubMesh;
	MeshUtil::submesh*					mpDestSubMesh;
	std::string							mMaterial;	
	std::string							mMergedName;
	std::string							mSourceSubName;

	DaeSplitQueueItem() : mpSourceSubMesh(0), mpDestSubMesh(0), mpSourceToolMesh(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct DaeSplitQueue
{
	int												miNumFinished;
	int												miNumQueued;
	LockedResource< orkvector<DaeSplitQueueItem> >	mJobSet;
	ork::mutex::standard_lock						mSourceLock;
	ork::mutex										mSourceMutex;

	DaeSplitQueue() : miNumFinished(0), miNumQueued(0), mSourceMutex("srcmutex"), mSourceLock(mSourceMutex) {}
};

///////////////////////////////////////////////////////////////////////////////

static void* AtlasSplitJobThread( void* pval )
{
	DaeSplitQueue* q = (DaeSplitQueue*) pval;

	bool bdone = false;
	while( ! bdone )
	{
		orkvector<DaeSplitQueueItem>& qq = q->mJobSet.LockForWrite();
		if( qq.size() )
		{
			orkvector<DaeSplitQueueItem>::iterator it = (qq.end()-1);
			DaeSplitQueueItem qitem = *it;
			qq.erase(it);
			q->mJobSet.UnLock();

			int inumpolys = qitem.mpSourceSubMesh->mMergedPolys.size();
			for( int ip=0; ip<inumpolys; ip++ )
			{	const MeshUtil::poly& ply = qitem.mpSourceSubMesh->RefPoly(ip);
				const std::string& pmaterial = ply.GetAnnotation( "material" );
				if( pmaterial==qitem.mMaterial )
				{	int inumpv = ply.GetNumSides();
					MeshUtil::poly NewPoly;
					NewPoly.miNumSides = inumpv;
					for( int iv=0; iv<inumpv; iv++ )
					{	int ivi = ply.GetVertexID(iv);
						const MeshUtil::vertex& vtx = qitem.mpSourceSubMesh->RefVertexPool().GetVertex( ivi );
						int inewvi = qitem.mpDestSubMesh->MergeVertex( vtx );
						NewPoly.miVertices[iv] = inewvi;
					}
					NewPoly.SetAnnoMap(ply.GetAnnoMap());
					qitem.mpDestSubMesh->MergePoly(NewPoly);
				}					
			}
			q->mSourceLock.Lock();
			{
				q->miNumFinished++;
				bdone = (q->miNumFinished==q->miNumQueued);
				//qitem.mpSourceToolMesh->RemoveSubMesh(qitem.mSourceSubName);
			}
			q->mSourceLock.UnLock();
			Sleep(0);
		}
		else
		{
			q->mJobSet.UnLock();
			bdone=true;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void WriteAtlasedDae( const MeshUtil::toolmesh& tmesh, const file::Path& OutputPath )
{
	DaeWriteOpts out_opts;
	out_opts.meMaterialSetup = DaeWriteOpts::EMS_PRESERVEMATERIALS;
	const std::string& BaseName = tmesh.GetAnnotation("BaseName");
	const orklut<std::string,MeshUtil::submesh*>& SubMeshLut = tmesh.RefSubMeshLut();
	///////////////////////////////////////////////////
	MeshUtil::toolmesh outputmesh;
	outputmesh.CopyMaterialsFromToolMesh( tmesh );
	///////////////////////////////////////////////////
	// split into material groups
	DaeSplitQueue	splitQ;
	orkvector<DaeSplitQueueItem>& QV = splitQ.mJobSet.LockForWrite();
	{	///////////////////////////////////////////////////
		for( orklut<std::string,MeshUtil::submesh*>::const_iterator it=SubMeshLut.begin(); it!=SubMeshLut.end(); it++ )
		{	const std::string& src_subname = it->first;
			const MeshUtil::submesh& pgroup = *it->second;
			const std::string& lmapgroup = pgroup.GetAnnotation( "LightMapGroup" );
			const std::string& shadinggroup = pgroup.GetAnnotation( "ShadingGroup" );
			const std::string& vtxlit = pgroup.GetAnnotation( "vtxlit" );

			int inumpolys = pgroup.mMergedPolys.size();
			orkset<std::string> MaterialSet;
			///////////////////////////////////////////////////
			// collect materials
			///////////////////////////////////////////////////
			for( int ip=0; ip<inumpolys; ip++ )
			{	const MeshUtil::poly& ply = pgroup.RefPoly(ip);
				const std::string& pmaterial = ply.GetAnnotation( "material" );
				if( pmaterial!="" )
				{	orkset<std::string>::const_iterator it=MaterialSet.find(pmaterial);
					if( it==MaterialSet.end() )
					{	MaterialSet.insert( pmaterial );
					}
				}	
			}
			///////////////////////////////////////////////////
			// create jobs
			///////////////////////////////////////////////////
			for( orkset<std::string>::const_iterator it=MaterialSet.begin(); it!=MaterialSet.end(); it++ )
			{
				const std::string& material = (*it);
				std::string merged_name = src_subname+std::string("_")+material;
				//////////////////////////
				MeshUtil::submesh::AnnotationMap merge_annos;
				merge_annos["SplitPrefix"] = src_subname;
				merge_annos["LightMapGroup"] = BaseName + std::string("_") + lmapgroup;
				if( vtxlit=="true" ) merge_annos["vtxlit"] = "true";

				//////////////////////////
				MeshUtil::submesh& sub = outputmesh.MergeSubMesh(merged_name.c_str(),merge_annos);
				//////////////////////////
				sub.SetAnnotation( "Material", material.c_str() );
				sub.SetAnnotation( "ShadingGroup", shadinggroup.c_str() );
				//////////////////////////
				DaeSplitQueueItem qitem;
				qitem.mpSourceToolMesh = & tmesh;
				qitem.mpSourceSubMesh = & pgroup;
				qitem.mpDestSubMesh = & sub;
				qitem.mMaterial = material;
				qitem.mMergedName = merged_name;
				qitem.mSourceSubName = src_subname;
				//////////////////////////
				QV.push_back(qitem);
			}
		}
	}
	splitQ.miNumQueued = QV.size();
	splitQ.mJobSet.UnLock();
	/////////////////////////////////////////////////////////
	// start threads
	/////////////////////////////////////////////////////////
	int inumcores = 1; //ork::OldSchool::GetNumCores();
	orkvector<pthread_t>	ThreadVect;
	for( int ic=0; ic<inumcores; ic++ )
	{
		pthread_t job_thread;
		if (pthread_create(&job_thread, NULL, AtlasSplitJobThread, (void*)&splitQ) != 0)
		{
			OrkAssert(false);
		}
		ThreadVect.push_back(job_thread);
	}
	/////////////////////////////////////////////////////////
	// wait for threads
	/////////////////////////////////////////////////////////
	for( orkvector<pthread_t>::iterator it=ThreadVect.begin(); it!=ThreadVect.end(); it++ )
	{
		pthread_t job = (*it);
		pthread_join(job, NULL);
	}
	/////////////////////////////////////////////////////////
	outputmesh.Dump( "OutputMesh::Final" );
	/////////////////////////////////////////////////////////
//	outputmesh.WriteToDaeFile( OutputPath, out_opts );

}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif
