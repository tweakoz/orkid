///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "gdche_common.fxi"

///////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float4 Position : POSITION;   // in object space
	float4 TexCoord : TEXCOORD0;
	float3 Normal   : TEXCOORD1;
	float3 Binormal : TEXCOORD2;
	float4 Color    : COLOR;
};

struct Fragment
{
	float4 ClipPos : POSITION;  // in clip space
	float4 Color : TEXCOORD0;
	float2 Uv0 : TEXCOORD1;
	float2 Uv1 : TEXCOORD2;
	float2 Uv2 : TEXCOORD3;
    float3 ObjPos : TEXCOORD4;
    float3 TanEye : TEXCOORD5;
};

///////////////////////////////////////////////////////////////////////////////

uniform float4 DiffuseColor;
uniform float2 LayerSpeedA;
uniform float2 LayerSpeedB;
uniform float2 LayerSpeedC;
uniform float BumpLevA;
uniform float BumpLevB;
uniform float BumpLevC;
uniform float BumpGlobalScale;
uniform float YBias;
uniform float UvScaleA;
uniform float UvScaleB;
uniform float UvScaleC;
uniform texture NormalMapA;
uniform textureCUBE EnvMap;

////////////////////////////////////////////////////////////////////////////////

sampler2D NormalMapASampler = sampler_state
{
    Texture = <NormalMapA>;
    magFilter = LINEAR;
    minFilter = LINEAR;
};

samplerCUBE EnvMapSampler = sampler_state
{
    Texture = <EnvMap>;
    magFilter = LINEAR;
    minFilter = LINEAR;
};

////////////////////////////////////////////////////////////////////////////////

float CalcDisplacement( float2 uv )
{
	float ftime = fmod( (reltime*0.04f), 10.0f );

	float2 Uv0 = uv.xy*float2(UvScaleA,-UvScaleA)+(LayerSpeedA*ftime);
	float2 Uv1 = uv.xy*float2(UvScaleB,-UvScaleB)+(LayerSpeedB*ftime);
	float2 Uv2 = uv.xy*float2(UvScaleC,-UvScaleC)+(LayerSpeedC*ftime);
	
	float3 NormalMapA = BumpLevA*normalize(tex2Dlod( NormalMapASampler , float4( Uv0, 0.0f, 0.0f ) ).xyz);
	float3 NormalMapB = BumpLevB*normalize(tex2Dlod( NormalMapASampler , float4( Uv1, 0.0f, 0.0f ) ).xyz);
	float3 NormalMapC = BumpLevC*normalize(tex2Dlod( NormalMapASampler , float4( Uv2, 0.0f, 0.0f ) ).xyz);

	float3 Normal = normalize(NormalMapA+NormalMapB+NormalMapC);
	float disp = Normal.z+YBias;
	
	return disp;
}

////////////////////////////////////////////////////////////////////////////////

Fragment MainVS( Vertex VtxIn, uniform float4x4 WorldViewProj )
{
	Fragment FragOut;

	float ftime = fmod( (reltime*0.01f), 10.0f );

	//////////////////////////////////////

	float Disp = CalcDisplacement( VtxIn.TexCoord.xy );
	
	float4 RealPos = float4( VtxIn.Position.xyz+float3(0.0f,(Disp*0.006f*BumpGlobalScale),0.0f), 1.0f );

	//////////////////////////////////////

	float3 worldPos = mul(RealPos,world).xyz;

	//////////////////////////////////////

	float3 worldNormal  = normalize( mul( float4( VtxIn.Normal, 0.0f ), world ).xyz );
	float3 worldEye     = vit[3].xyz;
	float3 worldToEye   = normalize( worldEye - worldPos );
	float3 worldToLyt   = worldToEye;

	/////////////////////////////////////
	// ( Tangent Space Lighting )

	float3 oNormal = normalize(VtxIn.Normal);
	float3 oBinormal = float3(1.0f,0.0f,1.0f);
	float3 oTangent = normalize(cross(oNormal,oBinormal));
	float3 wNormal = mul( world, float4( oNormal,0.0f ) ).xyz;
	float3 wBinormal = mul( world, float4( oBinormal,0.0f ) ).xyz;
	float3 wTangent = mul( world, float4( oTangent,0.0f ) ).xyz;
	TanLytData tldata = GenTanLytData( world, worldToLyt, worldToEye, wNormal, wBinormal, wTangent );

	/////////////////////////////////////

	FragOut.ClipPos = mul(RealPos,wvp);
	FragOut.Color = VtxIn.Color;
    FragOut.ObjPos = RealPos.xyz;
	FragOut.TanEye = tldata.TanEye;
	FragOut.Uv0 = VtxIn.TexCoord.xy*float2(UvScaleA,-UvScaleA)+(LayerSpeedA*ftime);
	FragOut.Uv1 = VtxIn.TexCoord.xy*float2(UvScaleB,-UvScaleB)+(LayerSpeedB*ftime);
	FragOut.Uv2 = VtxIn.TexCoord.xy*float2(UvScaleC,-UvScaleC)+(LayerSpeedC*ftime);

	/////////////////////////////////////

	return FragOut;
}

////////////////////////////////////////////////////////////////////////////////

float4 ps_basic( Fragment FragIn ) : COLOR
{
	return float4( 1.0f,0.0f,0.0f, 1.0f );
}

////////////////////////////////////////////////////////////////////////////////

float4 ps_water( Fragment FragIn ) : COLOR
{
	const float3 FlatNormal = float3( 0.5f, 0.5f, 1.0f );
	
	float3 NormalMapA = tex2D( NormalMapASampler , (FragIn.Uv0) ).xyz;
	float3 NormalMapB = tex2D( NormalMapASampler , (FragIn.Uv1) ).xyz;
	float3 NormalMapC = tex2D( NormalMapASampler , (FragIn.Uv2) ).xyz;

	NormalMapA = lerp( FlatNormal, NormalMapA, BumpLevA );
	NormalMapB = lerp( FlatNormal, NormalMapB, BumpLevB );
	NormalMapC = lerp( FlatNormal, NormalMapC, BumpLevC );

	float3 NormalMap = ( NormalMapA+NormalMapB+NormalMapC ) * 0.333f;

	float DDx = ddx( FragIn.ObjPos.y )*1000.0f;
	float DDy = ddy( FragIn.ObjPos.y )*1000.0f;
	
	float3 test = normalize( float3( DDx, DDy, 0.01f ) );

	float3 TanNormal = normalize( ((NormalMap*2.0f)-float3(1.0f,1.0f,1.0f))+(.03f*test) );
	
    float3 lookup = normalize( reflect( normalize(FragIn.TanEye), TanNormal) );

	float3 EnvMap = texCUBE( EnvMapSampler, lookup );

	return float4( EnvMap*DiffuseColor.xyz, DiffuseColor.w );
}

////////////////////////////////////////////////////////////////////////////////

technique dbg
{   pass p0
    {   VertexShader = compile vs_3_0 MainVS(wvp);
        PixelShader = compile ps_3_0 ps_basic();
    }
}

////////////////////////////////////////////////////////////////////////////////

technique water1
{   pass p0
    {   VertexShader = compile vs_3_0 MainVS(wvp);
        PixelShader = compile ps_3_0 ps_water();
		//DepthMask = false;
		//DepthTestEnable = true;
        //AlphaBlendEnable = true;
        //AlphaTestEnable = true;
		//BlendFunc = int2( SrcAlpha, OneMinusSrcAlpha );
    }
}
