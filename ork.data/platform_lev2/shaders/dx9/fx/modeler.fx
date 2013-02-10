///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

float4x4        bones[16];
float4x4        mvp;
float4x4        MatMV;
float4x4        MatP;
float4x4        mmatrix;
float4          modcolor;
float2          Scale2D;
float2          Bias2D;
float           BrushPressure;
float                        time;
float                        mipbias;

///////////////////////////////////////////////////////////////////////////////

texture ColorMap;

texture painttex
<
        string ResourceName = "p:\projects\orkid\data\particle01.dds";
>;

///////////////////////////////////////////////////////////////////////////////

sampler2D PaintMapSampler = sampler_state
{
        Texture = <painttex>;
        MagFilter = LINEAR;
        MinFilter = LINEAR;
        MipFilter = LINEAR; // LINEAR NONE
};


///////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
        Texture = <ColorMap>;
        MagFilter = LINEAR;
        MinFilter = LINEAR;
        MipFilter = LINEAR; // LINEAR NONE
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleVertex
{
        float3 Position                : POSITION;
        float4 Color                   : COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleTexVertex
{
        float3 Position                : POSITION;
        float4 UV0                     : TEXCOORD0;
        float4 Color                   : COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

struct VCTVertex
{
float3 Position                : POSITION;
float4 UV0                     : TEXCOORD0;
float4 Color                   : COLOR0;
};



///////////////////////////////////////////////////////////////////////////////

struct Vertex
{
        float3 Position                : POSITION;
        float4 Color                : COLOR0;
        float4 NormalBIDX        : COLOR1;
        float4 UV0                        : TEXCOORD0;
        float4 Normal_S                : NORMAL;
        float4 Binormal_T        : BINORMAL;
        float4 FUV                        : TEXCOORD5;
        int    BoneIndex        : BLENDINDICES0;
        float4 VertexID                : TEXCOORD6;
        float4 FaceID                : COLOR1;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxOut
{
        float4 ClipPos : Position;
        float4 Color   : Color;
        float4 UV0     : TEXCOORD0;
};

///////////////////////////////////////////////////////////////////////////////

float4 PSFragColor( VtxOut FragIn ) : COLOR
{
        float4 PixOut;
        PixOut = FragIn.Color;
        return PixOut;
}

float4 PSTexColor( VtxOut FragIn ) : COLOR
{
        float4 PixOut;
        float4 texA = tex2D( ColorMapSampler, FragIn.UV0.xy );
        PixOut = float4( texA.xyz*FragIn.Color.xyz, FragIn.Color.w );
        //PixOut = float4( FragIn.UV0.xy, 0.0f, 1.0f );
        return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVCTColorTex( VCTVertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = VtxIn.Color.bgra;
        FragOut.UV0 = 4.0f*VtxIn.UV0;
        return FragOut;
}

VtxOut VSTexColor( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = VtxIn.Color.bgra;
        FragOut.UV0 = VtxIn.UV0;
        return FragOut;
}


float4 PSTexColorA( VtxOut FragIn ) : COLOR
{
                float4 texUV = float4( FragIn.UV0.xy, 0.0f, mipbias );

        float4 texA = tex2Dbias( ColorMapSampler, texUV );
        float u = time+(FragIn.UV0.x*20.0f);
        float v = FragIn.UV0.y*20.0f;
        float sinu = 0.5f+0.5f*sin( u );
        float sinv = 0.5f+0.5f*sin( v );
        float fval = (sinu*sinv)*(sinu*sinv);
        float3 bg = float3( fval,fval,fval );

        float4 PixOut = float4( (texA.xyz*texA.w)+bg*(1.0f-texA.w), 1.0f );
        return PixOut;
}

float4 PSTexViewAlpha( VtxOut FragIn ) : COLOR
{
                float4 texUV = float4( FragIn.UV0.xy, 0.0f, mipbias );

        float4 texA = tex2Dbias( ColorMapSampler, texUV );
        float u = time+(FragIn.UV0.x*20.0f);
        float v = FragIn.UV0.y*20.0f;
        float sinu = 0.5f+0.5f*sin( u );
        float sinv = 0.5f+0.5f*sin( v );
        float fval = (sinu*sinv)*(sinu*sinv);
        float3 bg = float3( fval,fval,fval );

        float4 PixOut = float4( (texA.xyz*texA.w)+bg*(1.0f-texA.w), 1.0f );
        return PixOut;
}

float4 PSTexViewNoAlpha( VtxOut FragIn ) : COLOR
{
        float4 texUV = float4( FragIn.UV0.xy, 0.0f, mipbias );
        float4 texA = tex2Dbias( ColorMapSampler, texUV );
        float4 PixOut = float4( texA.xyz, 1.0f );
        return PixOut;
}

float4 PSTexViewOnlyAlpha( VtxOut FragIn ) : COLOR
{
        float4 texUV = float4( FragIn.UV0.xy, 0.0f, mipbias );
        float4 texA = tex2Dbias( ColorMapSampler, texUV );
        float4 PixOut = texA.wwww;
        return PixOut;
}

float4 PSTexViewAlphaBlend( VtxOut FragIn ) : COLOR
{
        float4 texUV = float4( FragIn.UV0.xy, 0.0f, mipbias );
        float4 texA = tex2Dbias( ColorMapSampler, texUV );
        float4 PixOut = texA.xyzw;
        return PixOut;
}

technique texviewBlend
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVCTColorTex();
                PixelShader = compile ps_2_0 PSTexViewAlphaBlend();

                //CullMode = CW;
                //ZEnable = true;
                //ZWriteEnable = true;
                //AlphaBlendEnable = TRUE;
                //AlphaTestEnable = TRUE;
                //AlphaFunc = GREATER;
                //AlphaRef = 0;
                //SrcBlend = SRCALPHA;
                //DestBlend = INVSRCALPHA;

                //StencilTestEnable = FALSE;
        }
};

technique texviewTransBlend
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVCTColorTex();
                PixelShader = compile ps_2_0 PSTexViewAlphaBlend();

                //CullMode = CCW;
                //ShadeMode = GOURAUD;
                //ZEnable = true;
                //ZWriteEnable = false;
                //AlphaBlendEnable = TRUE;
                //AlphaTestEnable = TRUE;
                //AlphaFunc = GREATER;
                //AlphaRef = 0;
                //SrcBlend = SRCALPHA;
                //DestBlend = INVSRCALPHA;
                //StencilTestEnable = FALSE;
        }
        pass p1
        {
                VertexShader = compile vs_2_0 VSVCTColorTex();
                PixelShader = compile ps_2_0 PSTexViewAlphaBlend();

                //CullMode = CW;
                //ShadeMode = GOURAUD;
                //ZEnable = true;
                //ZWriteEnable = false;
                //AlphaBlendEnable = TRUE;
                //AlphaTestEnable = TRUE;
                //AlphaFunc = GREATER;
                //AlphaRef = 0;
                //SrcBlend = SRCALPHA;
                //DestBlend = INVSRCALPHA;
                //StencilTestEnable = FALSE;
        }
};

