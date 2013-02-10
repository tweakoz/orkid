///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "gdche_common.fxi"

////////////////////////////////////////////////////////////////////////////////

uniform float2 uvanimspeed;
uniform texture2D ColorMap<>;

////////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float4 Position : POSITION;   // in object space
	float3 Normal : NORMAL;   // in object space
	float2 Uv0 : TEXCOORD0;
};

struct Fragment
{
	float4 ClipPos : POSITION;  // in clip space
	float2 Uv0 : TEXCOORD0;
	float4 ClipPos2 : TEXCOORD1;
	float4 NormalDepth : TEXCOORD2;
};

////////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
  	//MagFilter = Linear;
  	//MinFilter = LinearMipmapLinear;
};

////////////////////////////////////////////////////////////////////////////////

Fragment vs_skyblend( Vertex VtxIn )
{
	Fragment FragOut;

	float3 worldPos		= mul(float4( VtxIn.Position.xyz, 1.0f ),world).xyz;
	float3 worldNrm		= mul(float4( VtxIn.Normal.xyz, 0.0f ),world).xyz;
	float3 worldEye		= vit[3];
	float3 worldToEye	= normalize( worldEye - worldPos );
	float3 worldToLyt	= normalize( lightpos - worldPos );

	float dist = 1.0f-saturate( distance( worldEye, worldPos )/dofscale );
	float4 ClipPos = mul(VtxIn.Position,wvp);

	FragOut.ClipPos = ClipPos;
	FragOut.ClipPos2 = ClipPos;
	FragOut.Uv0 = VtxIn.Uv0;
	FragOut.NormalDepth = float4( normalize(VtxIn.Normal), dist );
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

MrtPixel ps_skyblend( Fragment FragIn )
{
	float2 UvA = FragIn.Uv0+(uvanimspeed*reltime);
	MrtPixel mrtout;
	mrtout.DiffuseBuffer = tex2D( ColorMapSampler , UvA ).xyzw;
	mrtout.SpecularBuffer = float4( 0.0f, 0.0f, 0.0f, FragIn.NormalDepth.w  );
	mrtout.NormalDepthBuffer = float4( FragIn.ClipPos2 );
	return mrtout;
}

///////////////////////////////////////////////////////////////////////////////

technique SkyBlend
{
	pass p0 < string miniork_rq_sortpass="skybox"; >
	{
		VertexShader = compile vs_3_0 vs_skyblend();
		PixelShader = compile ps_3_0 ps_skyblend();

		ZWriteEnable = true;
		ZFunc = LESS;
		ZEnable = TRUE;

		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
        //AlphaBlendEnable = TRUE;
        //AlphaTestEnable = TRUE;
        //AlphaRef = 0;
        //AlphaFunc = GREATER;
		//SrcBlend  = SRCALPHA;       
		//DestBlend = INVSRCALPHA;
		//ZWriteEnable = false;
	}
}


