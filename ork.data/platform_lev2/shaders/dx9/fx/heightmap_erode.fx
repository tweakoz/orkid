///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "bicubic.fxi"

///////////////////////////////////////////////////////////////////////////////
const float er_thresh = 2.0f;				// elevation above which no erosion happens
const float kbuffersize = 1024.0f;
///////////////////////////////////////////////////////////////////////////////
uniform float4x4        MatM;
uniform float4x4        MatMV;
uniform float4x4        MatMVP;
uniform float4x4        MatP;
uniform float4x4        MatAux;
uniform float4          modcolor;
uniform texture			ColorMap;
uniform texture			ColorMap2;
uniform float			MapFreq;
uniform float			MapAmp;
///////////////////////////////////////////////////////////////////////////////
sampler2D psampler = sampler_state
{   Texture = <ColorMap>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
};
sampler2D psampler2 = sampler_state
{   Texture = <ColorMap2>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
};
///////////////////////////////////////////////////////////////////////////////
struct HmMrtPixel
{	float4	Color		: COLOR0;
	float4	UVD			: COLOR1;
};
///////////////////////////////////////////////////////////////////////////////
struct UvVertex
{   float4 Position     : POSITION;
    float4 Color        : COLOR0;
    float4 Uv0          : TEXCOORD0;
};
///////////////////////////////////////////////////////////////////////////////
struct Fragment
{   float4 ClipPos		: Position;
    float4 Color		: Color;
    float4 UV0			: TEXCOORD0;
    float4 ClipUserPos	: TEXCOORD1;
};
///////////////////////////////////////////////////////////////////////////////
float4 TransformVertex( float4 inv )
{	float4x4 MyMatMVP = mul(MatMV,MatP);
	float4 hpos = mul( float4( inv.xyz, 1.0f ), MyMatMVP );
	return hpos;
}
///////////////////////////////////////////////////////////////////////////////
const int gXOffsets[9] =	// 8-dir index offset arrays
{	0,
   -1,	0,	1,
	1,		1,
	0, -1, -1
};
const int gYOffsets[9] =
{	0,
   -1, -1, -1,
	0,      1,
	1,  1,  0
};
///////////////////////////////////////////////////////////////////////////////
float2 OffsetUV( int index )
{	float2 out_uv = float2(	gXOffsets[index], gYOffsets[index] );
	return out_uv;
}
///////////////////////////////////////////////////////////////////////////////
float4 ReadClamped( sampler2D the_sampler, float x, float y, float kfsize )
{	float2 uv = float2(x,y)*(1.0f/kbuffersize); //kbuffersize
	return tex2Dlod( the_sampler, float4( uv, 0.0f, 0.0f ) );
}
///////////////////////////////////////////////////////////////////////////////
Fragment vs_stdterrain( UvVertex VtxIn )
{   Fragment FragOut;
	const float kfsize = modcolor.x;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = modcolor;
    FragOut.UV0 = float4( VtxIn.Uv0.xy, 0.0f, 0.0f );
    FragOut.ClipUserPos = VtxIn.Position*kfsize;
    return FragOut;
}
///////////////////////////////////////////////////////////////////////////////
// begin
// modcolor:	red<kfsize>
// input tex:	red<heightmap>
// output buf:	red<heightmap>
///////////////////////////////////////////////////////////////////////////////
float4 ps_begin( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.x;
	float2 xy = FragIn.ClipUserPos.xy;
	float4 input_read = ReadClamped( psampler,xy.x, xy.y, kfsize );
	return float4(input_read.x,0.0f,0.0f,0.0f);
}
technique begin
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_begin();
    }
};
///////////////////////////////////////////////////////////////////////////////
float3 raydir( float3 center, float3 point )
{
	float3 dir = normalize( point-center);
	return dir;
}
///////////////////////////////////////////////////////////////////////////////
// find_flow2
// modcolor:	red<kfsize>
// input tex:	red<heightmap>
// output buf:	red<heightmap> grn<flowdir>
///////////////////////////////////////////////////////////////////////////////
float4 ps_find_flow2( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.x;
	float2 xy = FragIn.ClipUserPos.xy;
	const float inv_sqrt_2 = 1.0f / 1.414f;
	float4 input_read = ReadClamped( psampler,xy.x, xy.y, kfsize );
	float4 mini = float4(input_read.x,0.0f,0.0f,0.0f);
	if( xy.x < kfsize && xy.y < kfsize )
	{ 
		float min = 1e30f;
		const float here = input_read.r;
		for( int i=1;i<9;i++)
		{	float2 xyo = xy+OffsetUV(i);
			float slope = ReadClamped(psampler,xyo.x,xyo.y,kfsize).r - here;
			if ( (i==1) || (i==3) || (i==5) || (i==7) )
			{	slope *= inv_sqrt_2;
			}
			if (slope < min)
			{	mini.y = float(i);
				min = slope;
			}
		}
	}
	return mini;
}
technique find_flow2
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_find_flow2();
    }
};
///////////////////////////////////////////////////////////////////////////////
// find_upflow
// modcolor:	red<kfsize>
// input tex:	red<heightmap> grn<flowdir>
// output buf:	red<heightmap> grn<flowdir> blu<peakflg> 
///////////////////////////////////////////////////////////////////////////////
float4 ps_find_upflow( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.x;
	float2 xy = FragIn.ClipUserPos.xy;
	float4 input_read = ReadClamped(psampler,xy.x,xy.y,kfsize);	// start with current point
	int output_flowdir = input_read.g;
	int output_peakflg = 0;
	if( xy.x < kfsize && xy.y < kfsize )
	{ 
		float minv = input_read.r;	// start with current point
		float maxv = minv;
		int inflows = 0;										// # neighbors pointing downhill to me
		for( int i=1;i<9;i++)
		{	float xn = xy.x + gXOffsets[i];
			float yn = xy.y + gYOffsets[i];
			float2 HD = ReadClamped(psampler,xn,yn,kfsize).rg;
			float tval = HD.r;	// neighbor higher?
			int dir = HD.g;
			if (tval > maxv)
			{	maxv = tval;									// yes, set uphill->this one 
				output_peakflg=i; 
			}
			// inflow from here?
			if ((gXOffsets[i]+gXOffsets[dir]==0) && (gYOffsets[i]+gYOffsets[dir]==0)) 
			{	inflows++;
			}
		} 
		if (inflows == 0)
		{   // no inflows: find_ua can start here
			output_peakflg += 0x10;
		}
		// and if we still think it's a peak
		if( output_peakflg==0 )
		{	// supposed to be peak
			for(int i=1;i<9;i++)
			{	int irx = xy.x+gXOffsets[i];
				int iry = xy.y+gYOffsets[i];
				int dir = ReadClamped(psampler,irx,iry,kfsize).g;
				if ((gXOffsets[i]+gXOffsets[dir]==0) && (gYOffsets[i]+gYOffsets[dir]==0))
				{	// inflow!
					output_peakflg = 1;
				}
			} 
		}
	}
	return float4( input_read.x, float(output_flowdir), float(output_peakflg), 0.0f );	
}
technique find_upflow
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_find_upflow();
    }
};
///////////////////////////////////////////////////////////////////////////////
// find_uphillarea1
// modcolor:	red<kfsize>
// input tex:	red<heightmap> grn<flowdir> blu<peakflg>
// input tex2:	red<uphillarea>
// output buf:	red<heightmap> grn<flowdir> blu<peakflg> alp<uphillarea> 
///////////////////////////////////////////////////////////////////////////////
float4 ps_find_uphillarea1( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.x;
	float2 xy = FragIn.ClipUserPos.xy;
	////////////////////////////////////
	const float4 input_read = ReadClamped(psampler,xy.x,xy.y,kfsize);
	const float4 input_read2 = ReadClamped(psampler2,xy.x,xy.y,kfsize);
	////////////////////////////////////
	const int input_pf = input_read.b;
	const int output_fd = input_read.g;
	const float input_ua = input_read2.r;
	////////////////////////////////////
	int output_pf = input_pf;
	float output_ua = input_ua;
	////////////////////////////////////
	if( xy.x < kfsize && xy.y < kfsize )
	{ 
		bool ba = (input_pf>15); // pre-marked summed (no inflows) 
		bool bb = ((input_pf%16)==0); // this point is a peak
		int pf_or_16 = (input_pf<16) ? input_pf+16 : input_pf; // input_pf|0x10
		output_ua = ba ? 1.0f : bb ? 1.0f : input_ua; 
		output_pf = bb ? pf_or_16 : input_pf;		
	}
	return float4( input_read.x, float(output_fd), float(output_pf), output_ua );	
}
technique find_uphillarea1
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_find_uphillarea1();
    }
};
///////////////////////////////////////////////////////////////////////////////
// find_uphillarea2
// modcolor:	red<kfsize>
// input tex:	red<heightmap> grn<flowdir> blu<peakflg> alp<uphillarea> 
// output buf:	red<heightmap> grn<flowdir> blu<peakflg> alp<uphillarea> 
///////////////////////////////////////////////////////////////////////////////
float4 ps_find_uphillarea2( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.x;
	float2 xy = FragIn.ClipUserPos.xy;
	////////////////////////////////////
	const float4 input_read = ReadClamped(psampler,xy.x,xy.y,kfsize);
	////////////////////////////////////
	int output_pk = input_read.b;
	float output_fd = input_read.g;
	float output_ua = input_read.a;
	////////////////////////////////////
	//int iadded = 0;
	int ff = input_read.b; // mPeakFlagVect variable for this loc 
	bool bcheck = ! (ff>=16); //(ff & 0x10);
	if( bcheck )
	{	///////////////////////////////////////
		// mPeakFlagVect bit b4: set = mUphillAreaVect for this node done 
		// if all neighbors flowing into this node done, can sum 
		///////////////////////////////////////
		int o_summed = 1;  // start assuming we're ok... 0=false
		for( int i=1;i<9;i++)
		{	int irx = xy.x+gXOffsets[i];
			int iry = xy.y+gYOffsets[i];
			float4 neighbor = ReadClamped(psampler,irx,iry,kfsize);
			// check if relevant neighbors finished
			int dir = neighbor.g; 
			if ( ((gXOffsets[dir]+gXOffsets[i])==0) && ((gYOffsets[dir]+gYOffsets[i])==0))
			{	// ie,that node points here
				int inpk = neighbor.b;
				if( inpk >= 16 ) //hfin_pf.ReadClamped( irx, iry ) & 0x10 )
				{	o_summed = 0;
				}
			}
		}
		/////////////////////
		if (o_summed)
		{	float area = 1.0f;	// sum of uphill area for this element
			/////////////////////
			for( int i=1;i<9;i++)
			{	int irx = xy.x+gXOffsets[i];
				int iry = xy.y+gYOffsets[i];
				float4 neighbor = ReadClamped(psampler,irx,iry,kfsize);
				int dir = neighbor.g; //hfin_fd.ReadClamped(irx,iry); 
				if ( ((gXOffsets[dir]+gXOffsets[i])==0) && ((gYOffsets[dir]+gYOffsets[i])==0))
				{	area += neighbor.a; //hfin_ua.ReadClamped( irx,iry );
				}
			}  
			/////////////////////
			output_ua = area; //hfout_ua.Write(ix,iy) = area;
			// set b4: node summed
			if( output_pk<16 ) output_pk += 16;	//hfout_pf.Write(ix,iy) |= 0x10;
			//iadded++;  // increment count of nodes summed
		}
		/////////////////////
	}
	return float4( input_read.r, float(output_fd), float(output_pk), output_ua );	
}
technique find_uphillarea2
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_find_uphillarea2();
    }
};
///////////////////////////////////////////////////////////////////////////////
// erode1
// modcolor:	red<kfsize> grn<kslopefactor> blu<erosionrate>
// input tex:	red<heightmap> grn<flowdir> blu<peakflg> alp<uphillarea> 
// output buf:	red<hfout> grn<hfbasin> blu<flowdir>
///////////////////////////////////////////////////////////////////////////////