technique texviewAlpha
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVCTColorTex();
                PixelShader = compile ps_2_0 PSTexViewAlpha();

                //CullMode = NONE; // NONE CW CCW
                //ShadeMode = GOURAUD;
                //ZEnable = true;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

                //StencilTestEnable = FALSE;
        }
};
technique texviewNoAlpha
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVCTColorTex();
                PixelShader = compile ps_2_0 PSTexViewNoAlpha();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;

                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

                //StencilTestEnable = FALSE;
        }
};
technique texviewOnlyAlpha
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVCTColorTex();
                PixelShader = compile ps_2_0 PSTexViewOnlyAlpha();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;

                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

                //StencilTestEnable = FALSE;
        }
};
technique texcolor
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSTexColor();
                PixelShader = compile ps_2_0 PSTexColor();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;

                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

                //StencilTestEnable = FALSE;
        }
};

technique texcolorDX7
{
        pass p0
        {
                VertexShader = compile vs_1_1 VSTexColor();

                Texture[0] = <ColorMap>;
                TextureTransformFlags[0] = DISABLE;
                TexCoordIndex[0] = 0;
                ResultArg[0] = CURRENT;
                ColorOp[0] = MODULATE;
                ColorArg1[0] = DIFFUSE;
                ColorArg2[0] = TEXTURE;
                ColorOp[1] = DISABLE;

                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;
                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;

        }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVCTSkinned( Vertex VtxIn )
{
        VtxOut FragOut;

        int iboneindex = VtxIn.NormalBIDX.a; // 0 .. 15
        //e tweak u bonehead int iboneindex = 0; // 0 .. 15

        float4 vin = float4( VtxIn.Position, 1.0f );
        float4 vbone = mul( vin, bones[iboneindex] );
        FragOut.ClipPos = mul( vbone, mvp );
        FragOut.Color = VtxIn.Color;
        FragOut.UV0 = VtxIn.UV0;
        return FragOut;
}

technique vctskinned
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVCTSkinned();
                PixelShader = compile ps_2_0 PSFragColor();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

        }
};

