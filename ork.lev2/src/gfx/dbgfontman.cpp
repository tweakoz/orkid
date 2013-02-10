////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxenv_targetinterfaces.h>

namespace ork { namespace lev2 {

const int CFont::kMaxChars = 16384;

///////////////////////////////////////////////////////////////////////////////

void CFontMan::BeginTextBlock( GfxTarget *pTARG, int imaxcharcount )
{
#if ! defined(_XBOX)
	static bool bLoadFonts = true;
	if( bLoadFonts )
	{
		CFontMan::InitFonts( pTARG );
		bLoadFonts = false;
	}
#endif

	if( 0==imaxcharcount )
		imaxcharcount = 1024;
	int inumv = imaxcharcount*8;
	
			
	VertexBufferBase& VB = pTARG->IMI()->RefTextVB();
	OrkAssert( false == VB.IsLocked() );
	VtxWriter<SVtxV12C4T16>& vw = GetRef().mTextWriter;
	new (&vw) VtxWriter<SVtxV12C4T16>;
	vw.miWriteCounter = 0;
	vw.miWriteBase = 0;
	int inuminvb = VB.GetNumVertices();
	int imaxinvb = VB.GetMax();
	//printf( "inuminvb<%d> imaxinvb<%d> inumv<%d>\n", inuminvb, imaxinvb, inumv );
	if( (inuminvb+inumv) >= imaxinvb )
	{
		VB.Reset();
		VB.SetNumVertices(0);
	}
	vw.Lock( pTARG, &pTARG->IMI()->RefTextVB(), inumv );
}

void CFontMan::EndTextBlock( GfxTarget *pTARG )
{
	CFontMan& fm = GetRef();
	OrkAssert( pTARG->IMI()->RefTextVB().IsLocked() );
	fm.mTextWriter.UnLock(pTARG);
	pTARG->BindMaterial( fm.mpCurrentFont->GetMaterial() );
	bool bdraw = fm.mTextWriter.miWriteCounter!=0;
	if( bdraw )
	{
		pTARG->GBI()->DrawPrimitive( fm.mTextWriter, ork::lev2::EPRIM_TRIANGLES );
	}
	pTARG->BindMaterial( 0 );
	
}

///////////////////////////////////////////////////////////////////////////////

CFontMan::CFontMan()
	: NoRttiSingleton< CFontMan >()
	, mpCurrentFont( 0 )
{
	InitFonts( ork::lev2::GfxEnv::GetRef().GetLoaderTarget() );
		
	for( int ich=0; ich<=255; ich++ )
	{	CharDesc& desc = mCharDescriptions[ich];
		desc.miRow = ich>>4;
		desc.miCol = ich%16;
		desc.ch = (char) ich;
	}

}
CFontMan::~CFontMan()
{
	for( auto item : GetRef().mFontVect ) delete item;
	GetRef().mFontMap.clear();
}

// Inconsolata font from http://www.levien.com/type/myfonts/inconsolata.html
// font textures built with F2IBuilder http://sourceforge.net/projects/f2ibuilder/
//  set texture size to 512
//  use metrics off
//  use a monospace font
//  show grid should be fine so long as the font stays away from the cell boundaries
//  a 'cell' is one of the 256(16x16) tiles in the texture, 
//   if the texture size is 512x512, then a single cell will be 32x32

void CFontMan::InitFonts( GfxTarget *pTARG )
{
	FontDesc Inconsolata12;
	Inconsolata12.mFontName = "i12";
	Inconsolata12.mFontFile = "lev2://textures/Inconsolata12";
	Inconsolata12.miTexWidth = 512;
	Inconsolata12.miTexHeight = 512;
	Inconsolata12.miCellWidth = (512/16);
	Inconsolata12.miCellHeight = (512/16);
	Inconsolata12.miCharWidth = 12;
	Inconsolata12.miCharHeight = 12;
	Inconsolata12.miCharOffsetX = 12;
	Inconsolata12.miCharOffsetY = 12;
	Inconsolata12.miAdvanceWidth = 6;
	Inconsolata12.miAdvanceHeight = 12;
	
	FontDesc Inconsolata13;
	Inconsolata13.mFontName = "i13";
	Inconsolata13.mFontFile = "lev2://textures/Inconsolata13";
	Inconsolata13.miTexWidth = 512;
	Inconsolata13.miTexHeight = 512;
	Inconsolata13.miCellWidth = (512/16);
	Inconsolata13.miCellHeight = (512/16);
	Inconsolata13.miCharWidth = 13;
	Inconsolata13.miCharHeight = 13;
	Inconsolata13.miCharOffsetX = 11;
	Inconsolata13.miCharOffsetY = 11;
	Inconsolata13.miAdvanceWidth = 7;
	Inconsolata13.miAdvanceHeight = 13;

	FontDesc Inconsolata14;
	Inconsolata14.mFontName = "i14";
	Inconsolata14.mFontFile = "lev2://textures/Inconsolata14";
	Inconsolata14.miTexWidth = 512;
	Inconsolata14.miTexHeight = 512;
	Inconsolata14.miCellWidth = (512/16);
	Inconsolata14.miCellHeight = (512/16);
	Inconsolata14.miCharWidth = 14;
	Inconsolata14.miCharHeight = 24;
	Inconsolata14.miCharOffsetX = 10;
	Inconsolata14.miCharOffsetY = 8;
	Inconsolata14.miYShift = 1;
	Inconsolata14.miAdvanceWidth = 7;
	Inconsolata14.miAdvanceHeight = 16;

	FontDesc Inconsolata16;
	Inconsolata16.mFontName = "i16";
	Inconsolata16.mFontFile = "lev2://textures/Inconsolata16";
	Inconsolata16.miTexWidth = 512;
	Inconsolata16.miTexHeight = 512;
	Inconsolata16.miCellWidth = (512/16);
	Inconsolata16.miCellHeight = (512/16);
	Inconsolata16.miCharWidth = 16;
	Inconsolata16.miCharHeight = 16;
	Inconsolata16.miCharOffsetX = 9;
	Inconsolata16.miCharOffsetY = 11;
	Inconsolata16.miYShift = 1;
	Inconsolata16.miAdvanceWidth = 9;
	Inconsolata16.miAdvanceHeight = 18;

	FontDesc Inconsolata24;
	Inconsolata24.mFontName = "i24";
	Inconsolata24.mFontFile = "lev2://textures/Inconsolata24";
	Inconsolata24.miTexWidth = 512;
	Inconsolata24.miTexHeight = 512;
	Inconsolata24.miCellWidth = (512/16);
	Inconsolata24.miCellHeight = (512/16);
	Inconsolata24.miCharWidth = 24;
	Inconsolata24.miCharHeight = 22;
	Inconsolata24.miCharOffsetX = 6;
	Inconsolata24.miCharOffsetY = 6;
	Inconsolata24.miYShift = 1;
	Inconsolata24.miAdvanceWidth = 11;
	Inconsolata24.miAdvanceHeight = 26;

	FontDesc Transponder24;
	Transponder24.mFontName = "d24";
	Transponder24.mFontFile = "lev2://textures/transponder24";
	Transponder24.miTexWidth = 512;
	Transponder24.miTexHeight = 512;
	Transponder24.miCellWidth = (512/16);
	Transponder24.miCellHeight = (512/16);
	Transponder24.miCharWidth = 24;
	Transponder24.miCharHeight = 24;
	Transponder24.miCharOffsetX = 6;
	Transponder24.miCharOffsetY = 6;
	Transponder24.miAdvanceWidth = 16;
	Transponder24.miAdvanceHeight = 24;

	AddFont( pTARG, Inconsolata12 );
	AddFont( pTARG, Inconsolata13 );
	AddFont( pTARG, Inconsolata14 );
	AddFont( pTARG, Inconsolata16 );
	AddFont( pTARG, Inconsolata24 );
	AddFont( pTARG, Transponder24 );
	
	PushFont("i14");
}

///////////////////////////////////////////////////////////////////////////////

void CFontMan::AddFont( GfxTarget *pTARG, const FontDesc& fdesc )
{
	CFont *pFontAlreadyLoaded = OrkSTXFindValFromKey( GetRef().mFontMap, fdesc.mFontName, (CFont *) 0 );

	if( 0 == pFontAlreadyLoaded )
	{
		CFont *pNewFont = new CFont( fdesc.mFontName, fdesc.mFontFile );

		pNewFont->LoadFromDisk( pTARG, fdesc );

		GetRef().mFontVect.push_back( pNewFont );
		OrkSTXMapInsert( GetRef().mFontMap, fdesc.mFontName, pNewFont );
		GetRef().mpCurrentFont = pNewFont;
	}
}

///////////////////////////////////////////////////////////////////////////////

void CFontMan::DrawText( GfxTarget *pTARG, int iX, int iY, const char *pFmt, ... )
{
	static char TextBuffer[256];
	va_list argp;
    va_start( argp, pFmt );
	vsnprintf( TextBuffer, sizeof(TextBuffer), pFmt, argp );
	va_end( argp );

	///////////////////////////////////

	CFont *pFont = GetRef().mpCurrentFont;
	size_t iLen = strlen( TextBuffer );
	const FontDesc& fdesc = pFont->GetFontDesc();
	
	int iSX = fdesc.miAdvanceWidth;
	int iSY = fdesc.miAdvanceHeight;
	int ishifty = fdesc.miYShift;
	
	SRect &VPRect = pTARG->FBI()->GetViewport();

	///////////////////////////////////
	int iRow = 0;
	int iCol = 0;

	for( size_t i=0; i<iLen; i++ )
	{
		char ch = TextBuffer[i];

		switch( ch )
		{	case 0x0a: // linefeed
			{	iRow++;
				iCol=0;
				break;
			}
			case 0x20: // space
			{	iCol++;
				break;
			}
			default:
			{
				int iCharRow = -1;
				int iCharCol = -1;

				const CharDesc& desc = GetRef().mCharDescriptions[int(ch)];

				if( desc.ch != 0 )
				{
					iCharRow = desc.miRow;
					iCharCol = desc.miCol;
				}

				if( (iCharRow>=0) && (iCharCol>=0) )
				{	int iX0 = iX+(iCol*iSX);
					int iY0 = iY+(iRow*iSY)+ishifty;
					//pFont->QueChar( pTARG, GetRef().mTextWriter, iX0, iY0, iCharCol, iCharRow, pTARG->RefModColor().GetABGRU32() );
					pFont->QueChar( pTARG, GetRef().mTextWriter, iX0, iY0, iCharCol, iCharRow, pTARG->RefModColor().GetBGRAU32() );
					iCol++;
				}

				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

CFont::CFont( const std::string & fontname, const std::string & filename )
	: msFileName( filename )
	, msFontName( fontname )
{
}

///////////////////////////////////////////////////////////////////////////////

inline void CFont::QueChar( GfxTarget *pTARG, VtxWriter<SVtxV12C4T16>& vw, int ix, int iy, int iu, int iv, U32 ucolor )
{	
	static const float kftexsiz = float(mFontDesc.miTexWidth);
	static const float kfU0offset = +0.0f;
	static const float kfV0offset = 0.0f;
	static const float kfU1offset = -0.0f;
	static const float kfV1offset = 0.0f;
	///////////////////////////////////////////////
	const float kfcharW = float(mFontDesc.miCharWidth);
	const float kfcharH = float(mFontDesc.miCharHeight);
	const float kfcellW = float(mFontDesc.miCellWidth);
	const float kfcellH = float(mFontDesc.miCellHeight);
	///////////////////////////////////////////////
	// calc vertex pos
	///////////////////////////////////////////////
	float fix1 = float(ix);
	float fiy1 = float(iy);
	///////////////////////////////////////////////
	// calc UVs
	///////////////////////////////////////////////
	float fiu = float(iu);
	float fiv = float(iv);
#if defined(_DARWIN) // REALLY GL
	float fcellbaseU = kfcellW*fiu;
	float fcellbaseV = kfcellH*fiv;
	float fuW = kfcharW;
	float fvH = kfcharH;
	const float fix2 = fix1+kfcharW;
	const float fiy2 = fiy1+kfcharH;
#else // DX
	const float khalftexoffsetX = 0.5f/float(mFontDesc.miTexWidth);
	const float khalftexoffsetY = 0.5f/float(mFontDesc.miTexHeight);
	float fcellbaseU = kfcellW*fiu;//-khalftexoffsetX;
	float fcellbaseV = kfcellH*fiv;//-khalftexoffsetY;
	float fuW = kfcharW;//+khalftexoffsetX;
	float fvH = kfcharH;//+khalftexoffsetY;
	fix1 -= 0.5f;
	fiy1 -= 0.5f;
	const float fix2 = fix1+kfcharW;
	const float fiy2 = fiy1+kfcharH;
#endif
	float fu1 = fcellbaseU+float(mFontDesc.miCharOffsetX);
	float fv1 = fcellbaseV+float(mFontDesc.miCharOffsetY);
	float fu2 = fu1+fuW;
	float fv2 = fv1+fvH;
	//
	float kitexs = 1.0f / kftexsiz;
	fu1 *= kitexs;
	fu2 *= kitexs;
	fv1 *= kitexs;
	fv2 *= kitexs;
	///////////////////////////////////////////////
	vw.AddVertex( TEXT_VTXFMT( fix1, fiy1,  0.0f, fu1, fv1, ucolor ) );
	vw.AddVertex( TEXT_VTXFMT( fix2, fiy1,  0.0f, fu2, fv1, ucolor ) );
	vw.AddVertex( TEXT_VTXFMT( fix2, fiy2, 0.0f, fu2, fv2, ucolor ) );
	vw.AddVertex( TEXT_VTXFMT( fix1, fiy1,  0.0f, fu1, fv1, ucolor ) );
	vw.AddVertex( TEXT_VTXFMT( fix2, fiy2, 0.0f, fu2, fv2, ucolor ) );
	vw.AddVertex( TEXT_VTXFMT( fix1,  fiy2, 0.0f, fu1, fv2, ucolor ) );
}

///////////////////////////////////////////////////////////////////////////////

void CFont::LoadFromDisk( GfxTarget* pTARG, const FontDesc& fdesc )
{	mpMaterial = new GfxMaterialUIText;
	mpMaterial->Init( pTARG );
	AssetPath apath( msFileName.c_str() );
	TextureAsset* ptexa = asset::AssetManager<ork::lev2::TextureAsset>::Load( apath.c_str() );
	auto ptex = ptexa->GetTexture();
	mpMaterial->SetTexture( ETEXDEST_DIFFUSE, ptex );
	ptex->TexSamplingMode().PresetPointAndClamp();
	pTARG->TXI()->ApplySamplingMode(ptex);
	mFontDesc = fdesc;
}

///////////////////////////////////////////////////////////////////////////////

} }


