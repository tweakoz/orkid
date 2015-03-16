////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// Synthetic Fluvial Erosion
//	based on "erode" code from John P. Beale 6/10/95-7/7/95
//
///////////////////////////////////////////////////////////////////////////////
//
//  notes from original erode:
//   
//	1. the algorithm may "bomb" on perfectly flat surfaces
//
//	2  sometimes there's some kind of instability where there is suddently
//		a spike pushing up out of the surface like a new volcano or something; this
//		seems to happen at the higher erosion or smoothing rates. It might not
//		appear with the default values. Then again it might.
//
///////////////////////////////////////////////////////////////////////////////
#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>
#include <ork/dataflow/scheduler.h>
///////////////////////////////////////////////////////////////////////////////
#include "terrain_synth.h"
#include "terrain_erosion.h"
#if 0
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
//////////////////////////////////////////////
static const int gXOffsets[9] =	/* 8-dir index offset arrays */
{
	0,-1,0,
	1,1,1,
	0,-1,-1
};
static const int gYOffsets[9] =
{
	0,-1,-1,
	-1,0,1,
	1,1,0
};
///////////////////////////////////////////////////////////////////////////////
template <typename T> T pow( T x, T y ) {	return std::powf(x,y); }
template <typename T> T min( T a, T b ) { return (a<b) ? a : b; }
template <typename T> T max( T a, T b ) { return (a>b) ? a : b; }
template <typename T> T clamp( T tin, T tmin, T tmax ) { return OrkSTXClampToRange( tin, tmin, tmax ); }
float P( float xprob ) { return ( ((float)rand()/RAND_MAX) < xprob);	}
//////////////////////////////////////////////
static float sgn(float a)
{	if (a > 0)		return (1);
	else if (a < 0)	return (-1);
	else			return (0);
}
///////////////////////////////////////////////////////////////////////////////
ErosionContext::ErosionContext()
	: xsize( 0 )
	, ysize( 0 )
{
}
///////////////////////////////////////////////////////////////////////////////
float ErosionContext::normalize()
{	float umax = -CFloat::TypeMax();
	float umin = CFloat::TypeMax();
	for( int iy=0; iy<ysize; iy++)
	{	for( int ix=0; ix<xsize; ix++)
		{	float uval = mHeightMap.Read(ix,iy);
			umax = max( umax, uval );
			umin = min( umin, uval );
		}
	}
	if ((umax-umin) == 0)
	{	orkprintf("Fatal error: input matrix is constant.\n");
		OrkAssert(false);
	}
	float scale = 1.0f/(umax-umin);
	orkprintf( "////////////////////////////////////////////\n" );
	orkprintf("Erosion Input max %1.3f  min %1.3f scale %1.3f\n",umax,umin,scale);
	orkprintf( "////////////////////////////////////////////\n" );
	for( int iy = 0; iy<ysize; iy++)
	{	for( int ix = 0; ix<xsize; ix++ )
		{	mHeightMap.Write(ix,iy) = scale * (mHeightMap.Read(ix,iy)-umin);
		}
	}
	return scale;
}
//////////////////////////////////////////////
float ErosionContext::ErosionFactor( float slope_exponent, float flow_exponent ) const
{	return	  sgn(slope_exponent) 
			* pow<float>(fabs(atan(slope_exponent)),float(1.4))
			* pow<float>((flow_exponent),float(0.8));
}
///////////////////////////////////////////////////////////////////////////////
void ErosionContext::Init( int isize, const float* psource )
{	xsize = isize;
	ysize = isize;
	//mhftmp_u8.Resize(isize);
	//mFlowDirMap.Resize(isize);
	//mPeakFlagMap.Resize(isize);
	mHeightMap.Resize(isize);
	//mhftmp_float.Resize(isize);
	mUphillAreaMap.Resize(isize);
	mBasinAccumMap.Resize(isize);
	for( int iz=0; iz<isize; iz++ )
	{	for( int ix=0; ix<isize; ix++ )
		{	int isrcaddr = (iz*isize) + ix;
			float fin = psource[ isrcaddr ];
			mHeightMap.Write(ix,iz) = fin;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
// erode_1()  --  erode one step, taking input hf1[] into output hf2[]
///////////////////////////////////////////////////////////////////////////////
void erode_1(	const ErosionContext& ec,
				const Map2D<float>& hfin,
				const Map2D<float>& hfuphillarea,
				const Map2D<u8>& hfflowdir,
				const Map2D<u8>& hfpeakflag,
				Map2D<float>& hfbasin,
				Map2D<float>& hfout
				)
{	const float inv_sq2(1.0f/1.414f);
	int upd;				// direction of uphill neighbor 
	float outslope;			// product of slope and flow_rate 
	float outflow;			// flow out of this cell 
	float erval;			// erosion potential at this point 
	float slopefactor;		// correction factor to scale slope 
	float dh, slope;		// dh- delta height  dr- horizontal travel 
	slopefactor = ec.mfTerrainHeight / (ec.mfTerrainSize / ec.xsize);  // slope of 0.0 next to 1.0 
	//////////////////////////////////////////////////////////
	for( int iy = 0; iy<ec.ysize; iy++) 
	{	for( int ix = 0; ix<ec.xsize; ix++)
		{	float h = hfin.Read(ix,iy);	// altitude at this point ix,iy
			float output_h = h;
			float output_basin = hfbasin.Read(ix,iy);
			const int input_dir = hfflowdir.Read(ix,iy); // direction of outflow 
			if (h < er_thresh)
			{	//////////////////////////////////////////
				// find sum of inflowing neighbors (slope*flow)
				//////////////////////////////////////////
				float inslope = 0; // product of slope and flow_rate 
				//////////////////////////////////////////
				for( int i=1;i<9;i++) // loop over neighbors of ix,iy
				{	int x1 = ix+gXOffsets[i]; 
					int y1 = iy+gYOffsets[i];
					//  direction index of this neighbor
					int dir = hfflowdir.ReadClamped(x1,y1);
					if ((gXOffsets[i]+gXOffsets[dir]==0) && (gYOffsets[i]+gYOffsets[dir]==0))
					{	/* inflow from here */
						slope = hfin.ReadClamped(x1,y1) - output_h;	// pos = neigh. higher
						slope *= slopefactor;						// scale to corr. units
						if( i&1 ) // diagonal neighbor; reduce slope
						{	slope *= inv_sq2;      
						}
						float fa( hfuphillarea.ReadClamped(x1,y1) );
						inslope += ec.ErosionFactor(slope,fa);
					}
				} 
				//////////////////////////////////////////
				// now we've got sum of (slope*flow)
				//////////////////////////////////////////
				if (input_dir > 0) // slope positive: neighbor lower
				{	// if not a minimum
					//h = hfin.Read(ix,iy);      // current elevation at this point
					h = min<float>( h, float(1.0) );
					slope = h - hfin.ReadClamped(ix+gXOffsets[input_dir],iy+gYOffsets[input_dir]); 
					slope *= slopefactor;
					if( input_dir&1 ) // diagonal neighbor; reduce slope
					{	slope *= inv_sq2;	
					}
					outflow = (float) hfuphillarea.Read( ix,iy );
					if (outflow<1) outflow = float(0.9);
					outslope = ec.ErosionFactor(slope,outflow);
					///////////////////////////////////////////////
					// positive dh = erosion happens, neg = sedimentation 
					// sedimentation when (inslope) greater than (er_sedfac*outslope)
					///////////////////////////////////////////////
					erval = ec.mErosionRate * (er_sedfac*outslope - inslope); 
					erval *= pow(outflow,er_pow);
					dh = ec.mErosionRate * erval; 
					h = output_h;										// old altitude at this point
					if (dh > ec.mErosionRate) dh = ec.mErosionRate;		// don't erode too fast
					if  ( (h-dh) < 0 )									// don't let altitudes go negative
					{	dh = -h;										// min elevation will be == 0.0
					}
					/////////////////////////////////
					// don't sediment too fast..
					// this should really depend on neighboring elevations
					/////////////////////////////////
					if (dh < -(float(0.03)*ec.mErosionRate))
					{	dh = -(float(0.03)*ec.mErosionRate); 
					}
					//////////////////////////////////////////////////////////
					// the actual erosion. right here!
					//////////////////////////////////////////////////////////
					output_h = float(h - dh);
					output_basin -= float(dh);
					// accumulate erosion rate separately
				}	//////////////////////////////////////////////////////////
				else // if point is a local minimum (oops!)
				{	upd = hfpeakflag.Read(ix,iy) & 0xf; // direction of uphill neighbor
					output_h = hfin.ReadClamped(ix+gXOffsets[upd],iy+gYOffsets[upd])+0.00001f;
				} 
			}
			//////////////////////////////////////
			hfout.Write(ix,iy) = output_h; 
			hfbasin.Write(ix,iy) = output_basin;
		}
	}
} 
///////////////////////////////////////////////////////////////////////////////
// correct_1()  --      Input hf1, output hf2.  Checks that every point  
//                      in hf1 has an output flow (from mFlowDirVect[]) that is    
//                      actually downhill, and if it isn't, makes it so. 
//                      Does not set the border pixels in hf2[].         
///////////////////////////////////////////////////////////////////////////////
static int correct_1(	const ErosionContext& ec,
						const Map2D<float>& hf1,
						const Map2D<u8>& hfdflow,
						Map2D<float>& hf2,
						Map2D<float>& hfbasin
						)

{	int fixed = 0; // number of pixels with altitude adjusted
	for( int iy = 0; iy < ec.ysize; iy++)
	{	for( int ix = 0; ix < ec.xsize; ix++)
		{	float h = hf1.Read(ix,iy);													// elevation at this point
			int dir = hfdflow.Read(ix,iy); 												// flow direction
			float hdown = hf1.ReadClamped(ix+gXOffsets[dir],iy+gYOffsets[dir]);			// neighbor receiving flow
			if (hdown > h)	// oops, flow is uphill! we over-eroded
			{				// so, this pixel -> just a tad higher than the downflow neighbor
				hf2.Write(ix,iy) = (0.0001f + hdown);
				hfbasin.Write(ix,iy) += (0.0001f + hdown - h);
				fixed++;
			}
			else // otherwise, just copy over to hf2
			{	hf2.Write(ix,iy) = h;
			}
		}
	}
	return(fixed);
}
///////////////////////////////////////////////////////////////////////////////
// smooth  --  simulate random smoothing (raindrops?) in mHeightVect[]
//             not linear; points move down faster than up
///////////////////////////////////////////////////////////////////////////////
static float smooth(	const ErosionContext& ec,
						const Map2D<float>& hf1,
						Map2D<float>& basinhf,
						Map2D<float>& hf2,
						float smooth_rate )
{	const float inv_smoothrate( 1.0f / smooth_rate );
	float dscale(0);    // average difference between neighboring pixels
	for (int iy = 0; iy<ec.ysize; iy++) 
	{  	for (int ix = 0; ix<ec.xsize; ix++)
		{	float h = hf1.Read(ix,iy);
			float sum1(0);
			float af1(0);
			for( int i=1;i<9;i++ )			// average of all neighbors deltas
			{	float delta = hf1.ReadClamped(ix+gXOffsets[i],iy+gYOffsets[i]) - h;
				if (delta < float(0) )		// count lower elements more
				{	af1 += 1;
					sum1 += delta;
				}
				af1 += 1;
				sum1 += delta;
			}
			float avg_delta = (sum1/af1);
			h = clamp( h, float(0.0), float(1.0) );
			// sign convention: positive afac = addition to elevation
			// what is the effect of nonlinear smoothing? hmmm.
			if (h < er_thresh) // only smooth terrain below altitude limit
			{	float afac = avg_delta * float(PCONST)*atan(0.1f * fabs(avg_delta*inv_smoothrate));
				// altitude-dependent smoothing
				if (pow_fac != 0)
				{
					afac *= float(fabs(float(1.0) - (pow_offset + pow(float(h),float(pow_fac)))));  
				}
				h += afac;
				basinhf.Write(ix,iy) += afac;	// record addition
				dscale = max(dscale,afac);
			}
			hf2.Write(ix,iy) = h;
		}
	}
	return(dscale); // average difference in height
}
///////////////////////////////////////////////////////////////////////////////
// slump  --  simulate land slippage; mudslides
///////////////////////////////////////////////////////////////////////////////
static void slump(	const ErosionContext& ec,
					const Map2D<float>& hfin,
					Map2D<float>& hfout,
					Map2D<float>& hfbasin,
					const float rate)
{	float dh,avg;
	float dscale;    /* average difference between neighboring pixels */
	int dcount;
	///////////////////////////////////////////////
	// find dscale by sampling a few pixels in the grid
	///////////////////////////////////////////////
	dcount = 0;
	dscale = 0;
	for( int iy = 0; iy<ec.ysize; iy+=5) // take samples of 1/25th of all
	{  	for( int ix = 0; ix<ec.xsize; ix+=5)
		{	float h = hfin.Read(ix,iy);
			float sum1 = 0;
			float af1 = 4;
			for( int i=1;i<9;i+=2)
			{	float n1 = hfin.ReadClamped(ix+gXOffsets[i+1],iy+gYOffsets[i+1]) - h; /* adj. neighbors */
				if (n1 < 0) // count lower elements more
				{	n1 *= float(2.0);
					af1 += 1;
				}
				sum1 += n1;
			}
			dscale += sum1/af1;
			dcount++;	// count number of summations
		}
	}
	dscale /= dcount;       /* really, dscale should be a fixed value */
	///////////////////////////////////////////////
	const float inv_dscale = 1.0f / dscale;
	///////////////////////////////////////////////
	for( int iy = 0; iy<ec.ysize; iy++) 
	{  	for( int ix = 0; ix<ec.xsize; ix++)
		{	float h = hfin.Read(ix,iy);
			float sum1 = 0;
			float sum2 = 0;
			float af1 = 4;
			float af2 = 4;
			for( int i=1;i<9;i+=2)
			{	float n1 = hfin.ReadClamped(ix+gXOffsets[i+1],iy+gYOffsets[i+1]) - h; // adj. neighbors 
				if (n1 < 0) // count lower elements more
				{	n1 *= float(2.0);
					af1 += 1;
				}
				sum1 += n1;
				float n2 = hfin.ReadClamped(ix+gXOffsets[i],iy+gYOffsets[i]) - h;     // diagonal neighbors  
				if (n2 < h)
				{	n2 *= float(2.0);
					af2 += 1;
				}
				sum2 += n2;
			}
			avg = 0.8f*(sum1/af1) + 0.2f*(sum2/af2);	/* weight closest neighbors heavily */
			dh = avg;									/* difference between this point and avg of neighbors */
			h = clamp( h, float(0.0), float(1.0) );
			//////////////////////////////////////////////
			// slump!
			//////////////////////////////////////////////
			if (h < er_thresh) // only smooth terrain below altitude limit
			{	/* sign convention: positive afac = addition to elevation */
				/* what is the effect of nonlinear smoothing? hmmm. */
				float afac(dh * PCONST*atan( pow(float(rate*fabs(dh*inv_dscale)),float(3)) )); 
				if (pow_fac != 0) // altitude-dependent slumping
				{	afac *= float(float(1.0) - (pow_offset + pow(float(h),float(pow_fac))));  
				}
				h += afac;
				hfbasin.Write( ix, iy ) += afac; // record addition
			}
			hfout.Write(ix,iy) = h;
			//////////////////////////////////////////////
		} 
	} 
}
///////////////////////////////////////////////////////////////////////////////
// find_upflow -- find direction of stream flow at each lattice 
//                point, and set element of mPeakFlagVect (uphill).      
//                Must be called after find_flow2() so mFlowDirVect set.  
///////////////////////////////////////////////////////////////////////////////
static void find_upflow(	const ErosionContext& ec,
							const Map2D<float>& hfin,
							const Map2D<u8>& hfflowin,
							Map2D<u8>& hfpeakout
						)
{	int dir;
	for( int y = 0; y < ec.ysize; y++)
	{	for( int x = 0; x < ec.xsize; x++)
		{	u8 pf_this = 0;
			float minv = hfin.Read(x,y);		// start with current point
			float maxv = minv;
			int inflows = 0;							// # neighbors pointing downhill to me
			for( int i=1;i<9;i++)
			{	int xn = x + gXOffsets[i];
				int yn = y + gYOffsets[i];
				float tval = hfin.ReadClamped(xn,yn);	// neighbor higher?
				if (tval > maxv)
				{	maxv = tval;					// yes, set uphill->this one 
					pf_this=u8(i); 
				}
				dir = hfflowin.ReadClamped(xn,yn);
				// inflow from here?
				if ((gXOffsets[i]+gXOffsets[dir]==0) && (gYOffsets[i]+gYOffsets[dir]==0)) 
				{	inflows++;
				}
			} 
			if (inflows == 0)
			{   // no inflows: find_ua can start here
				pf_this |= 0x10;
			}
			// and if we still think it's a peak
			if( pf_this==0 )
			{	// supposed to be peak
				for(int i=1;i<9;i++)
				{	int irx = x+gXOffsets[i];
					int iry = y+gYOffsets[i];
					dir = hfflowin.ReadClamped( irx, iry );
					if ((gXOffsets[i]+gXOffsets[dir]==0) && (gYOffsets[i]+gYOffsets[dir]==0))
					{	// inflow!
						pf_this = 1;
					}
				} 
			}
			hfpeakout.Write(x,y) = pf_this;	
		} 
	} 
} 
///////////////////////////////////////////////////////////////////////////////
// find_flow2  --  find direction of stream flow at each lattice 
//                point, and set element of mFlowDirVect matrix (downhill) 
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// lowestn()    --      returns direction of steepest descent from 
//                      element x,y in hf1[] array                 
///////////////////////////////////////////////////////////////////////////////
void find_flow2(	const ErosionContext& ec,
					const Map2D<float>& hfin,
					Map2D<u8>& hfflowmap )
{	for( int y=0; y<ec.ysize; y++)
	{	for( int x=0; x<ec.xsize; x++)
		{	u8 mini = 0;  
			float min = 1e30f;
			float here = hfin.Read(x,y);
			const float inv_sqrt_2 = 1.0f / 1.414f;
			for( int i=1;i<9;i++)
			{	float slope = hfin.ReadClamped(x+gXOffsets[i],y+gYOffsets[i]) - here;
				if ( (i==1) || (i==3) || (i==5) || (i==7) )
				{	slope *= inv_sqrt_2;
				}
				if (slope < min)
				{	mini = u8(i);
					min = slope;
				}
			}
			hfflowmap.Write(x,y) = mini; //lowestn(ec,hfin,x,y,&slope);
		}
	}  
}
///////////////////////////////////////////////////////////////////////////////
//  quick_flow()  --  set mFlowDirVect[] array to: 0 if element is a      
//                    local minimum, 1 otherwise                
///////////////////////////////////////////////////////////////////////////////
void quick_flow(	const ErosionContext& ec,
					const Map2D<float>& hf1,
					Map2D<u8>& hfdirmap
				)
{	for( int y = 0; y < ec.ysize; y++)
	{	for( int x = 0; x < ec.xsize; x++)
		{	float minv = hf1.Read(x,y);	// start with current point
			u8 output_dir = 0;
			for( int i = 1;i<9;i++)
			{	// check 4-neighbors
				int x1 = x+gXOffsets[i];
				int y1 = y+gYOffsets[i];
				float tval = hf1.ReadClamped(x1,y1);
				if (tval < minv)
				{	output_dir = 1;
				}
			} 
			hfdirmap.Write(x,y) = output_dir;
		}
	}
} 
///////////////////////////////////////////////////////////////////////////////
//  fill_bn  --  fill in basins in heightfield area             
//               input is hf1, output is in hf2                 
///////////////////////////////////////////////////////////////////////////////
int fill_basins_iter(	const ErosionContext& ec,
						const Map2D<float>& hf1,
						const Map2D<u8>& dirmap,
						Map2D<float>& hfbasin,
						Map2D<float>& hf2 
					)
{	///////////////////////////////////////////////
	// start with exact copy
	hf2.mData = hf1.mData;
	///////////////////////////////////////////////
	int changed = 0;	// no changes yet
	float dhmax(0.0);	// maximum elevation (?)
	///////////////////////////////////////////////
	for( int iy=0;iy<ec.ysize;iy++)
	{	for( int ix=0;ix<ec.xsize;ix++)
		{	int iflow = dirmap.Read(ix,iy); 
			float h = hf1.Read(ix,iy);
			if(iflow== 0)
			{   // local depression
				float sum = 0.0f;
				float hmax = -9E20f;
				//////////////////////////////
				for( int i=1;i<9;i++) 
				{	float neb = hf1.ReadClamped(ix+gXOffsets[i],iy+gYOffsets[i]);	// neighbor elev. 
					sum += neb;												// get sum of neighbor elevs 
					hmax = max( neb, hmax );								// get max elev
				}
				//////////////////////////////
				float wavg = sum / 8;						// simple average
				float delta_h = wavg - h;					// diff between avg and this 
															// max elevation change this run
				dhmax = max( delta_h, dhmax );
				h = wavg;							// hf2(x,y) <- weighted local avg
				hfbasin.Write(ix,iy) += delta_h;	// add to basin accumulation
				changed++;
			}
			hf2.Write(ix,iy) = h;
		} 
	} 
	///////////////////////////////////////////////
	return(changed);
}
///////////////////////////////////////////////////////////////////////////////
// fill_basins   --   call fill_bn() (fill basins) up to imax times
//			            in order to fill in basins, returns # of calls
///////////////////////////////////////////////////////////////////////////////
int fill_basins(	ErosionContext& ec,
					const int imax
				)
{	int icount = 0;
#if 0
	int tmp = 1;
	int i=0;
	while ((tmp > 0) && (icount<imax) )
	{	quick_flow( ec, ec.mHeightMap, ec.mFlowDirMap ); // find local minima only 
		tmp = fill_basins_iter( ec, ec.mHeightMap, ec.mFlowDirMap, ec.mBasinAccumMap, ec.mhftmp_float );    // temp <- original
		ec.mHeightMap = ec.mhftmp_float;
		icount++;
	} 
#endif
	return(icount);
}
///////////////////////////////////////////////////////////////////////////////
int find_ua2(	const ErosionContext& ec,
				const Map2D<u8>& hfin_pf,
				const Map2D<u8>& hfin_fd,
				const Map2D<float>& hfin_ua,
				Map2D<u8>& hfout_pf,
				Map2D<float>& hfout_ua
				)
{
	int iadded = 0;
	for( int iy = 0; iy<ec.ysize; iy++)
	{	for( int ix = 0; ix<ec.xsize; ix++)
		{	u8 ff = hfin_pf.Read(ix,iy); // mPeakFlagVect variable for this loc 
			bool bcheck = !(ff & 0x10);
			if( bcheck )
			{	///////////////////////////////////////
				// mPeakFlagVect bit b4: set = mUphillAreaVect for this node done 
				// if all neighbors flowing into this node done, can sum 
				///////////////////////////////////////
				int o_summed = 1;  // start assuming we're ok... 0=false
				for( int i=1;i<9;i++)
				{	int irx = ix+gXOffsets[i];
					int iry = iy+gYOffsets[i];
					// check if relevant neighbors finished
					int dir = hfin_fd.ReadClamped(irx,iry); 
					if ( ((gXOffsets[dir]+gXOffsets[i])==0) && ((gYOffsets[dir]+gYOffsets[i])==0))
					{	// ie,that node points here 
						if( hfin_pf.ReadClamped( irx, iry ) & 0x10 )
						{	o_summed = 0;
						}
					}
				}
				/////////////////////
				if (o_summed)
				{	float area = 1.0f;	// sum of uphill area for this element
					/////////////////////
					for( int i=1;i<9;i++)
					{	int irx = ix+gXOffsets[i];
						int iry = iy+gYOffsets[i];
						int dir = hfin_fd.ReadClamped(irx,iry); 
						if ( ((gXOffsets[dir]+gXOffsets[i])==0) && ((gYOffsets[dir]+gYOffsets[i])==0))
						{	area += hfin_ua.ReadClamped( irx,iry );
						}
					}  
					/////////////////////
					hfout_ua.Write(ix,iy) = area;
					// set b4: node summed
					hfout_pf.Write(ix,iy) |= 0x10;
					iadded++;  // increment count of nodes summed
				}
				/////////////////////
			}
		}
	}
	return iadded;
}
/////////////////////////////////////////////////////////////////////////////// 
// find_ua()  --  find uphill area for each mx element       
/////////////////////////////////////////////////////////////////////////////// 
static void find_ua1(	const ErosionContext& ec,
						const Map2D<u8>& hfin_pf,
						const Map2D<u8>& hfin_fd,
						const Map2D<float>& hfin_ua,
						Map2D<u8>& hfout_pf,
						Map2D<float>& hfout_ua
					
					)
{	///////////////////////////////////////////////////////////
	// first pass: set local highpoints to "summed", area to 1
	///////////////////////////////////////////////////////////
	for( int iy = 0; iy<ec.ysize; iy++ )
	{	for( int ix = 0; ix<ec.xsize; ix++ )
		{	u8 pf_orig = hfin_pf.Read(ix,iy);
			float ua_orig = hfin_ua.Read( ix, iy );
			bool ba = ( (pf_orig&0x10) == 0x10 );	// pre-marked summed (no inflows) 
			bool bb = ( (pf_orig&0x0f) == 0x0 );		// this point is a peak
			hfout_ua.Write(ix,iy) = ba ? 1 : bb ? 1 : ua_orig; 
			hfout_pf.Write(ix,iy) = bb ? pf_orig|0x10 : pf_orig;
		} 
	} 
	///////////////////////////////////////////////////////////
	// 2nd through n passes: cumulative add areas in a downhill-flow hierarchy 
	///////////////////////////////////////////////////////////
	int added = 1; // # found unsummed nodes this pass 
	while ( added > 0 )
	{	Map2D<u8> hftmp_pf = hfout_pf;
		Map2D<float> hftmp_ua = hfout_ua;
		added = find_ua2(	ec,								// context 
							hftmp_pf, hfin_fd, hftmp_ua,	// const inputs
							hfout_pf, hfout_ua				// outputs
						);
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ErosionContext::Execute()
{	//////////////////////////////////////////////////////
	orkprintf("eroding %d cycles... \n",miNumErosionCycles);
	//////////////////////////////////////////////////////

	miGridSize = mHeightMap.misize;

	if( miNumErosionCycles )
	{
		GpuErodeBegin( *this, mHeightMap );
		{
			for( int icyc=0; icyc<miNumErosionCycles; icyc++ )
			{	orkprintf("erosion cycle<%d>\n",icyc);
				/////////////////////////////////////////////
				// find new flowlines
				GpuFindFlow2( *this ); 
				GpuFindUpFlow( *this ); 
				/////////////////////////////////////////////
				// solve for cumulative flow 
				GpuFindUphillArea1(	*this, mUphillAreaMap );
				/////////////////////////////////////////////
				// erode  --  simulate erosion by shifting material in mHeightVect[]
				//            downhill depending on slope and stream flow
				//            depends on mUphillAreaVect (uphill area) being set
				/////////////////////////////////////////////
				GpuErode1( *this );
				GpuErodeCorrect( *this );
				/////////////////////////////////////////////
				// slump sidewalls (mudslide)
				//GpuSlump( *this );
				/////////////////////////////////////////////
				// fill any inadvertently-created basins
				//fill_basins(*this,miFillBasinsCycle);
				/////////////////////////////////////////////
				// very important ridge smoothing
				for( int i=0; i<this->miFillBasinsCycle; i++ )
				{
					//FillBasinsInitial
					GpuSmooth(*this );
				}
				/////////////////////////////////////////////
			}
		}
		GpuErodeEnd( *this, mHeightMap );
	}
}
///////////////////////////////////////////////////////////////////////////////
}}
#endif