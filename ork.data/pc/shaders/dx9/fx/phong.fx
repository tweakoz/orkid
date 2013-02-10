///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "tanspace.fxi"
#include "lighting_common.fxi"

////////////////////////////////////////////////////////////////////////////////

uniform float		UvScaleClr;
uniform float		UvScaleNrm;
uniform float		UvScaleEmi;
uniform float		UvScaleSpc;
uniform float		BumpLevA;
uniform float		EmissiveMix;
uniform float		DiffuseMix;
uniform float		SpecularMix;
uniform float		Shininess;
uniform float		DotBiasDiffuse;
uniform float		DotScaleDiffuse;
uniform float		DotBiasSpecular;
uniform float		DotScaleSpecular;

uniform texture2D ColorMap<>;
uniform texture2D EmissiveMap<>;
uniform texture2D NormalMap<>;
uniform texture2D SpecuMap<>;

////////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
  	MagFilter = Linear;
  	MinFilter = Anisotropic;
    MipFilter = Linear;
};
sampler2D EmissiveMapSampler = sampler_state
{
	Texture = <EmissiveMap>;
  	MagFilter = Linear;
  	MinFilter = Anisotropic;
    MipFilter = Linear;
};
sampler2D NormalMapSampler = sampler_state
{
	Texture = <NormalMap>;
  	MagFilter = Linear;
  	MinFilter = Anisotropic;
    MipFilter = Linear;
};
sampler2D SpecuMapSampler = sampler_state
{
	Texture = <SpecuMap>;
  	MagFilter = Linear;
  	MinFilter = Anisotropic;
    MipFilter = Linear;
};

////////////////////////////////////////////////////////////////////////////////

struct Vertex
{	float4 Position : POSITION;   // in object space
	float4 TexCoord : TEXCOORD0;
	float3 Normal   : NORMAL;
	float3 Binormal : BINORMAL;
};
struct VertexSkinned
{
    float4 Position     : POSITION;
    float4 TexCoord     : TEXCOORD0;
    float3 Normal       : NORMAL;
    float3 Binormal		: BINORMAL;
    int4   BoneIndices	: BLENDINDICES;
    float4 BoneWeights	: BLENDWEIGHT;
};
struct VertexWithCol
{	float4 Position : POSITION;   // in object space
	float4 TexCoord : TEXCOORD0;
	float3 Normal   : NORMAL;
	float3 Binormal : TEXCOORD1;
	float4 Color    : COLOR0;
};
struct Fragment
{	float4 ClipPos			: POSITION;  // in clip space
	float4 Color			: COLOR;
	float3 UVW0             : TEXCOORD0;
	float3 Diffuse		    : TEXCOORD1;
	float3 Specular		    : TEXCOORD2;

};
struct FragmentPP
{	float4 ClipPos			: POSITION;  // in clip space
	float4 Color			: COLOR;
	float3 UVW0             : TEXCOORD0;
	float3 WorldPos			: TEXCOORD1;
	float3 WorldNormal		: TEXCOORD2;
};
struct FragmentN
{	float4 ClipPos : POSITION;  // in clip space
	float4 Color : COLOR;
	float3 UVW0             : TEXCOORD0;
	float3 TanLight         : TEXCOORD1;
	float3 TanHalf          : TEXCOORD2;
	float3 TanEye           : TEXCOORD3;
	float3 VtxTangent       : TEXCOORD4;
	float3 VtxPosition      : TEXCOORD5;
};
struct phongdatainp
{	float4 Position;
	float4 TexCoord;
	float3 Normal;
	float3 Binormal;
	float4 Color;
};

////////////////////////////////////////////////////////////////////////////////

