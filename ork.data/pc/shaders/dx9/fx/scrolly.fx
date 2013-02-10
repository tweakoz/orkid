
#include "tanspace.fxi"

uniform float4          ModColor;
uniform float2			uvscroll;
uniform float2			uvscale;
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
    float4 Color		: COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexSkinned
{
    float4 Position     : POSITION;
    float4 TexCoord     : TEXCOORD0;
    float3 Normal       : NORMAL;
    float3 Binormal		: BINORMAL;
    int4   BoneIndices	: BLENDINDICES;
    float4 BoneWeights	: BLENDWEIGHT;
    float4 Color		: COLOR0;
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

VtxOut vs_scrollontime_skinned( VertexSkinned VtxIn )
{
    float3 ObjectPos = SkinPosition( VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Position );

	VtxOut FragOut;
	FragOut.ClipPos = TransformVertex(float4(ObjectPos, 1.0f));
    FragOut.Color = float4(1.0, 1.0f, 1.0f, 1.0f);
	FragOut.Uv0 = (VtxIn.TexCoord * uvscale) + uvscroll * time;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

VtxOut vs_scrollontime( SimpleVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex(VtxIn.Position);
    FragOut.Color = VtxIn.Color;
	FragOut.Uv0 = (VtxIn.Uv0 * uvscale) + uvscroll * time;
    FragOut.ClipUserPos = FragOut.ClipPos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut vs_scrollonengine( SimpleVertex VtxIn )
{
    VtxOut FragOut;
	FragOut.ClipPos = TransformVertex(VtxIn.Position);
    FragOut.Color = VtxIn.Color;
	FragOut.Uv0 = (VtxIn.Uv0 * uvscale) + uvscroll * engine_float_0;
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

float4 ps_vtxtexcolor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;

   	float4 tex1 = tex2D( Sampler1_2D, FragIn.Uv0 ).xyzw;
	
	PixOut = tex1.xyzw*FragIn.Color.xyzw;
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

technique time_scroll
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_scrollontime();
        PixelShader = compile ps_3_0 ps_texcolor();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
	}
};
technique time_scroll_vtxcol {
	pass p0 {
        VertexShader = compile vs_3_0 vs_scrollontime();
        PixelShader = compile ps_3_0 ps_vtxtexcolor();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
	}
}

technique time_scrollSkinned
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_scrollontime_skinned();
        PixelShader = compile ps_3_0 ps_texcolor();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
	}
};

technique engine_scroll
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_scrollonengine();
        PixelShader = compile ps_3_0 ps_texcolor();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
	}
};

#include "pick.fxi"
