///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

uniform float4x4 mvp;
uniform float4 ModColor;
uniform texture2D ColorMap;

///////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
    MagFilter = Linear;
    MinFilter = Linear;
    MipFilter = Point;
};

///////////////////////////////////////////////////////////////////////////////

sampler2D TextMapSampler = sampler_state
{
	Texture = <ColorMap>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler2D TextStretchMapSampler = sampler_state
{
	Texture = <ColorMap>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
	float2 Position	: POSITION;
	float4 Color	: COLOR;
};

///////////////////////////////////////////////////////////////////////////////

struct UvVertex
{
	float2 Position	: POSITION;
	float2 Uv0		: TEXCOORD0;
};

struct UvcVertex
{
	float2 Position	: POSITION;
	float2 Uv0		: TEXCOORD0;
	float4 Color	: COLOR;
};

struct UvcDepthVertex
{
	float3 Position : POSITION0;
	float2 Uv0		: TEXCOORD0;
	float4 Color	: COLOR;
};

///////////////////////////////////////////////////////////////////////////////
struct Fragment
{
    float4 ClipPos	: POSITION;
	float2 Uv0		: TEXCOORD0;
	float4 Color	: COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

Fragment vs_vtx( SimpleVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
    float4 npos = mul( vpos, mvp );

    Fragment FragOut;
    FragOut.ClipPos = npos;
    FragOut.Color = VtxIn.Color;
    FragOut.Uv0 = float2(0.0,0.0f);
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

Fragment vs_vtxtex( UvcVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
    float4 npos = mul( vpos, mvp );

    Fragment FragOut;
    FragOut.ClipPos = npos;
    FragOut.Color = VtxIn.Color;
    FragOut.Uv0 = VtxIn.Uv0;
    return FragOut;
}

Fragment vs_depthvtxtex(UvcDepthVertex VtxIn )
{
	float4 vpos = float4(VtxIn.Position, 1.0f);
	float4 npos = mul(vpos, mvp);
	
	Fragment FragOut;
	FragOut.ClipPos = npos;
	FragOut.Color = VtxIn.Color;
	FragOut.Uv0 = VtxIn.Uv0;
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

Fragment vs_vtxmod( SimpleVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
    float4 npos = mul( vpos, mvp );

    Fragment FragOut;
    FragOut.ClipPos = npos;
    FragOut.Color = VtxIn.Color*ModColor;
    FragOut.Uv0 = float2(0.0,0.0f);
    return FragOut;
}

Fragment VSUI( SimpleVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
    float4 npos = mul( vpos, mvp );

    Fragment FragOut;
    FragOut.ClipPos = npos;
    FragOut.Color = npos;
    FragOut.Uv0 = float2(0.0,0.0f);
    return FragOut;
}

Fragment VSUITEXTURED( UvVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
    float4 npos = mul( vpos, mvp );

    Fragment FragOut;
    FragOut.ClipPos = npos;
    FragOut.Uv0 = VtxIn.Uv0;
    FragOut.Color = npos;
    return FragOut;
}

Fragment VSUITEXT( UvVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 0.0f, 1.0f );
    float4 npos = mul( vpos, mvp );
    
    Fragment FragOut;
    FragOut.ClipPos = npos;
    
    FragOut.Uv0 = VtxIn.Uv0;
    FragOut.Color = npos;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSTextured( Fragment FragIn ) : COLOR
{	float2 Uv0 = FragIn.Uv0.xy;
    float4 PixOut = tex2D( ColorMapSampler, Uv0 );
    return PixOut;
}

float4 PSKeyTex( Fragment FragIn ) : COLOR
{
	float4 PixOut;
	float2 Uv0 = FragIn.Uv0.xy;
	float4 Tex = tex2D( ColorMapSampler, Uv0 );
	PixOut.rgb = ModColor.rgb;
	PixOut.a = Tex.r;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSText( Fragment FragIn ) : COLOR
{	float2 Uv0 = FragIn.Uv0.xy;
    float4 PixOut = tex2D( TextMapSampler, Uv0 );
    return float4( PixOut.xyz*ModColor.xyz, PixOut.x );
    //return float4( 1.0f,0.0f,0.0f,1.0f );
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_vtxtex( Fragment FragIn ) : COLOR
{
	float2 Uv0 = FragIn.Uv0.xy;
    float4 PixOut = tex2D( TextMapSampler, Uv0 );
    return float4(PixOut.xyz * FragIn.Color.xyz * ModColor.xyz, PixOut.a * ModColor.w );
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_modtex( Fragment FragIn ) : COLOR
{
	float2 Uv0 = FragIn.Uv0.xy;
    float4 PixOut = tex2D( TextStretchMapSampler, Uv0 );
    return float4(PixOut.xyz * ModColor.xyz, PixOut.a * ModColor.w );
}

///////////////////////////////////////////////////////////////////////////////

float4 PSModColor( Fragment FragIn ) : COLOR
{
    return ModColor;
}

float4 ps_fragcolor( Fragment FragIn ) : COLOR
{
    return FragIn.Color;
}

///////////////////////////////////////////////////////////////////////////////

technique ui_vtxmod
{
    pass
    {
        VertexShader = compile vs_3_0 vs_vtxmod();
        PixelShader = compile ps_3_0 ps_fragcolor(); 
    }
}

technique uitext
{
    pass
    {
        VertexShader = compile vs_3_0 VSUITEXT();
        PixelShader = compile ps_3_0 PSKeyTex(); 
		AlphaTestEnable=true;
		AlphaFunc = GREATER;
		AlphaRef = 0.0;
        AlphaBlendEnable = TRUE;
        SrcBlend = srcalpha;
        DestBlend = invsrcalpha;
    }
}
technique uidev_modcolor
{
    pass
    {
        VertexShader = compile vs_3_0 VSUI();
        PixelShader = compile ps_3_0 PSModColor(); 
    }
}
technique ui_vtx
{
    pass
    {
        VertexShader = compile vs_3_0 vs_vtx();
        PixelShader = compile ps_3_0 ps_fragcolor(); 
    }
}
technique ui_vtxtex
{
    pass
    {
        VertexShader = compile vs_3_0 vs_vtxtex();
        PixelShader = compile ps_3_0 ps_vtxtex(); 
    }
}

technique ui_modtex
{
	pass
	{
		VertexShader = compile vs_3_0 vs_depthvtxtex();
		PixelShader = compile ps_3_0 ps_modtex();
	}
}

technique ui_depthvtxtex
{
	pass
	{
		VertexShader = compile vs_3_0 vs_depthvtxtex();
		PixelShader = compile ps_3_0 ps_vtxtex();
	}
}

technique uicolor
{
    pass
    {
        VertexShader = compile vs_3_0 VSUI();
        PixelShader = compile ps_3_0 PSModColor(); 
    }
}
technique uicircle
{
    pass
    {
        VertexShader = compile vs_3_0 VSUI();
        PixelShader = compile ps_3_0 PSModColor(); 
    }
}

technique uitextured
{
    pass
    {
        VertexShader = compile vs_3_0 VSUITEXTURED();
        PixelShader = compile ps_3_0 PSTextured();
    }
}

///////////////////////////////////////////////////////////////////////////////
