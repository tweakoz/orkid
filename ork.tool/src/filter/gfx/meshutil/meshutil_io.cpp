////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/collada/collada.h>

///////////////////////////////////////////////////////////////////////////////

#if defined(_USE_D3DX)
INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::D3DX_OBJ_Filter,"D3DX_OBJ_Filter");
INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::XGM_D3DX_Filter,"XGM_D3DX_Filter");
INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::OBJ_D3DX_Filter,"OBJ_D3DX_Filter");
#endif

INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::XGM_OBJ_Filter,"XGM_OBJ_Filter");
INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::OBJ_OBJ_Filter,"OBJ_OBJ_Filter");
INSTANTIATE_TRANSPARENT_RTTI(ork::MeshUtil::OBJ_XGM_Filter,"OBJ_XGM_Filter");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

void OBJ_XGM_Filter::Describe() {}
void XGM_OBJ_Filter::Describe() {}
void OBJ_OBJ_Filter::Describe() {}

OBJ_XGM_Filter::OBJ_XGM_Filter( ){}
XGM_OBJ_Filter::XGM_OBJ_Filter( ){}
OBJ_OBJ_Filter::OBJ_OBJ_Filter( ){}

bool OBJ_XGM_Filter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	MeshUtil::toolmesh tmesh;
	tmesh.ReadFromWavefrontObj( inf.c_str());
	tmesh.WriteToXgmFile( outf.c_str() );
	return true;
}
bool XGM_OBJ_Filter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	MeshUtil::toolmesh tmesh;
	tmesh.ReadFromXGM( inf.c_str());
	tmesh.WriteToWavefrontObj( outf.c_str() );
	return true;
}
bool OBJ_OBJ_Filter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	MeshUtil::toolmesh tmesh;
	tmesh.ReadFromWavefrontObj( inf.c_str() );
	tmesh.WriteToWavefrontObj( outf.c_str() );
	return true;
}

#if defined(_USE_D3DX)
void XGM_D3DX_Filter::Describe() {}
void D3DX_OBJ_Filter::Describe() {}
void OBJ_D3DX_Filter::Describe() {}
D3DX_OBJ_Filter::D3DX_OBJ_Filter( ){}
XGM_D3DX_Filter::XGM_D3DX_Filter( ){}
OBJ_D3DX_Filter::OBJ_D3DX_Filter( ){}

bool D3DX_OBJ_Filter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	MeshUtil::toolmesh tmesh;
	tmesh.ReadFromD3DXFile( inf.c_str() );
	tmesh.WriteToWavefrontObj( outf.c_str() );
	return true;
}
bool XGM_D3DX_Filter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	MeshUtil::toolmesh tmesh;
	tmesh.ReadFromXGM( inf.c_str() );
	tmesh.WriteToD3DXFile( outf.c_str() );
	return true;
}
bool OBJ_D3DX_Filter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	MeshUtil::toolmesh tmesh;
	tmesh.ReadFromWavefrontObj( inf.c_str() );
	tmesh.WriteToD3DXFile( outf.c_str() );
	return true;
}

#endif

///////////////////////////////////////////////////////////////////////////////

