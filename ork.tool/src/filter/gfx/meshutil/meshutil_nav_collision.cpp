////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DAE(Collada) -> EFC(edge adjacency / fixed grid) metacollision converter
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/file/chunkfile.h>
#include <orktool/filter/gfx/meshutil/meshutil_tool.h>
#include <orktool/filter/gfx/collada/collada.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::tool::meshutil {
///////////////////////////////////////////////////////////////////////////////

bool NAVOutput(const file::Path& outpath, const ToolMesh& mesh);

bool DAEToNAVCollision(const tokenlist& options) {
  return true;
#if 0
	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault("--in", "coldae_in.dae" );
	OptionsMap.SetDefault("--out", "coldae_out.nav" );
	OptionsMap.SetDefault("-layer", "collision" );
	OptionsMap.SetOptions( options );

	ToolMesh inmesh;

	std::string ttv_in = OptionsMap.GetOption( "--in" )->GetValue();
	std::string ttv_out = OptionsMap.GetOption( "--out" )->GetValue();
	std::string layer_name = OptionsMap.GetOption( "-layer" )->GetValue();

	file::Path InPath( ttv_in.c_str() );
	file::Path OutPath( ttv_out.c_str() );

	ork::file::Path::SmallNameType ext = InPath.GetExtension();

	if( ext == "dae" )
	{
		DaeReadOpts opts;

		ColladaExportPolicy policy;
		policy.mNormalPolicy.meNormalPolicy = ColladaNormalPolicy::ENP_ALLOW_SPLIT_NORMALS;
		policy.mReadComponentsPolicy.mReadComponents = ColladaReadComponentsPolicy::EPOLICY_READCOMPONENTS_POSITION;

		opts.mReadLayers.insert( layer_name );
		inmesh.ReadFromDaeFile( InPath, opts );

		if(inmesh.GetNumPolys() == 0)
		{
			orkerrorlog("ERROR: LAYER NAME: Was the layer named '%s' in the NAV DAE?\n", layer_name.c_str());
			return false;
		}
		return NAVOutput( OutPath, inmesh );
	}
	return false;
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool NAVOutput(const file::Path& outpath, const ToolMesh& inmesh) {
  return true;
#if 0
	bool error = false;

	bool bwii = (0!=strstr(outpath.c_str(),"wii"));
	bool bxb360 = (0!=strstr(outpath.c_str(),"xb360"));

	EndianContext* pendianctx = 0;

	if( bxb360 || bwii )
	{
		pendianctx = new EndianContext;
		pendianctx->mendian = ork::EENDIAN_BIG;
	}

	///////////////////////////////////
	chunkfile::Writer chunkwriter( "nav" );
	///////////////////////////////////
	chunkfile::OutputStream* DataStream = chunkwriter.AddStream("data");

	file::Path dbgpath = outpath;
	dbgpath.SetExtension( "nad" );
	FILE* fdbgout = fopen( dbgpath.ToAbsolute().c_str(), "wt" );

	fprintf( fdbgout, "NAV Debug Output for <%s>\n", outpath.c_str() );

	const orkmap<std::string, submesh *> &polygroups = inmesh.RefPolyGroupMap();
	const orkmap<std::string, std::string> &materialnames = inmesh.RefShadingGroupToMaterialMap();

	int inumgroups = polygroups.size();
	DataStream->AddItem(inumgroups);

	fprintf(fdbgout, "NumGroups<%d>\n", inumgroups);

	for(orkmap<std::string, submesh *>::const_iterator it = polygroups.begin(); it != polygroups.end(); it++)
	{
		submesh *group = it->second;

		std::string materialname = materialnames.find(group->name)->second;
		int gname = chunkwriter.stringIndex(materialname.c_str());
		DataStream->AddItem(gname);

		fprintf(fdbgout, "Group<%d, %s>\n", gname, materialname.c_str());
	}

	for(orkmap<std::string, submesh *>::const_iterator it = polygroups.begin(); it != polygroups.end(); it++)
	{
		submesh *group = it->second;

		int inumattr = group->_annotations.size();
		DataStream->AddItem(inumattr);

		std::string materialname = materialnames.find(group->name)->second;
		fprintf(fdbgout, "GroupAttr<%s, %d>\n", materialname.c_str(), inumattr);

		int num = 0;
		for(orkmap<std::string, std::string>::const_iterator it = group->_annotations.begin(); it != group->_annotations.end(); it++)
		{
			fprintf(fdbgout, "Attr<%d> %s=%s\n", num, it->first.c_str(), it->second.c_str());

			int name = chunkwriter.stringIndex(it->first.c_str());
			int value = chunkwriter.stringIndex(it->second.c_str());
			DataStream->AddItem(name);
			DataStream->AddItem(value);
		}
	}

	const AABox& bbox = inmesh.GetAABox();
	fvec3 bbox_size = bbox.Max()-bbox.Min();
	int inumtriangles = inmesh.GetNumPolys(3);
	int inumfaces = inmesh.GetNumPolys();
	int inumvertices = inmesh.RefVertexPool().GetNumVertices();

	OrkAssert( inumfaces==inumtriangles );

	DataStream->AddItem( inumvertices );
	DataStream->AddItem( inumtriangles );

	/////////////////////////////////////////////////
	// write vertices and vertex->poly adjacency
	/////////////////////////////////////////////////

	fprintf( fdbgout, "NumVerts<%d> NumTris<%d>\n", inumvertices, inumtriangles );

	int inumcontribase = 0;
	for( int iv=0; iv<inumvertices; iv++ )
	{
		const vertex& vtx = inmesh.RefVertexPool().GetVertex(iv);

		const fvec3& pos = vtx.mPos;
		const fvec3& nrm = vtx.mNrm;
		int inumcontri = int( vtx.mConnectedPolys.size() );

		DataStream->AddItem( pos );
		DataStream->AddItem( nrm );
		DataStream->AddItem( inumcontribase );
		DataStream->AddItem( inumcontri );

		fprintf( fdbgout, "Vtx<%03d> NumConTris<%d> ", iv, inumcontri );

		fprintf( fdbgout, "pos<%+3.2f,%+3.2f,%+3.2f> ", pos.GetX(), pos.GetY(), pos.GetZ() );

		for( orkvector<int>::const_iterator cplyit=vtx.mConnectedPolys.begin(); cplyit!=vtx.mConnectedPolys.end(); cplyit++ )
		{
			int ipoly = (*cplyit);
			DataStream->AddItem( ipoly );
			inumcontribase++;
			fprintf( fdbgout, "%d ", ipoly );
		}
		fprintf( fdbgout, "\n" );
	}

	/////////////////////////////////////////////////
	// write tris and edge adjacency
	/////////////////////////////////////////////////

	struct nav_tri
	{
		int				mGroup;
		int				mVertexID[3];
		int				mConnected[3];

		nav_tri() : mGroup()
		{
			for( int i=0; i<3; i++ )
			{
				mVertexID[i] = -1;
				mConnected[i] = -1;
			}
		}
	};

	orkvector<nav_tri> NavTris;
	NavTris.resize( inumtriangles );

	for( int it=0; it<inumtriangles; it++ )
	{
		const poly& ply = inmesh.GetPoly( it );

		nav_tri& ntri = NavTris[ it ];

		fprintf( fdbgout, "Tri<%03d> Vtx<", it );

		for( int iv=0; iv<3; iv++ )
		{
			int ivi = ply.GetVertexID(iv);
			int iv2 = ply.GetVertexID((iv + 1) % 3);
			const vertex& vtx1 = inmesh.RefVertexPool().GetVertex(ivi);
			const vertex& vtx2 = inmesh.RefVertexPool().GetVertex(iv2);
			const fvec3& pos1 = vtx1.Pos();
			const fvec3& pos2 = vtx2.Pos();

			U64 uei = ply.mEdges[iv];
			const edge& edg = inmesh.GetEdge( uei );

			ntri.mVertexID[iv] = ivi;

			int inumcon = edg.GetNumConnectedPolys();

			fprintf( fdbgout, "%3d ", ivi );

			if( inumcon > 1 )
			{
				int ic0 = edg.GetConnectedPoly(0);
				int ic1 = edg.GetConnectedPoly(1);
				int ioth = (it==ic0) ? ic1 : ic0;
				ntri.mConnected[iv] = ioth;

			}

			if(submesh *group = inmesh.RefPolyGroupByPolyIndex()[it])
			{
				std::string materialname = materialnames.find(group->name)->second;
				int igroup = chunkwriter.stringIndex(materialname.c_str());
				ntri.mGroup = igroup;
			}
		}

		for( int iv=0; iv<3; iv++ )
		{
			U64 uei = ply.mEdges[iv];
			const edge& edg = inmesh.GetEdge( uei );

			int inumcon = edg.GetNumConnectedPolys();
			OrkAssertI(inumcon > 0, "There should never exist an edge with no connected triangles!");
			if(inumcon > 2)
			{
				int iv1 = edg.GetVertexID(0);
				int iv2 = edg.GetVertexID(1);
				const vertex& vtx1 = inmesh.RefVertexPool().GetVertex(iv1);
				const vertex& vtx2 = inmesh.RefVertexPool().GetVertex(iv2);
				const fvec3& v1 = vtx1.Pos();
				const fvec3& v2 = vtx2.Pos();
				orkerrorlog("ERROR: 2-MANIFOLD: %s - There is not 1 or 2 connected connected triangles to an edge\n", outpath.c_str(), inumcon);
				orkerrorlog("  edge: (%g,%g,%g) - (%g,%g,%g)\n", v1.GetX(), v1.GetY(), v1.GetZ(), v2.GetX(), v2.GetY(), v2.GetZ() );
				error = true;
			}
		}

		DataStream->AddItem( ntri.mGroup );
		for( int iv=0; iv<3; iv++ ) DataStream->AddItem( ntri.mVertexID[iv] );
		for( int iv=0; iv<3; iv++ ) DataStream->AddItem( ntri.mConnected[iv] );
		fprintf( fdbgout, "> Group<%d> Adj<", ntri.mGroup);
		for( int iv=0; iv<3; iv++ )
		{
			int ic = ntri.mConnected[iv];
			fprintf( fdbgout, "%3d ", ic );
		}
		fprintf( fdbgout, ">\n" );
	}

	if(!error)
	{
		///////////////////////////////////
		// write out NAV file
		///////////////////////////////////

		file::Path apath = outpath.ToAbsolute();
		apath.SetExtension( "nav" );
		chunkwriter.WriteToFile( apath );

		////////////////////////////////////////////////////////////////////////////////////
	}

	///////////////////////////////////
	// close NAD file
	///////////////////////////////////
	if(fdbgout)
		fclose(fdbgout);

	if( pendianctx )
	{
		delete pendianctx;
	}

	return error;
#endif
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::tool::meshutil
///////////////////////////////////////////////////////////////////////////////
