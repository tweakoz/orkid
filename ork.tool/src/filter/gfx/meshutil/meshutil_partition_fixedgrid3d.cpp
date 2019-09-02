////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/math/plane.hpp>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/kernel/prop.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

GridGraph::GridGraph( int fgsize )
	: kfixedgridsize( fgsize ) 
	, miDimX(0)
	, miDimY(0)
	, miDimZ(0)
	, miNumGrids(0)
	, mppGrids(0)
	, vsize( 0.0f, 0.0f, 0.0f )
	, vmin( 0.0f, 0.0f, 0.0f )
	, vmax( 0.0f, 0.0f, 0.0f )
	, areamax(0.0f)
	, areamin(0.0f)
	, areatot(0.0f)
	, areaavg(0.0f)
	, totpolys(0)
	, mMtxWorldToGrid( fmtx4::Identity )
	, miNumFilledGrids( 0 )
{
}

GridGraph::~GridGraph()
{
	for( int ig=0; ig<miNumGrids; ig++ )
	{
		GridNode* pnode = mppGrids[ig];
		if( pnode )
		{
			delete pnode;
		}
	}
	delete[] mppGrids;
}

////////////////////////////////////////////////////

void GridGraph::BeginPreMerge()
{
	vsize = fvec3(0.0f,0.0f,0.0f);
	vmin = fvec3(0.0f,0.0f,0.0f);
	vmax = fvec3(0.0f,0.0f,0.0f);
	areamax=0.0f;
	areamin=0.0f;
	areatot=0.0f;
	areaavg=0.0f;
	totpolys = 0;
	maab.BeginGrow();
}

////////////////////////////////////////////////////

void GridGraph::PreMergeMesh( const submesh& MeshIn )
{
	const vertexpool& InVPool		= MeshIn.RefVertexPool();
	const orkvector<poly>& polys	= MeshIn.RefPolys();

	int inumpolys = int(polys.size());

	///////////////////////////////////////////
	// 1st step, calc some data
	//  min/max/avg/tot surface area
	//  aa bbox / extents
	///////////////////////////////////////////

	float thisareamax = -CFloat::TypeMax();
	float thisareamin = CFloat::TypeMax();
	float thisareaavg = 0.0f;

	for( int ipoly=0; ipoly<inumpolys; ipoly++ )
	{
		const poly& ply = polys[ ipoly ];
		//vertex center = ply.ComputeCenter( InVPool );

		int inumsides = ply.GetNumSides();
		for( int iv=0; iv<inumsides; iv++ )
		{
			int ivi = ply.GetVertexID(iv);
			const vertex& v = InVPool.GetVertex(ivi);
			maab.Grow( v.mPos );
		}

		///////////////////////////////
		float thisarea = ply.ComputeArea( InVPool, fmtx4::Identity );
		thisareamax = CFloat::Max( thisareamax, thisarea );
		thisareamin = CFloat::Min( thisareamin, thisarea );
		thisareaavg += thisarea;
		///////////////////////////////
		areatot += thisarea;
		areamin = CFloat::Min( areamin, thisarea );
		areamax = CFloat::Max( areamax, thisarea );
		///////////////////////////////
		totpolys++;
	}

}

////////////////////////////////////////////////////

GridNode* GridGraph::GetGridNode( const GridAddr& addr )
{
	OrkAssert( addr.ix>=0 );
	OrkAssert( addr.iy>=0 );
	OrkAssert( addr.iz>=0 );
	OrkAssert( addr.ix<miDimX );
	OrkAssert( addr.iy<miDimY );
	OrkAssert( addr.iz<miDimZ );
	int iaddr = addr.ix + (addr.iy*miDimX) + (addr.iz*miDimX*miDimY);
	return mppGrids[ iaddr ];
}

