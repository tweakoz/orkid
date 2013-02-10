///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "gdche_common.fxi"

////////////////////////////////////////////////////////////////////////////////

uniform texture2D ColorMap<>;
uniform texture2D ColorMap2<>;
uniform texture2D ColorMap3<>;
uniform texture2D ColorMap4<>;

uniform float4x4 MatM;
uniform float4x4 MatV;
uniform float4x4 MatP;
uniform float4x4 MatMV;
uniform float4x4 MatMVP;
uniform float4x4 MatAux;

uniform float4 ModColor;

////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float4 Position : POSITION;   // in object space
	float3 Normal : NORMAL;
	float2 Uv0 : TEXCOORD0;
};

struct VertexS
{
	float4 Position : POSITION;   // in object space
	float2 Uv0 : TEXCOORD0;
};

struct Fragment
{
	float4 ClipPos : POSITION;  // in clip space
	float2 Uv0 : TEXCOORD0;
	float4 ClipPos2 : TEXCOORD1;
	float4 NormalDepth : TEXCOORD2;
	float4 Color : TEXCOORD3;
};

////////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
  	//MagFilter = Linear;
  	//MinFilter = LinearMipmapLinear;
};

////////////////////////////////////////////////////////////////////////////////

Fragment vs_simple( Vertex VtxIn )
{
	Fragment FragOut;

	float3 worldPos		= mul(float4( VtxIn.Position.xyz, 1.0f ),MatM).xyz;
	float3 worldNrm		= normalize(mul(float4( VtxIn.Normal.xyz, 0.0f ),MatM).xyz);
	float3 worldEye		= ViewInverseTranspose[3];
	float3 worldToEye	= normalize( worldEye - worldPos );
	float3 worldToLyt	= normalize( lightpos - worldPos );

	float dist = 1.0f-saturate( distance( worldEye, worldPos )/dofscale );
	float4 ClipPos = mul(VtxIn.Position,MatMVP);

	float fdotL = saturate(dot(worldNrm,worldToLyt));

	FragOut.ClipPos = ClipPos;
	FragOut.ClipPos2 = ClipPos;
	FragOut.Uv0 = VtxIn.Uv0;
	FragOut.Color = float4( fdotL, fdotL, fdotL, 1.0f );
	FragOut.NormalDepth = float4( worldNrm, dist );
	return FragOut;
}

////////////////////////////////////////////////////////////////////////////////

Fragment vs_butt_simple( VertexS VtxIn )
{
	Fragment FragOut;

	float3 worldPos	= mul(float4( VtxIn.Position.xyz, 1.0f ),MatM).xyz;
	float4 ClipPos = mul(VtxIn.Position,MatMVP);

	FragOut.ClipPos = ClipPos;
	FragOut.ClipPos2 = ClipPos;
	FragOut.Uv0 = VtxIn.Uv0;
	FragOut.Color = float4( 0.0f, 0.0f, 0.0f, 1.0f );
	FragOut.NormalDepth = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_simple( Fragment FragIn ) : COLOR
{
	float2 UvA = FragIn.Uv0;
	float4 outp = tex2D( ColorMapSampler , UvA ).xyzw;
	return outp;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_shadow( Fragment FragIn ) : COLOR
{
	float2 UvA = FragIn.Uv0;
	float4 ShadowTex = tex2D( ColorMapSampler , UvA ).xyzw;
	float fsh = ShadowTex.x;
	float fish = 1.0f-fsh;
	
	float4 outp = float4( 0.0f,0.0f,0.0f, fsh*0.5f );
	
	return outp;
}

///////////////////////////////////////////////////////////////////////////////

technique shadow
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_butt_simple();
		PixelShader = compile ps_3_0 ps_shadow();
        AlphaBlendEnable = TRUE;
		SrcBlend  = SRCALPHA;       
		DestBlend = INVSRCALPHA;

		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}


///////////////////////////////////////////////////////////////////////////////

technique simple
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_simple();
		PixelShader = compile ps_3_0 ps_simple();

		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;

        //AlphaBlendEnable = FALSE;
        //AlphaTestEnable = TRUE;
        //AlphaRef = 0;
        //AlphaFunc = GREATER;
		//SrcBlend  = SRCALPHA;       
		//DestBlend = INVSRCALPHA;

		//ZWriteEnable = true;
		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;
	}
}

technique simple_blendmix_nozwrite
{
	pass p0 <>
	{
		VertexShader = compile vs_3_0 vs_simple();
		PixelShader = compile ps_3_0 ps_simple();

		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
        //AlphaBlendEnable = FALSE;
        //AlphaTestEnable = TRUE;
        //AlphaRef = 0;
        //AlphaFunc = GREATER;
		//SrcBlend  = SRCALPHA;       
		//DestBlend = INVSRCALPHA;
		//ZWriteEnable = true;
		//ZFunc = LESS;
		//ZEnable = TRUE;

		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;

	}
}