technique vctskinnedDX7
{
        pass p0
        {
                VertexShader = compile vs_1_1 VSVCTSkinned();

                ResultArg[0] = CURRENT;
                ColorOp[0] = MODULATE;
                ColorArg1[0] = TEXTURE;
                ColorArg2[0] = DIFFUSE;
                ColorOp[1] = DISABLE;

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

        }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSST( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = float4( VtxIn.Normal_S.x, VtxIn.Normal_S.y, VtxIn.Normal_S.w, VtxIn.Binormal_T.w );
        FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        return FragOut;
}
float4 PSST( VtxOut FragIn ) : COLOR
{
        float4 PixOut;
        float2 ST = float2( FragIn.Color.xy );

        float HiS = ST.x;
        float LoS = frac( (ST.x*256.0f) );
        float HiT = ST.y;
        float LoT = fmod( (ST.y*256.0f), 1.0f );

        PixOut = float4( FragIn.Color );
        //PixOut = float4( LoS, LoT, HiS, HiT );
        return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSNormal( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = float4( VtxIn.Normal_S.xyz, 1.0f );
        FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        return FragOut;
}

float4 PSNormal( VtxOut FragIn ) : COLOR
{
        float4 PixOut;
        float3 Normal = normalize( FragIn.Color.xyz );
        float2 nrm = float2( Normal.xy );

        float HiS = nrm.x;
        float LoS = fmod( (nrm.x*256.0f), 1.0f );
        float HiT = nrm.y;
        float LoT = fmod( (nrm.y*256.0f), 1.0f );

        //PixOut = float4( LoS, LoT, HiS, HiT );
        PixOut = float4( 0.5f+(Normal*0.5f), 1.0f );
        return PixOut;
}

technique normal
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSNormal();
                PixelShader = compile ps_2_0 PSNormal();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;

        }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSFID( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = VtxIn.FaceID;
        FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVID( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = VtxIn.VertexID;
        FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSUV0Color( Vertex VtxIn )
{
    VtxOut FragOut;

    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
    FragOut.Color = VtxIn.UV0;
    FragOut.UV0 = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxColor( SimpleTexVertex VtxIn )
{
    VtxOut FragOut;
	float4x4 MatMVP = MatP*MatMV;
	FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), MatMVP );
    FragOut.Color = VtxIn.Color.argb;
    FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSVtxModColor( SimpleTexVertex VtxIn )
{
    VtxOut FragOut;
	float4x4 MatMVP = MatMV*MatP;
	FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), MatMVP );
    FragOut.Color = VtxIn.Color.argb*modcolor;
    FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSModColor( Vertex VtxIn )
{
    VtxOut FragOut;

	float4x4 MatMVP = MatMV*MatP;
	FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), MatMVP );
    FragOut.Color = modcolor;
    FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSObject( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = modcolor.zyxw; // swizzle for pointer retrieval
        FragOut.UV0 = float4( 0.0f, 0.0f, 0.0f, 0.0f );
        return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

VtxOut VSOrthoUI( Vertex VtxIn )
{
        VtxOut FragOut;
        const float2 ClipScale = float2 ( 2.0f, -2.0f );
        const float2 ClipBias = float2 ( -1.0f, 1.0f );
        FragOut.ClipPos = float4( (ClipBias+ClipScale*VtxIn.Position.xy), 0.0f, 1.0f );
        FragOut.Color = float4( VtxIn.Position.xy, 0.0f, 1.0f );
        FragOut.UV0 = float4( VtxIn.Position.xy, 0.0f, 0.0f );
        return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

technique object
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSObject();
                PixelShader = compile ps_2_0 PSFragColor();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = FLAT;
                ZEnable = true;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;
        }
};

///////////////////////////////////////////////////////////////////////////////

technique vtxcolor
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSVtxColor();
        PixelShader = compile ps_2_0 PSFragColor();

        CullMode = NONE; // NONE CW CCW
        //ShadeMode = GOURAUD;
        //ZEnable = true;
        //AlphaBlendEnable = FALSE;
        //AlphaTestEnable = FALSE;
    }
};

