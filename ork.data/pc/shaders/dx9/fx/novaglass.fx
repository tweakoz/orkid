#include "tanspace.fxi"
#include "lighting_common.fxi"

///////////////////////////////////////////////////////////////////////////////

uniform float3			BaseColor;
uniform float			RenderSort;
uniform float			TexScroll;
uniform float			time : reltime;

uniform float BaseTrans;

uniform const float khlamb = 0.8f;
uniform const float khilamb = 0.2f;

///////////////////////////////////////////////////////////////////////////////

uniform texture2D SamplerCol; //ColorMap;
uniform texture2D SamplerRef; //ReflectionMap;

uniform sampler2D SamplerColSampler = sampler_state
{
	Texture = <SamplerCol>;
    MagFilter = Linear;
    MinFilter = Anisotropic;
    MipFilter = Linear;
    AddressU = WRAP;
    AddressV = WRAP;
	MaxAnisotropy = 4.0f;
};

uniform sampler2D SamplerRefSampler = sampler_state
{
	Texture = <SamplerRef>;
    MagFilter = Linear;
    MinFilter = Anisotropic;
    MipFilter = Linear;
    AddressU = WRAP;
    AddressV = WRAP;
	MaxAnisotropy = 4.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexNrmTex
{
    float3 Position             : POSITION;
    float2 Uv0                  : TEXCOORD0;
    float3 Normal               : NORMAL;
    float4 Color				: COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexNrmTex2
{
    float3 Position             : POSITION;
    float2 Uv0                  : TEXCOORD0;
    float2 Uv1                  : TEXCOORD1;
    float3 Normal               : NORMAL;
    float4 Color				: COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexNrmTexSkinned
{
    float3 Position             : POSITION;
    float2 Uv0                  : TEXCOORD0;
    float3 Normal               : NORMAL;
    int4   BoneIndices			: BLENDINDICES;
    float4 BoneWeights			: BLENDWEIGHT;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
    float4 ClipPos		: Position;
    float3 Color		: Color;
    float2 Uv0			: TEXCOORD0;
    float2 Uv1			: TEXCOORD1;
    float2 Uv2			: TEXCOORD2;
    float4 ClipUserPos	: TEXCOORD3;
};

///////////////////////////////////////////////////////////////////////////////

VtxOut vs_shiny( VertexNrmTex VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), View)).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb+khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = VtxIn.Color.xyz*headlight;//VtxIn.Color.xyz*ModColor.xyz*headlight;
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = float2(0.0f,0.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;
	
	return FragOut;
}


VtxOut vs_shiny_vtxlit( VertexNrmTex VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), View)).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb+khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
	float3 LightColor = ProcPreVtxLitColor(VtxIn.Color.xyz)*headlight;
	
////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = LightColor*ModColor.xyz;
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = float2(0.0f,0.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;
	
	return FragOut;
}


VtxOut vs_diffuse( VertexNrmTex VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), View)).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb+khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
	float3 LightColor = float3(headlight,headlight,headlight);
	
////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = LightColor;//*ModColor.xyz;
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = float2(0.0f,0.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;
	
	return FragOut;
}

VtxOut vs_diffuse_vtxlit( VertexNrmTex VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul( ObjectNormal, WorldRot3 );
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), View)).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb+khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
	float3 LightColor = ProcPreVtxLitColor(VtxIn.Color.xyz)*headlight;
	
