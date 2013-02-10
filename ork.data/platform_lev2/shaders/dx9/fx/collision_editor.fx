///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

float4x4        mvp;
float4          modcolor;

///////////////////////////////////////////////////////////////////////////////

struct SimpleTexVertex
{
    float3 Position	: POSITION;
    float4 UV0		: TEXCOORD0;
    float4 Color	: COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
    float4 ClipPos : Position;
    float4 Color   : Color;
    float4 UV0     : TEXCOORD0;
    float3 DevPos  : TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxColor( SimpleTexVertex VtxIn )
{
    VtxOut FragOut;
    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
    FragOut.DevPos = FragOut.ClipPos.xyz/FragOut.ClipPos.w;
    FragOut.Color = VtxIn.Color.argb;
    FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    return FragOut;
}

float4 PSSolid( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    PixOut = FragIn.Color;
    return PixOut;
}

float4 PSStriped( VtxOut FragIn ) : COLOR
{
	float fx = FragIn.DevPos.x*100.0f;
	float fy = FragIn.DevPos.y*100.0f;
	float fz = (200.0f+fx+fy);
	int ior = int(fz)%3;
	float fmask = float( (ior==0) );
	
	if( fmask<0.1f )
	{
		
	}
    float4 PixOut = FragIn.Color*fmask;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

technique solid
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSVtxColor();
        PixelShader = compile ps_2_0 PSSolid();
        AlphaTestEnable = FALSE;
        AlphaFunc = GREATER;
        AlphaRef = 0.7f;
    }
};

///////////////////////////////////////////////////////////////////////////////

technique striped
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSVtxColor();
        PixelShader = compile ps_2_0 PSStriped();
        AlphaTestEnable = TRUE;
        AlphaFunc = GREATER;
        AlphaRef = 0.7f;
    }
};
