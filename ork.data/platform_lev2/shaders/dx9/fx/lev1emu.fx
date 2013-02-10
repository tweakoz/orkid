///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

float4x4        mvp;
texture			ColorMap;

///////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = POINT;
    MinFilter = POINT;
    MipFilter = LINEAR; // LINEAR NONE
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
    float3 Position             : POSITION;
    float4 Color                : COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleTexVertex
{
    float3 Position             : POSITION;
    float4 UV0                  : TEXCOORD0;
    float4 Color                : COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
    float4 ClipPos : Position;
    float4 Color   : Color;
    float4 UV0     : TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxColor( SimpleVertex VtxIn )
{
    VtxOut FragOut;

    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
    FragOut.Color = VtxIn.Color.argb;
    FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    return FragOut;
}

VtxOut VSTexColor( SimpleTexVertex VtxIn )
{
    VtxOut FragOut;

    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
    FragOut.Color = VtxIn.Color.bgra;
    FragOut.UV0 = VtxIn.UV0;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSFragColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    //PixOut = float4( FragIn.UV0.xy, 0.0f, 1.0f );
    PixOut = FragIn.Color;
    return PixOut;
}

float4 PSTexColor( VtxOut FragIn ) : COLOR
{
    float4 PixOut;
    float4 texA = tex2D( ColorMapSampler, FragIn.UV0.xy );
    PixOut = float4( texA.xyzw )*FragIn.Color;
    //PixOut = float4( FragIn.UV0.xy, 0.0f, 1.0f );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

technique vtxcolor // just vertex colors
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSVtxColor();
        PixelShader = compile ps_2_0 PSFragColor();

        CullMode = NONE;
        ZEnable = true;
        AlphaBlendEnable = TRUE;
        AlphaTestEnable = FALSE;
    }
};

///////////////////////////////////////////////////////////////////////////////

technique vtxcolortex // vertex color modulated texture
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSTexColor();
        PixelShader = compile ps_2_0 PSTexColor();

        CullMode = NONE; // NONE CW CCW
        ZEnable = true;
        AlphaBlendEnable = TRUE;
        AlphaTestEnable = TRUE;
    }
};
