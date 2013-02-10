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

#include <D3DX9Mesh.h>

namespace devil {
void InitDevIL();
}
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

IDirect3DDevice9* GetNullD3dDevice();

struct ColorComp
{
	bool operator()( const CColor4& c1, const CColor4& c2 ) const
	{
		boost::Crc64 crc1 = boost::crc64( (const void *) & c1, sizeof(c1) );
		boost::Crc64 crc2 = boost::crc64( (const void *) & c2, sizeof(c2) );
		return (crc1.crc0<crc2.crc0);
	}
};

ID3DXMesh* ToD3DXMesh( const submesh& mesh );
void FromD3DXMesh( ID3DXMesh* indxmesh, submesh& mesh );

///////////////////////////////////////////////////////////////////////////////

int ComputeIndex( const void* pdata, int idx, bool b32bitidx )
{
	int irval = -1;
	if( b32bitidx )
	{
		const U32* pidx = (const U32*) pdata;
		U32 IDX = pidx[idx];
		irval = IDX;
	}
	else
	{
		const U16* pidx = (const U16*) pdata;
		U16 IDX = pidx[idx];
		irval = IDX;
	}
	return irval;
}

///////////////////////////////////////////////////////////////////////////////

static float ComputePolyArea( const char* pdata, int istride, int ivA, int ivB, int ivC )
{
	const float* pposA = (const float*)
	( pdata+ (ivA*istride) );
	const float* pposB = (const float*)
	( pdata+ (ivB*istride) );
	const float* pposC = (const float*)
	( pdata+(ivC*istride) );

	CVector3 vtx[3];
	vtx[0].SetXYZ( pposA[0], pposA[1], pposA[2] );
	vtx[1].SetXYZ( pposB[0], pposB[1], pposB[2] );
	vtx[2].SetXYZ( pposC[0], pposC[1], pposC[2] );

	// area of triangle 1/2 length of cross product the vector of any two edges
	float farea = (vtx[1]-vtx[0]).Cross(vtx[2]-vtx[0]).Mag() * 0.5f;
	
	return farea;
}

///////////////////////////////////////////////////////////////////////////////