Fragment vs_phong_common( phongdatainp VtxIn )
{
    Fragment pxout;
	
	//time = fmod( time, 10000 );

    //////////////////////////////////////

    float3 camPos = mul(VtxIn.Position,WorldView ).xyz;
    float3 worldPos = mul(VtxIn.Position,World).xyz;
                
    //////////////////////////////////////
        
    float3 worldNormal  = normalize( mul( float4( VtxIn.Normal,0.0f ), World ).xyz );
    float3 worldEye     = GetEyePos();
    float3 worldToEye   = normalize( worldEye - worldPos );
    float3 worldToLyt   = worldToEye;

	float4 DotScaleBias = float4( DotBiasDiffuse, DotScaleDiffuse, DotBiasSpecular, DotScaleSpecular );

	DifAndSpec das = DiffuseSpecularLight( worldPos, worldNormal, worldEye, Shininess, DotScaleBias );
	
	pxout.Diffuse = das.Diffuse*DiffuseMix;
	pxout.Specular = das.Specular*SpecularMix;
	
    /////////////////////////////////////
    pxout.UVW0.xyz = float3( VtxIn.TexCoord.xy, 0.0f );
    /////////////////////////////////////

    pxout.ClipPos = mul(VtxIn.Position,WorldViewProjection);
	pxout.Color = VtxIn.Color;
	
    return pxout;
}