void GridGraph::SetGridNode( const GridAddr& addr, GridNode*node )
{
	OrkAssert( addr.ix>=0 );
	OrkAssert( addr.iy>=0 );
	OrkAssert( addr.iz>=0 );
	OrkAssert( addr.ix<miDimX );
	OrkAssert( addr.iy<miDimY );
	OrkAssert( addr.iz<miDimZ );
	int iaddr = addr.ix + (addr.iy*miDimX) + (addr.iz*miDimX*miDimY);
	mppGrids[ iaddr ] = node;
	miNumFilledGrids++;
}

////////////////////////////////////////////////////

GridAddr GridGraph::GetGridAddress( const fvec3& v )
{
	fvec3 vxf = v.Transform( mMtxWorldToGrid ).xyz();

	GridAddr ret;
	ret.ix = int(vxf.GetX());
	ret.iy = int(vxf.GetY());
	ret.iz = int(vxf.GetZ());

	return ret;
}

////////////////////////////////////////////////////

void GridGraph::GetCuttingPlanes(	const GridAddr& addr, 
						fplane3& topplane, fplane3& botplane,
						fplane3& lftplane, fplane3& rgtplane,
						fplane3& frnplane, fplane3& bakplane )
{
	const int ix = addr.ix;
	const int iy = addr.iy;
	const int iz = addr.iz;

	float fx0 = float(ix*kfixedgridsize)+vmin.GetX();
	float fy0 = float(iy*kfixedgridsize)+vmin.GetY();
	float fz0 = float(iz*kfixedgridsize)+vmin.GetZ();

	float fx1 = fx0+kfixedgridsize;
	float fy1 = fy0+kfixedgridsize;
	float fz1 = fz0+kfixedgridsize;

	static const fvec3 vn_top( 0.0f, -1.0f,  0.0f );
	static const fvec3 vn_bot( 0.0f,  1.0f,  0.0f );
	static const fvec3 vn_lft( 1.0f,  0.0f,  0.0f );
	static const fvec3 vn_rgt( -1.0f, 0.0f,  0.0f );
	static const fvec3 vn_bak( 0.0f,  0.0f, -1.0f );
	static const fvec3 vn_frn( 0.0f,  0.0f,  1.0f );

	fvec3 vp_top( 0.0f, fy1, 0.0f );
	fvec3 vp_bot( 0.0f, fy0, 0.0f );
	fvec3 vp_lft( fx0, 0.0f, 0.0f );
	fvec3 vp_rgt( fx1, 0.0f, 0.0f );
	fvec3 vp_frn( 0.0f, 0.0f, fz0 );
	fvec3 vp_bak( 0.0f, 0.0f, fz1 );

	topplane.CalcFromNormalAndOrigin( vn_top, vp_top );
	botplane.CalcFromNormalAndOrigin( vn_bot, vp_bot );
	lftplane.CalcFromNormalAndOrigin( vn_lft, vp_lft );
	rgtplane.CalcFromNormalAndOrigin( vn_rgt, vp_rgt );
	frnplane.CalcFromNormalAndOrigin( vn_frn, vp_frn );
	bakplane.CalcFromNormalAndOrigin( vn_bak, vp_bak );



}

////////////////////////////////////////////////////

