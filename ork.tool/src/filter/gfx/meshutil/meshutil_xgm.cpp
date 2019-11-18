////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/file/chunkfile.h>
#include <ork/application/application.h>
#include <orktool/filter/gfx/collada/collada.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

void toolmesh::WriteToRgmFile( const file::Path& outpath ) const
{	chunkfile::Writer chunkwriter( "rgm" );
	chunkfile::OutputStream* HeaderStream = chunkwriter.AddStream("header");
	chunkfile::OutputStream* ModelDataStream = chunkwriter.AddStream("modeldata");
	///////////////////////////////////////////////////////////
	int inumannos =(int)  mAnnotations.size();
	HeaderStream->AddItem(inumannos);
	for( orkmap<std::string,std::string>::const_iterator it=mAnnotations.begin(); it!=mAnnotations.end(); it++ )
	{	const std::string& key = it->first;
		const std::string& val = it->second;
		int ikey = chunkwriter.GetStringIndex(key.c_str());
		int ival = chunkwriter.GetStringIndex(val.c_str());
		HeaderStream->AddItem(ikey);
		HeaderStream->AddItem(ival);
	}	
	///////////////////////////////////////////////////////////
	int inumsubs = (int) mPolyGroupLut.size();
	int inumtotv = 0;
	int inumtotp = 0;
	HeaderStream->AddItem(inumsubs);
	///////////////////////////////////////////////////////////
	for( orklut<std::string, submesh*>::const_iterator it=mPolyGroupLut.begin(); it!=mPolyGroupLut.end(); it++ )
	{	const submesh& sub = *it->second;
		const vertexpool& vpool = sub.RefVertexPool();
		const std::string& name = it->first;
		///////////////////////////////////////////////////////////
		int inumannos = (int) sub.RefAnnotations().size();
		HeaderStream->AddItem(inumannos);
		for( submesh::AnnotationMap::const_iterator
			it2=sub.RefAnnotations().begin();
			it2!=sub.RefAnnotations().end();
			it2++ )
		{	const std::string& key = it2->first;
			const std::string& val = it2->second;
			int ikey = chunkwriter.GetStringIndex(key.c_str());
			int ival = chunkwriter.GetStringIndex(val.c_str());
			HeaderStream->AddItem(ikey);
			HeaderStream->AddItem(ival);
		}	
		///////////////////////////////////////////////////////////
		int inumv = (int) vpool.GetNumVertices();
		int isubname = chunkwriter.GetStringIndex(name.c_str());
		HeaderStream->AddItem(isubname);
		HeaderStream->AddItem(inumv);
		HeaderStream->AddItem(inumtotv);
		inumtotv += inumv;
		for( int iv=0; iv<inumv; iv++ )
		{	const vertex& vtx = vpool.GetVertex(iv);
			ModelDataStream->AddItem(vtx.mPos);
			ModelDataStream->AddItem(vtx.mNrm);
			ModelDataStream->AddItem(vtx.mUV[1].mMapTexCoord);
		}
		///////////////////////////////////////////////////////////
		int inump = sub.GetNumPolys();
		HeaderStream->AddItem(inump);
		HeaderStream->AddItem(inumtotp);
		inumtotp += inump;
		for( int ip=0; ip<inump; ip++ )
		{	const poly& ply = sub.RefPoly(ip); 
			int inumv = ply.GetNumSides();
			HeaderStream->AddItem(inumv);
			for( int iv=0; iv<inumv; iv++ )
			{	int ivi = ply.GetVertexID(iv);
				ModelDataStream->AddItem(ivi);
			}
		}
		///////////////////////////////////////////////////////////
	}
	chunkwriter.WriteToFile( outpath );
}

///////////////////////////////////////////////////////////////////////////////

