///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

uniform float4x4        MatM;
uniform float4x4        MatMV;
uniform float4x4        MatMVP;
uniform float4x4        MatP;
uniform float4x4		MatAux;

uniform float4			modcolor;
uniform float4          user0;

uniform texture2D		ColorMap;
uniform texture2D		ColorMap2;

float					CurTime;
float					InvLifeTime;
float					RampScale;

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
};
	
sampler2D RampMapSampler = sampler_state
{
	Texture = <ColorMap2>;
	MagFilter = LINEAR;
	MinFilter = LINEAR;
	MipFilter = LINEAR;
};

///////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float3 Position		: POSITION;
	float4 Color		: COLOR;
	float3 Normal		: NORMAL;
	float2 UVW			: TEXCOORD0;
	float2 UVW2			: TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////

struct Fragment
{
	float4 ClipPos : POSITION;
	float4 Color   : COLOR;
	float4 UVW     : TEXCOORD0;
	float  Blend   : TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////

Fragment basicparticle( Vertex VtxIn )
{
	float4 vpos = float4( VtxIn.Position, 1.0f );
	float4 npos = mul( vpos, MatMVP );
	
	float2 uv = VtxIn.UVW;
	float texframe = VtxIn.UVW2.x;
	float AnimTexDim = VtxIn.UVW2.y;
	////////////////////////////////////////////
	float texB = frac( texframe );
	float ftexS = 1.0f / AnimTexDim;
	////////////////////////////////////////////
	float itexframe0 = floor(texframe-texB);
	float itexframe1 = itexframe0+1.0f;
	////////////////////////////////////////////
	float ftexX0 = fmod( itexframe0 , AnimTexDim );
	float ftexY0 = floor(itexframe0 / AnimTexDim );
	float2 fuv0 = uv+float2( ftexX0, ftexY0 )*ftexS;
	////////////////////////////////////////////
	float ftexX1 = fmod( itexframe1 , AnimTexDim );
	float ftexY1 = floor(itexframe1 / AnimTexDim );
	float2 fuv1 = uv+float2( ftexX1, ftexY1 )*ftexS;
	////////////////////////////////////////////
					
	Fragment FragOut;

	FragOut.ClipPos = npos;
	FragOut.Color = VtxIn.Color;
	FragOut.UVW = float4(fuv0,fuv1);
	FragOut.Blend = texB;
	
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 texparticle( Fragment FragIn ) : COLOR
{	float4 PixOut;
	float4 texA = tex2D( ColorMapSampler, FragIn.UVW.xy );
	float4 texB = tex2D( ColorMapSampler, FragIn.UVW.zw );
	PixOut = lerp( texA, texB, FragIn.Blend )*FragIn.Color;
	return PixOut;
}

///////////////////////////////////////////////////////////////////////////////
		
technique tbasicparticle
{
	pass p0
	{
		VertexShader = compile vs_2_0 basicparticle();
		PixelShader = compile ps_2_0 texparticle(); //PSTextured();
		
		//AlphaFunc = GREATER;
		//AlphaTestEnable = TRUE;
		//AlphaRef = 0;
		//CullMode = NONE; // NONE CW CCW
	}
}

///////////////////////////////////////////////////////////////////////////////
