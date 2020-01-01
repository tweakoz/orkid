////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/orkconfig.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ledfont.h>

///NOT compiling on the WII --> LED FONT
///NOT compiling on the WII --> LED FONT
#ifndef WII



using namespace ork;

///////////////////////////////////////////////////////////////////////////////

#define SEG_0   0x0001
#define SEG_1   0x0002
#define SEG_2   0x0004
#define SEG_3   0x0008
#define SEG_4   0x0010
#define SEG_5   0x0020
#define SEG_6   0x0040
#define SEG_7   0x0080
#define SEG_8   0x0100
#define SEG_9   0x0200
#define SEG_10	0x0400
#define SEG_11	0x0800

///////////////////////////////////////////////////////////////////////////////

#ifdef PS2
int CDebugFont::msiNumCols = 60;
int CDebugFont::msiNumRows = 32;
#else
int CDebugFont::msiNumCols = 100;
int CDebugFont::msiNumRows = 60;
#endif

bool CDebugFont::msbBottomUp = false;
F32 CDebugFont::msfXS = 0.65f;
F32 CDebugFont::msfYS = 0.65f;

F32 CDebugFont::msfR = 1.0f;
F32 CDebugFont::msfG = 1.0f;
F32 CDebugFont::msfB = 1.0f;
F32 CDebugFont::msfA = 0.0f;

/*

                 555555555
              0	     11     4
              0	 7   11  9  4
              0	  7  11 9   4
              0	     11     4
                 666666666
              1	     11     3
              1	  8  11 10  3
              1	 8   11  10 3
              1	     11     3
                 222222222

*/

U16 CDebugFont::msuaLedDigits[] =
{
	// CODES 0..9

    SEG_0|SEG_1|SEG_2|SEG_3|SEG_4|SEG_5|SEG_8|SEG_9,		// 0
    SEG_11,													// 1
    SEG_1|SEG_2|SEG_4|SEG_5|SEG_6,							// 2
    SEG_2|SEG_3|SEG_4|SEG_5|SEG_6,							// 3
    SEG_0|SEG_3|SEG_4|SEG_6,								// 4
    SEG_0|SEG_2|SEG_3|SEG_5|SEG_6,							// 5
    SEG_0|SEG_1|SEG_2|SEG_3|SEG_5|SEG_6,					// 6
    SEG_3|SEG_4|SEG_5,										// 7
    SEG_0|SEG_1|SEG_2|SEG_3|SEG_4|SEG_5|SEG_6,				// 8
    SEG_0|SEG_2|SEG_3|SEG_4|SEG_5|SEG_6,					// 9

	// CODES 10..35
    SEG_0|SEG_1|SEG_3|SEG_4|SEG_5|SEG_6,					// A
    SEG_0|SEG_1|SEG_2|SEG_3|SEG_6,							// b
    SEG_0|SEG_1|SEG_2|SEG_5,								// C
    SEG_1|SEG_2|SEG_3|SEG_4|SEG_6,							// d
    SEG_0|SEG_1|SEG_2|SEG_5|SEG_6,							// E
    SEG_0|SEG_1|SEG_5|SEG_6,								// F
	SEG_0|SEG_1|SEG_2|SEG_3|SEG_5,							// G
	SEG_0|SEG_1|SEG_3|SEG_4|SEG_6,							// H
	SEG_11|SEG_5|SEG_2,										// I
	SEG_4|SEG_3|SEG_2,										// J
	SEG_0|SEG_1|SEG_8|SEG_10,								// K
	SEG_0|SEG_1|SEG_2,										// L
	SEG_0|SEG_1|SEG_4|SEG_3|SEG_7|SEG_8,					// M
	SEG_0|SEG_1|SEG_4|SEG_3|SEG_7|SEG_10,					// N
	SEG_0|SEG_1|SEG_2|SEG_3|SEG_4|SEG_5,					// O
	SEG_0|SEG_1|SEG_5|SEG_6|SEG_4,							// P
	SEG_0|SEG_1|SEG_2|SEG_3|SEG_4|SEG_5|SEG_10,				// Q
	SEG_0|SEG_1|SEG_8|SEG_10|SEG_5,							// R
	SEG_2|SEG_5|SEG_7|SEG_10,								// S
	SEG_11|SEG_5,											// T
    SEG_0|SEG_1|SEG_2|SEG_3|SEG_4,							// U
	SEG_0|SEG_1|SEG_9|SEG_8,								// V
	SEG_0|SEG_1|SEG_4|SEG_3|SEG_9|SEG_10,					// W
	SEG_7|SEG_9|SEG_8|SEG_10,								// X
	SEG_7|SEG_8|SEG_9,										// Y
	SEG_5|SEG_2|SEG_8|SEG_9,								// Z

};

