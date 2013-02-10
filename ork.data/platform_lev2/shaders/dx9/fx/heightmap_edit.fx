///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "bicubic.fxi"

///////////////////////////////////////////////////////////////////////////////
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
uniform texture			ColorMap3;
uniform texture			ColorMap4;

uniform float			MapFreq;
uniform float			MapAmp;

uniform float4			RotMtx;

///////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap2Sampler = sampler_state
{
    Texture = <ColorMap2>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap3Sampler = sampler_state
{
    Texture = <ColorMap3>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap4Sampler = sampler_state
{
    Texture = <ColorMap4>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = NONE; // LINEAR NONE
};

///////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapPointSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap2PointSampler = sampler_state
{
    Texture = <ColorMap2>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap3PointSampler = sampler_state
{
    Texture = <ColorMap3>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap4PointSampler = sampler_state
{
    Texture = <ColorMap4>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE; // LINEAR NONE
};

///////////////////////////////////////////////////////////////////////////////

sampler2D HeightMapSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = NONE; // LINEAR NONE
};

///////////////////////////////////////////////////////////////////////////////

struct HmMrtPixel
{
	float4	Color		: COLOR0;
	float4	UVD			: COLOR1;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
    float4 Position                : POSITION;
    float4 Color                   : COLOR0;
};


struct UvVertex
{
    float4 Position                : POSITION;
    float4 Color                   : COLOR0;
    float4 Uv0                     : TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
    float4 ClipPos		: Position;
    float4 Color		: Color;
    float4 UV0			: TEXCOORD0;
    float4 ClipUserPos	: TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////

float4 TransformVertex( float4 inv )
{
	float4x4 MyMatMVP = mul(MatMV,MatP);
	float4 hpos = mul( float4( inv.xyz, 1.0f ), MyMatMVP );
	return hpos;
}

///////////////////////////////////////////////////////////////////////////////

float3 raydir( float3 center, float3 point )
{
	float3 dir = normalize( point-center);
	return dir;
}

///////////////////////////////////////////////////////////////////////////////

float4 compnormal( VtxOut FragIn ) 
{
	const float kfsize = modcolor.x;
	const float kfsizefactor = (kfsize/kbuffersize);

	float imageSize = modcolor.z;
	float texelSize = modcolor.w;

	float fU = FragIn.UV0.x*kfsizefactor;
	float fV = FragIn.UV0.y*kfsizefactor;

	//////////////////////////////////////
	// per fragment partial derivative of uv0 with respect to targetxy
	//////////////////////////////////////

	float fdU = ddx( fU );
	float fdV = ddy( fV );
	float fdX = ddx( FragIn.UV0.z );
	float fdZ = ddy( FragIn.UV0.w );

	//////////////////////////////////////
	// sampling 9 points, a center point and 8 surrounding points
	//////////////////////////////////////

	float2 uv4 = float2(fU,fV);

	float2 uv0 = uv4+float2(-fdU,-fdV);
	float2 uv1 = uv4+float2(0.0,-fdV);
	float2 uv2 = uv4+float2(fdU,-fdV);

	float2 uv3 = uv4+float2(-fdU,0.0);
	float2 uv5 = uv4+float2(fdU,0.0);

	float2 uv6 = uv4+float2(-fdU,fdV);
	float2 uv7 = uv4+float2(0.0,fdV);
	float2 uv8 = uv4+float2(fdU,fdV);

	//////////////////////////////////////

	float3 p4 = tex2D( HeightMapSampler, uv4 ).xyz;
	
	float3 p0 = tex2D( HeightMapSampler, uv0 ).xyz;
	float3 p1 = tex2D( HeightMapSampler, uv1 ).xyz;
	float3 p2 = tex2D( HeightMapSampler, uv2 ).xyz;

	float3 p3 = tex2D( HeightMapSampler, uv3 ).xyz;
	float3 p5 = tex2D( HeightMapSampler, uv5 ).xyz;
	
	float3 p6 = tex2D( HeightMapSampler, uv6 ).xyz;
	float3 p7 = tex2D( HeightMapSampler, uv7 ).xyz;
	float3 p8 = tex2D( HeightMapSampler, uv8 ).xyz;

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

	float3 nrm = normalize(n0+n1+n2+n3+n4+n5+n6+n7);
	
	//////////////////////////////////////

	float4 PixOut = float4( nrm, 1.0f ); 
    
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSDepthColor( UvVertex VtxIn )
{
    VtxOut FragOut;

	//float4 fcpos = mul( VtxIn.Position, MatMVP  );
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
	
	float2 NormalXZ = VtxIn.Uv0.xy;
	
    FragOut.Color = float4(0.0f,0.0f,0.0f,1.0f);
        
    FragOut.UV0 = float4( NormalXZ, 0.0f, 0.0f );
    
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

float4 PSDepthColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    
    float2 uv = FragIn.UV0.xy;
    float4 hpos = FragIn.ClipUserPos;
    float3 dpos = hpos.xyz/hpos.w;
    
	const float near = 1.0f;
	const float far = 3000.0f;
   	float depth = ((hpos.z) - near)/(far - near);

    PixOut = float4(depth,uv,1.0f);
    return PixOut;
}

technique depthcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSDepthColor();
        PixelShader = compile ps_3_0 PSDepthColor();
    }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut vs_shadowrecv( UvVertex VtxIn )
{
	VtxOut FragOut;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = modcolor;
    FragOut.UV0 = float4( VtxIn.Uv0.xy, 0.0f, 0.0f );
    FragOut.ClipUserPos = VtxIn.Position;
    return FragOut;
}

float4 ps_shadowrecv( VtxOut FragIn ) : COLOR
{
	const float near = 1.0f;
	const float far = 3000.0f;

	float4 PixOut = float4( FragIn.UV0.xy, 0.0f, 1.0f );
	const float kfsize = modcolor.x;
	float imageSize = modcolor.z;
	float texelSize = modcolor.w;


	float3 favg_color = float3( 0.0f, 0.0f, 0.0f );
	
	float4 oposmapuv = float4(FragIn.UV0.xy*(kfsize/kbuffersize),0.0f,0.0f);
	float3 base_color = tex2Dlod( ColorMap4PointSampler, oposmapuv ).xyz;

	float4 opos = float4( tex2Dlod( ColorMapPointSampler, oposmapuv ).xyz, 1.0f );
	float4 hpos = mul(opos,MatAux);
	float  depth = ((hpos.z) - near)/(far - near);
	float3 dpos = hpos.xyz*(1.0f/hpos.w);
	float4 dmuv = float4( 0.5f+dpos.x*0.5f, -(0.5f+dpos.y*0.5f),0.0f,0.0f );
	float3 depthmap = tex2Dlod( ColorMap2PointSampler, dmuv ).xyz;
	//////////////////////////
	// recover normal
	// d = sqrt(dx^2+dy^2+dz^2);
	// d^2 = dx^2+dy^2+dz^2
	float2 normal_xz = depthmap.yz;
	float nmag = pow(distance(normal_xz,float2(0.0f,0.0f)),2.0);
	float nrmy = 1.0f-nmag;
	float3 Normal = normalize(float3( normal_xz.x, nrmy, normal_xz.y ));
	float EnvLightU = 0.5f+(Normal.x*0.5);
	float EnvLightV = 0.5f+(Normal.z*0.5);
	float3 EnvLight = normalize(tex2D( ColorMap3Sampler, float2( EnvLightU, EnvLightV ) ))*1.4f;
	//////////////////////////
	for( int iu=-2; iu<3; iu++ )
	{	float fu = float(iu)/1024.0f;
		for( int iv=-2; iv<3; iv++ )
		{	float fv = float(iv)/1024.0f;
			//////////////////////////
			float2 fovvsetuv = float2(fu,fv);
			float4 oposmapuv_offset = float4((FragIn.UV0.xy+fovvsetuv)* (kfsize/kbuffersize),0.0f,0.0f);
			float4 opos_offset = float4( tex2Dlod( ColorMapPointSampler, oposmapuv_offset ).xyz, 1.0f );
			float4 hpos_offset = mul(opos_offset,MatAux);
			float  depth_offset = ((hpos_offset.z) - near)/(far - near);
			float3 dpos_offset = hpos_offset.xyz*(1.0f/hpos_offset.w);
			float4 dmuv_offset = float4( 0.5f+dpos_offset.x*0.5f, -(0.5f+dpos_offset.y*0.5f),0.0f,0.0f );
			float3 depthmap_offset = tex2Dlod( ColorMap2PointSampler, dmuv_offset ).xyz;
			//////////////////////////
			float dbias = 0.01f;
			float fvis =		(dpos_offset.x <= 1.0f)
							&&	(dpos_offset.x >= -1.0f)
							&&	(dpos_offset.y < 1.0f)
							&&	(dpos_offset.y > -1.0f)
							&&	(depthmap_offset.x+dbias>depth_offset);
			float3 fcolor = EnvLight*fvis;
			favg_color += fcolor;
		}
	}
	//////////////////////////
	favg_color = base_color+(favg_color/25.0f);
	PixOut = float4(favg_color,1.0f);
	//////////////////////////
	return PixOut;
}

technique shadowrecv
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_shadowrecv();
        PixelShader = compile ps_3_0 ps_shadowrecv();
    }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSNormalsToColors( UvVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = modcolor;
    FragOut.UV0 = float4( VtxIn.Uv0.xy, 0.0f, 0.0f );
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

float4 PSNormalsToColors( VtxOut FragIn ) : COLOR0
{
	float4 PixOut;
	const float kfsize = modcolor.x;
	float imageSize = modcolor.z;
	float texelSize = modcolor.w;
	
	float2 nrmuv = FragIn.UV0.xy * (kfsize/kbuffersize);

	float3 normal = tex2D( ColorMapPointSampler, nrmuv ).rgb;
	float fu = 0.5f+(normal.x*0.5);
	float fv = 0.5f+(normal.z*0.5);

	float4 lightenv = tex2D( ColorMap2Sampler, float2( fu, fv ) );
	
	//float fdot = dot( normal, float3(0.0f,1.0f,0.0f) );
		
	PixOut = float4( lightenv.xyz, 1.0f ); 
	//PixOut = float4( normal.xyz, 1.0f ); 
	//PixOut = float4( FragIn.UV0.xy,0.0f,0.0f ); 
    return PixOut;

	
}


///////////////////////////////////////////////////////////////////////////////

float2 aohm_xz2uv( float2 xz, float kfworldsize )
{
	float fu = 0.5f+xz.x/kfworldsize;
	float fv = 0.5f+xz.y/kfworldsize;
	return float2( fu, fv );
}

float4 PSaohmshade( VtxOut FragIn ) : COLOR0
{
	float4 PixOut;

	float3 InNormal = compnormal(FragIn).xyz;
	float fu = 0.5f+(InNormal.x*0.5);
	float fv = 0.5f+(InNormal.z*0.5);
	float4 lightenvN = tex2D( ColorMap2Sampler, float2( fu, fv ) );

	const float kfsize = modcolor.x;
	const float worldSize = modcolor.y;
	const float imageSize = modcolor.z;
	const float texelSize = modcolor.w;
	
	float2 nrmuv = FragIn.UV0.xy * (kfsize/kbuffersize);

	float3 xyz = tex2D( ColorMapPointSampler, nrmuv ).rgb;

	float4 lightenv = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	
	float fsum = 0.0f;
	
	const float fmaxdist = 10.0f;
	
	for( float fx =-8.0f; fx<=8.0f; fx+= 2.0f )
	{
		for( float fz =-8.0f; fz<=8.0f; fz+= 2.0f )
		{
			float3 normal = normalize( float3( fx, 1.0f, fz ) );//*3.0f;

			bool boccluded = false;
			bool boob = false;
						
			float3 fxyz2 = xyz;
			
			for( int i=0; (i<3) && !boccluded && !boob; i++ ) //( (false==boccluded) && (false==boob) )
			{
				fxyz2 += normal*(1024.0f*16.0f/1024.0f);
				
				float2 uvc = aohm_xz2uv(fxyz2.xz,worldSize);
				
				if( uvc.x > 1.0f ) boob=true;
				if( uvc.x < 0.0f ) boob=true;
				if( uvc.y > 1.0f ) boob=true;
				if( uvc.y < 0.0f ) boob=true;

				if( fxyz2.y > 200.0f ) boob=true;
				
				float3 nxyz = tex2D( ColorMapPointSampler, uvc ).rgb;

				if( nxyz.y > fxyz2.y ) boccluded = true;
				
			}
			
			if( false == boccluded )
			{
				float fu = 0.5f+(normal.x*0.5);
				float fv = 0.5f+(normal.z*0.5);
				lightenv = lightenv+lightenvN*tex2D( ColorMap2Sampler, float2( fu, fv ) );
			}
			fsum += 1.0f;
		}
	}
	PixOut = float4( lightenv.xyz*(1.0f/fsum), 1.0f ); 
	//PixOut = float4( InNormal, 1.0f );
    return PixOut;

	
}

technique aohmshade
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSNormalsToColors();
        PixelShader = compile ps_3_0 PSaohmshade();
    }
};

///////////////////////////////////////////////////////////////////////////////

float3 mycross( float3 a, float3 b )
{
	//return cross( b, a );
	return cross( a, b );
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSComputeNormals( UvVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = modcolor;
    FragOut.UV0 = float4( VtxIn.Uv0.xy, VtxIn.Position.xz );
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

float4 PSComputeNormals( VtxOut FragIn ) : COLOR0
{
	return compnormal( FragIn );
}

technique computenormals
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSComputeNormals();
        PixelShader = compile ps_3_0 PSComputeNormals();
    }
};

///////////////////////////////////////////////////////////////////////////////

float4 PSTexFragColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
	float imageSize = modcolor.z;
	float texelSize = modcolor.w;
    PixOut = MapAmp*BiLinearSampleTex2D( HeightMapSampler, FragIn.UV0.xy, imageSize, texelSize ).xyzw;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSFragColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    PixOut = FragIn.Color;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSTexColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    PixOut = float4( tex2D( ColorMapSampler, FragIn.UV0.xy ).xyz, 1.0f );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSSkyColor( UvVertex VtxIn )
{
    VtxOut FragOut;

	float3 normal = normalize( VtxIn.Position.xyz );

	float fu = 0.5f+(normal.x*0.5);
	float fv = 0.5f+(normal.z*0.5);

	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = float4( normal, 1.0f );
    FragOut.UV0 = float4( fu, fv, 0.0f, 0.0f );
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

technique skytest
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSSkyColor();
        PixelShader = compile ps_3_0 PSTexColor();
    }
};

///////////////////////////////////////////////////////////////////////////////

technique normal2color
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSNormalsToColors();
        PixelShader = compile ps_3_0 PSNormalsToColors();
    }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSTexDepthColor( UvVertex VtxIn )
{
    VtxOut FragOut;

	FragOut.ClipPos = TransformVertex( VtxIn.Position );
			
    FragOut.Color = modcolor;
    
    float2 uv0 = VtxIn.Uv0.xy*MapFreq;
    
    float2x2 rotmtx;
    rotmtx[0].x = RotMtx.x;
    rotmtx[0].y = RotMtx.y;
    rotmtx[1].x = RotMtx.z;
    rotmtx[1].y = RotMtx.w;
    
    float2 uva = mul(uv0,rotmtx);
    
    FragOut.UV0 = float4( uva, 0.0f, 0.0f );
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

technique texdepthcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTexDepthColor();
        PixelShader = compile ps_3_0 PSTexFragColor();
    }
};

///////////////////////////////////////////////////////////////////////////////