float* ComputeAreaIMT( const UvAtlasContext& Ctx, ID3DXMesh* dxinmesh )
{
	int inumfaces = dxinmesh->GetNumFaces();

	float* InImtArray = new float [ 3*inumfaces ];

	bool b32bitidx = dxinmesh->GetOptions()&D3DXMESH_32BIT;
	D3DVERTEXELEMENT9 SourceDeclaration[MAX_FVF_DECL_SIZE];
	LPVOID pvbdata = 0;
	LPVOID pibdata = 0;
	HRESULT hr = dxinmesh->LockVertexBuffer(D3DLOCK_READONLY,&pvbdata);
	hr = dxinmesh->LockIndexBuffer(D3DLOCK_READONLY,&pibdata);
	hr = dxinmesh->GetDeclaration(SourceDeclaration);
	int inuminsrcdecl = D3DXGetDeclLength( SourceDeclaration );
	int isrcvtxstride = dxinmesh->GetNumBytesPerVertex();

	orkmultimap<int,D3DVERTEXELEMENT9> DeclMap;

	for( int i=0; i<inuminsrcdecl; i++ )
	{
		const D3DVERTEXELEMENT9& elem = SourceDeclaration[i];
		DeclMap.insert( std::make_pair<int,D3DVERTEXELEMENT9>( elem.Usage, elem ) );
	}

	const char* pcharbase = (const char*) pvbdata;
	///////////////////////////////////////////////////
	// first get position, prime the SourceVerts vector
	///////////////////////////////////////////////////
	orkmultimap<int,D3DVERTEXELEMENT9>::const_iterator itpos = DeclMap.find( D3DDECLUSAGE_POSITION );
	OrkAssert( itpos != DeclMap.end() );
	const D3DVERTEXELEMENT9& poselem = itpos->second;
	OrkAssert( poselem.Type == D3DDECLTYPE_FLOAT3 );

	int inumidx = inumfaces*3;
	int idxcache[3];

	float fminarea = 10000000.0f;
	float fmaxarea = -10000000.0f;
	float ftotalarea = 0.0f;

	orkvector<float> polygon_areas;
	for( int i=0; i<inumidx; i++ )
	{	idxcache[ (i%3) ] = ComputeIndex( pibdata, i, b32bitidx );
		if( 2 == (i%3) )
		{	float farea = ComputePolyArea( pcharbase+poselem.Offset, isrcvtxstride,
											idxcache[0],idxcache[1],idxcache[2] );
			ftotalarea += farea;
			polygon_areas.push_back(farea);
			if( farea<fminarea ) fminarea=farea;
			if( farea>fmaxarea ) fmaxarea=farea;
		}
	}
	OrkAssert( int(polygon_areas.size()) == inumfaces );
	if( fminarea==0.0f) fminarea=0.01f;
	float farearange = (fmaxarea-fminarea);
	float farearatio = fmaxarea/fminarea;
	float fscaleunification = Ctx.mfAreaUnification;
	for( int iface=0; iface<inumfaces; iface++ )
	{
		float farea = polygon_areas[iface];
		float funita = (farea==fminarea) ? 1.0f : (farea-fminarea)/farearange;
		
		float polyscale = fscaleunification*(1.0f-funita) + ((1.0f/fscaleunification)*funita);

		float imt_a = polyscale;
		float imt_b = 0.0f;
		float imt_c = polyscale;

		InImtArray[(iface*3)+0] = imt_a;
		InImtArray[(iface*3)+1] = imt_b;
		InImtArray[(iface*3)+2] = imt_c;
	}

	hr = dxinmesh->UnlockVertexBuffer();
	hr = dxinmesh->UnlockIndexBuffer();

	return InImtArray;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> class AutoDeleteAry
{
public:

	AutoDeleteAry(T*pa=0) : mpArray(pa) {}
	~AutoDeleteAry()
	{
		if( mpArray )
			delete[] mpArray;
	}
	void SetArray( T* p ) { mpArray=p; }
private:	
	T*	mpArray;
};

///////////////////////////////////////////////////////////////////////////////

bool UvAtlasSubMesh( const UvAtlasContext& Ctx )
{
	ID3DXMesh* dxinmesh = 0;
	
	int inumf = 0;
	int inumv = 0;

	const submesh& inp_mesh = Ctx.mInpMesh;

	annopolyposlut AnnoPolyPosLut;
	inp_mesh.ExportPolyAnnotations( AnnoPolyPosLut );

	//toolmesh PolyGroupMesh;
	//PolyGroupMesh.MergeSubMesh( Ctx.mInpMesh, pgroup );

	inumf = inp_mesh.GetNumPolys();
	inumv = (int) inp_mesh.RefVertexPool().GetNumVertices();

	//inp_mesh.SetAnnotation( "OutVtxFormat", inp_mesh.GetAnnotation("OutVtxFormat") );

	dxinmesh = ToD3DXMesh( inp_mesh );

	//PolyGroupMesh.CopyMaterialsFromToolMesh(inp_mesh);
	//DaeWriteOpts out_opts;
	//PolyGroupMesh.WriteToDaeFile( "./data/temp/uvatlasdbg/uvt1.dae", out_opts );

	////////////////////////

	orkprintf( "uvatlas mesh<%p> numf<%d> numv<%d>\n",& Ctx.mInpMesh, inumf, inumv );

	////////////////////////
	// Compute Adjacency
	
	float fepsilon = 10e-6f;

	DWORD* InAdjacency = new DWORD[ dxinmesh->GetNumFaces()*3 ];
	AutoDeleteAry<DWORD>	autodel_inpadj( InAdjacency );

	HRESULT hr = dxinmesh->GenerateAdjacency(fepsilon,InAdjacency);
	OrkAssert( SUCCEEDED(hr) );

	if( false == SUCCEEDED(hr) ) return false;


	////////////////////////
	// "Clean Mesh"

	D3DXCLEANTYPE  CleanInOptions = D3DXCLEAN_SIMPLIFICATION;
	ID3DXMesh* DxCleanMesh = 0;
	DWORD* DxCleanAdjacency = new DWORD[ dxinmesh->GetNumFaces()*3 ];
	AutoDeleteAry<DWORD>	autodel_cleanadj( DxCleanAdjacency );

	LPD3DXBUFFER DxCleanErrors = 0;

	hr = D3DXCleanMesh( CleanInOptions, dxinmesh, InAdjacency, & DxCleanMesh, DxCleanAdjacency, & DxCleanErrors );
	OrkAssert( SUCCEEDED( hr ) );

	if( false == SUCCEEDED(hr) ) return false;

	//submesh CleanMesh;
	//CleanMesh.SetAnnotation( "OutVtxFormat", Ctx.mInpMesh.GetAnnotation("OutVtxFormat") );
	//FromD3DXMesh( DxCleanMesh, CleanMesh );
	//CleanMesh.CopyMaterialsFromToolMesh(Ctx.mInpMesh);
	//DaeWriteOpts out_opts;
	//CleanMesh.WriteToDaeFile( "./data/temp/uvatlasdbg/uvtc.dae", out_opts );

	////////////////////////
	// Compute Atlas

	DWORD* InFalseEdges = 0;
	FLOAT* InImtArray = 0;
	LPD3DXUVATLASCB InCallback = 0;
	FLOAT InCallbackFrequency = 0.00001f;
	LPVOID InUserContent = 0;
	//DWORD InOptions = D3DXUVATLAS_GEODESIC_QUALITY; //D3DXUVATLAS_GEODESIC_FAST; //D3DXUVATLAS_DEFAULT;
	DWORD InOptions = D3DXUVATLAS_GEODESIC_QUALITY; //D3DXUVATLAS_GEODESIC_FAST; //D3DXUVATLAS_DEFAULT;
	
	int InTexIdx = 1;
	int InMaxCharts = 0;
	float InStretch = Ctx.mfStretching;
	float InGutter = Ctx.mfGutter;
	int InWidth = Ctx.miTexRes;
	int InHeight = Ctx.miTexRes;

	ID3DXMesh*		OutDxMesh = 0;
	LPD3DXBUFFER	OutFacePartitioning = 0;
	LPD3DXBUFFER	OutVertexRemapArray = 0;
	FLOAT			OutStretched = 0.0f;
	UINT			OutNumCharts = 0;
	LPD3DXBUFFER	ImtArray = 0;

	/////////////////////////////////////////////////////////////

	AutoDeleteAry<float>	autodel_imt;

	if( Ctx.mbDoIMT )
	{
		file::Path texbase = Ctx.mIMTTexturePath;
		texbase.SetExtension("");

		std::string imgpth = CreateFormattedString( "./%s.tga",
						texbase.c_str() );

		texbase = file::Path(imgpth.c_str());
		file::Path abspath = texbase.ToAbsolute();
		orkprintf( "attempting to use <%s> for IMT\n", abspath.c_str() );

		if( CFileEnv::DoesFileExist( abspath ) )
		{
			orkprintf( "ok, <%s> exists\n", abspath.c_str() );

			file::Path::NameType op;
			file::Path blurred_path = abspath;
			blurred_path.SetExtension("");
			op.format( "%s_blurred", blurred_path.GetName().c_str() );
			blurred_path.SetFile( op.c_str() );
			blurred_path.SetExtension( ".png" );

			//////////////////////////////////////////////////////////////
			// blur the texture to remove upper frequency signals (noise)
			// this will give the IMT more breathing room to optimize 
			// uv layout
			//////////////////////////////////////////////////////////////

			devil::InitDevIL();

			ILuint ImageName;
			ilGenImages(1, &ImageName);
			ilBindImage(ImageName);
			ILboolean OriginOK = ilOriginFunc( IL_ORIGIN_LOWER_LEFT );
			bool bv = ilLoadImage( (const ILstring) abspath.c_str() );
			OrkAssert(bv);
			bv = ilActiveImage( 0 );
			OrkAssert(bv);
			bv = iluBlurGaussian(1);
			OrkAssert(bv);
			bv = ilEnable( IL_FILE_OVERWRITE );
			OrkAssert(bv);
			bv = ilSaveImage( blurred_path.c_str() );
			OrkAssert(bv);

			//////////////////////////////////////////////////////////////

			LPDIRECT3DTEXTURE9 pIMTTEXTURE = 0;

			hr = D3DXCreateTextureFromFile(
					GetNullD3dDevice(),
					blurred_path.c_str(),
					& pIMTTEXTURE );

			if( SUCCEEDED(hr) )
			{
				hr = D3DXComputeIMTFromTexture(
				  DxCleanMesh,
				  pIMTTEXTURE,
				  0, // uv index
				  0, // wrap options
				  0, // progress callback
				  0, // user context
				  & ImtArray
				);

				if( SUCCEEDED(hr) )
				{
					orkprintf( "WOOHOO, USING IMT\n" );

					InImtArray = (float*) ImtArray->GetBufferPointer();
				}
			}
		}
	}
	else
	{
		InImtArray = ComputeAreaIMT( Ctx, DxCleanMesh );
		autodel_imt.SetArray( InImtArray );
	}

	hr = D3DXUVAtlasCreate(
			DxCleanMesh, 
			InMaxCharts, InStretch,
			InWidth, InHeight, InGutter, InTexIdx,
			DxCleanAdjacency, InFalseEdges, 
			InImtArray,
			InCallback,
			InCallbackFrequency,
			InUserContent,
			InOptions,
			& OutDxMesh,
			& OutFacePartitioning,
			& OutVertexRemapArray,
			& OutStretched,
			& OutNumCharts 
			);

	if( false == SUCCEEDED(hr) )
	{
		std::string dbgstr = CreateFormattedString("Sorry, could not atlas this submesh<%s> probably too many polys in it! try reducing dice size for this group or splitting it into multiple groups\n", Ctx.mGroupName.c_str() );
		
		orkprintf( "%s\n", dbgstr.c_str() );

		MessageBox( 0, dbgstr.c_str(), "Error While Atlasing", MB_OK );

		return false;
	}

	Ctx.mOutMesh.SetAnnotation( "OutVtxFormat", Ctx.mInpMesh.GetAnnotation("OutVtxFormat") );
	FromD3DXMesh( OutDxMesh, Ctx.mOutMesh );
	Ctx.mOutMesh.ImportPolyAnnotations( AnnoPolyPosLut );

	OutDxMesh->Release();
	DxCleanMesh->Release();
	if( dxinmesh )
	{
		dxinmesh->Release();
	}
	if( OutFacePartitioning )
	{
		OutFacePartitioning->Release();
	}
	if( OutVertexRemapArray )
	{
		OutVertexRemapArray->Release();
	}

	return true;
}


void GenerateUVAtlas( const tokenlist& options )
{
	/*ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault( "-inobj", "uvatlas_in.obj" );
	OptionsMap.SetDefault( "-outobj", "uvatlas_out.obj" );
	OptionsMap.SetDefault( "-texres", "256" );
	OptionsMap.SetDefault( "-gutter", "2.0" );
	OptionsMap.SetOptions( options );

	OptionsMap.DumpOptions();

	MeshUtil::toolmesh inmesh;

	std::string uva_in = OptionsMap.GetOption( "-inobj" )->GetValue();
	std::string uva_out = OptionsMap.GetOption( "-outobj" )->GetValue();
	int itexres = atoi( OptionsMap.GetOption("-texres")->GetValue().c_str());
	float fgutter = float(atof( OptionsMap.GetOption("-gutter")->GetValue().c_str()));

	inmesh.ReadAuto( uva_in.c_str() );

	orkprintf( "uvain<%s> uvaout<%s>\n", uva_in.c_str(), uva_out.c_str() );

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////
	// split into separate toolmesh'es per lightmap group
	// perhaps we can generalize this into a 'when to split' policy
	////////////////////////////////////////////////////////////////////

	orkvector<MeshUtil::toolmesh*> tmeshes;

	
	}
	else
	{
		tmeshes.push_back( & inmesh );
	}

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	int inummeshes = tmeshes.size();

	for( int im=0; im<inummeshes; im++ )
	{
		////////////////////////
		MeshUtil::toolmesh* srcmesh = tmeshes[im];
		PropTypeString vtxfmt; 
		CPropType<lev2::EVtxStreamFormat>::ToString(lev2::EVTXSTREAMFMT_V12N12B12T16,vtxfmt);
		srcmesh->SetAnnotation( "OutVtxFormat", vtxfmt.c_str() );
		MeshUtil::toolmesh outmesh;
		UvAtlasContext UvaCtx( *srcmesh, outmesh );
		UvaCtx.mfGutter = fgutter;
		UvaCtx.miTexRes = itexres;
		UvAtlasToolMesh( UvaCtx );
		////////////////////////
		ork::file::Path outpath( uva_out.c_str() );
		std::string outname = outpath.GetName().c_str();
		outname += CreateFormattedString( "_lmap%d", im );
		outpath.SetFile( outname.c_str() );
		outmesh.WriteAuto( outpath );
		////////////////////////
	}*/
}

UvAtlasContext::UvAtlasContext( const submesh& inmesh, submesh& outmesh )
	: mInpMesh( inmesh )
	, mOutMesh( outmesh )
	, mfGutter( 2.0f )
	, miTexRes( 256 )
{
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
#endif
