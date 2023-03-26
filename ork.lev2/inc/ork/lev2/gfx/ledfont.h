////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

namespace ork
{

class CDebugFont
{
	private:
	
	static U16 msuaLedDigits[36];
	static F32 msfR, msfG, msfB, msfA;
	static F32 msfXS, msfYS;
	static int msiNumRows, msiNumCols;
	static bool msbBottomUp;

	public:

	static void SetColor( F32 fR, F32 fG, F32 fB, F32 fA=0.0f )
	{	msfR = fR;	msfG = fG;	msfB = fB;	msfA = fA;	
	}
	static inline void SetView( int iNumRows, int iNumCols, F32 fXS, F32 fYS, bool bBottomUp )
	{	msiNumRows = iNumRows;	msiNumCols = iNumCols; msbBottomUp = bBottomUp;	msfXS = fXS; msfYS = fYS;
	}
	static void DrawCharacter( int iRow, int iCol, char cChar );
	static void DrawText( int iRow, int iCol, char *formatstring, ... );

};

}