void GridGraph::EndPreMerge()
{
	maab.EndGrow();

	/////////////////////////////
	vmin = maab.Min();
	vmax = maab.Max();
	vctr = (vmin+vmax)*0.5f;
	vsize = vmax-vmin;
	/////////////////////////////
	areaavg = areatot / float(totpolys);
	/////////////////////////////

	orkprintf( "fg3d aabb min<%f,%f,%f> max<%f,%f,%f> siz<%f,%f,%f>\n", 
		vmin.GetX(), vmin.GetY(), vmin.GetZ(), 
		vmax.GetX(), vmax.GetY(), vmax.GetZ(),
		vsize.GetX(), vsize.GetY(), vsize.GetZ() );

	orkprintf( "MaxPolyArea<%f> MinPolyArea<%f> AvgPolyArea<%f> TotPolyArea<%f>\n", areamax, areamin, areaavg, areatot );

	///////////////////////////////////////////

	miDimX = int(vsize.GetX());
	miDimY = int(vsize.GetY());
	miDimZ = int(vsize.GetZ());

	int icntX = miDimX/kfixedgridsize;
	int icntY = miDimY/kfixedgridsize;
	int icntZ = miDimZ/kfixedgridsize;

	if( (miDimX%kfixedgridsize) != 0 ) icntX++;
	if( (miDimY%kfixedgridsize) != 0 ) icntY++;
	if( (miDimZ%kfixedgridsize) != 0 ) icntZ++;

	//int icntY = 0; while( miDimY!=0 ) { icntY++; miDimY>>=1; } 
	//int icntZ = 0; while( miDimZ!=0 ) { icntZ++; miDimZ>>=1; }

	float fscal = 1.0f / kfixedgridsize;
	
	miDimX = icntX; //(miDimX*icntX)/kfixedgridsize;
	miDimY = icntY; //(miDimX*icntY)/kfixedgridsize;
	miDimZ = icntZ; //(miDimX*icntZ)/kfixedgridsize;

	miDimX = (miDimX>0) ? miDimX : 1;
	miDimY = (miDimY>0) ? miDimY : 1;
	miDimZ = (miDimZ>0) ? miDimZ : 1;

	///////////////////////////////////////////////

	fmtx4 MatS;
	fmtx4 MatT;

	MatS.Scale( fscal, fscal, fscal );
	MatT.SetTranslation( -vmin );
	mMtxWorldToGrid = MatT*MatS;

	fvec3 vtest_min = vmin.Transform( mMtxWorldToGrid ).xyz();
	fvec3 vtest_max = vmax.Transform( mMtxWorldToGrid ).xyz();
	fvec3 vtest_ctr = vctr.Transform( mMtxWorldToGrid ).xyz();

	///////////////////////////////////////////////

	miNumGrids = (miDimX*miDimY*miDimZ);
	mppGrids = new GridNode*[ miNumGrids ];
	for( int i=0; i<miNumGrids; i++ ) mppGrids[i] = 0;

}

////////////////////////////////////////////////////

