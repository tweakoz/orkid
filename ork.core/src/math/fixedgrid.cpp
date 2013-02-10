///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2009, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <cmath>
#include <ork/orkconfig.h>
#include <ork/orktypes.h>
#include <ork/math/cvector2.h>
#include <ork/math/cfloat.h>
#include <ork/math/octree.h>
#include <ork/math/plane.h>
#include <ork/math/sphere.h>
#include <ork/math/collision_test.h>
#include <ork/math/raytracer.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <queue>
//#include <IL/il.h>

//#pragma comment( lib, "devil.lib" )

extern s64 giNumRays;

using namespace ork;

///////////////////////////////////////////////////////////////////////////////

FixedGrid::FixedGrid()
	: mpCells(0)
{
	mpCells = new ObjectList*[GRIDSIZE * GRIDSIZE * GRIDSIZE];
	memset( mpCells, 0, GRIDSIZE * GRIDSIZE * GRIDSIZE * sizeof(ObjectList*) );
}

FixedGrid::~FixedGrid()
{
	DestroyGrid();
}

void FixedGrid::DestroyGrid()
{
	for( int ix=0; ix<GRIDSIZE; ix++ )
	{
		for( int iy=0; iy<GRIDSIZE; iy++ )
		{
			for( int iz=0; iz<GRIDSIZE; iz++ )
			{
				int idx = CalcAddress(ix,iy,iz);
				ObjectList* plist = mpCells[idx];
				if( plist )
				{
					delete plist;
					mpCells[idx] = 0;
				}
			}
		}
	}

	delete[] mpCells;

}
void FixedGrid::BuildGrid( const AABox& extends, const orkvector<const Primitive*>& prims )
{
	m_SR[0] = GRIDSIZE / extends.GetSize().GetX();
	m_SR[1] = GRIDSIZE / extends.GetSize().GetY();
	m_SR[2] = GRIDSIZE / extends.GetSize().GetZ();
	// precalculate size of a cell (for x, y, and z)
	m_CW = extends.GetSize() * (1.0f / GRIDSIZE);

	// initialize regular grid
	const CVector3& p1 = extends.Min();
	const CVector3& p2 = extends.Max();
	// calculate cell width, height and depth
	const float dx = (p2.GetX() - p1.GetX()) / GRIDSIZE;
	const float dx_reci = 1.0f / dx;
	const float dy = (p2.GetY() - p1.GetY()) / GRIDSIZE;
	const float dy_reci = 1.0f / dy;
	const float dz = (p2.GetZ() - p1.GetZ()) / GRIDSIZE;
	const float dz_reci = 1.0f / dz;
	mExtends = extends; //AABox( p1, p2 - p1 );
	//m_Light = new Primitive*[MAXLIGHTS];
	//m_Lights = 0;
	// store primitives in the grid cells

	int inump = prims.size();

	int inumpc = 0;

	int p;
	#pragma omp parallel private(p)
	#pragma omp for schedule(dynamic,16384)
	for ( p = 0; p < inump; p++ )
	{
		if( p%1024 == 0 ) orkprintf( "p<%d>\n", p );

		const Primitive* prim = prims[p];

		//if (m_Primitive[p]->IsLight()) m_Light[m_Lights++] = m_Primitive[p];
		const AABox& bound = prim->GetAABox();
		const CVector3& bv1 = bound.Min();
		const CVector3& bv2 = bound.Max();
		// find out which cells could contain the primitive (based on aabb)
		int x1 = (int)((bv1.GetX() - p1.GetX()) * dx_reci);
		int x2 = (int)((bv2.GetX() - p1.GetX()) * dx_reci) + 1;
		x1 = (x1 < 0)?0:x1;
		x2 = (x2 > (GRIDSIZE - 1))?GRIDSIZE - 1:x2;
		int y1 = (int)((bv1.GetY() - p1.GetY()) * dy_reci);
		int y2 = (int)((bv2.GetY() - p1.GetY()) * dy_reci) + 1;
		y1 = (y1 < 0)?0:y1;
		y2 = (y2 > (GRIDSIZE - 1))?GRIDSIZE - 1:y2;
		int z1 = (int)((bv1.GetZ() - p1.GetZ()) * dz_reci);
		int z2 = (int)((bv2.GetZ() - p1.GetZ()) * dz_reci) + 1;
		z1 = (z1 < 0)?0:z1;
		z2 = (z2 > (GRIDSIZE - 1))?GRIDSIZE - 1:z2;
		// loop over candidate cells
		
		for ( int x = x1; x < x2; x++ )
		{
			for ( int y = y1; y < y2; y++ )
			{
				for ( int z = z1; z < z2; z++ )
				{
					// construct aabb for current cell
					int idx = CalcAddress(x,y,z);
					CVector3 pos( p1.GetX() + x * dx, p1.GetY() + y * dy, p1.GetZ() + z * dz );
					AABox cell( pos, pos+CVector3( dx, dy, dz ) );
					// do an accurate aabb / primitive intersection test
					if (prim->IntersectBox( cell ))
					{
						inumpc++;

						// object intersects cell; add to object list
						ObjectList* l = new ObjectList();
						l->SetPrimitive( prim );

						l->SetNext( mpCells[idx] );

						ObjectList* prev = mpCells[idx];

						if( prev ) prev->Lock();
						{
							mpCells[idx] = l;
						}
						if( prev ) prev->UnLock();
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

