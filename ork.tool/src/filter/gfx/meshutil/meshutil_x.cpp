////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#if defined(_USE_D3DX)

#include <D3DX9Mesh.h>

namespace ork { namespace lev2 { 
	D3DVERTEXELEMENT9* GetHardwareVertexDeclaration( EVtxStreamFormat eType );
} }

IDirect3DDevice9* GetNullD3dDevice();

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

IDirect3DDevice9* GetNullD3dDevice()
{
	static IDirect3DDevice9* gpdev = 0;

	if( 0 == gpdev )
	{
		HRESULT hr;
		IDirect3D9* pD3D = Direct3DCreate9( D3D_SDK_VERSION );
		if( NULL == pD3D )
			return NULL;

		D3DDISPLAYMODE Mode;
		pD3D->GetAdapterDisplayMode(0, &Mode);

		D3DPRESENT_PARAMETERS pp;
		ZeroMemory( &pp, sizeof(D3DPRESENT_PARAMETERS) );
		pp.BackBufferWidth  = 1;
		pp.BackBufferHeight = 1;
		pp.BackBufferFormat = Mode.Format;
		pp.BackBufferCount  = 1;
		pp.SwapEffect       = D3DSWAPEFFECT_COPY;
		pp.Windowed         = TRUE;

		hr = pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, GetConsoleWindow(),
								 D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &gpdev );
//		SAFE_RELEASE( pD3D );

		OrkAssert( SUCCEEDED(hr) );
	}

    return gpdev;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ID3DXMesh* ToD3DXMesh( const submesh& mesh )
{
	FlatSubMesh fsub( mesh );

	///////////////////////////////////////////////////////////////////////////

	int inumtriindices = int(fsub.MergeTriIndices.size());

	OrkAssert( 0 == (inumtriindices%3) );

	////////////////////////////////////////////////////////

	LPDIRECT3DDEVICE9 dx9dev = GetNullD3dDevice();
	D3DVERTEXELEMENT9* hwdecl = lev2::GetHardwareVertexDeclaration( fsub.evtxformat );

	ID3DXMesh* dxmergemesh = 0;
	DWORD opts = D3DXMESH_SYSTEMMEM|D3DXMESH_32BIT;
	HRESULT hr = D3DXCreateMesh( inumtriindices/3, fsub.inumverts, opts, hwdecl, dx9dev, & dxmergemesh );
	OrkAssert( SUCCEEDED(hr) );

	////////////////////////////////////////////////////////
	void* pwhere = 0;
	hr = dxmergemesh->LockVertexBuffer( 0, & pwhere );
	OrkAssert( SUCCEEDED(hr) );
	{
		memcpy( pwhere, fsub.poutvtxdata, fsub.inumverts*fsub.ivtxsize );
	}
	hr = dxmergemesh->UnlockVertexBuffer(); 
	OrkAssert( SUCCEEDED(hr) );

	////////////////////////////////////////////////////////
	hr = dxmergemesh->LockIndexBuffer( 0, & pwhere ); 
	OrkAssert( SUCCEEDED(hr) );
	{
		memcpy( pwhere, & fsub.MergeTriIndices.at(0), inumtriindices*sizeof(int) );
	}
	hr = dxmergemesh->UnlockIndexBuffer(); 
	OrkAssert( SUCCEEDED(hr) );

	return dxmergemesh;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void toolmesh::WriteToD3DXFile( const file::Path& BasePath ) const
{
	ork::file::Path XPath = BasePath;
	XPath.SetExtension( "x" );

	toolmesh merged_mesh;
	merged_mesh.MergeToolMeshAs( *this, "merged" );

	submesh* sub_mesh = merged_mesh.FindSubMesh("merged");

	ID3DXMesh* dxmergemesh = ToD3DXMesh(*sub_mesh);

	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////
	DWORD* padj = 0; //new DWORD[ dxmergemesh->GetNumFaces()*3 ];
	//hr = dxmergemesh->GenerateAdjacency(0.1f,padj);
	//OrkAssert( SUCCEEDED(hr) );
	////////////////////////////////////////////////////////
	HRESULT hr = D3DXSaveMeshToX( XPath.ToAbsolute().c_str(), dxmergemesh, padj, 0, 0, 0, D3DXF_FILEFORMAT_TEXT );
	////////////////////////////////////////////////////////
	OrkAssert( SUCCEEDED( hr ) );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FromD3DXMesh( ID3DXMesh* indxmesh, submesh& mesh )
{
	const char* vtxformat = mesh.GetAnnotation("OutVtxFormat");
	lev2::EVtxStreamFormat evtxformat = PropType<lev2::EVtxStreamFormat>::FromString( vtxformat );
	////////////////////////////////////////////////////////
	switch( evtxformat )
	{
		case lev2::EVTXSTREAMFMT_V12N12B12T16:
			break;
		case lev2::EVTXSTREAMFMT_V12N12B12T8C4:
			break;
		default:
			OrkAssert(false);
			break;
	}
	////////////////////////////////////////////////////////
	bool b32bitidx = indxmesh->GetOptions()&D3DXMESH_32BIT;
	////////////////////////////////////////////////////////
	int inumfaces = indxmesh->GetNumFaces();
	int inumvertices = indxmesh->GetNumVertices();
	int isrcvtxstride = indxmesh->GetNumBytesPerVertex();
	////////////////////////////////////////////////////////
	D3DVERTEXELEMENT9* targethwdecl = lev2::GetHardwareVertexDeclaration( evtxformat );
	D3DVERTEXELEMENT9 SourceDeclaration[MAX_FVF_DECL_SIZE];
	HRESULT hr = indxmesh->GetDeclaration( SourceDeclaration );
	OrkAssert( SUCCEEDED(hr) );
	int inuminsrcdecl = D3DXGetDeclLength( SourceDeclaration );
	////////////////////////////////////////////////////////

	orkvector<vertex> SourceVerts;

	orkmultimap<int,D3DVERTEXELEMENT9> DeclMap;

	for( int i=0; i<inuminsrcdecl; i++ )
	{
		const D3DVERTEXELEMENT9& elem = SourceDeclaration[i];

		DeclMap.insert( std::make_pair<int,D3DVERTEXELEMENT9>( elem.Usage, elem ) );

	}

	////////////////////////////////////////////////////////
	void* pwhere = 0;
	hr = indxmesh->LockVertexBuffer( 0, & pwhere );
	OrkAssert( SUCCEEDED(hr) );
	{
		const char* pcharbase = (const char*) pwhere;
		///////////////////////////////////////////////////
		// first get position, prime the SourceVerts vector
		///////////////////////////////////////////////////
		orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator itpos = DeclMap.find( D3DDECLUSAGE_POSITION );
		OrkAssert( itpos != DeclMap.end() );
		const D3DVERTEXELEMENT9& poselem = itpos->second;
		OrkAssert( poselem.Type == D3DDECLTYPE_FLOAT3 );
		for( int iv=0; iv<inumvertices; iv++ )
		{
			const float* ppos = (const float*)( pcharbase+poselem.Offset + (iv*isrcvtxstride) );
			
			vertex basevert;
			basevert.mPos.SetXYZ( ppos[0], ppos[1], ppos[2] );
			SourceVerts.push_back( basevert );
		}
		///////////////////////////////////////////////////
		// get normals
		///////////////////////////////////////////////////
		orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator itnrm = DeclMap.find( D3DDECLUSAGE_NORMAL );
		OrkAssert( itnrm != DeclMap.end() );
		const D3DVERTEXELEMENT9& nrmelem = itnrm->second; 
		OrkAssert( nrmelem.Type == D3DDECLTYPE_FLOAT3 );
		for( int iv=0; iv<inumvertices; iv++ )
		{
			const float* pnrm = (const float*)( pcharbase+nrmelem.Offset + (iv*isrcvtxstride) );
			SourceVerts[iv].mNrm = fvec3( pnrm[0], pnrm[1], pnrm[2] );
		}
		///////////////////////////////////////////////////
		// get binormals
		///////////////////////////////////////////////////
		orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator itbinbeg = DeclMap.find( D3DDECLUSAGE_BINORMAL );
		orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator itbinend = DeclMap.upper_bound( D3DDECLUSAGE_BINORMAL );
		int ibinidx = 0;
		for( orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator itbin=itbinbeg; itbin!=itbinend; itbin++ )
		{	const D3DVERTEXELEMENT9& binelem = itbin->second;
			OrkAssert( binelem.Type == D3DDECLTYPE_FLOAT3 );
			if( ibinidx < 4 )
			{	for( int iv=0; iv<inumvertices; iv++ )
				{	const float* pnrm = (const float*)( pcharbase+binelem.Offset + (iv*isrcvtxstride) );
					SourceVerts[iv].mUV[ibinidx].mMapBiNormal = fvec3( pnrm[0], pnrm[1], pnrm[2] );
				}
			}
			ibinidx++;
		}
		///////////////////////////////////////////////////
		// get UVs
		///////////////////////////////////////////////////
		orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator ituvbeg = DeclMap.find( D3DDECLUSAGE_TEXCOORD );
		orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator ituvend = DeclMap.upper_bound( D3DDECLUSAGE_TEXCOORD );
		int iuvidx = 0;
		for( orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator ituv=ituvbeg; ituv!=ituvend; ituv++ )
		{	const D3DVERTEXELEMENT9& uvelem = ituv->second;
			OrkAssert( uvelem.Type == D3DDECLTYPE_FLOAT2 );
			if( iuvidx < 4 )
			{	for( int iv=0; iv<inumvertices; iv++ )
				{	const float* puv0 = (const float*)( pcharbase+uvelem.Offset + (iv*isrcvtxstride) );
					SourceVerts[iv].mUV[iuvidx].mMapTexCoord = fvec2( puv0[0], puv0[1] );
					SourceVerts[iv].miNumUvs = iuvidx+1;
				}
			}
			iuvidx++;
		}
	}
	hr = indxmesh->UnlockVertexBuffer(); 
	OrkAssert( SUCCEEDED(hr) );
	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////
	hr = indxmesh->LockIndexBuffer( 0, & pwhere ); 
	OrkAssert( SUCCEEDED(hr) );
	{
		int inumidx = inumfaces*3;
		int idxcache[3];
		for( int i=0; i<inumidx; i++ )
		{
			if( b32bitidx )
			{
				const U32* pidx = (const U32*) pwhere;
				U32 IDX = pidx[i];
				idxcache[ (i%3) ] = IDX;
			}
			else
			{
				const U16* pidx = (const U16*) pwhere;
				U16 IDX = pidx[i];
				idxcache[ (i%3) ] = IDX;
			}

			if( 2 == (i%3) )
			{
				const vertex& tv0 = SourceVerts[ idxcache[0] ];
				const vertex& tv1 = SourceVerts[ idxcache[1] ];
				const vertex& tv2 = SourceVerts[ idxcache[2] ];

				int imapped0 = mesh.MergeVertex( tv0 );
				int imapped1 = mesh.MergeVertex( tv1 );
				int imapped2 = mesh.MergeVertex( tv2 );

				poly toolpoly( imapped0, imapped1, imapped2 );
				mesh.MergePoly( toolpoly );
			}
		}
	}
	hr = indxmesh->UnlockIndexBuffer(); 
	OrkAssert( SUCCEEDED(hr) );
	////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void toolmesh::ReadFromD3DXFile( const file::Path& BasePath )
{
	/*ork::file::Path XPath = BasePath;
	XPath.SetExtension( "x" );

	////////////////////////////////////////////////////////

	LPDIRECT3DDEVICE9 dx9dev = GetNullD3dDevice();
	//lev2::GfxTarget* ptarg = lev2::CXGMModelLoaderL2::GetLoaderTarget();

	////////////////////////////////////////////////////////

	LPD3DXBUFFER padj = 0;
	LPD3DXBUFFER pmats = 0;
	LPD3DXBUFFER pfxi = 0;
	DWORD nummat = 0;
	ID3DXMesh* dxreadmesh = 0;

	HRESULT hr = D3DXLoadMeshFromX(
		XPath.ToAbsolute().c_str(), 
		D3DXMESH_SYSTEMMEM,
		dx9dev,
		& padj,
		& pmats,
		& pfxi,
		& nummat,
		& dxreadmesh );
	OrkAssert( SUCCEEDED(hr) );

	FromD3DXMesh( dxreadmesh, *this );*/

}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif
