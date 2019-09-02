////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//#include "pch.h"
#include "ork/pch.h"

//#include <ork/gfx/gfxglyphs.h>
#include <ork/lev2/gfx/gfxenv.h>

using ork::lev2::Texture;

namespace ork {

/*void PlaceGlyph( ork::lev2::Renderer *pRenderer, ork::lev2::CGlyphsRenderable& renderable, const CFontGlyph &glyph, float scaleFactor, CVector2 pos, float z, U32 color, float invW, float invH)
{
	typedef CVector4 CRect; // (LEFT,TOP,RIGHT,BOTTOM)
	static float eggtestr = 0.5f;
	static float eggtestl = 0.5f;
	static float eggtestt = 0.5f;
	static float eggtestb = 0.5f;
	const CRect &glyphTexRect = glyph.GetAssetInfo();

	ork::lev2::GfxTarget* pTARG = pRenderer->GetTarget();

	float invScreenWidth = 1.0f;  
	float invScreenHeight = 1.0f;  
	
	float boundAmount(0.0f);
	float left = (glyphTexRect[0] - boundAmount + eggtestl) * invW;
	float right = ((glyphTexRect[0] + glyphTexRect[2] + boundAmount + eggtestr) * invW);
	float top = (glyphTexRect[1] + eggtestt ) * invH;
	float bottom = (glyphTexRect[1] + glyphTexRect[3] + eggtestb ) * invH;

	CVector2 uv1(left, top);
	CVector2 uv2(left, bottom);
	CVector2 uv3(right, bottom);
	CVector2 uv4(right, top);


	float posX1((pos[0])*invScreenWidth),  posX2((pos[0] + (scaleFactor * glyphTexRect[2]))*invScreenWidth);
	float posY1((pos[1] - (scaleFactor * glyphTexRect[3]))*invScreenHeight), posY2((pos[1])*invScreenHeight);

	// same order

	CVector3 pos1(posX1, posY1, z);
	CVector3 pos2(posX1, posY2, z);
	CVector3 pos3(posX2, posY2, z);
	CVector3 pos4(posX2, posY1, z);

	CColor3 col;
	col.SetARGBU32(color);

	ork::lev2::SVtxV12C4T16 v0(pos1, uv1, color);
	ork::lev2::SVtxV12C4T16 v1(pos2, uv2, color);
	ork::lev2::SVtxV12C4T16 v2(pos3, uv3, color);
	ork::lev2::SVtxV12C4T16 v3(pos4, uv4, color);	
		 
	renderable.AddGlyph(v0,v1,v2,v3);
}*/

}
