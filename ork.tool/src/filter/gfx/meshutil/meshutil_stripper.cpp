////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include "meshutil_stripper.h"

#include <ork/kernel/csystem.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

TriStripper::TriStripper( const std::vector<unsigned int> &InTriIndices, int icachesize, int iminstripsize )
	: tristripper( InTriIndices )
{
	const bool StripEnable = true;

	int inumtriindices = int(InTriIndices.size());

	bool StripJoinPolicy = true;//CSystem::GetGlobalStringVariable( "StripJoinPolicy" ) == "true";

	int iminindex = 1<<30;
	int imaxindex = 0;
	int inumoutstripindices = 0;
	int inumouttriindices = 0;
	
	if( StripEnable )
	{
		//////////////////////////////////
		// Tristrip

		tristripper.SetCacheSize( 16 );		// GPU vertex cache size
		tristripper.SetMinStripSize( 2 );

		triangle_stripper::tri_stripper::primitives_vector StripperPrims;
		tristripper.Strip( & StripperPrims );

		//////////////////////////////////
		// PrimGroups

		size_t iNumPrimGroups = StripperPrims.size();

		//orkprintf( "<<tristrip>> NumPrimGroups %d\n", iNumPrimGroups );
		int inumidx = 0;
		int inumstrips = 0;
				
		for( size_t iprimgrp=0; iprimgrp<iNumPrimGroups; iprimgrp++ )
		{
			triangle_stripper::tri_stripper::primitives &prim = StripperPrims[iprimgrp];

			size_t inumindicesinprim = prim.m_Indices.size();

			///////////////////////////////////////////
			// find min and max indices
			///////////////////////////////////////////

			for( size_t idx=0; idx<inumindicesinprim; idx++ )
			{
				int index = prim.m_Indices[ idx ];
				iminindex = (index<iminindex) ? index : iminindex;
				imaxindex = (index>imaxindex) ? index : imaxindex;
			}

			switch( prim.m_Type )
			{
				/////////////////////////////////////////
				// triangles, => no stripping
				/////////////////////////////////////////
				case triangle_stripper::tri_stripper::PT_Triangles:
				{	
					for( size_t i=0; i<int(prim.m_Indices.size()); i++ )
					{
						int index = prim.m_Indices[ i ];
						mTriGroup.mIndices.push_back( index );
						inumouttriindices++;
						inumidx++;
					}
					//orkprintf( "<<tristrip>> PrimGroup[%d] Triangles NumIndices:%d\n",  iprimgrp, inumindicesinprim );
					break;
				}
				/////////////////////////////////////////
				// strips! 
				/////////////////////////////////////////
				case triangle_stripper::tri_stripper::PT_Triangle_Strip:
				{	
					if( StripJoinPolicy ) // merge to one strip with degenerates?
					{
						//////////////////////////////
						// add first strip group
						//////////////////////////////
						if( 0 == inumstrips )
						{
							mStripGroups.push_back( TriStripperPrimGroup() );
							for( size_t idx=0; idx<inumindicesinprim; idx++ )
							{
								int index = prim.m_Indices[ idx ];
								mStripGroups[0].mIndices.push_back( index );
								inumoutstripindices++;
								inumidx++;
							}
						}
						else // add degenerates
						{
							int icount = (int) mStripGroups[0].mIndices.size();
							int ilast = mStripGroups[0].mIndices[ icount-1 ];
							mStripGroups[0].mIndices.push_back( ilast );
							mStripGroups[0].mIndices.push_back( prim.m_Indices[ 0 ] );
							inumoutstripindices+=2;
							inumidx+=2;

							if( icount&1 )
							{
								mStripGroups[0].mIndices.push_back( prim.m_Indices[ 0 ] );
								inumoutstripindices++;
								inumidx++;
							}

							for( size_t idx=0; idx<inumindicesinprim; idx++ )
							{
								int index = prim.m_Indices[ idx ];
								mStripGroups[0].mIndices.push_back( index );
								inumoutstripindices++;
								inumidx++;
							}

						}
						inumstrips++;
					}
					else // not joining strips
					{
						TriStripperPrimGroup StripGroup;
						for( size_t idx=0; idx<inumindicesinprim; idx++ )
						{
							int index = prim.m_Indices[ idx ];
							StripGroup.mIndices.push_back( index );
							inumoutstripindices++;
							inumidx++;
						}
						mStripGroups.push_back( StripGroup );
					}
					//orkprintf( "<<tristrip>> PrimGroup[%d] TriStrip NumInpIdxs<%d> NumIndices:%d\n",  iprimgrp, inumtriindices, inumindicesinprim );
					break;
				}
				default:
					break;
			}
		}
		//orkprintf( "<<tristrip>> total NumInpIdxs<%d> NumOut:%d\n", inumtriindices, inumidx );

	}
	else
	{
		int inum = int( InTriIndices.size() );

		for( int i=0; i<inum; i++ )
		{
			unsigned int index = InTriIndices[i];
			mTriGroup.mIndices.push_back( index );
			inumouttriindices++;
		}

	}
	orkprintf( "<<TRISTRIP>> NumIndices In %d : OutStrip %d OutTri %d MinIndex<%d> MaxIndex<%d>\n", InTriIndices.size(), inumoutstripindices, inumouttriindices, iminindex, imaxindex );
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