float ErosionFactor( float slope_exponent, float flow_exponent )
{	
	const float powf_slope = 1.2f; //1.4f;
	const float powf_flow = 0.6f; //0.8f;
	
	return	  sign(slope_exponent) 
			* pow(abs(atan(slope_exponent)),powf_slope)
			* pow(flow_exponent,powf_flow);
}
float4 ps_erode1( Fragment FragIn ) : COLOR0
{	///////////////////////////////
	const float kfsize = modcolor.x;
	const float kfslopefactor = modcolor.y;		// correction factor to scale slope 
	///////////////////////////////
	const float inv_sq2 = (1.0f/1.414f);
	const float er_sedfac = 1.6f;				// slope ratio thresh. for sedimentation
	const float er_pow = 1.0f;					// power of flow in erosion rate
	///////////////////////////////
	const float ix = FragIn.ClipUserPos.x;
	const float iy = FragIn.ClipUserPos.y;
	///////////////////////////////
	const float4 input_read = ReadClamped(psampler,ix,iy,kfsize).xyzw;
	const int input_dir = input_read.g; // direction of outflow 
	const float input_h = input_read.r;	// altitude at this point ix,iy
	///////////////////////////////
	float output_h = input_h;
	///////////////////////////////
	const float kerosionrate = modcolor.z; //*input_h;
	if( ix < kfsize && iy < kfsize )
	{
		int upd;				// direction of uphill neighbor 
		float outslope;			// product of slope and flow_rate 
		float outflow;			// flow out of this cell 
		float erval;			// erosion potential at this point 
		float dh, slope;		// dh- delta height  dr- horizontal travel 
		//////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////
		float h = input_h;		// altitude at this point ix,iy
		if (h < er_thresh)
		{	//////////////////////////////////////////
			// find sum of inflowing neighbors (slope*flow)
			//////////////////////////////////////////
			float inslope = 0; // product of slope and flow_rate 
			//////////////////////////////////////////
			for( int i=1;i<9;i++) // loop over neighbors of ix,iy
			{	int x1 = ix+gXOffsets[i]; 
				int y1 = iy+gYOffsets[i];
				float4 input_offA = ReadClamped(psampler,x1,y1,kfsize).xyzw;
				float4 input_offB = ReadClamped(psampler2,x1,y1,kfsize).xyzw;
				int dir = input_offA.g;								//  direction index of this neighbor
				if ((gXOffsets[i]+gXOffsets[dir]==0) && (gYOffsets[i]+gYOffsets[dir]==0))
				{	// inflow from here
					slope = input_offA.r - output_h;				// pos = neigh. higher
					slope *= kfslopefactor;							// scale to corr. units
					bool b_is_i_even = (2*(i/2)) == i; 
					if( false == b_is_i_even )						// diagonal neighbor; reduce slope
					{	slope *= inv_sq2;      
					}
					float fua = input_offA.a;
					inslope += ErosionFactor(slope,fua);
				}
			} 
			//////////////////////////////////////////
			// now we've got sum of (slope*flow)
			//////////////////////////////////////////
			if (input_dir > 0) // slope positive: neighbor lower
			{	// if not a minimum
				h = min( h, 1.0f );
				int inx = ix+gXOffsets[input_dir];
				int iny = iy+gYOffsets[input_dir];
				slope = h - ReadClamped(psampler,inx,iny,kfsize).x; 
				slope *= kfslopefactor;
				bool b_is_i_even = (2*(input_dir/2)) == input_dir; 
				if( false == b_is_i_even )							// diagonal neighbor: reduce slope
				{	slope *= inv_sq2;	
				}
				outflow = input_read.a;
				if (outflow<1.0f) outflow = 0.9f;
				outslope = ErosionFactor(slope,outflow);
				///////////////////////////////////////////////
				// positive dh = erosion happens, neg = sedimentation 
				// sedimentation when (inslope) greater than (er_sedfac*outslope)
				///////////////////////////////////////////////
				erval = kerosionrate * (er_sedfac*outslope - inslope); 
				erval *= pow(outflow,er_pow);
				dh = kerosionrate * erval; 
				h = output_h;										// old altitude at this point
				if (dh > kerosionrate) dh = kerosionrate;			// don't erode too fast
				if  ( (h-dh) < 0.0f )								// don't let altitudes go negative
				{	dh = -h;										// min elevation will be == 0.0
				}
				/////////////////////////////////
				// don't sediment too fast..
				// this should really depend on neighboring elevations
				/////////////////////////////////
				const float ksedrate = 0.00f; // 0.03f;
				if (dh < -(ksedrate*kerosionrate))
				{	dh = -(ksedrate*kerosionrate); 
				}
				//////////////////////////////////////////////////////////
				// the actual erosion. right here!
				//////////////////////////////////////////////////////////
				output_h = float(h - dh);
				// accumulate erosion rate separately
			}	//////////////////////////////////////////////////////////
			else // if point is a local minimum (oops!)
			{	const int input_peak = input_read.b;
				//upd = input_peak & 0xf; // direction of uphill neighbor
				upd = clamp( input_peak, 0, 0xf ); // direction of uphill neighbor
				int inx = ix+gXOffsets[upd];
				int iny = iy+gYOffsets[upd];
				output_h = ReadClamped(psampler,inx,iny,kfsize).x+0.00001f;
			} 
		}
		//////////////////////////////////////
	}
	return float4( output_h, 0.0f, float(input_dir), 0.0f );
} 
technique erode1
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_erode1();
    }
};
///////////////////////////////////////////////////////////////////////////////
// erode_correct
// modcolor:	red<kfsize>
// input tex:	red<hfin> grn<hfbasin> blu<flowdir>
// output buf:	red<hfout> grn<hfbasin>
///////////////////////////////////////////////////////////////////////////////
float4 ps_erode_correct( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.r;
	///////////////////////////////		
	const float ix = FragIn.ClipUserPos.x;
	const float iy = FragIn.ClipUserPos.y;
	///////////////////////////////
	const float4 input_read = ReadClamped(psampler,ix,iy,kfsize).xyzw;
	const float input_height = input_read.x;
	const int input_flowdir = int(input_read.z);
	///////////////////////////////////////////
	float output_height = input_height;
	///////////////////////////////////////////
	int ixd = ix+gXOffsets[input_flowdir];
	int iyd = iy+gYOffsets[input_flowdir];
	///////////////////////////////////////////
	// neighbor receiving flow
	float hdown = ReadClamped(psampler,ixd,iyd,kfsize).x;		
	///////////////////////////////////////////
	if (hdown > input_height)	// oops, flow is uphill! we over-eroded
	{				// so, this pixel -> just a tad higher than the downflow neighbor
		//output_height = (0.0001f + hdown);
	}
	///////////////////////////////////////////
	return float4( output_height, 0.0f, 0.0f, 0.0f );
} 
technique erode_correct
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_erode_correct();
    }
};
///////////////////////////////////////////////////////////////////////////////
// slump
// modcolor:	red<kfsize> grn<ksmoothrate>
// input tex:	red<hfin> grn<hfbasin>
// output buf:	red<hfout> grn<hfbasin>
///////////////////////////////////////////////////////////////////////////////
float4 ps_slump( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.r;
	const float ksmoothrate = modcolor.g;
	const float kslumpscale = modcolor.b;
	const float inv_smoothrate = 1.0f / ksmoothrate;
	const float inv_slumpscale = 1.0f / kslumpscale;
	const float PCONST = 0.6366197723f;											// constant value 2/pi
	const float pow_fac = 0.0f;													// power of altititude in smoothing rate
	const float pow_offset = 1.0f;												// constant term in erosion rate 
	///////////////////////////////		
	const float ix = FragIn.ClipUserPos.x;
	const float iy = FragIn.ClipUserPos.y;
	///////////////////////////////
	const float4 input_read = ReadClamped(psampler,ix,iy,kfsize).xyzw;
	const float input_h = input_read.x;
	///////////////////////////////////////////
	float output_h = input_h;
	///////////////////////////////////////////
	if( ix < kfsize && iy < kfsize )
	{	float dscale = 0.0f;														// average difference between neighboring pixels
		float sum1 = 0.0f;
		float sum2 = 0.0f;
		float af1 = 4.0f;
		float af2 = 4.0f;
		for( int i=1;i<9;i+=2 )														// average of all neighbors deltas
		{	float n1 = ReadClamped(psampler,ix+gXOffsets[i+1],iy+gYOffsets[i+1],kfsize).x;
			if( n1 < 0.0f )
			{	n1 *= float(2.0);
				af1 += 1;
			}
			sum1 += n1;
			float n2 = ReadClamped(psampler,ix+gXOffsets[i],iy+gYOffsets[i],kfsize).x - output_h;     // diagonal neighbors  
			if (n2 < output_h)
			{	n2 *= float(2.0);
				af2 += 1;
			}
			sum2 += n2;
		}
		float avg = 0.8f*(sum1/af1) + 0.2f*(sum2/af2);	/* weight closest neighbors heavily */
		float dh = avg;									/* difference between this point and avg of neighbors */
		output_h = clamp( output_h, 0.0f, 1.0f );
		//////////////////////////////////////////////
		// slump!
		//////////////////////////////////////////////
		if( output_h < er_thresh) // only smooth terrain below altitude limit
		{	/* sign convention: positive afac = addition to elevation */
			/* what is the effect of nonlinear smoothing? hmmm. */
			float afac = dh * PCONST*atan( pow(ksmoothrate*abs(dh*inv_slumpscale),3.0f ) ); 
			if (pow_fac != 0) // altitude-dependent slumping
			{	afac *= 1.0f - (pow_offset + pow(output_h,pow_fac));  
			}
			output_h += afac;
			//output_basin += afac; // record addition
		}
	}
	///////////////////////////////////////////
	return float4( output_h, 0.0f, 0.0f, 0.0f );
} 
technique slump
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_slump();
    }
};
///////////////////////////////////////////////////////////////////////////////
// smooth
// modcolor:	red<kfsize> grn<ksmoothrate>
// input tex:	red<hfin> grn<hfbasin>
// output buf:	red<hfout> grn<hfbasin>
///////////////////////////////////////////////////////////////////////////////
float4 ps_smooth( Fragment FragIn ) : COLOR0
{	const float kfsize = modcolor.r;
	const float ksmoothrate = modcolor.g;
	const float ksmoothfrac = ksmoothrate/100.0f;
	const float inv_smoothrate = 1.0f / ksmoothrate;
	const float PCONST = 0.6366197723f;											// constant value 2/pi
	const float pow_fac = 0.0f;													// power of altititude in smoothing rate
	const float pow_offset = 1.0f;												// constant term in erosion rate 
	///////////////////////////////		
	const float ix = FragIn.ClipUserPos.x;
	const float iy = FragIn.ClipUserPos.y;
	///////////////////////////////
	const float4 input_read = ReadClamped(psampler,ix,iy,kfsize).xyzw;
	const float input_h = input_read.x;
	///////////////////////////////////////////
	float output_h = input_h;
	///////////////////////////////////////////
	if( ix < kfsize && iy < kfsize )
	{
		float sum1 = 0.0f;
		float af1 = 0.0f;
		for( int i=1;i<9;i++ )														// average of all neighbors deltas
		{	int inx = ix+gXOffsets[i];
			int iny = iy+gYOffsets[i];
			float ho = ReadClamped(psampler,inx,iny,kfsize).x;
			float delta_h = ho - input_h;
			//if (delta_h < 0.0f )													// count lower elements more
			//{	af1 += 1.0f;
			//	sum1 += delta_h;
			//}
			af1 += 1.0f;
			sum1 += delta_h;
		}
		float avg_delta = (sum1/af1);
		output_h = clamp( input_h, 0.0f, 1.0f );
		// sign convention: positive afac = addition to elevation
		// what is the effect of nonlinear smoothing? hmmm.
		//if (output_h < er_thresh) // only smooth terrain below altitude limit
		{	float afac = avg_delta*ksmoothfrac;// * PCONST*atan(0.1f * abs(avg_delta*inv_smoothrate));
			// altitude-dependent smoothing
			//if (pow_fac != 0.0f)
			//{
			//	afac *= abs(1.0f - (pow_offset + pow(output_h,pow_fac)));  
			//}
			output_h += afac;
		}
	}
	///////////////////////////////////////////
	return float4( output_h, 0.0f, 0.0f, 0.0f );
} 
technique smooth
{   pass p0
    {   VertexShader = compile vs_3_0 vs_stdterrain();
        PixelShader = compile ps_3_0 ps_smooth();
    }
};
///////////////////////////////////////////////////////////////////////////////
