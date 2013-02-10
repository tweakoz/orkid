///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

float2 VPW;
float2 Bias;
float2 Scale;
float UVScale;
float4x4 Transform;
float4 ModColor;
float CircleInnerRadius;
float CircleOuterRadius;

texture ColorMap;

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
	MagFilter = POINT;
	MinFilter = POINT;
	MipFilter = NONE;
};
	
///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
	float2 Position		: POSITION;
};

///////////////////////////////////////////////////////////////////////////////

struct Vertex
{
	float2 Position		: POSITION;
	float4 Color		: COLOR;
	//float4 Normal_S		: NORMAL;
	//float4 Binormal_T	: BINORMAL;
	float4 UVW			: TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

struct Fragment
{
	float4 ClipPos : POSITION;
	float4 Color   : COLOR;
	float4 UVW     : TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

float4 PSDFlowNode( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float4 texA = tex2D( ColorMapSampler, FragIn.UVW.xy );
	PixOut = float4( texA.xyz, texA.x*(texA.w*2) );
	//PixOut = float4(  FragIn.UVW );
	return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSTextured( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float4 texA = tex2D( ColorMapSampler, FragIn.UVW.xy );
	PixOut = float4( texA.xyz, texA.w );
	//PixOut = float4(  FragIn.UVW );
	return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSModColor( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	PixOut = float4(  FragIn.Color );
	return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

Fragment VSHUI( Vertex VtxIn )
{
	float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
	float4 npos = mul( vpos, Transform );

	Fragment FragOut;
	FragOut.ClipPos = float4( npos.xy, 0.0f, 1.0f );
	FragOut.Color = ModColor;
	FragOut.UVW = VtxIn.UVW;
	return FragOut;
}

Fragment VSHUINoTex( SimpleVertex VtxIn )
{
	float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
	float4 npos = mul( vpos, Transform );

	Fragment FragOut;
	FragOut.ClipPos = float4( npos.xy, 0.0f, 1.0f );
	FragOut.Color = ModColor;
	FragOut.UVW = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	return FragOut;
}

Fragment VSBG( Vertex VtxIn )
{
	Fragment FragOut;
	FragOut.ClipPos = float4( VtxIn.Position, 0.0f, 1.0f );
	FragOut.Color = ModColor;
	FragOut.UVW = VtxIn.UVW;
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

technique dfbg
{
	pass p0
	{
		VertexShader = compile vs_2_0 VSBG();
		PixelShader = compile ps_2_0 PSTextured();
		
		AlphaFunc = ALWAYS;
		AlphaTestEnable = FALSE;
		AlphaRef = 0;

		CullMode = NONE; // NONE CW CCW
	}
};

technique dfgrid
{
	pass p0
	{
		VertexShader = compile vs_2_0 VSHUINoTex();
		PixelShader = compile ps_2_0 PSModColor();
		
		AlphaFunc = ALWAYS;
		AlphaTestEnable = FALSE;
		AlphaRef = 0;

		CullMode = NONE; // NONE CW CCW
	}
};

technique dfpath
{
	pass p0
	{
		VertexShader = compile vs_2_0 VSHUINoTex();
		PixelShader = compile ps_2_0 PSModColor();
		
		AlphaFunc = ALWAYS;
		AlphaTestEnable = FALSE;
		AlphaRef = 0;

		CullMode = NONE; // NONE CW CCW
	}
};

technique dflow
{
	pass p0
	{
		VertexShader = compile vs_2_0 VSHUI();
		PixelShader = compile ps_2_0 PSTextured();
		
		AlphaFunc = ALWAYS;
		AlphaTestEnable = TRUE;
		AlphaRef = 0;

		CullMode = NONE; // NONE CW CCW
	}
};

technique dflownode
{
	pass p0
	{
		VertexShader = compile vs_2_0 VSHUI();
		PixelShader = compile ps_2_0 PSDFlowNode();
		
		AlphaFunc = ALWAYS;
		AlphaTestEnable = TRUE;
		AlphaRef = 0;

		CullMode = NONE; // NONE CW CCW
	}
};