void GridGraph::MergeMesh( const submesh& MeshIn, toolmesh& MeshOut )
{
	const vertexpool& InVPool		= MeshIn.RefVertexPool();
	const orkvector<poly>& polys	= MeshIn.RefPolys();

	int inumpolys = int(polys.size());

	int inumgtot = 0;

	static int ginuminners = 0;
	static int ginumouters = 0;

	fplane3 topplane, bottomplane, leftplane, rightplane, frontplane, backplane;

	for( int ipoly=0; ipoly<inumpolys; ipoly++ )
	{
		const poly& ply = polys[ ipoly ];

		ginumouters++;

		/////////////////////////////////
		// Compute AABB of polygon
		/////////////////////////////////
		AABox thaab;
		thaab.BeginGrow();
		for( int iv=0; iv<ply.GetNumSides(); iv++ )
		{
			const fvec3& vpos = InVPool.GetVertex( ply.GetVertexID(iv) ).mPos;
			thaab.Grow(vpos);
		}
		thaab.EndGrow();


		fvec3 pntA = InVPool.GetVertex( ply.GetVertexID(0)).mPos;
		fvec3 pntB = InVPool.GetVertex( ply.GetVertexID(1)).mPos;
		fvec3 pntC = InVPool.GetVertex( ply.GetVertexID(2)).mPos;
		fvec3 dirA = (pntA-pntB).Normal();
		fvec3 dirB = (pntA-pntC).Normal();
		fvec3 pnrm = dirA.Cross(dirB);
		fvec3 pctr = (pntA+pntB+pntC)*0.333333333f;
		fplane3 PolyPlane(pnrm,pctr);

		/////////////////////////////////
		const fvec3& polymin = thaab.Min();
		const fvec3& polymax = thaab.Max();
		/////////////////////////////////

		const GridAddr ga_min = GetGridAddress( polymin );
		const GridAddr ga_max = GetGridAddress( polymax );

		int igW = ga_max.ix - ga_min.ix;
		int igH = ga_max.iy - ga_min.iy;
		int igD = ga_max.iz - ga_min.iz;

		int inumg = igW*igH*igD;
		inumgtot += inumg;

		GridAddr abs_nodeaddr;
		GridAddr rel_nodeaddr;

		for( int igx=ga_min.ix; igx<=ga_max.ix; igx++ )
		{
			rel_nodeaddr.ix = igx-ga_min.ix;
			abs_nodeaddr.ix = igx;

			for( int igy=ga_min.iy; igy<=ga_max.iy; igy++ )
			{
				rel_nodeaddr.iy = igy-ga_min.iy;
				abs_nodeaddr.iy = igy;

				for( int igz=ga_min.iz; igz<=ga_max.iz; igz++ )
				{
					rel_nodeaddr.iz = igz-ga_min.iz;
					abs_nodeaddr.iz = igz;

					/////////////////////////////////////////////////

					GetCuttingPlanes( abs_nodeaddr, topplane, bottomplane, leftplane, rightplane, frontplane, backplane );
					
					mupoly_clip_adapter poly_top, poly_bot;
					mupoly_clip_adapter poly_lft, poly_rgt;
					mupoly_clip_adapter poly_fnt, poly_bak;
					mupoly_clip_adapter srcp;

					int inumv = ply.GetNumSides();
					for( int iv=0; iv<inumv; iv++ )
					{
						srcp.AddVertex( InVPool.GetVertex(ply.GetVertexID(iv)) );
					}

					ginuminners++;

					if( topplane.ClipPoly( srcp, poly_top ) )
					if( bottomplane.ClipPoly( poly_top, poly_bot ) )
					if( leftplane.ClipPoly( poly_bot, poly_lft ) )
					if( rightplane.ClipPoly( poly_lft, poly_rgt ) )
					if( frontplane.ClipPoly( poly_rgt, poly_fnt ) )
					if( backplane.ClipPoly( poly_fnt, poly_bak ) )
					{
						GridNode* node = GetGridNode( abs_nodeaddr );

						if( 0 == node )
						{
							node = new GridNode;
							node->GridNodeName = CreateFormattedString( "fg_%d_%d_%d", igx, igy, igz );
							SetGridNode( abs_nodeaddr, node );
						}

						const char* NodeName = node->GridNodeName.c_str();

						int inumclippedverts = (int) poly_bak.GetNumVertices();

						submesh& node_submesh = MeshOut.MergeSubMesh(NodeName);

						node_submesh.RefAnnotations() = MeshIn.RefAnnotations();

						/////////////////////////////////////////////////////////
						// debug output (wavefront obj fmt)
						/////////////////////////////////////////////////////////

						if( 0 )
						{
							for( int ic=0; ic<inumclippedverts; ic++ )
							{
								const fvec3& pos = poly_bak.GetVertex(ic).Pos();
								PropTypeString pts;
								CPropType<fvec3>::ToString( pos, pts );
								
								orkprintf( "v %s\n", ic, pts.c_str() );

							}
						}

						/////////////////////////////////////////////////////////

						//const char* NodeName = node->GridNodeName.c_str();
						switch( inumclippedverts )
						{
							case 5:
							{	
								int iv0 = node_submesh.MergeVertex( poly_bak.GetVertex(0) );
								int iv1 = node_submesh.MergeVertex( poly_bak.GetVertex(1) );
								int iv2 = node_submesh.MergeVertex( poly_bak.GetVertex(2) );
								int iv3 = node_submesh.MergeVertex( poly_bak.GetVertex(3) );
								int iv4 = node_submesh.MergeVertex( poly_bak.GetVertex(4) );

								poly polya( iv0, iv1, iv2 );
								poly polyb( iv0, iv2, iv4 );
								poly polyc( iv4, iv2, iv3 );
								polya.SetAnnoMap(ply.GetAnnoMap());
								polyb.SetAnnoMap(ply.GetAnnoMap());
								polyc.SetAnnoMap(ply.GetAnnoMap());

								node_submesh.MergePoly( polya );
								node_submesh.MergePoly( polyb );
								node_submesh.MergePoly( polyc );
								break;
							}
							case 3:
							case 4:
							case 6:
							case 7:
							case 8:
							{	
								int iv0 = node_submesh.MergeVertex( poly_bak.GetVertex(0) );
								
								for( int ic=2; ic<inumclippedverts; ic++ )
								{
									int iva = node_submesh.MergeVertex( poly_bak.GetVertex(ic-1) );
									int ivb = node_submesh.MergeVertex( poly_bak.GetVertex(ic) );
									poly polya( iv0, iva, ivb );
									polya.SetAnnoMap(ply.GetAnnoMap());
									node_submesh.MergePoly( polya );
								}
								break;
							}
#if 0 // not yet
							case 4:
							{	
								int iv0 = node_submesh.MergeVertex( poly_bak.GetVertex(0) );
								int iv1 = node_submesh.MergeVertex( poly_bak.GetVertex(1) );
								int iv2 = node_submesh.MergeVertex( poly_bak.GetVertex(2) );
								int iv3 = node_submesh.MergeVertex( poly_bak.GetVertex(3) );
								poly polya( iv0, iv1, iv2, iv3 );
								polya.SetAnnoMap(ply.GetAnnoMap());
								node_submesh.MergePoly( polya );
								break;
							}
#endif
							default:
								OrkAssert(false);
								break;
						}
					}
				}
			}
		}
	}
	float fng = float(inumgtot)/float(inumpolys);
	orkprintf( "mergemesh nodetouces avg<%d> tot<%d>\n", int(fng), inumgtot );
	orkprintf( "NumFilledGrids<%d> numinners<%d> numouters<%d>\n", miNumFilledGrids, ginuminners, ginumouters );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void PartitionMesh_FixedGrid3d( const toolmesh& MeshIn, orkvector<toolmesh*>& OutMeshes )
{
	/*GridGraph thegraph;
	
	thegraph.BeginPreMerge();
	{
		thegraph.PreMergeMesh( MeshIn );
	}
	thegraph.EndPreMerge();
	
	toolmesh OutMesh;

	thegraph.MergeMesh( MeshIn,OutMesh );
	
	//const toolmesh& outmesh = thegraph.RefOutMesh();

	if( OutMesh.GetNumPolys() > 0 )
	{
		ork::file::Path outpath( "tmp/yo.obj" );

		OutMesh.WriteToWavefrontObj( outpath );
	}*/

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void PartitionMesh_FixedGrid3d_Driver( const tokenlist& options )
{
	////////////////////////////////////////

	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault("-inobj", "partition_in.obj" );
	OptionsMap.SetDefault("-outobj", "partition_out.obj" );
	OptionsMap.SetOptions(options);
	
	OptionsMap.DumpOptions();

	MeshUtil::toolmesh inmesh;

	std::string uva_in = OptionsMap.GetOption( "-inobj" )->GetValue();
	std::string uva_out = OptionsMap.GetOption( "-outobj" )->GetValue();

	inmesh.ReadAuto( uva_in.c_str() );

	orkprintf( "uvain<%s> uvaout<%s>\n", uva_in.c_str(), uva_out.c_str() );

	orkvector<toolmesh*> OutMeshes;

	PartitionMesh_FixedGrid3d( inmesh, OutMeshes );

}


///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
