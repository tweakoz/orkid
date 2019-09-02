///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2009, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#ifndef _ORK_MATH_OCTREE_H
#define _ORK_MATH_OCTREE_H

#include <ork/kernel/any.h>
#include <ork/math/line.h>
#include <ork/math/box.h>
#include <ork/math/raytracer.h>
#include <queue>

extern s64 giNumRays;

namespace ork {

class FixedGrid
{
	static const int GRIDSIZE = 256;
	static const int SHIFTY = 8;
	static const int SHIFTZ = 16;
	
	inline int CalcAddress(int ix, int iy, int iz) const
	{
		if( (ix<0) | (iy<0) | (iz<0) ) return -1;
		if( (ix>=GRIDSIZE) | (iy>=GRIDSIZE) | (iz>=GRIDSIZE) ) return -1;
		int iaddr = ((iz<<SHIFTZ)+(iy<<SHIFTY)+ix);
		return iaddr;
	}

public:
    FixedGrid();
    ~FixedGrid();
	

	AABox			mExtends;
	ObjectList**	mpCells;
	fvec3		m_SR;
	fvec3		m_CW;

	void DestroyGrid();
	void BuildGrid(const AABox& extends, const orkvector<const Primitive*>& prims);
	//bool FastCall FindNearest( const fray3& a_Ray, float& a_Dist, fvec3& isect, const Primitive*& a_Prim ) const;
	/*inline int FastCall CalcAddress( int ix, int iy, int iz ) const
	{
		OrkAssert( ix<GRIDSIZE );
		OrkAssert( iy<GRIDSIZE );
		OrkAssert( iz<GRIDSIZE );
		int iaddress = (iz<<SHIFTZ)+(iy<<SHIFTY)+ix;
		//x + y * GRIDSIZE + z * GRIDSIZE * GRIDSIZE;
		return iaddress;
	}*/

	inline bool CheckCell( const fray3& InpRay, const ObjectList* plistbase, float& OutDistance, const Primitive*& OutPrim ) const
	{
		bool bretval = false;
		const ObjectList* list = plistbase;
		float TempDistance = OutDistance;
		while (list)
		{	const Primitive* pr = list->GetPrimitive();
			//if (pr->GetLastRayID() != a_Ray.GetID())
			{
				fvec3 tisect;
                int result = pr->Intersect( InpRay, tisect, TempDistance );
				if( result ) 
				{	
					if( TempDistance<OutDistance && TempDistance>0.0f )
					{
						//isect = tisect;
						OutDistance = TempDistance;
						//flowd = a_Dist;
						OutPrim = pr;
						bretval = result;
					}
				}
				list = list->GetNext();
			}
		}
		return bretval;
	}

	inline bool FindNearest( const fray3& InpRay, const fvec3& the_start, const fvec3& the_end, fvec3& isect, float& OutDistance, const Primitive*& OutPrim ) const
	{
		struct Args
		{
			fvec3 start;
			fvec3 end;
		};

		std::queue<Args> args;

		Args a;
		a.start = the_start;
		a.end = the_end;
		args.push(a);

		const AABox& e = mExtends;

		while( args.empty() == false )
		{
			Args a = args.front();
			args.pop();
			const fvec3& start = a.start;
			const fvec3& end = a.end;

			fvec3 cell1 = (start - e.Min()) * m_SR;
			fvec3 cell2 = (end - e.Min()) * m_SR;
			int X1 = (int)cell1.GetX();
			int Y1 = (int)cell1.GetY();
			int Z1 = (int)cell1.GetZ();
			int X2 = (int)cell2.GetX();
			int Y2 = (int)cell2.GetY();
			int Z2 = (int)cell2.GetZ();

			bool ONEVALID = (CalcAddress(X1,Y1,Z1)>=0);
			bool TWOVALID = (CalcAddress(X2,Y2,Z2)>=0);

			//////////////////////////////////////////////////////

			if( (X1!=X2) || (Y1!=Y2) || (Z1!=Z2) )
			{
				fvec3 center = (start+end)*0.5f;
				fvec3 cellC = (center - e.Min()) * m_SR;
				int XC = (int)cellC.GetX();
				int YC = (int)cellC.GetY();
				int ZC = (int)cellC.GetZ();
				bool CVALID = (CalcAddress(XC,YC,ZC)>=0);

				bool br1 = false;
				bool br2 = false;

				Args b, c;
				b.start = start;
				b.end = center;
				c.start = center;
				c.end = end;

				args.push(b);
				args.push(c);
			}
			else // leaf
			{
				int idx = CalcAddress(X1,Y1,Z1);
				if( idx>=0 )
				{
					const ObjectList* plistbase = mpCells[ idx ];
					//float CheckDistance = OutDistance;
					bool brval = CheckCell( InpRay, plistbase, OutDistance, OutPrim );
					return brval;
				}
				else
				{
					return false;
				}
			}
		}
		return false;
	}

	inline bool FindNearest( const fray3& a_Ray, float& a_Dist, fvec3& isect, const Primitive*& a_Prim ) const
	{
		giNumRays++;

		float fdisttmp;
		bool retval = false;
		const AABox& e = mExtends;
		const fvec3& start = a_Ray.mOrigin; // + a_Ray.mDirection* fbias;
		const fvec3& end = a_Ray.mOrigin + a_Ray.mDirection*a_Dist; // + a_Ray.mDirection* fbias;
		const fvec3& raydir = a_Ray.mDirection;

		fvec3 isect_in, isect_out;

		bool bisect = e.Intersect( a_Ray, isect_in, isect_out );
		
		return bisect ? FindNearest( a_Ray, isect_in, isect_out, isect, a_Dist, a_Prim ) : false;
		

	}
};

}
///////////////////////////////////////////////////////////////////////////////

#endif
