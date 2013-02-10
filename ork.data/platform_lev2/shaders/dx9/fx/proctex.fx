///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "bicubic.fxi"

uniform float4x4        MatM;
uniform float4x4        MatMV;
uniform float4x4        MatMVP;
uniform float4x4        MatP;
uniform float4x4		MatAux;

uniform float4			modcolor;
uniform float4          User0;

uniform texture2D		ColorMap;
uniform texture2D		ColorMap2;
uniform texture2D		ColorMap3;

#define kbufsize (User0.w)

///////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};
	
sampler2D ColorMapPointSampler = sampler_state
{
	Texture = <ColorMap>;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = NONE;
	AddressU = Wrap;
	AddressV = Wrap;
};

sampler2D ColorMapPointClampSampler = sampler_state
{
	Texture = <ColorMap>;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler2D ColorMap2Sampler = sampler_state
{
	Texture = <ColorMap2>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
};

sampler2D ColorMap2PointSampler = sampler_state
{
	Texture = <ColorMap2>;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = NONE;
};

sampler2D ColorMap3Sampler = sampler_state
{
	Texture = <ColorMap3>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
};

sampler2D ColorMap3PointSampler = sampler_state
{
	Texture = <ColorMap3>;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = NONE;
};

///////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float3 Position		: POSITION;
	float4 Color		: COLOR;
	//float3 Normal		: NORMAL;
	float4 UVW			: TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

struct Fragment
{
	float4 ClipPos : POSITION;
	float4 Color   : COLOR;
	float4 UVW     : TEXCOORD0;
	float3 UserPos : TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////

Fragment vs_basic( Vertex VtxIn )
{
	float4 vpos = float4( VtxIn.Position, 1.0f );
	float4 npos = mul( vpos, MatMVP );
	
	Fragment FragOut;

	FragOut.ClipPos = npos;
	FragOut.UserPos = npos;
	FragOut.Color.xyz = float3(1.0f,1.0f,1.0f);
	FragOut.Color.w = 1.0f;
	FragOut.UVW = VtxIn.UVW;
	return FragOut;
}

float4 ps_basic( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapSampler, FragIn.UVW.xy ).xyz;
	PixOut = float4(texA,1.0f);
	return PixOut;
}

float4 ps_basic_cubic( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = BiCubicSampleTex2D( ColorMapPointSampler, FragIn.UVW.xy, kbufsize, 1.0f/kbufsize ).xyz;
	PixOut = float4(texA,1.0f);
	return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

Fragment vs_uvxf( Vertex VtxIn )
{
	float4 vpos = float4( VtxIn.Position, 1.0f );
	float4 npos = mul( vpos, MatMVP );
	float2 uvxf = mul( float4( VtxIn.UVW.xy,0.0,1.0), MatAux ).xy;
	Fragment FragOut;

	FragOut.ClipPos = npos;
	FragOut.UserPos = npos;
	FragOut.Color.xyz = float3(1.0f,1.0f,1.0f);
	FragOut.Color.w = 1.0f;
	FragOut.UVW = float4(uvxf,0.0f,1.0f);
	return FragOut;
}

float4 ps_octave( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float famp = MatAux[2][2];
	float3 texA = tex2D( ColorMapSampler, FragIn.UVW.xy ).xyz*famp;
	PixOut = float4(texA,1.0f);
	return PixOut;
}
technique octaves
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_uvxf();
		PixelShader = compile ps_3_0 ps_octave(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_downsample( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float2 o = float2(1.0f/kbufsize,0.0f);
	const float wa = 7.0f;
	const float wb = 1.0f;
	const float wc = 0.707f;
	const float wt = (wa+4.0f*wb+4.0f*wc);
	
	float4 texA = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy ).xyzw*wa;

	float4 texB = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy+o.xy ).xyzw*wb;
	float4 texC = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy-o.xy ).xyzw*wb;
	float4 texD = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy+o.yx ).xyzw*wb;
	float4 texE = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy-o.yx ).xyzw*wb;

	float4 texF = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy-o.xy-o.yx ).xyzw*wc;
	float4 texG = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy+o.xy-o.yx ).xyzw*wc;
	float4 texH = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy-o.xy+o.yx ).xyzw*wc;
	float4 texI = tex2D( ColorMapPointClampSampler, FragIn.UVW.xy+o.xy+o.yx ).xyzw*wc;

	PixOut = (texA+texB+texC+texD+texE+texF+texG+texH+texI)/wt;
	return PixOut;
}
technique downsample
{	pass p0
	{	VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_downsample(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_downsample16( Fragment FragIn ) : COLOR
{
	float o = 1.0f/kbufsize;

	float4 outp = float4(0.0f,0.0f,0.0f,0.0f);
	for( int i=0; i<4; i++ )
	{	float fx = float(i)*o;
		for( int j=0; j<4; j++ )
		{	float fy = float(j)*o;
			float2 tc = FragIn.UVW.xy+float2(fx,fy);
			float2 tc2 = float2( tc.x, tc.y );
			outp += tex2D( ColorMapPointClampSampler, tc2 ).xyzw*(1.0f/16.0f);		
		}
	}
	return outp;
}
technique downsample16
{	pass p0
	{	VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_downsample16(); 
        SrcBlend = ONE;
        DestBlend = ONE;
		AlphaBlendEnable = true;
		AlphaTestEnable = false;
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_colorize( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2Sampler, float2(texA.x,0.5f) ).xyz;
	PixOut = float4(texB,1.0f);
	return PixOut;
}
technique colorize
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_colorize(); 
        AddressU[1] = CLAMP;
        AddressV[1] = CLAMP;
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_uvmap( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, texA.xy ).xyz;
	PixOut = float4(texB,1.0f);
	return PixOut;
}
technique uvmap
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_uvmap(); 
        AddressU[0] = CLAMP;
        AddressV[0] = CLAMP;
        AddressU[1] = CLAMP;
        AddressV[1] = CLAMP;
	}
}


///////////////////////////////////////////////////////////////////////////////

float4 ps_sphmap( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	
	float dir = User0.z;
	
	float2 uspos = (FragIn.UVW.xy-float2(0.5f,0.5f))*2.0f;
	
	float3 xyz = normalize(lerp( float3(0.0f,1.0f,0.0f), float3( uspos.x, 0.01f, uspos.y ), dir ));
	
	
	float3 objn = normalize((tex2D( ColorMapSampler, FragIn.UVW.xy ).xyz*2.0f)-float3(1.0f,1.0f,1.0f));
    float3 objrefl = normalize(reflect(xyz,objn));
    float3 uvt = (objrefl.xyz*0.5f)+float3(0.5f,0.5f,0.5f);

	float3 texB = tex2D( ColorMap2Sampler, uvt.xz ).xyz;
	
	//float3 texB = BiCubicSampleTex2D( ColorMap2PointSampler, uvt.xz, kbufsize, 1.0f/kbufsize ).xyz;
	
	PixOut = float4(texB,1.0f);
	return PixOut;
}
technique sphmap
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_sphmap(); 
        AddressU[0] = CLAMP;
        AddressV[0] = CLAMP;
        AddressU[1] = CLAMP;
        AddressV[1] = CLAMP;
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_sphrefract( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	
	float ior = User0.z;
	float dir = User0.y;
	
	float2 uspos = (FragIn.UVW.xy-float2(0.5f,0.5f))*2.0f;

	float3 xyz = normalize(lerp( float3(0.0f,1.0f,0.0f), float3( uspos.x, 0.01f, uspos.y ), dir ));
	
	float3 objn = normalize((tex2D( ColorMapSampler, FragIn.UVW.xy ).xyz*2.0f)-float3(1.0f,1.0f,1.0f));
    float3 objrefl = normalize(refract(xyz,objn,ior));
    float3 uvt = (objrefl.xyz*0.5f)+float3(0.5f,0.5f,0.5f);

	float3 texB = tex2D( ColorMap2Sampler, uvt.xz ).xyz;
	PixOut = float4(texB,1.0f);
	return PixOut;
}
technique sphrefract
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_sphrefract(); 
        AddressU[0] = CLAMP;
        AddressV[0] = CLAMP;
        AddressU[1] = CLAMP;
        AddressV[1] = CLAMP;
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop2_add( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	PixOut = float4(texA+texB,1.0f);
	return PixOut;
}
technique imgop2_add
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop2_add(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop2_aminusb( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	PixOut = float4(texA-texB,1.0f);
	return PixOut;
}
technique imgop2_aminusb
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop2_aminusb(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop2_bminusa( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	PixOut = float4(saturate(texB-texA),1.0f);
	return PixOut;
}
technique imgop2_bminusa
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop2_bminusa(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop2_mul( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	PixOut = float4(texA*texB,1.0f);
	return PixOut;
}
technique imgop2_mul
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop2_mul(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop3_lerp( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	float3 texM = tex2D( ColorMap3PointSampler, FragIn.UVW.xy ).xyz;
	
	PixOut = float4( lerp(texA,texB,texM), 1.0f );
	return PixOut;
}
technique imgop3_lerp
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop3_lerp(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop3_addw( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	float3 texM = tex2D( ColorMap3PointSampler, FragIn.UVW.xy ).xyz;
	
	PixOut = float4( texA+(texB*texM), 1.0f );
	return PixOut;
}
technique imgop3_addw
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop3_addw(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop3_subw( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	float3 texM = tex2D( ColorMap3PointSampler, FragIn.UVW.xy ).xyz;
	
	PixOut = float4( saturate(texA-(texB*texM)), 1.0f );
	return PixOut;
}
technique imgop3_subw
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop3_subw(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_imgop3_mul3( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float3 texA = tex2D( ColorMapPointSampler, FragIn.UVW.xy ).xyz;
	float3 texB = tex2D( ColorMap2PointSampler, FragIn.UVW.xy ).xyz;
	float3 texM = tex2D( ColorMap3PointSampler, FragIn.UVW.xy ).xyz;
	
	PixOut = float4( texA*texB*texM, 1.0f );
	return PixOut;
}
technique imgop3_mul3
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_imgop3_mul3(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_texture( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float kfc = kbufsize/modcolor.x;
	float3 texA = BiCubicSampleTex2D( ColorMapPointSampler, FragIn.UVW.xy, modcolor.x, 1.0f/modcolor.x ).xyz;
	PixOut = float4(texA,1.0f);
	return PixOut;
}
technique ttex
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_texture(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

technique transform
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_uvxf();
		PixelShader = compile ps_3_0 ps_basic_cubic(); 
	}
}

///////////////////////////////////////////////////////////////////////////////

float3 raydir( float3 center, float3 pnt )
{
	float3 dir = normalize( pnt-center);
	return dir;
}

///////////////////////////////////////////////////////////////////////////////

float3 compnormal( Fragment FragIn ) 
{
	const float kidiv = 1.0f/kbufsize;

	float fU = FragIn.UVW.x;
	float fV = FragIn.UVW.y;

	//////////////////////////////////////
	// sampling 9 points, a center point and 8 surrounding points
	//////////////////////////////////////

	float3 nrm = float3(0.0f,0.0f,0.0f);
	
	for( int ix=-1; ix<2; ix++ )
	{
		float fx = float(kidiv)*float(ix);
		
		for( int iy=-1; iy<2; iy++ )
		{
			float fy = float(kidiv)*float(iy);
	
			float2 uv4 = float2(fU+fx,fV+fy);

			float2 uv0 = uv4+float2(-kidiv,-kidiv);
			float2 uv1 = uv4+float2(0.0,-kidiv);
			float2 uv2 = uv4+float2(kidiv,-kidiv);

			float2 uv3 = uv4+float2(-kidiv,0.0);
			float2 uv5 = uv4+float2(kidiv,0.0);

			float2 uv6 = uv4+float2(-kidiv,kidiv);
			float2 uv7 = uv4+float2(0.0,kidiv);
			float2 uv8 = uv4+float2(kidiv,kidiv);

			//////////////////////////////////////
			#if 0
			float h4 = BiCubicSampleTex2D( ColorMapPointSampler, uv4, kbufsize, kidiv ).x*modcolor.x;

			float h0 = BiCubicSampleTex2D( ColorMapPointSampler, uv0, kbufsize, kidiv ).x*modcolor.x;
			float h1 = BiCubicSampleTex2D( ColorMapPointSampler, uv1, kbufsize, kidiv ).x*modcolor.x;
			float h2 = BiCubicSampleTex2D( ColorMapPointSampler, uv2, kbufsize, kidiv ).x*modcolor.x;

			float h3 = BiCubicSampleTex2D( ColorMapPointSampler, uv3, kbufsize, kidiv ).x*modcolor.x;
			float h5 = BiCubicSampleTex2D( ColorMapPointSampler, uv5, kbufsize, kidiv ).x*modcolor.x;

			float h6 = BiCubicSampleTex2D( ColorMapPointSampler, uv6, kbufsize, kidiv ).x*modcolor.x;
			float h7 = BiCubicSampleTex2D( ColorMapPointSampler, uv7, kbufsize, kidiv ).x*modcolor.x;
			float h8 = BiCubicSampleTex2D( ColorMapPointSampler, uv8, kbufsize, kidiv ).x*modcolor.x;
			#else
			float h4 = tex2D( ColorMapPointSampler, uv4 ).x*modcolor.x;
			
			float h0 = tex2D( ColorMapPointSampler, uv0 ).x*modcolor.x;
			float h1 = tex2D( ColorMapPointSampler, uv1 ).x*modcolor.x;
			float h2 = tex2D( ColorMapPointSampler, uv2 ).x*modcolor.x;

			float h3 = tex2D( ColorMapPointSampler, uv3 ).x*modcolor.x;
			float h5 = tex2D( ColorMapPointSampler, uv5 ).x*modcolor.x;
			
			float h6 = tex2D( ColorMapPointSampler, uv6 ).x*modcolor.x;
			float h7 = tex2D( ColorMapPointSampler, uv7 ).x*modcolor.x;
			float h8 = tex2D( ColorMapPointSampler, uv8 ).x*modcolor.x;
			#endif
			//////////////////////////////////////
			
			float3 p4 = float3( uv4.x, h4, uv4.y );

			float3 p0 = float3( uv0.x, h0, uv0.y );
			float3 p1 = float3( uv1.x, h1, uv1.y );
			float3 p2 = float3( uv2.x, h2, uv2.y );

			float3 p3 = float3( uv3.x, h3, uv3.y );
			float3 p5 = float3( uv5.x, h5, uv5.y );

			float3 p6 = float3( uv6.x, h6, uv6.y );
			float3 p7 = float3( uv7.x, h7, uv7.y );
			float3 p8 = float3( uv8.x, h8, uv8.y );

			float3 d0 = raydir( p4, p0 );
			float3 d1 = raydir( p4, p1 );
			float3 d2 = raydir( p4, p2 );
			float3 d3 = raydir( p4, p5 );
			float3 d4 = raydir( p4, p8 );
			float3 d5 = raydir( p4, p7 );
			float3 d6 = raydir( p4, p6 );
			float3 d7 = raydir( p4, p3 );
			
			float3 n0 = cross( d1, d0 );
			float3 n1 = cross( d2, d1 );
			float3 n2 = cross( d3, d2 );
			float3 n3 = cross( d4, d3 );
			float3 n4 = cross( d6, d4 );
			float3 n5 = cross( d6, d5 );
			float3 n6 = cross( d7, d6 );
			float3 n7 = cross( d0, d7 );

			nrm += normalize(n0+n1+n2+n3+n4+n5+n6+n7);
		}
	}
	
	nrm = normalize(nrm);
	
	//////////////////////////////////////
    
    return nrm;
}

float4 ps_h2n( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	/*const float kidiv = 1.0f/kbufsize;
		
	float uvox = float2(-kidiv,0.0f);
	float uvoy = float2(0.0f,-kidiv);
		
	float2 uvl = FragIn.UVW.xy+uvox;
	float2 uvt = FragIn.UVW.xy+uvoy;
	float2 uvl2 = FragIn.UVW.xy+(uvox*2.0f);
	float2 uvt2 = FragIn.UVW.xy+(uvoy*2.0f);
	float2 uvl3 = FragIn.UVW.xy+(uvox*3.0f);
	float2 uvt3 = FragIn.UVW.xy+(uvoy*3.0f);
	
	float texC = BiCubicSampleTex2D( ColorMapPointSampler, FragIn.UVW.xy, kbufsize, kidiv ).x;
	float texL = BiCubicSampleTex2D( ColorMapPointSampler, uvl, kbufsize, kidiv ).x;
	float texT = BiCubicSampleTex2D( ColorMapPointSampler, uvt, kbufsize, kidiv ).x;
	float texL2 = BiCubicSampleTex2D( ColorMapPointSampler, uvl2, kbufsize, kidiv ).x;
	float texT2 = BiCubicSampleTex2D( ColorMapPointSampler, uvt2, kbufsize, kidiv ).x;
	float texL3 = BiCubicSampleTex2D( ColorMapPointSampler, uvl3, kbufsize, kidiv ).x;
	float texT3 = BiCubicSampleTex2D( ColorMapPointSampler, uvt3, kbufsize, kidiv ).x;
	
	float dx1 = (texC-texL);
	float dz1 = (texC-texT);
	float dx2 = (texL-texL2);
	float dz2 = (texT-texT2);
	float dx3 = (texL2-texL3);
	float dz3 = (texT2-texT3);
	
	float dx = (dx1+dx2+dx3)*0.333f;
	float dz = (dz1+dz2+dz3)*0.333f;
	
	float3 n = float3(0.5f,0.5f,0.5f)+normalize(float3(dx*modcolor.x,0.1f,dz*modcolor.x))*0.5f;*/
	
	float3 n = float3(0.5f,0.5f,0.5f)+compnormal( FragIn )*0.5f;
	
	//float3 texB = tex2D( ColorMap2Sampler, n.xy ).xyz;
	
	//PixOut = float4(texB,1.0f);
	PixOut = float4(n,1.0f);
	return PixOut;
}

Fragment vs_h2n( Vertex VtxIn )
{
	float4 vpos = float4( VtxIn.Position, 1.0f );
	float4 npos = mul( vpos, MatMVP );
	
	Fragment FragOut;

	FragOut.ClipPos = npos;
	FragOut.UserPos = npos;
	FragOut.Color.xyz = float3(1.0f,1.0f,1.0f);
	FragOut.Color.w = 1.0f;
	FragOut.UVW = float4(VtxIn.UVW.xy,0.0f,0.0f);
	return FragOut;
}

technique h2n
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_basic();
		PixelShader = compile ps_3_0 ps_h2n(); 
        AddressU[0] = CLAMP;
        AddressV[0] = CLAMP;
        AddressU[1] = CLAMP;
        AddressV[1] = CLAMP;
	}
}

///////////////////////////////////////////////////////////////////////////////