technique vtxmodcolor
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSVtxModColor();
        PixelShader = compile ps_2_0 PSFragColor();
    }
};

technique vtxcolortex
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSTexColor();
        PixelShader = compile ps_2_0 PSTexColor();

        CullMode = NONE; // NONE CW CCW
        ShadeMode = GOURAUD;
        ZEnable = true;
        AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
    }
};
///////////////////////////////////////////////////////////////////////////////

technique modcolor
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSModColor();
        PixelShader = compile ps_2_0 PSFragColor();
        CullMode = NONE; // NONE CW CCW
        ZEnable = true;
        ShadeMode = GOURAUD;
        //AlphaBlendEnable = FALSE;
        AlphaTestEnable = FALSE;
    }
};

///////////////////////////////////////////////////////////////////////////////

technique st
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSST();
                PixelShader = compile ps_2_0 PSST();

                CullMode = NONE; // NONE CW CCW
                ZEnable = true;
                ShadeMode = GOURAUD;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;
        }
};

///////////////////////////////////////////////////////////////////////////////

technique vertexID
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSVID();
                PixelShader = compile ps_2_0 PSFragColor();

                CullMode = NONE; // NONE CW CCW
                ZEnable = true;
                ShadeMode = FLAT;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;
        }
};

///////////////////////////////////////////////////////////////////////////////

technique faceID
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSFID();
                PixelShader = compile ps_2_0 PSFragColor();

                CullMode = NONE; // NONE CW CCW
                ZEnable = true;
                ShadeMode = FLAT;
                AlphaBlendEnable = FALSE;
                AlphaTestEnable = FALSE;
        }
};

///////////////////////////////////////////////////////////////////////////////

float4 PSCircleBrush( VtxOut FragIn ) : COLOR
{
        float4 PixOut;
        float3 texA = tex2D( PaintMapSampler, FragIn.UV0.xy );
        const float2 Center = float2( 0.5f, 0.5f );
        float distX = Center.x - FragIn.UV0.x;
        float distY = Center.y - FragIn.UV0.y;
        float dist = (distX*distX)+(distY*distY);
        float d2 = (dist*dist*dist*dist);
        float fout = BrushPressure*0.1f*(1.0f-(64.0f*d2));
        PixOut = float4( fout*FragIn.UV0.xy, 0.0f, 1.0f );
        return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

technique ortho2D
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSOrthoUI();
                PixelShader = compile ps_2_0 PSCircleBrush();

                CullMode = NONE; // NONE CW CCW
                ZEnable = true;
                ShadeMode = GOURAUD;

                AlphaBlendEnable = TRUE;
                AlphaTestEnable = FALSE;
                //AlphaBlendEnable = TRUE;
                SrcBlend = ONE;
                DestBlend = ONE;
                AlphaTestEnable = FALSE;


                //AlphaFunc = GREATER;
                //AlphaRef = 1;

                //PointScaleEnable = FALSE;
                //PointSize = 4;
                //PointSpriteEnable = TRUE;
        }
};

///////////////////////////////////////////////////////////////////////////////

VtxOut VSModCircle( Vertex VtxIn )
{
        VtxOut FragOut;

        FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), mvp );
        FragOut.Color = float4( 1.0f, 1.0f, 1.0f, 1.0f );
        FragOut.UV0 = float4( 0.5f, 0.5f, 0.0f, 0.0f );
        return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 PSCircle( VtxOut FragIn ) : COLOR
{
        float4 PixOut;
        const float2 Center = float2( 0.5f, 0.5f );
        float distX = Center.x - FragIn.UV0.x;
        float distY = Center.y - FragIn.UV0.y;
        float dist = (distX*distX)+(distY*distY);
        float d2 = 1.0f-dist;
        float fout = pow(d2,10.0f);
        PixOut = float4( fout, fout, fout, 1.0f );
        return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

technique modcolorcircle
{
        pass p0
        {
                VertexShader = compile vs_2_0 VSModCircle();
                PixelShader = compile ps_2_0 PSCircle();

                CullMode = NONE; // NONE CW CCW
                ShadeMode = GOURAUD;
                ZEnable = true;
                AlphaBlendEnable = TRUE;
                SrcBlend = ONE;
                DestBlend = ONE;
                AlphaTestEnable = TRUE;
                AlphaFunc = GREATER;
                AlphaRef = 1;

        }
};

///////////////////////////////////////////////////////////////////////////////
