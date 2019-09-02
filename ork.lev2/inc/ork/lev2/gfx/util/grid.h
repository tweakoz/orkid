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

	void Calc( const ork::CameraData& camdat );
	void Render( RenderContextFrameData& FrameData ) const;

	void SetGridMode( EGrid egrid ) { meGridMode=egrid; }

	float				GetVisGridBase() const { return mVisGridBase; }
	float				GetVisGridDiv() const { return mVisGridDiv; }
	float				GetVisGridSize() const { return mVisGridSize; }

	Grid3d();

private:

	EGrid				meGridMode;
	float				mVisGridBase;
	float				mVisGridDiv;
	float				mVisGridHiliteDiv;
	float				mGridDL;
	float				mGridDR;
	float				mGridDB;
	float				mGridDT;
	float				mVisGridSize;

};

///////////////////////////////////////////////////////////////////////////////

class Grid2d
{
public:

	void Render( GfxTarget* pTARG, int iw, int ih );

	float				GetVisGridBase() const { return mVisGridBase; }
	float				GetVisGridDiv() const { return mVisGridDiv; }
	float				GetVisGridSize() const { return mVisGridSize; }

	Grid2d();

	fvec2			Snap( fvec2 inp ) const;

	float				GetZoom() const { return mZoom; }
	float				GetExtent() const { return mExtent; }
	const fvec2&		GetCenter() const { return mCenter; }

	void				SetZoom( float fz );
	void				SetExtent( float fz );
	void				SetCenter( const fvec2& ctr );
	
	const fmtx4&		GetOrthoMatrix() const { return mMtxOrtho; }

	const fvec2&		GetTopLeft() const { return mTopLeft; }
	const fvec2&		GetBotRight() const { return mBotRight; }

private:

	void ReCalc(int iw, int ih);


	fmtx4			mMtxOrtho;
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
	fvec2			mCenter;
	fvec2			mTopLeft;
	fvec2			mBotRight;

};

///////////////////////////////////////////////////////////////////////////////
}}

#endif
