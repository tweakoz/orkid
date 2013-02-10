///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "tanspace.fxi"
#include "lighting_common.fxi"
uniform const float khlamb = 0.8f;
uniform const float khilamb = 0.2f;

////////////////////////////////////////////////////////////////////////////////

uniform float2 clr_uvscale;
uniform float2 clr_uvbias;
uniform float2 det_uvscale;
uniform float2 det_uvbias;

////////////////////////////////////////////////////////////////////////////////

uniform texture2D ColorMap<>;
uniform texture2D DetailMap<>;

////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float3 Position : POSITION;   // in object space
	float3 Normal : NORMAL;
	float2 Uv0 : TEXCOORD0;
};
struct Vertex_pnctt
{
    float3 Position	: POSITION;
    float2 Uv0      : TEXCOORD0;
    float2 Uv1      : TEXCOORD1;
    float3 Normal	: NORMAL;
    float4 Color	: COLOR0;
};

struct Fragment
{
	float4 ClipPos : POSITION;  // in clip space
	float2 Uv0 : TEXCOORD0;
	float2 Uv1 : TEXCOORD1;
	float4 Color : TEXCOORD4;
};

////////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
    MagFilter = Linear;
    MinFilter = Anisotropic;
    MipFilter = Linear;
};
sampler2D DetailMapSampler = sampler_state
{
	Texture = <DetailMap>;
    MagFilter = Linear;
    MinFilter = Anisotropic;
    MipFilter = Linear;
};

////////////////////////////////////////////////////////////////////////////////

Fragment vs_simple( Vertex VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), transpose(ViewInverseTranspose))).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb+khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
	float3 LightColor = float3(headlight,headlight,headlight);
	
////////////////////////////////////////////

	Fragment FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = float4(LightColor*ModColor.xyz,1.0f);
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;

	return FragOut;
}


///////////////////////////////////////////////////////////////////////////////

Fragment vs_layered_det(Vertex_pnctt VtxIn)
{
	Fragment FragOut;

	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), transpose(ViewInverseTranspose))).xyz;

////////////////////////////////////////////
	
	float headlight = 0.4f+saturate(dot(ViewNormal, float3(0.0f, 0.0f, 1.0f)))*0.6f;

////////////////////////////////////////////

	//FragOut.Uv0 = VtxIn.Uv0;	
	//FragOut.Uv1 = VtxIn.Uv0;	
	FragOut.Uv0 = (clr_uvbias+(VtxIn.Uv0 * clr_uvscale));
	FragOut.Uv1 = (det_uvbias+(VtxIn.Uv0 * det_uvscale));	
	FragOut.Color = float4(VtxIn.Color.xyz * headlight,VtxIn.Color.w);
	
    float4 outp = mul(WorldPos, ViewProjection);
    FragOut.ClipPos = outp;

	return FragOut;
}

Fragment vs_layered_det_vtxlit(Vertex_pnctt VtxIn)
{
	Fragment FragOut;

	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), transpose(ViewInverseTranspose))).xyz;

////////////////////////////////////////////
	
	float headlight = 0.9f+saturate(dot(ViewNormal, float3(0.0f, 0.0f, 1.0f)))*0.1f;

////////////////////////////////////////////

	FragOut.Uv0 = (clr_uvbias+(VtxIn.Uv0 * clr_uvscale));
	FragOut.Uv1 = (det_uvbias+(VtxIn.Uv0 * det_uvscale));	
	FragOut.Color = float4(VtxIn.Color.xyz,VtxIn.Color.w);
	
    float4 outp = mul(WorldPos, ViewProjection);
    FragOut.ClipPos = outp;

	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_simple( Fragment FragIn ) : COLOR
{
	float2 UvA = FragIn.Uv0;
	float4 outp = tex2D( ColorMapSampler , UvA ).xyzw;
	return outp;
}


float4 ps_blendvtx(Fragment FragIn) : COLOR
{
	float3 amap = tex2D(ColorMapSampler, FragIn.Uv0).xyz;
	float3 bmap = tex2D(DetailMapSampler, FragIn.Uv1).xyz;
	float  blend = FragIn.Color.w;
	float3 blended = lerp( amap, bmap, blend );
	
	return float4(blended*FragIn.Color.xyz,1.0f);
}


float4 ps_blendvtx2(Fragment FragIn) : COLOR
{
	float3 amap = tex2D(ColorMapSampler, FragIn.Uv0).xyz;
	float3 bmap = tex2D(DetailMapSampler, FragIn.Uv1).xyz;
	float  blend = FragIn.Color.w;
	float3 blended = lerp( amap*FragIn.Color.xyz, bmap, blend );
	
	return float4(blended,1.0f);
}


float4 ps_texvtxdetail(Fragment FragIn) : COLOR
{
	float4 dmap = tex2D(DetailMapSampler, FragIn.Uv1);
	float3 cmap = tex2D(ColorMapSampler, FragIn.Uv0).xyz;
	float  dmap_blend = FragIn.Color.w*dmap.w;
	float3 dmapl = lerp( float3(1.0f,1.0f,1.0f), dmap.xyz, dmap_blend );
	
	return float4(cmap*dmapl*FragIn.Color.xyz,1.0f);
}
///////////////////////////////////////////////////////////////////////////////

technique LayeredVtxABModulate {
	pass p0 {
		VertexShader = compile vs_3_0 vs_layered_det();
		PixelShader = compile ps_3_0 ps_texvtxdetail();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}
technique LayeredVtxABBlend2 {
	pass p0 {
		VertexShader = compile vs_3_0 vs_layered_det();
		PixelShader = compile ps_3_0 ps_blendvtx2();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique LayeredVtxABModulateVertexLit {
	pass p0 {
		VertexShader = compile vs_3_0 vs_layered_det_vtxlit();
		PixelShader = compile ps_3_0 ps_texvtxdetail();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}
technique LayeredVtxABBlend2VertexLit {
	pass p0 {
		VertexShader = compile vs_3_0 vs_layered_det_vtxlit();
		PixelShader = compile ps_3_0 ps_blendvtx2();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique LayeredVtxABBlend
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_layered_det();
		PixelShader = compile ps_3_0 ps_blendvtx();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}
technique LayeredVtxABBlendVertexLit
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_layered_det_vtxlit();
		PixelShader = compile ps_3_0 ps_blendvtx();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

///////////////////////////////////////////////////////////////////////////////

#include "pick.fxi"
