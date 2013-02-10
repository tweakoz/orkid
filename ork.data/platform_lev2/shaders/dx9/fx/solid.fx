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

uniform float4          modcolor;
uniform float4          user0;

uniform texture			ColorMap;
uniform texture			ColorMap2;
uniform texture			ColorMap3;

/////////////////////////////////////////////////////
// Stupid FxLite hax
//fxlitesux_samplermap <ColorMap> ColorMapSampler Color3DMapSampler ColorMapPointSampler
//fxlitesux_samplermap <ColorMap2> ColorMap2Sampler
/////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler3D Color3DMapSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = Point;
    MinFilter = Point;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMapPointSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = Point;
    MinFilter = Point;
    MipFilter = NONE; // LINEAR NONE
};
sampler2D ColorMap2Sampler = sampler_state
{
    Texture = <ColorMap2>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = None; // LINEAR NONE
};
sampler2D ColorMap3Sampler = sampler_state
{
    Texture = <ColorMap3>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = None; // LINEAR NONE
};

///////////////////////////////////////////////////////////////////////////////

struct MrtPixel
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

MrtPixel PSFragColor( VtxOut FragIn )
{
    MrtPixel PixOut;
    PixOut.Color	= FragIn.Color;
    PixOut.UVD.x = FragIn.UV0.x;
    PixOut.UVD.y = FragIn.UV0.y;
    PixOut.UVD.z = FragIn.ClipUserPos.z;
    PixOut.UVD.w = 0.0f;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 TransformVertex( float4 inv )
{
	float4x4 MyMatMVP = mul(MatMV,MatP);
	float4 hpos = mul( float4( inv.xyz, 1.0f ), MyMatMVP );
	return hpos;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxColor( UvVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.UV0.x = VtxIn.Uv0.x;
    FragOut.UV0.y = VtxIn.Uv0.y;
    FragOut.UV0.z = 0.0f;
    FragOut.UV0.w = 1.0f;
    FragOut.Color = VtxIn.Color;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////
VtxOut VSChecker3d( UvVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = VtxIn.Color*modcolor;
    FragOut.UV0 = mul( float4( VtxIn.Position.xyz, 1.0f ), MatM )*0.01f;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}
///////////////////////////////////////////////////////////////////////////////
float4 PSChecker3d(VtxOut FragIn) : COLOR
 {
    //float3 checker = tex3D( Color3DMapSampler, FragIn.UV0.xyz ).xyz;
    float fr = FragIn.UV0.x*0.1f;
    float fg = FragIn.UV0.y*0.1f;
    float fb = FragIn.UV0.z*0.1f;
	float3 checker = saturate(float3(fr,fg,fb)+float3(0.7f,0.7f,0.7f));
	float fdx = abs(ddx(FragIn.ClipUserPos.z));
	float fdy = abs(ddy(FragIn.ClipUserPos.z));
	float3 fd = saturate(normalize(float3(fdx,0.5f,fdy))+float3(0.5f,0.5f,0.5f));
	float3 fc = (checker*fd);
	float fgrey = (fc.x+fc.y+fc.z)*0.3333f;
	float3 outc = float3(fgrey,fgrey,fgrey); //lerp( fc, float3(fgrey,fgrey,fgrey), 0.5f );
	float3 outp = outc;
    return float4(outp,1.0f);
}
///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxModColor( UvVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = VtxIn.Color*modcolor;
    FragOut.UV0 = float4( VtxIn.Uv0.xy, 0.0f, 0.0f );
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSModColor( UvVertex VtxIn )
{
    VtxOut FragOut;

	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.Color = modcolor;
    FragOut.UV0 = float4( VtxIn.Uv0.xy, 0.0f, 0.0f );
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VtxOut VSTex( UvVertex VtxIn )
{
    VtxOut FragOut;
    
    float2 outuv = mul( float4( VtxIn.Uv0.xy,0.0f,1.0f),MatAux).xy;
    
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.UV0.x = outuv.x;
    FragOut.UV0.y = outuv.y;
    FragOut.UV0.z = 0.0f;
    FragOut.UV0.w = 1.0f;
    FragOut.Color = VtxIn.Color;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

VtxOut VSTexMod( UvVertex VtxIn )
{
    VtxOut FragOut;
    
    float2 outuv = mul( float4( VtxIn.Uv0.xy,0.0f,1.0f),MatAux).xy;
    
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.UV0.x = outuv.x;
    FragOut.UV0.y = outuv.y;
    FragOut.UV0.z = 0.0f;
    FragOut.UV0.w = 1.0f;
   // FragOut.Color = float4(1.0f,1.0f,1.0f,1.0f); //VtxIn.Color*modcolor;
    FragOut.Color = VtxIn.Color;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSTexColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    //PixOut = float4( FragIn.UV0.xy, 0.0f, 1.0f );
    //PixOut = float4( FragIn.UV0.xy, 0.0f, 1.0f );
    PixOut = float4( tex2D( ColorMapSampler, FragIn.UV0.xy ).xyzw ); // * FragIn.Color;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_proctex_colorize( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
	float3 clra = tex2D( ColorMapPointSampler, FragIn.UV0.xy ).xyz;
	float3 clrb = tex2D( ColorMap2Sampler, FragIn.UV0.xy ).xyz;
	
    PixOut = float4( clra*clrb, 1.0f );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSTexVtxColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    PixOut = tex2D( ColorMapSampler, FragIn.UV0.xy ).xyzw*FragIn.Color.xyzw;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSTexFragColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    float famp = FragIn.UV0.z;
    PixOut = float4( tex2D( ColorMapPointSampler, FragIn.UV0.xy ).xyz, 1.0f ) * famp;
    return PixOut;
}

float4 PSTexModColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
	PixOut = float4( tex2D( ColorMapPointSampler, FragIn.UV0.xy ).xyz*FragIn.Color.xyz, 1.0f );
    return PixOut;
}

float4 PSTexModColorFB( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
 	float t0 = tex2D( ColorMapPointSampler, FragIn.UV0.xy ).x;
    PixOut = float4( FragIn.Color.xyz, t0*FragIn.Color.w );
    //PixOut = float4( 0.0f,0.0f,0.0f, 0.9999f ); //t0*FragIn.Color.w );
    //PixOut = float4( FragIn.Color.xyz, FragIn.Color.w );
    return PixOut;
}

float4 PSTexTexModColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    // original modulated by (1.0f-(t1*fw))
    float4 t0 = tex2D( ColorMapPointSampler, FragIn.UV0.xy );			// previous frame
    //float t1 = 1.0f; //tex2D( ColorMap2Sampler, FragIn.UV0.xy ).x;		// radial (white==more previous)
    float t1 = tex2D( ColorMap2Sampler, FragIn.UV0.xy ).x;		// radial (white==more previous)
	//t1 = pow(abs(t1*FragIn.Color.x),0.5f)*1.41;
	t1 *= FragIn.Color.x;
    PixOut = float4( t0.xyz*t1, 1.0f );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

technique vtxcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSVtxColor();
        PixelShader = compile ps_3_0 PSFragColor();
    }
};
technique checker3d
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSChecker3d();
        PixelShader = compile ps_3_0 PSChecker3d();
    }
};

