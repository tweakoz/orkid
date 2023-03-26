////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace ui {

struct Coordinate
{	
	Coordinate()
		: mfRawX( 0.0f )
		, mfRawY( 0.0f )
		, mfUnitX( 0.0f )
		, mfUnitY( 0.0f )
	{
	}

	float GetRawX( void ) const { return mfRawX; }
	float GetRawY( void ) const { return mfRawY; }
	float GetUnitX( void ) const { return mfUnitX; }
	float GetUnitY( void ) const { return mfUnitY; }

	void SetRawX( float u ) { mfRawX=u; }
	void SetRawY( float u ) { mfRawY=u; }
	void SetUnitX( float u ) { mfUnitX=u; }
	void SetUnitY( float u ) { mfUnitY=u; }

private:

	float mfRawX;
	float mfRawY;
	float mfUnitX;
	float mfUnitY;
	
};

}} // namespace ork/ui