FragmentPP vs_phong_commonpp( phongdatainp VtxIn )
{
    FragmentPP pxout;
	
    //////////////////////////////////////

    float3 camPos = mul(VtxIn.Position,WorldView ).xyz;
    float3 worldPos = mul(VtxIn.Position,World).xyz;        
    float3 worldNormal  = normalize( mul( float4( VtxIn.Normal,0.0f ),World ).xyz );
    float3 worldEye     = GetEyePos();
	
    /////////////////////////////////////

    pxout.ClipPos = mul(VtxIn.Position,WorldViewProjection);
	pxout.Color = VtxIn.Color;
	pxout.WorldNormal = worldNormal;
	pxout.WorldPos = worldPos;
    pxout.UVW0.xyz = float3( VtxIn.TexCoord.xy, 0.0f );
	
    return pxout;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Fragment vs_gouraud( Vertex VtxIn )
{
	phongdatainp pxinp;
	pxinp.Position = VtxIn.Position;
	pxinp.TexCoord = float4(VtxIn.TexCoord.x, VtxIn.TexCoord.y, 0.0f, 0.0f);
	pxinp.Normal = VtxIn.Normal;
	pxinp.Binormal = VtxIn.Binormal;
	pxinp.Color = float4(1.0f,1.0f,1.0f,1.0f);
	return vs_phong_common( pxinp );
}
Fragment vs_phongV( VertexWithCol VtxIn )
{
	phongdatainp pxinp;
	pxinp.Position = VtxIn.Position;
	pxinp.TexCoord = float4(VtxIn.TexCoord.x, VtxIn.TexCoord.y, 0.0f, 0.0f);
	pxinp.Normal = VtxIn.Normal;
	pxinp.Binormal = VtxIn.Binormal;
	pxinp.Color = VtxIn.Color.xyzw;
	return vs_phong_common( pxinp );
}    
FragmentPP vs_phongpp( Vertex VtxIn )
{
	phongdatainp pxinp;
	pxinp.Position = VtxIn.Position;
	pxinp.TexCoord = float4(VtxIn.TexCoord.x, VtxIn.TexCoord.y, 0.0f, 0.0f);
	pxinp.Normal = VtxIn.Normal;
	pxinp.Binormal = VtxIn.Binormal;
	pxinp.Color = float4(1.0f,1.0f,1.0f,1.0f);
	return vs_phong_commonpp( pxinp );
}

FragmentPP vs_phongpp_skinned( VertexSkinned VtxIn )
{
	float3 ObjectPos = SkinPosition4( VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Position );
	float3 ObjectNormal = SkinNormal4( VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Normal );
	float3 ObjectBinormal = SkinNormal4( VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Binormal );

	phongdatainp pxinp;
	pxinp.Position = float4( ObjectPos, 1.0f );
	pxinp.TexCoord = float4(VtxIn.TexCoord.x, VtxIn.TexCoord.y, 0.0f, 0.0f);
	pxinp.Normal = ObjectNormal;
	pxinp.Binormal = ObjectBinormal;
	pxinp.Color = float4(1.0f,1.0f,1.0f,1.0f);
	return vs_phong_commonpp( pxinp );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

float4 ps_phongpp( FragmentPP FragIn ) : COLOR
{	const float3 FlatNormal = float3( 0.5f, 0.5f, 1.0f );
	/////////////////////////
	float2 UvC = FragIn.UVW0.xy*UvScaleClr;
	float2 UvS = FragIn.UVW0.xy*UvScaleSpc;
	/////////////////////////
	// Get Other Maps
	float4 cmap = tex2D( ColorMapSampler , UvC );
	float3 smap = tex2D( SpecuMapSampler , UvS ).xyz;
	/////////////////////////
	float3 WorldEye = GetEyePos();
	float3 WorldNormal = normalize(FragIn.WorldNormal);
	float4 DotScaleBias = float4( DotBiasDiffuse, DotScaleDiffuse, DotBiasSpecular, DotScaleSpecular );
	DifAndSpec das = DiffuseSpecularLight( FragIn.WorldPos, WorldNormal, WorldEye, Shininess, DotScaleBias );
	/////////////////////////
	float3 DC = cmap.xyz*FragIn.Color.xyz*das.Diffuse*DiffuseMix;
	float3 SC = smap*das.Specular*SpecularMix;
	/////////////////////////
	//float4 mrtout = float4( DC+SC, cmap.w );
	float4 mrtout = float4( 1.0f,0.0f,1.0f, cmap.w );
    return mrtout;
}

float4 ps_gouraud( Fragment FragIn ) : COLOR
{
	const float3 FlatNormal = float3( 0.5f, 0.5f, 1.0f );
	/////////////////////////
	float2 UvC = FragIn.UVW0.xy*UvScaleClr;
	float2 UvS = FragIn.UVW0.xy*UvScaleSpc;
	/////////////////////////
	// Get Other Maps
	float4 cmap = tex2D( ColorMapSampler , UvC );
	float3 smap = tex2D( SpecuMapSampler , UvS ).xyz;
	////////////////////////
	float3 DC = cmap.xyz*FragIn.Diffuse*FragIn.Color.xyz;
	float3 SC = smap*FragIn.Specular;
	/////////////////////////
	float4 mrtout = float4( DC+SC, cmap.w );
    return mrtout;
}

float4 ps_phongppE( FragmentPP FragIn ) : COLOR
{	/////////////////////////
	float2 UvC = FragIn.UVW0.xy*UvScaleClr;
	float2 UvS = FragIn.UVW0.xy*UvScaleSpc;
	float2 UvE = FragIn.UVW0.xy*UvScaleEmi;
	/////////////////////////
	// Get Other Maps
	float4 cmap = tex2D( ColorMapSampler , UvC );
	float3 smap = tex2D( SpecuMapSampler , UvS ).xyz;
	float3 emap = tex2D( EmissiveMapSampler , UvE ).xyz;
	/////////////////////////
	float3 WorldEye = GetEyePos();
	float3 WorldNormal = normalize(FragIn.WorldNormal);
	//float4 DotScaleBias = float4( DotBiasDiffuse, DotScaleDiffuse, DotBiasSpecular, DotScaleSpecular );
	float4 DotScaleBias = float4( 0, 0, 0, 1 );
	DifAndSpec das = DiffuseSpecularLight( FragIn.WorldPos, WorldNormal, WorldEye, Shininess, DotScaleBias );
	/////////////////////////
	float3 DC = cmap.xyz*FragIn.Color.xyz*das.Diffuse*DiffuseMix;
	float3 SC = smap*das.Specular*SpecularMix;
	float3 EC = emap*EmissiveMix;
	/////////////////////////
	float4 mrtout = float4( EC+DC+SC, cmap.w );
    return mrtout;
}


float4 ps_gouraudE( Fragment FragIn ) : COLOR
{
	/////////////////////////
	float2 UvC = FragIn.UVW0.xy*UvScaleClr;
	float2 UvS = FragIn.UVW0.xy*UvScaleSpc;
	float2 UvE = FragIn.UVW0.xy*UvScaleEmi;
	/////////////////////////
	// Get Other Maps
	float4 cmap = tex2D( ColorMapSampler , UvC );
	float3 smap = tex2D( SpecuMapSampler , UvS ).xyz;
	float3 emap = tex2D( EmissiveMapSampler , UvE ).xyz;
	/////////////////////////
	float3 DC = cmap.xyz*FragIn.Color.xyz*FragIn.Diffuse*DiffuseMix;
	float3 SC = smap*FragIn.Specular*SpecularMix;
	float3 EC = emap*EmissiveMix;
	/////////////////////////
	float4 mrtout = float4( EC+DC+SC, cmap.w );
	return mrtout;
}


float4 ps_difemionly( FragmentPP FragIn ) : COLOR
{	/////////////////////////
	float2 UvC = FragIn.UVW0.xy*UvScaleClr;
	float2 UvE = FragIn.UVW0.xy*UvScaleEmi;
	/////////////////////////
	// Get Other Maps
	float4 cmap = tex2D( ColorMapSampler , UvC );
	float3 emap = tex2D( EmissiveMapSampler , UvE ).xyz;
	/////////////////////////
	float3 WorldEye = GetEyePos();
	float3 WorldNormal = normalize(FragIn.WorldNormal);
	float4 DotScaleBias = float4( DotBiasDiffuse, DotScaleDiffuse, DotBiasSpecular, DotScaleSpecular );
	DifAndSpec das = DiffuseSpecularLight( FragIn.WorldPos, WorldNormal, WorldEye, Shininess, DotScaleBias );
	/////////////////////////
	float3 DC = cmap.xyz*FragIn.Color.xyz*das.Diffuse*DiffuseMix;
	float3 EC = emap*EmissiveMix;
	/////////////////////////
	float4 mrtout = float4( EC+DC, cmap.w );
	//float4 mrtout = float4(cmap.xyz,1.0f);
    return mrtout;
}

float4 ps_spconly( FragmentPP FragIn ) : COLOR
{	/////////////////////////
	float2 UvS = FragIn.UVW0.xy*UvScaleSpc;
	/////////////////////////
	// Get Other Maps
	float3 smap = tex2D( SpecuMapSampler , UvS ).xyz;
	/////////////////////////
	float3 WorldEye = GetEyePos();
	float3 WorldNormal = normalize(FragIn.WorldNormal);
	float4 DotScaleBias = float4( DotBiasDiffuse, DotScaleDiffuse, DotBiasSpecular, DotScaleSpecular );
	DifAndSpec das = DiffuseSpecularLight( FragIn.WorldPos, WorldNormal, WorldEye, Shininess, DotScaleBias );
	/////////////////////////
	float3 SC = smap*das.Specular*SpecularMix;
	/////////////////////////
	float4 mrtout = float4( SC, 1.0f );
    return mrtout;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

technique Gouraud
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_gouraud();
		PixelShader = compile ps_3_0 ps_gouraud();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////

technique PhongPP
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_phongpp();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}
technique PhongPPSkinned
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_phongpp();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////

technique PhongEPP
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_phongppE();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique PhongEPPSkinned
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_phongppE();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////

technique PhongEBlendPP
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_phongppE();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique PhongEBlendPPSkinned
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_phongppE();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////

technique PhongEBlendPP2S
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_phongppE();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		CullMode = CCW; // CCW
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
	pass p1 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_spconly();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		CullMode = CW; // CCW
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique PhongEBlendPP2SSkinned
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_phongppE();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		CullMode = CCW; // CCW
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
	pass p1 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_spconly();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		CullMode = CW; // CCW
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////

technique PhongEBlend2PP
{
	pass p0 <> 
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_difemionly();
		AlphaBlendEnable = false;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
		CullMode = CW; // CCW
	}
	pass p1 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_spconly();
        SrcBlend = ONE;
        DestBlend = ONE;
		AlphaBlendEnable = true;
		CullMode = CW; // CCW
		ZWriteEnable = false;
		ZFunc = LESSEQUAL;
		ZEnable = TRUE;
	}
}

technique PhongEBlend2PPSkinned
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_difemionly();
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;
		CullMode = CW; // CCW
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
	pass p1 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp_skinned();
		PixelShader = compile ps_3_0 ps_spconly();
        SrcBlend = ONE;
        DestBlend = ONE;
		AlphaBlendEnable = true;
		CullMode = CW; // CCW
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

////////////////////////////////////////////////////////////////////////

technique PhongEBlend2PP2S
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_phongpp();
		PixelShader = compile ps_3_0 ps_difemionly();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique GouraudE
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_gouraud();
		PixelShader = compile ps_3_0 ps_gouraudE();
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

///////////////////////////////////////////////////////////////////////////////

#include "pick.fxi"

///////////////////////////////////////////////////////////////////////////////