////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = LightColor*ModColor.xyz;
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = float2(0.0f,0.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;
	
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut vs_shiny_lightmapped( VertexNrmTex2 VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = normalize(mul( ObjectNormal, WorldRot3 ));
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), ViewInverseTranspose)).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb + khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = ModColor*headlight;
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = VtxIn.Uv1*float2(1.0f,-1.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;
	
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut vs_shiny_skinned(VertexNrmTexSkinned VtxIn)
{
	//float3 ObjectPos = VtxIn.Position;
	//float3 ObjectNormal = VtxIn.Normal;
	float3 ObjectPos = SkinPosition(VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Position);
	float3 ObjectNormal = SkinNormal(VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Normal);

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = mul(ObjectNormal, WorldRot3);
	float3 ViewNormal = float3(0.0f, 0.0f, -1.0f); //normalize(mul(float4(WorldNormal, 0.0f), transpose(ViewInverseTranspose))).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = float4(ModColor.xyz* headlight, 1.0f);
	FragOut.Uv0 = VtxIn.Uv0;
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = float2(0.0f,0.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;

	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_modcolor(VtxOut FragIn) : COLOR
{
    float4 PixOut;
    PixOut = ModColor;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_vtxcolor(VtxOut FragIn) : COLOR
{
    float4 PixOut = float4(FragIn.Color.xyz,1.0f);
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_testuv(VtxOut FragIn) : COLOR
{
	float2 in_uv = FragIn.Uv0.xy;
    float4 PixOut;
    PixOut = float4(in_uv, 0.0f, 0.5f);
    return PixOut;
}

///////////////////////////////////////////////////////////

float4 ps_diffuse(VtxOut FragIn)	: COLOR
{
	float4 texCol = tex2D( SamplerColSampler, FragIn.Uv0.xy );
	float3 refCol = tex2D( SamplerRefSampler, FragIn.Uv1.xy ).xyz;
	float outa = texCol.a*BaseTrans;
	float3 myout = (texCol.xyz*BaseColor);//*FragIn.Color.xyz);
	
	//float3 myout = float3(0.0f,1.0f,0.0f);
	return float4(myout,outa);
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_shiny(VtxOut FragIn) : COLOR
{	
	float4 texCol = tex2D(SamplerColSampler, FragIn.Uv0.xy);
	
	float gloss = texCol.w;
	
	float4 texRef = tex2D(SamplerRefSampler, FragIn.Uv1.xy);
	
	float3 diffuse = (texCol.xyz*BaseColor.xyz);//*FragIn.Color.xyz);
	float3 specular = (texRef.xyz* gloss)* BaseTrans;
	
    float4 PixOut = float4((diffuse + specular), 1.0f);
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_shiny_lightmapped(VtxOut FragIn) : COLOR
{	
	float4 texCol = tex2D(SamplerColSampler, FragIn.Uv0.xy);
	float3 lmpCol = GetLightmap(FragIn.Uv2.xy);	
	
	float gloss = texCol.w;
	
	float4 texRef = tex2D(SamplerRefSampler, FragIn.Uv1.xy);
	
	float3 diffuse = (texCol.xyz * BaseColor.xyz*FragIn.Color.xyz*lmpCol.xyz);
	float3 specular = (texRef.xyz * gloss * BaseTrans);
	
    float4 PixOut = float4((diffuse + specular), 1.0f);
    //float4 PixOut = float4(lmpCol.xyz, 1.0f);
    
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////


VtxOut vs_shiny_lightpreview( VertexNrmTex2 VtxIn )
{
	float3 ObjectPos = VtxIn.Position;
	float3 ObjectNormal = VtxIn.Normal;

	float4 WorldPos = mul(float4(ObjectPos.xyz, 1.0f), World);
	float3 WorldNormal = normalize(mul( ObjectNormal, WorldRot3 ));
	float3 ViewNormal = normalize(mul(float4(WorldNormal, 0.0f), ViewInverseTranspose)).xyz;

////////////////////////////////////////////

	float3 WorldEyePos = GetEyePos();
	float3 WorldEyeDir = normalize(WorldPos - WorldEyePos);
	float3 WR = normalize(reflect(WorldEyeDir, WorldNormal));

	float3 uvt = (WR.xyz * 0.5f) + float3(0.5f, 0.5f, 0.5f);

////////////////////////////////////////////

	float headlight = khlamb + khilamb*saturate(dot(ViewNormal, float3(0.0f, 0.0f, -1.0f)));
	
	float3 dlight = PreviewLightSaturate(DiffuseLight( WorldPos.xyz, WorldNormal ));

////////////////////////////////////////////

	VtxOut FragOut;

    FragOut.ClipPos = mul(WorldPos, ViewProjection);
	FragOut.Color = dlight*headlight;
	FragOut.Uv0 = VtxIn.Uv0*float2(1.0f,1.0f);
	FragOut.Uv1 = uvt.xz;
	FragOut.Uv2 = VtxIn.Uv1*float2(1.0f,-1.0f);
	FragOut.ClipUserPos = FragOut.ClipPos;
	
	return FragOut;
}
float4 ps_shiny_lightpreview(VtxOut FragIn) : COLOR
{	
	float4 texCol = tex2D(SamplerColSampler, FragIn.Uv0.xy);
	float4 lmpCol = float4(FragIn.Color.xyz,1.0f);
	float3 lmpSpc = float3(0.2f,0.2f,0.2f)+lmpCol.xyz*0.8f;
	
	float gloss = texCol.w;
	
	float4 texRef = tex2D(SamplerRefSampler, FragIn.Uv1.xy);
	
	float3 diffuse = (texCol.xyz * BaseColor.xyz*FragIn.Color.xyz*lmpCol.xyz);
	float3 specular = (texRef.xyz * gloss * BaseTrans*lmpSpc);
	
    float4 PixOut = float4((diffuse + specular), 1.0f);
    //float4 PixOut = float4(lmpCol.xyz, 1.0f);
    //float4 PixOut = float4(FragIn.Color.xyz, 1.0f);
    //float4 PixOut = float4(lmpCol.xyz*FragIn.Color.xyz, 1.0f);
    
    return PixOut;

}

///////////////////////////////////////////////////////////

float4 ps_sep_glass(VtxOut FragIn)	: COLOR
{
	float2 uv0 = FragIn.Uv0;
	float2 uv1 = FragIn.Uv1;
	float4 refCol = tex2D( SamplerRefSampler, uv1 );
	float4 refAlp = tex2D( SamplerRefSampler, uv0 );
	
	float gloss = refAlp.a;
	
	float3 myout = (refCol.xyz*gloss);
	
	return float4(myout,1.0f);
}

float4 ps_sep_glass_mod(VtxOut FragIn)	: COLOR
{
	float2 uv0 = FragIn.Uv0;
	float2 uv1 = FragIn.Uv1;
	float4 refCol = tex2D( SamplerRefSampler, uv1 );
	float4 refAlp = tex2D( SamplerRefSampler, uv0 );
	
	float gloss = refAlp.a;
	
	//float3 myout = (refCol.xyz*FragIn.Color.xyz*gloss);
	float3 myout = (refCol.xyz*gloss);
	
	return float4(myout,1.0f);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

technique main
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_shiny();
        PixelShader = compile ps_3_0 ps_shiny();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};
technique translucent
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_diffuse();
        PixelShader = compile ps_3_0 ps_diffuse();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
    pass p1
    {
        VertexShader = compile vs_3_0 vs_shiny();
        PixelShader = compile ps_3_0 ps_sep_glass();
        SrcBlend = ONE;
        DestBlend = ONE;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
		ZWriteEnable = true;
		ZFunc = LESSEQUAL;
		ZEnable = TRUE;
	}
};
technique mainLightMapped
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_shiny_lightmapped();
        PixelShader = compile ps_3_0 ps_shiny_lightmapped();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};
technique translucentVertexLit
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_diffuse_vtxlit();
        PixelShader = compile ps_3_0 ps_diffuse();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
    pass p1
    {
        VertexShader = compile vs_3_0 vs_shiny_vtxlit();
        PixelShader = compile ps_3_0 ps_sep_glass_mod();
        SrcBlend = ONE;
        DestBlend = ONE;
		AlphaBlendEnable = true;
		AlphaTestEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};
technique mainVertexLit
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_shiny_vtxlit();
        PixelShader = compile ps_3_0 ps_shiny();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};
technique mainLightPreview
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_shiny_lightpreview();
        PixelShader = compile ps_3_0 ps_shiny_lightpreview();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};

technique mainSkinned
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_shiny_skinned();
        PixelShader = compile ps_3_0 ps_shiny();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};

///////////////////////////////////////////////////////////////////////////////

#include "pick.fxi"