void toolSubMeshToXgmSubMesh(const toolmesh& mesh, const submesh& smesh, ork::lev2::XgmSubMesh& meshout)
{
	lev2::GfxTargetDummy DummyTarget;
	FlatSubMesh fsub( smesh );
	const ork::lev2::MaterialMap& FxmMtlMap = mesh.RefFxmMaterialMap();

	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	int inumvertices = fsub.inumverts;
	int ivtxsize = fsub.ivtxsize;
	int inumtriindices = int(fsub.MergeTriIndices.size());
	OrkAssert( 0 == (inumtriindices%3) );
	////////////////////////////////////////////////////////
	int inumclus = 1;
	static int gicolor = 0;
	static fvec4 gColors[8] = 
	{
		fvec4::Black(),		//0
		fvec4::Red(),		//1
		fvec4::Green(),		//2
		fvec4::Yellow(),		//3
		fvec4::Blue(),		//4
		fvec4::Magenta(),	// 5
		fvec4::Cyan(),		// 6
		fvec4::White(),		// 7
	};
	fvec4 outcolor = gColors[gicolor];
	gicolor = (gicolor+1)%8;
	meshout.miNumClusters = inumclus;
	meshout.mpClusters = new ork::lev2::XgmCluster[inumclus];
	ork::lev2::XgmCluster& cluster = meshout.RefCluster(0);
	ork::lev2::MaterialMap::const_iterator itMTL = FxmMtlMap.find(smesh.name);
	ork::lev2::GfxMaterial* pmtl = 0;
	if( itMTL!=FxmMtlMap.end() ) // match from FXM file
	{
		pmtl = itMTL->second;
		printf( "FOUND FXM material<%p:%s>\n", pmtl, smesh.name.c_str() );
	}
	else
	{
		printf( "NOTFOUND FXM material<%s>\n", smesh.name.c_str() );
		ork::lev2::GfxMaterial3DSolid* pmtlSOL = new ork::lev2::GfxMaterial3DSolid;
		pmtlSOL->SetColorMode( ork::lev2::GfxMaterial3DSolid::EMODE_INTERNAL_COLOR );
		pmtlSOL->SetColor( outcolor );
		pmtl = pmtlSOL;
	}
	meshout.mpMaterial = pmtl;
	std::string mtlname = smesh.name.c_str();
	pmtl->SetName( ork::AddPooledString(mtlname.c_str()) );
	////////////////////////////////////////////////////////
	cluster.miNumPrimGroups = 1;
	cluster.mpPrimGroups = new lev2::XgmPrimGroup[1];
	cluster.mpVertexBuffer = lev2::VertexBufferBase::CreateVertexBuffer( fsub.evtxformat, inumvertices, true );
	lev2::VertexBufferBase& vb = *cluster.mpVertexBuffer;
	void *poutverts = DummyTarget.GBI()->LockVB( vb );
	OrkAssert(poutverts!=0);
	{
		const void* psrc = (const void*) fsub.poutvtxdata;
		int ivblen = inumvertices*ivtxsize;
		memcpy( poutverts, psrc, ivblen );
		vb.SetNumVertices( inumvertices );
	}
	DummyTarget.GBI()->UnLockVB( vb );
	////////////////////////////////////////////////////////
	lev2::XgmPrimGroup& pg = cluster.mpPrimGroups[0];	
	////////////////////////////////////////////////////////
	pg.mePrimType = lev2::EPRIM_TRIANGLES;
	pg.miNumIndices = inumtriindices;
	ork::lev2::StaticIndexBuffer<U16> *pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(pg.miNumIndices);
	pg.mpIndices = pidxbuf;
	ork::lev2::StaticIndexBuffer<U16>& ib = *pidxbuf;
	U16* poutidx = (U16*) DummyTarget.GBI()->LockIB( ib );
	OrkAssert(poutidx!=0);
	for( int ii=0; ii<pg.miNumIndices; ii++ )
	{
		int merged_idx = fsub.MergeTriIndices[ii];
		OrkAssert(merged_idx<0x10000);
		poutidx[ii] = U16(merged_idx);
	}
	DummyTarget.GBI()->UnLockIB( ib );
}

///////////////////////////////////////////////////////////////////////////////

