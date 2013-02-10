///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

float4x4        mvp;
texture			ColorMap;
float4			modcolor;

struct MrtPixel
{
	float4	ColorBuffer			: COLOR0;
	float4	UvBuffer			: COLOR1;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
    float3 Position             : POSITION;
    float4 Color                : COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
    float4 ClipPos : Position;
    float4 Color   : Color;
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxColor( SimpleVertex VtxIn )
{
    VtxOut FragOut;

    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
    FragOut.Color = VtxIn.Color;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSModColor( VtxOut FragIn ) : COLOR
{
    return modcolor;
}

///////////////////////////////////////////////////////////////////////////////

technique std // just vertex colors
{
    pass p0 // draw occluded portion of manips over top everything (but blended)
    {
    	ZWriteEnable = TRUE;
    	ZEnable = TRUE;
    	ZFunc = LESSEQUAL;
        CullMode = NONE; // NONE CW CCW
        AlphaBlendEnable = TRUE;
        AlphaTestEnable = FALSE;
        SrcBlend = SRCALPHA; 
        DestBlend = INVSRCALPHA;
        DepthBias = -0.500000f;
            
        VertexShader = compile vs_2_0 VSVtxColor();
        PixelShader = compile ps_2_0 PSModColor();
    }
};

///////////////////////////////////////////////////////////////////////////////

technique pick // just vertex colors
{
    pass p0 // draw non occluded portion of manips in solid
    {
    	ZWriteEnable = TRUE;
    	ZEnable = TRUE;
    	ZFunc = LESSEQUAL;
        CullMode = NONE; // NONE CW CCW
        AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
        DepthBias = -0.100002f;
    		
		VertexShader = compile vs_2_0 VSVtxColor();
		PixelShader = compile ps_2_0 PSModColor();
    }
};

///////////////////////////////////////////////////////////////////////////////