technique vtxmodcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSVtxModColor();
        PixelShader = compile ps_3_0 PSFragColor();
    }
};
technique mmodcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSModColor();
        PixelShader = compile ps_3_0 PSFragColor();
	}
};

technique texcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTex();
        PixelShader = compile ps_3_0 PSTexColor();
	}
};
technique texcolorwrap
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTex();
        PixelShader = compile ps_3_0 PSTexColor();
		AddressU[0] = WRAP;
		AddressV[0] = WRAP;
	}
};

technique texmodcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTexMod();
        PixelShader = compile ps_3_0 PSTexModColor();
	}
};
technique texmodcolorFB
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTexMod();
        PixelShader = compile ps_3_0 PSTexModColorFB();
	}
};
technique textexmodcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTexMod();
        PixelShader = compile ps_3_0 PSTexTexModColor();
	}
};

technique texvtxcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTex();
        PixelShader = compile ps_3_0 PSTexVtxColor();
	}
};

technique particle
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTex();
        PixelShader = compile ps_3_0 PSTexVtxColor();
        //CullMode = NONE; 
        AddressU[0] = CLAMP;
        AddressV[0] = CLAMP;
	}
};

technique proctex_colorize
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTex();
        PixelShader = compile ps_3_0 ps_proctex_colorize();
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// 512x512->512x512

VtxOut vs_distorted_feedback( UvVertex VtxIn )
{
    VtxOut FragOut;
    
	const float kftexX = 1.0f/512.0f;
	const float kftexY = 0.0f/512.0f;
    float2 outuv = mul( float4( VtxIn.Uv0.xy,0.0f,1.0f),MatAux).xy;
    
	FragOut.ClipPos = TransformVertex( VtxIn.Position );
    FragOut.UV0.x = outuv.x+kftexX;
    FragOut.UV0.y = outuv.y+kftexY;
    FragOut.UV0.z = 0.0f;
    FragOut.UV0.w = 1.0f;
    FragOut.Color = modcolor;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}
float4 ps_distorted_feedback( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    float3 uv0 = tex2D( ColorMap3Sampler, FragIn.UV0.xy ).xyz;
    //float3 t0 = tex2D( ColorMapSampler, uv0.zy ).xyz;
	float3 t0 = tex2D( ColorMapSampler, FragIn.UV0.xy ).xyz;
    PixOut = float4( t0.xyz*FragIn.Color.xyz, 1.0f );
    //PixOut = float4( 0.0f, t0.yz,1.0f );
    return PixOut;
}

technique distortedfeedback
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_distorted_feedback();
        PixelShader = compile ps_3_0 ps_distorted_feedback();
    }
};
