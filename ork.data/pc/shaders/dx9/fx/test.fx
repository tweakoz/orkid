///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "gdche_common.fxi"

///////////////////////////////////////////////////////////////////////////////
// artist parameters

uniform texture2D	AmbMap;
uniform texture2D	ObjNormalMap;
uniform texture2D	DetailNormalMap;
uniform texture2D	ShineMap;
uniform textureCUBE CubeMap;

uniform float3		AmbientColor;
uniform float3		DiffuseColor;
uniform float3		SpecularColor;
uniform float2		nmap_dir0;
uniform float2		nmap_dir1;

uniform float		DetailUvScale0;
uniform float		DetailUvScale1;
uniform float		DetailBumpScale0;
uniform float		DetailBumpScale1;
uniform float		ShineUvScale;
uniform float		AmbientMix;
uniform float		DiffuseMix;
uniform float		SpecularMix;
uniform float		Shininess;

///////////////////////////////////////////////////////////////////////////////

sampler2D ObjNormalMapSampler = sampler_state
{
    Texture = <ObjNormalMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
};

sampler2D DetailNormalMapSampler = sampler_state
{
    Texture = <DetailNormalMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
};

sampler2D ShineMapSampler = sampler_state
{
    Texture = <ShineMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
};

sampler2D AmbMapSampler = sampler_state
{
	Texture = <AmbMap>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
};

samplerCUBE CubeMapSampler = sampler_state
{
	Texture = <CubeMap>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
};

///////////////////////////////////////////////////////////////////////////////

MrtPixel PSTexColor( TanSpaceFragment FragIn )
{
#if defined( FASTMODE )

	MrtPixel mrtout;
	mrtout.DiffuseBuffer = modcolor;
	mrtout.SpecularBuffer = float4( 0.0f, 0.0f, 0.0f, FragIn.NormalDepth.w  );
	mrtout.NormalDepthBuffer = float4( FragIn.ClipPos2 );
	return mrtout;

#else

	float4 Uv0 = float4( FragIn.Uv0Uv1.xy, 0.0f ,texbias );

	float3 AmbientMap = tex2Dbias( AmbMapSampler , Uv0 ).rgb;
	float3 NormalMap = (tex2Dbias( ObjNormalMapSampler , Uv0 ).rgb-float3(0.5f,0.5f,0.5f))*2.0f;
	float3 ShineMap = tex2D( ShineMapSampler , FragIn.Uv0Uv1.xy*ShineUvScale ).rgb;

    float2 DetUv0 = (FragIn.Uv0Uv1.xy*DetailUvScale0)+(nmap_dir0*reltime);
    float2 DetUv1 = (FragIn.Uv0Uv1.xy*DetailUvScale1)+(nmap_dir1*reltime);
    float3 DetNormalMap = (tex2Dbias( DetailNormalMapSampler , float4(DetUv0,0.0f,texbias) ).rgb-float3(0.5f,0.5f,0.5f))*2.0f;
    float3 DetNormalMap2 = (tex2Dbias( DetailNormalMapSampler , float4(DetUv1,0.0f,texbias) ).rgb-float3(0.5f,0.5f,0.5f))*2.0f;
    float3 DetScale0 = float3( DetailBumpScale0, DetailBumpScale0, 1.0f );
    float3 DetScale1 = float3( DetailBumpScale1, DetailBumpScale1, 1.0f );
    DetNormalMap = normalize((DetNormalMap*DetScale0)+(DetNormalMap2*DetScale1));

	float3 ObjSpaceNormal = normalize(NormalMap);
    float3x3 TanFrame = ComputeTangentFrame( ObjSpaceNormal, FragIn.ObjPos, DetUv0 );
    float3 ObjDetNorm = normalize(mul( DetNormalMap, TanFrame ));
	float3 WldNormal = normalize(mul( float4(ObjDetNorm,0.0f), world ).xyz);

	float fdotL = saturate( dot( WldNormal, FragIn.TanToLyt ) );
	float fdotH = saturate( dot( WldNormal, FragIn.TanToHalf ) );

	float Specular = SpecularMix*pow( fdotH , Shininess*(1.0f+((1.0f-ShineMap.x))) )*(1.0f-ShineMap.y);
	float Diffuse = DiffuseMix*fdotL;

	float3 lookup = normalize( reflect( FragIn.TanToLyt, ObjDetNorm) );

	float3 texCube = texCUBE( CubeMapSampler, lookup ).rgb;

	float3 DiffuseColor = (AmbientMap*AmbientMix*AmbientColor*FragIn.Color.rgb) + (Diffuse*DiffuseColor.rgb*AmbientMap*FragIn.Color.rgb);
	float3 spc = (Specular*SpecularColor.rgb*texCube);

	MrtPixel mrtout;
	mrtout.DiffuseBuffer = float4( DiffuseColor*modcolor+spc, 1.0f );
	mrtout.SpecularBuffer = float4(WldNormal.xyz,FragIn.NormalDepth.w);
	mrtout.NormalDepthBuffer = float4( FragIn.ClipPos2 );
    return mrtout;

#endif
}

///////////////////////////////////////////////////////////////////////////////

technique FullLighting
{
	pass p0
	{
		VertexShader = compile vs_3_0 VSTanSpace();
		PixelShader = compile ps_3_0 PSTexColor();
		//CullMode = CW; // NONE CW CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
		//ZWriteEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};

///////////////////////////////////////////////////////////////////////////////

technique FullLightingSkinned
{
	pass p0
	{
		VertexShader = compile vs_3_0 VSTanSpaceSkinned();
		PixelShader = compile ps_3_0 PSTexColor();
		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
		//ZWriteEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
};

///////////////////////////////////////////////////////////////////////////////