///////////////////////////////////////////////////////////////////////////////

void CDebugFont::DrawCharacter( int iCol, int iRow, char cChar )
{
	///////////////////////
	// calc positions
	//F32 fCharSizeRow = 1.0f/(F32)msiNumRows;
	//F32 fCharSizeCol = 1.0f/(F32)msiNumCols;

	int iW1 = 3;
	int iW2 = 7;
	int iWS = 9;
	int iH1 = 3;
	int iH2 = 7;
	int iHS = 9;

	int topy = iRow*iHS;
	int midy = topy+iH1;
	int boty = topy+iH2;
	int lftx = iCol*iWS;
	int midx = lftx+iW1;
	int rgtx = lftx+iW2;

	///////////////////////
	// which LED segments are on?
	int iDigit = 0;
	if( (cChar>='0') && (cChar<='9') )			iDigit = (int) (cChar - '0');
	else if( (cChar>='a') && (cChar<='z') )		iDigit = 10 + (int) (cChar - 'a');
	else if( (cChar>='A') && (cChar<='Z') )		iDigit = 10 + (int) (cChar - 'A');
	else return;

	U16 led = msuaLedDigits[iDigit];

	///////////////////////
	// Draw

	//GfxHWContext *pCTX = GfxEnv::GetCurrentContext();

	//if (led & SEG_0)	gGfxEnv.DrawLine( lftx, topy,	lftx, midy );
	//if (led & SEG_1)	gGfxEnv.DrawLine( lftx, midy,	lftx, boty );
	//if (led & SEG_2)   	gGfxEnv.DrawLine( lftx, boty,	rgtx, boty );
    //if (led & SEG_3)   	gGfxEnv.DrawLine( rgtx, midy,	rgtx, boty );
    //if (led & SEG_4)   	gGfxEnv.DrawLine( rgtx, topy,	rgtx, midy );
    //if (led & SEG_5)   	gGfxEnv.DrawLine( lftx, topy,	rgtx, topy );
    //if (led & SEG_6)   	gGfxEnv.DrawLine( lftx, midy,	rgtx, midy );
    //if (led & SEG_7)   	gGfxEnv.DrawLine( lftx, topy,	midx, midy );
    //if (led & SEG_8)   	gGfxEnv.DrawLine( midx, midy,	rgtx, topy );
    //if (led & SEG_9)   	gGfxEnv.DrawLine( lftx, boty,	midx, midy );
    //if (led & SEG_10)	gGfxEnv.DrawLine( midx, midy,	rgtx, boty );
    //if (led & SEG_11)  	gGfxEnv.DrawLine( midx, topy,	midx, boty );	// |

}

///////////////////////////////////////////////////////////////////////////////
void CDebugFont::DrawText( int iCol, int iRow, char *formatstring, ... )
{	static char textbuffer[128];
	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &textbuffer[0],128, formatstring, argp );
	va_end( argp );

	int iLen = (int) strlen( textbuffer );

	for( int iIDX=0; iIDX<iLen; iIDX++ )
	{
		DrawCharacter( iCol+iIDX, iRow, textbuffer[ iIDX ] );
	}
}


#endif
