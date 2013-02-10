////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#if ! defined(IX)

///////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ork/file/fileenv.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include <IL/ilut.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/collada/collada.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

bool UvAtlasSubMesh2( const UvAtlasContext& Ctx )
{

	int inumf = 0;
	int inumv = 0;

	const submesh& inp_mesh = Ctx.mInpMesh;

	annopolyposlut AnnoPolyPosLut;
	inp_mesh.ExportPolyAnnotations( AnnoPolyPosLut );

	inumf = inp_mesh.GetNumPolys();
	inumv = (int) inp_mesh.RefVertexPool().GetNumVertices();

	orkset<int> PolysRemaining;
	orkmap<int,orkset<int>>	PolysByNumCon;

	for( int i=0; i<inumf; i++ )
	{
		PolysRemaining.insert(i);
		orkset<int> Connected;
		const poly& ply = inp_mesh.RefPoly(i);
		int inumsides = ply.GetNumSides();
		inp_mesh.GetAdjacentPolys( i, Connected );
		int inumcon = (int) Connected.size();
		orkprintf( "ipoly<%d> numcon<%d> <", i, inumcon );

		for( orkset<int>::const_iterator it2=Connected.begin(); it2!=Connected.end(); it2++ )
		{
			int ic = *it2;
			orkprintf( "%d ", ic );
		}
		orkprintf( "\n" );

		orkset<int>& TheSet = PolysByNumCon[inumcon];
		TheSet.insert(i);
		if( inumsides==4 )
		{
			int inumc = ply.GetNumSides();
		}
	}
	
	///////////////////////////////////////////////////////////////////////////////////////

	while( PolysRemaining.size() )
	{
		orkset<int>::iterator it=PolysRemaining.begin();
		int ip0 = *it;
		PolysRemaining.erase(it);
		const poly& ply = inp_mesh.RefPoly(ip0);

		int inumsides = ply.GetNumSides();

		if( 4 == inumsides )
		{
			orkset<int> Connected;
			inp_mesh.GetAdjacentPolys( ip0, Connected );

		}
		else
		{

		}
	}

	///////////////////////////////////////////////////////////////////////////////////////

	Ctx.mOutMesh.SetAnnotation( "OutVtxFormat", Ctx.mInpMesh.GetAnnotation("OutVtxFormat") );
	Ctx.mOutMesh.ImportPolyAnnotations( AnnoPolyPosLut );

	return true;
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif
