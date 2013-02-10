////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_LEV2_GRID_H
#define _ORK_LEV2_GRID_H

#include <ork/gfx/camera.h>

namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

class Grid3d
{
public:

	enum EGrid
	{
		EGRID_XY = 0,
		EGRID_XZ,
		EGRID_END,
	};

	void Calc( const ork::CCameraData& camdat );
	void Render( RenderContextFrameData& FrameData ) const;

	void SetGridMode( EGrid egrid ) { meGridMode=egrid; }

	CReal				GetVisGridBase() const { return mVisGridBase; }
	CReal				GetVisGridDiv() const { return mVisGridDiv; }
	CReal				GetVisGridSize() const { return mVisGridSize; }

	Grid3d();

private:

	EGrid				meGridMode;
	CReal				mVisGridBase;
	CReal				mVisGridDiv;
	CReal				mVisGridHiliteDiv;
	CReal				mGridDL;
	CReal				mGridDR;
	CReal				mGridDB;
	CReal				mGridDT;
	CReal				mVisGridSize;

};

///////////////////////////////////////////////////////////////////////////////

class Grid2d
{
public:

	void Render( GfxTarget* pTARG );

	float				GetVisGridBase() const { return mVisGridBase; }
	float				GetVisGridDiv() const { return mVisGridDiv; }
	float				GetVisGridSize() const { return mVisGridSize; }

	Grid2d();

	CVector2			Snap( CVector2 inp ) const;

	float				GetZoom() const { return mZoom; }
	float				GetExtent() const { return mExtent; }
	const CVector2&		GetCenter() const { return mCenter; }

	void				SetZoom( float fz );
	void				SetExtent( float fz );
	void				SetCenter( const CVector2& ctr );
	
	const CMatrix4&		GetOrthoMatrix() const { return mMtxOrtho; }

	const CVector2&		GetTopLeft() const { return mTopLeft; }
	const CVector2&		GetBotRight() const { return mBotRight; }

private:

	void ReCalc(GfxTarget* pTARG);


	CMatrix4			mMtxOrtho;
	float				mVisGridBase;
	float				mVisGridDiv;
	float				mVisGridHiliteDiv;
	//float				mGridDL;
	//float				mGridDR;
	//float				mGridDB;
	//float				mGridDT;
	float				mZoom;
	float				mVisGridSize;
	float				mExtent;
	CVector2			mCenter;
	CVector2			mTopLeft;
	CVector2			mBotRight;

};

///////////////////////////////////////////////////////////////////////////////
}}

#endif