void toolmeshToXgmModel(const toolmesh& tmesh, ork::lev2::XgmModel& mdlout)
{
	int inumsubs = tmesh.GetNumSubMeshes();
	mdlout.ReserveMeshes(1);
	ork::lev2::XgmMesh* outmesh = new ork::lev2::XgmMesh;
	outmesh->SetMeshName(ork::AddPooledString("Mesh1"));
	mdlout.AddMesh( ork::AddPooledString("Mesh1"), outmesh );
	/////////////////////////////////////////////////////////
	outmesh->ReserveSubMeshes(inumsubs);
	const orklut<std::string, ork::MeshUtil::submesh* >& submeshlut = tmesh.RefSubMeshLut();
	
	for( orklut<std::string, ork::MeshUtil::submesh* >::const_iterator it=submeshlut.begin(); it!=submeshlut.end(); it++ )
	{
		const std::string& pgname = it->first;
		const ork::MeshUtil::submesh* srcsub = it->second;

		ork::lev2::XgmSubMesh* dstsub = new ork::lev2::XgmSubMesh;
		toolSubMeshToXgmSubMesh(tmesh,*srcsub,*dstsub);
		mdlout.AddMaterial(dstsub->mpMaterial);
		outmesh->AddSubMesh( dstsub );
	}
	/////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::WriteToXgmFile( const file::Path& outpath ) const
{
	ork::lev2::XgmModel outmodel;
	toolmeshToXgmModel( *this, outmodel );
	bool rv = ork::lev2::SaveXGM( outpath, & outmodel );
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::ReadFromXGM( const file::Path& BasePath )
{
	lev2::GfxTargetDummy DummyTarget;

	lev2::XgmModel* mdl = new lev2::XgmModel;

	bool bOK = lev2::XgmModel::LoadUnManaged( mdl, BasePath );

	if( bOK )
	{
		////////////////////////////////////////////////////////
		int inummesh = mdl->GetNumMeshes();
		for( int imesh=0; imesh<inummesh; imesh++ )
		{
			lev2::XgmMesh& mesh = * mdl->GetMesh(imesh);
			int inumcs = mesh.GetNumSubMeshes();

			for( int ics=0; ics<inumcs; ics++ )
			{
				std::string mesh_name = CreateFormattedString( "xgm_import_mesh<%d>", ics );
				const lev2::XgmSubMesh& cs = * mesh.GetSubMesh(ics);

				submesh& outsub = MergeSubMesh( mesh_name.c_str() );

				int inumclus = cs.GetNumClusters();

				for( int ic=0; ic<inumclus; ic++ )
				{
					const lev2::XgmCluster& clus = cs.RefCluster(ic);
					lev2::VertexBufferBase* pvb = const_cast<lev2::VertexBufferBase*>(clus.GetVertexBuffer());

					;
					int inumv = pvb->GetMax();
					int isrcsize = inumv*pvb->GetVtxSize();
					const void *pvertbase = static_cast<lev2::GfxTarget&>(DummyTarget).GBI()->LockVB(*pvb);
					OrkAssert(pvertbase!=0);
					 //pvb->GetVertexPointer();
					{
						int inumpg = clus.GetNumPrimGroups();
						for( int ipg=0; ipg<inumpg; ipg++ )
						{
							const lev2::XgmPrimGroup& pg = clus.RefPrimGroup(ipg);
							if( pg.GetPrimType() == lev2::EPRIM_TRIANGLES )
							{
								const lev2::IndexBufferBase *pidxbuf = pg.GetIndexBuffer();
								const lev2::StaticIndexBuffer<U16>* pidxbuf16 = (const lev2::StaticIndexBuffer<U16>*) pidxbuf;
								//const U16* pidx16 = pidxbuf16->GetIndexPointer();
								const U16 *pidx16 = (const U16*) static_cast<lev2::GfxTarget&>(DummyTarget).GBI()->LockIB(*pidxbuf);
								
								
								
								OrkAssert(pidx16!=0);
								
								int inumidx = pidxbuf16->GetNumIndices();

								switch( pvb->GetStreamFormat() )
								{
									//////////////////////////////////////////////////////////////////
									case lev2::EVTXSTREAMFMT_V12N12T8I4W4:
									{	const lev2::SVtxV12N12T8I4W4* ptypedsource = (const lev2::SVtxV12N12T8I4W4*) pvertbase;
										OrkAssert( 0 == (inumidx%3) );
										int indexcache[3];
										for( int ii=0; ii<inumidx; ii++ )
										{	U16 uidx = pidx16[ ii ];
											const lev2::SVtxV12N12T8I4W4& InVtx = ptypedsource[ uidx ];
											vertex ToolVertex;
											ToolVertex.mPos = InVtx.mPosition;
											ToolVertex.mNrm = InVtx.mNormal;
											ToolVertex.mUV[0].mMapTexCoord = InVtx.mUV0;
											ToolVertex.mCol[0] = fcolor4::White();
											int ioutidx = outsub.MergeVertex( ToolVertex );
											indexcache[ (ii%3) ] = ioutidx;
											if( 2 == (ii%3) )
											{	poly ToolPoly( indexcache[0], indexcache[1], indexcache[2] );
												outsub.MergePoly( ToolPoly );
											}
										}
										break;
									}
									//////////////////////////////////////////////////////////////////
									case lev2::EVTXSTREAMFMT_V12N12B12T8C4:
									{	const lev2::SVtxV12N12B12T8C4* ptypedsource = (const lev2::SVtxV12N12B12T8C4*) pvertbase;
										OrkAssert( 0 == (inumidx%3) );
										int indexcache[3];
										for( int ii=0; ii<inumidx; ii++ )
										{	U16 uidx = pidx16[ ii ];
											const lev2::SVtxV12N12B12T8C4& InVtx = ptypedsource[ uidx ];
											vertex ToolVertex;
											ToolVertex.mPos = InVtx.mPosition;
											ToolVertex.mNrm = InVtx.mNormal;
											ToolVertex.mUV[0].mMapTexCoord = InVtx.mUV0;
											ToolVertex.mCol[0] = fcolor4::White();
											int ioutidx = outsub.MergeVertex( ToolVertex );
											indexcache[ (ii%3) ] = ioutidx;
											if( 2 == (ii%3) )
											{	poly ToolPoly( indexcache[0], indexcache[1], indexcache[2] );
												outsub.MergePoly( ToolPoly );
											}
										}
										break;
									}
									//////////////////////////////////////////////////////////////////
									case lev2::EVTXSTREAMFMT_V12N12T16C4:
									{	const lev2::SVtxV12N12T16C4* ptypedsource = (const lev2::SVtxV12N12T16C4*) pvertbase;
										OrkAssert( 0 == (inumidx%3) );
										int indexcache[3];
										printf( "scanning numindices<%d>\n", inumidx );
										for( int ii=0; ii<inumidx; ii++ )
										{	U16 uidx = pidx16[ ii ];
											const lev2::SVtxV12N12T16C4& InVtx = ptypedsource[ uidx ];
											vertex ToolVertex;
											ToolVertex.mPos = InVtx.mPosition;
											ToolVertex.mNrm = InVtx.mNormal;
											ToolVertex.mUV[0].mMapTexCoord = InVtx.mUV0;
											ToolVertex.mCol[0] = fcolor4::White();
											int ioutidx = outsub.MergeVertex( ToolVertex );
											indexcache[ (ii%3) ] = ioutidx;
											if( 2 == (ii%3) )
											{	poly ToolPoly( indexcache[0], indexcache[1], indexcache[2] );
												outsub.MergePoly( ToolPoly );
											}
										}
										break;
									}
									default:
									{
										orkprintf( "toolmesh::ReadFromXGM() vtxfmt<%d> not supported\n", int(pvb->GetStreamFormat()) );
										OrkAssert(false);
										break;
									}
								}
							}
						}
					}
					static_cast<lev2::GfxTarget&>(DummyTarget).GBI()->UnLockVB(*pvb);
				}
			}
		}



	}
	
}

///////////////////////////////////////////////////////////////////////////////
void ToolMaterialGroup::ComputeVtxStreamFormat()
{
	ork::lev2::GfxMaterialFx* MatFx = 0;

	if( _orkMaterial )
	{
		MatFx = rtti::downcast<ork::lev2::GfxMaterialFx*>( _orkMaterial );
	}

	meVtxFormat = lev2::EVTXSTREAMFMT_END;

	const orkvector<ork::lev2::VertexConfig>& VertexConfigDataAvl = mAvailVertexConfigData;
	int inumvtxcfgavailable = VertexConfigDataAvl.size();

	const auto& TargetFormatsAvailable = tool::ColladaExportPolicy::GetContext()->mAvailableVertexFormats.GetFormats();

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

	for( auto itf=TargetFormatsAvailable.begin(); itf!=TargetFormatsAvailable.end(); itf++ )
	{
		auto format = itf->second;

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
}}
///////////////////////////////////////////////////////////////////////////////