void toolmesh::ReadAuto( const file::Path& BasePath )
{
	ork::file::Path::SmallNameType ext = BasePath.GetExtension();

	if( ext == "dae" )
	{
		DaeReadOpts opts;
		ReadFromDaeFile( BasePath, opts );
	}
	else
	if( ext == "obj" )
	{
		ReadFromWavefrontObj( BasePath );
	}
	else if( ext == "xgm" )
	{
		ReadFromXGM( BasePath );
	}
#if defined(_USE_D3DX)
	else if( ext == "x" )
	{
		ReadFromD3DXFile( BasePath );
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::WriteAuto( const file::Path& BasePath ) const
{
	ork::file::Path::SmallNameType ext = BasePath.GetExtension();

	if( ext == "dae" )
	{
		DaeWriteOpts out_opts;
		WriteToDaeFile( BasePath, out_opts );
	}
	else
		if( ext == "obj" )
	{
		WriteToWavefrontObj( BasePath );
	}
#if ! defined(IX)
	else if( ext == "x" )
	{
		WriteToD3DXFile( BasePath );
	}
#endif
}
///////////////////////////////////////////////////////////////////////////////

FlatSubMesh::FlatSubMesh( const submesh& mesh )
{
	const vertexpool& vpool = mesh.RefVertexPool();

	mesh.FindNSidedPolys( TrianglePolyIndices, 3 );
	mesh.FindNSidedPolys( QuadPolyIndices, 4 );

	const char* vtxformat = mesh.GetAnnotation("OutVtxFormat");
	evtxformat = PropType<lev2::EVtxStreamFormat>::FromString( vtxformat );

	orkprintf( "vtxformat<%s>\n", vtxformat );
	
	////////////////////////////////////////////////////////
	int inumv = (int) vpool.GetNumVertices();
	int inumtri = int(TrianglePolyIndices.size());
	int inumqua = int(QuadPolyIndices.size());
	////////////////////////////////////////////////////////
	
	switch( evtxformat )
	{
		case lev2::EVTXSTREAMFMT_V12N12B12T16:
		{	
			lev2::SVtxV12N12B12T16 OutVertex;
			for( int iv0=0; iv0<inumv; iv0++ )
			{	
				const vertex& invtx =  vpool.GetVertex(iv0);

				OutVertex.mPosition = invtx.mPos;
				OutVertex.mNormal = invtx.mNrm;
				OutVertex.mBiNormal = invtx.mUV[0].mMapBiNormal;
				OutVertex.mUV0 = invtx.mUV[0].mMapTexCoord;
				OutVertex.mUV1 = invtx.mUV[1].mMapTexCoord;

				MergeVertsT16.push_back( OutVertex );
			}
			inumverts = int(MergeVertsT16.size());
			ivtxsize = sizeof(lev2::SVtxV12N12B12T16);
			poutvtxdata = (void*) & MergeVertsT16.at(0);
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8C4:
		{	
			orkvector<lev2::SVtxV12N12B12T8C4>	MergeVerts;
			lev2::SVtxV12N12B12T8C4 OutVertex;
			for( int iv0=0; iv0<inumv; iv0++ )
			{	
				const vertex& invtx =  vpool.GetVertex(iv0);

				OutVertex.mPosition = invtx.mPos;
				OutVertex.mNormal = invtx.mNrm;
				OutVertex.mBiNormal = invtx.mUV[0].mMapBiNormal;
				OutVertex.mUV0 = invtx.mUV[0].mMapTexCoord;
				OutVertex.mColor = invtx.mCol[0].GetRGBAU32();

				MergeVertsT8.push_back( OutVertex );
			}
			inumverts = int(MergeVertsT8.size());
			ivtxsize = sizeof(lev2::SVtxV12N12B12T8C4);
			poutvtxdata = (void*) & MergeVertsT8.at(0);
			break;
		}
		default: // vertex format not supported
			OrkAssert(false);
			break;
	}

	for( int it=0; it<inumtri; it++ )
	{
		int idx = TrianglePolyIndices[it];
		const poly& intri = mesh.RefPoly(idx);
		int inumsides = intri.GetNumSides();
		for( int iv=0; iv<inumsides; iv++ )
		{
			int idx = intri.GetVertexID(iv);
			MergeTriIndices.push_back( idx );
		}
	}

	for( int iq=0; iq<inumqua; iq++ )
	{
		int idx = QuadPolyIndices[iq];
		const poly& inqua = mesh.RefPoly(idx);
		int inumsides = inqua.GetNumSides();
		int idx0 = inqua.GetVertexID(0);
		int idx1 = inqua.GetVertexID(1);
		int idx2 = inqua.GetVertexID(2);
		int idx3 = inqua.GetVertexID(3);

		MergeTriIndices.push_back( idx0 );
		MergeTriIndices.push_back( idx1 );
		MergeTriIndices.push_back( idx2 );

		MergeTriIndices.push_back( idx0 );
		MergeTriIndices.push_back( idx2 );
		MergeTriIndices.push_back( idx3 );
	}
}

///////////////////////////////////////////////////////////////////////////////

}}
///////////////////////////////////////////////////////////////////////////////
