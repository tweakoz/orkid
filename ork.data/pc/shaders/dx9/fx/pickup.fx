#include "tanspace.fxi"
#include "lighting_common.fxi"

uniform float2			uvscroll;
uniform float2			uvscale;
uniform float2			uvshift;
uniform float			time : reltimemod300;

uniform float			engine_float_0 : engine_float_0;

///////////////////////////////////////////////////////////////////////////////

uniform texture2D		Sampler1;

sampler2D Sampler1_2D = sampler_state
{
    Texture = <Sampler1>;
    MagFilter = Linear;
    MinFilter = Linear;
    MipFilter = Linear;
    AddressU = WRAP;
    AddressV = WRAP;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
    float4 Position     : POSITION;
    float2 Uv0			: TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
    float4 ClipPos		: Position;
    float4 Color		: Color;
    float2 Uv0			: TEXCOORD1;
    float4 ClipUserPos	: TEXCOORD2;
};

///////////////////////////////////////////////////////////////////////////////

float4 TransformVertex( float4 inv )
{
	float4 hpos = mul( float4( inv.xyz, 1.0f ), WorldViewProjection );
	return hpos;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut vs_pickup( SimpleVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex(VtxIn.Position);
    FragOut.Color = float4(1.0, 1.0f, 1.0f, 1.0f);
	float2 shift = uvshift * float(engine_float_0 < 1.0f);
    FragOut.Uv0 = shift + (VtxIn.Uv0 * uvscale) + (uvscroll*time);
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_texcolor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
   	float4 tex1 = tex2D( Sampler1_2D, FragIn.Uv0 ).xyzw;
	PixOut = tex1.xyzw;
	return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_modcolor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    PixOut = ModColor;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

technique pickup
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_pickup();
        PixelShader = compile ps_3_0 ps_texcolor();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};

#include "pick.fxi"
