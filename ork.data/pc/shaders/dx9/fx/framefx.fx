///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

uniform float4x4 mvp;
uniform float4 modcolor;
uniform float time;
uniform float BlurFactor;
uniform int BlurFactorI;
uniform float2 viewportdim;
uniform float EffectAmount;

///////////////////////////////////////////////////////////////////////////////

uniform texture2D MrtMap0;
uniform texture2D MrtMap1;
uniform texture2D MrtMap2;
uniform texture2D MrtMap3;

uniform texture2D AuxMap0;
uniform texture2D AuxMap1;

uniform texture2D NoiseMap;

///////////////////////////////////////////////////////////////////////////////

sampler2D MrtMap0Sampler = sampler_state
{
	Texture = <MrtMap0>;
    MagFilter = Linear;
    MinFilter = Linear;
    MipFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler2D MrtMap1Sampler = sampler_state
{
	Texture = <MrtMap1>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler2D MrtMap2Sampler = sampler_state
{
	Texture = <MrtMap2>;
    MagFilter = Linear;
    MinFilter = Linear;
    MipFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler2D MrtMap3Sampler = sampler_state
{
	Texture = <MrtMap3>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler2D AuxMap0Sampler = sampler_state
{
	Texture = <AuxMap0>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler2D AuxMap1Sampler = sampler_state
{
	Texture = <AuxMap1>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler2D NoiseMapSampler = sampler_state
{
	Texture = <NoiseMap>;
    MagFilter = Linear;
    MinFilter = Linear;
    AddressU = WRAP;
    AddressV = WRAP;
};

///////////////////////////////////////////////////////////////////////////////
struct SimpleVertex
{
	float3 Position	: POSITION;
	float3 Uv0		: TEXCOORD0;
};

struct Fragment
{
    float4 ClipPos	: POSITION;
	float3 Uv0		: TEXCOORD0;
	float4 Color	: COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

Fragment vs_basic( SimpleVertex VtxIn )
{
    float4 vpos = float4( VtxIn.Position, 1.0f );
    float4 npos = mul( vpos, mvp );
    Fragment FragOut;
    FragOut.ClipPos = npos;
    FragOut.Uv0 = VtxIn.Uv0;
    FragOut.Color = modcolor;
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_edgeblur( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

	static const float kdx = 1.007f/1024.0f;
	static const float kdy = 1.007f/512.0f;
	static const float kf = 0.01f;
	

	float4 PixOut = float4(0.0f,0.0f,0.0f,1.0f);
	float dist = 0.0f;
	
    float4 PixCtr = tex2D( MrtMap0Sampler, Uv0 );

	static const float kaadim = 4.0f;
	static const float ksc = (1.0f/(kaadim*kaadim)); // 9 16 25 49

	static const float kfb = -2.0f;
	static const float kfe = -kfb;
	static const float kfed = (kfe-kfb)/kaadim;
	
	float fwt = 0.01f;
	
	for( float fy=0.0001f; fy<kaadim; fy+=1.0f )
	{
		float fya = kfb+(fy*kfed);
		
		for( float fx=0.0001f; fx<kaadim; fx+=1.0f )
		{
			float fxa = kfb+(fx*kfed);
		
			float fdistctr = length(float2(fxa,fya));
			float fweight = pow(length(float2(kfb,kfb))-fdistctr,2.0f);
			fwt += fweight;
			float2 Uv = Uv0 + float2( kdx*fxa, kdy*fya );
		    float4 ThisPix = tex2D( MrtMap0Sampler, Uv );
			dist += distance(ThisPix,PixCtr)*fweight;
			PixOut += ThisPix*fweight;
		}
	}

	PixOut = PixOut * (1.0f/fwt);
	dist *= (1.0f/fwt);
    
    float idist = (1.0f-dist);
    idist = pow(idist,5.0f);
    //PixOut = float4( idist, idist, idist, 1.0f );
    //PixOut = PixCtr*idist;
    //PixOut = lerp(PixCtr,PixOut,idist);
    PixOut = lerp(PixOut,PixCtr,idist);
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////
// Standard FrameEffect, just add Mrt0 and Mrt1 together

float4 ps_downsample( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

	static const float ksx = (1920.0f*2.0f);
	static const float ksy = (1280.0f*2.0f);
	static const float kdx = (1920.0f);
	static const float kdy = (1280.0f);

	static const float kisx = 1.007f/ksx;
	static const float kisy = 1.007f/ksy;
	static const float kidx = 1.007f/kdx;
	static const float kidy = 1.007f/kdy;

	static const float krx = ksx/kdx;
	static const float kry = ksy/kdy;
		
	float fwt = 0.01f;
	
	float4 PixOut = float4(0.0f,0.0f,0.0f,1.0f);

	for( float fy=0.0; fy<kry; fy++ )
	{
		float fya = (fy-(kry*0.5f))*kisy;
		
		for( float fx=0; fx<krx; fx++ )
		{
			float fxa = (fx-(krx*0.5f))*kisx;
		
			float2 Uv = Uv0 + float2( fxa, fya );

		    PixOut += tex2D( MrtMap0Sampler, Uv );

			fwt += 1.0f;
		}
	}

	PixOut = PixOut * (1.0f/fwt);
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////
float4 ps_radialblur( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );
	float4 f0 = tex2D( MrtMap0Sampler, Uv0 );
	return f0;
}

///////////////////////////////////////////////////////////////////////////////
// Standard FrameEffect, just add Mrt0 and Mrt1 together

float4 ps_standard( Fragment FragIn ) : COLOR
{
#if 0
    return ps_downsample( FragIn );
#elif 0
    return ps_edgeblur( FragIn );
#else
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );
    return tex2D( MrtMap0Sampler, Uv0 );
#endif
}

float4 ps_comic( Fragment FragIn ) : COLOR
{
	//static const float kd = 0.015f;
	//static const float kf = 0.3f;
	static const float kdx = 1.007f/1024.0f;
	static const float kdy = 1.007f/768.0f;
	static const float kf = 0.01f;
	const float fphase = 6.283*kf*time;

	
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

	float4 PixOut = float4(0.0f,0.0f,0.0f,1.0f);
	float dist = 0.0f;
	
    float4 PixCtr = tex2D( MrtMap0Sampler, Uv0 );

	static const float kaadim = 2.0f;
	static const float ksc = (1.0f/(kaadim*kaadim)); // 9 16 25 49

	static const float kfb = -1.5f;
	static const float kfe = -kfb;
	static const float kfed = (kfe-kfb)/kaadim;
	
	float fwt = 0.01f;
	
	for( float fy=0.0001f; fy<kaadim; fy+=1.0f )
	{
		float fya = kfb+(fy*kfed);
		
		for( float fx=0.0001f; fx<kaadim; fx+=1.0f )
		{
			float fxa = kfb+(fx*kfed);
		
			//float fdistctr = length(float2(fxa,fya));
			float fweight = 1.0f;//pow(length(float2(kfb,kfb))-fdistctr,2.0f);
			fwt += fweight;
			float2 Uv = Uv0 + float2( kdx*fxa, kdy*fya );
		    float4 ThisPix = tex2D( MrtMap0Sampler, Uv );
			dist += distance(ThisPix,PixCtr)*fweight;
			PixOut += ThisPix*fweight;
		}
	}

	PixOut = PixOut * (1.0f/fwt);
	dist *= (1.0f/fwt);
    
    float idist = (1.0f-dist);
    idist = pow(idist,3.0f);
    //PixOut = float4( idist, idist, idist, 1.0f );
    PixOut = PixCtr*idist;
    //PixOut = lerp(PixCtr,PixOut,idist);
    //PixOut = lerp(PixOut,PixCtr,idist);
	//PixOut =
	
	PixOut = lerp( PixCtr, PixOut, EffectAmount );
	
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////
// Standard FrameEffect, just add Mrt0 and Mrt1 together

float4 ps_debugdepth( Fragment FragIn ) : COLOR
{
	const float power = 48.0f;

	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );
	float4 ClipPosZ = tex2D( MrtMap1Sampler, Uv0.xy ).z;
	float4 ClipPosW = tex2D( MrtMap2Sampler, Uv0.xy ).w;
	float depth = pow( ClipPosZ/ClipPosW, power );
    float4 PixOut = float4( depth, depth, depth, 1.0f );
    return PixOut;
}

float4 ps_debugnormals( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );
	float3 nrm = tex2D( MrtMap2Sampler, Uv0.xy ).xyz;
	float3 outn = (nrm+float3(1.0f,1.0f,1.0f))*0.5f;
    float4 PixOut = float4( outn, 1.0f );
    return PixOut;
}

float4 ps_debugdeflit( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );	
	float4 WorldNormal_GlowZ = tex2D( MrtMap1Sampler, Uv0.xy );
	float4 ClipPos = tex2D( MrtMap2Sampler, Uv0.xy );
	
	//ClipPos.xy = (ClipPos.xy*0.5f)+float2(0.5f,0.5f);
	ClipPos.z /= ClipPos.w;
	ClipPos.z = pow( abs(ClipPos.z), 7.0f );
	ClipPos.xy /= viewportdim;
    
    //float4 PixOut = float4( WorldNormal_GlowZ.xyz*WorldNormal_GlowZ.w, 1.0f );
    float4 PixOut = float4( ClipPos.xyz, 1.0f );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_painterly( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

	const float fbase = 1.5f;
	const float ftimebase = 5.5f;
	const float2 fdir0 = float2( 0.0f, 0.02f )*ftimebase;
	const float2 fdir1 = float2( 0.005f, 0.015f )*ftimebase;
	const float2 fdir2 = float2( 0.007f, 0.005f )*ftimebase;

	float ftime = fmod( time, 100.0f )*0.5f;

	const float2 khalf = float2( 0.5f, 0.5f );

	float2	UvDistortion =  (tex2D( NoiseMapSampler, Uv0.xy*2.0f+(fdir0*ftime) ).xy-khalf)*0.025f*fbase;
			UvDistortion += (tex2D( NoiseMapSampler, Uv0.xy*3.3f+(fdir1*ftime) ).xy-khalf)*0.01f*fbase;
			UvDistortion += (tex2D( NoiseMapSampler, Uv0.xy*4.7f+(fdir2*ftime) ).xy-khalf)*0.005f*fbase;
	
	float2 RealUv = Uv0.xy+UvDistortion;
	
	float4 PixOut = tex2D( MrtMap0Sampler, RealUv );
	//float4 PixOut = float4( normalize( UvDistortion ), 0.0f, 1.0f );
    return PixOut;
}

////////////////////////////////////////////////////////////////////////////////

const uniform float stepval = (1.0f/512.0f);

////////////////////////////////////////////////////////////////////////////////

float4 ps_blurx( Fragment FragIn ) : COLOR
{
	const float glowscale = (1.0f/BlurFactor);
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

	float2 RealUv = Uv0.xy;
	float4 PixOut = float4(0.0f,0.0f,0.0f,0.0f);
	float2 UvOffset = float2(stepval*-BlurFactor,0.0f);
	float framp = 0.0f;
	for( int i=0; i<BlurFactorI; i++ )
	{
		PixOut += tex2D( MrtMap0Sampler, RealUv+UvOffset ).xyzw*framp;
		UvOffset += float2( stepval,0.0f );
		framp += (1/BlurFactor);
	}
	for( int i=0; i<BlurFactorI; i++ )
	{
		PixOut += tex2D( MrtMap0Sampler, RealUv+UvOffset ).xyzw*framp;
		UvOffset += float2( stepval,0.0f );
		framp -= (1/BlurFactor);
	}

	PixOut *= glowscale;
    return PixOut;
}

////////////////////////////////////////////////////////////////////////////////

float4 ps_blury( Fragment FragIn ) : COLOR
{
	const float glowscale = (1.0f/BlurFactor);
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

	float2 RealUv = Uv0.xy;
	float4 PixOut = float4(0.0f,0.0f,0.0f,0.0f);
	float2 UvOffset = float2(0.0f,stepval*-BlurFactor);
	float framp = 0.0f;
	for( int i=0; i<BlurFactorI; i++ )
	{
		PixOut += tex2D( AuxMap0Sampler, RealUv+UvOffset ).xyzw*framp;
		UvOffset += float2( 0.0f,stepval );
		framp += (1.0f/BlurFactor);
	}
	for( int i=0; i<BlurFactorI; i++ )
	{
		PixOut += tex2D( AuxMap0Sampler, RealUv+UvOffset ).xyzw*framp;
		UvOffset += float2( 0.0f,stepval );
		framp -= (1.0f/BlurFactor);
	}
	PixOut *= glowscale;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_hdr_join( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );
    float3 Diffuse = tex2D( MrtMap0Sampler, Uv0.xy ).xyz;
    float4 Glow = tex2D( AuxMap1Sampler, Uv0.xy );
    
    float3 Full = ((Diffuse+(Glow.rgb*Glow.a)))*.5f;
	float3 None = Diffuse;
	return float4( lerp(None,Full,EffectAmount), 1.0f ); 
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_hdr_join_ghost( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );
    float3 Diffuse = tex2D( MrtMap0Sampler, Uv0.xy ).xyz;
    float4 Glow = tex2D( AuxMap1Sampler, Uv0.xy );
	float dist = pow( saturate( distance( Glow.rgb*Glow.a, Diffuse ) ), 1.2f );
	
	float3 black = float3(0.0f,0.0f,0.0f);
	float3 blue = float3(0.0f,0.0f,1.0f);
	float3 white = float3(1.0f,1.0f,1.0f);
	
	float3	OutColor = lerp( black, blue, dist );
			OutColor = lerp( OutColor, white, dist );
			
	OutColor = lerp( Diffuse, OutColor, EffectAmount );
			
	return float4( OutColor, 1.0f ); 
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_hdr_join_dof( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

    float3 Diffuse = tex2D( MrtMap0Sampler, Uv0.xy ).xyz;
    float4 SpecularDepth = tex2D( MrtMap2Sampler, Uv0.xy );

	float Depth = 1.0f-SpecularDepth.w;
	
    float4 Glow = tex2D( AuxMap1Sampler, Uv0.xy );
					
	return float4( lerp( Diffuse, Glow.xyz, Depth ), 1.0f ); 
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_hdr_join_afterlife( Fragment FragIn ) : COLOR
{
	float2 Uv0 = float2( FragIn.Uv0.x, 1.0f-FragIn.Uv0.y );

    float3 Diffuse = tex2D( MrtMap0Sampler, Uv0.xy ).xyz;
    float4 SpecularDepth = tex2D( MrtMap2Sampler, Uv0.xy );

	float Depth = pow( abs(1.0f-SpecularDepth.w), 0.25f );
	
    float4 Glow = tex2D( AuxMap1Sampler, Uv0.xy );
					
	//return float4( Glow.xyz*Depth, 1.0f ); 
	return float4( Diffuse - Depth*Glow.xyz, 1.0f ); 
	//return float4( Depth, Depth, Depth, 1.0f ); 
}

//////////////////////////////////////////////////////////////////////////////

technique frameeffect_standard
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_standard();
    }
}

//////////////////////////////////////////////////////////////////////////////

technique frameeffect_comic
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_comic();
    }
}

//////////////////////////////////////////////////////////////////////////////

technique frameeffect_radialblur
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_radialblur();
    }
}

///////////////////////////////////////////////////////////////////////////////
/*
technique frameeffect_painterly
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_painterly();
    }
}
*/
///////////////////////////////////////////////////////////////////////////////

technique frameeffect_glow_blurx
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_blurx();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_glow_blury
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_blury();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_glow_join
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_hdr_join();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_ghost_join
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_hdr_join_ghost();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_dof_join
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_hdr_join_dof();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_afterlife_join
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_hdr_join_afterlife();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_debugdepth
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_debugdepth();
    }
}

///////////////////////////////////////////////////////////////////////////////

technique frameeffect_debugnormals
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_debugnormals();
    }
}

technique frameeffect_debugdeflit
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_basic();
        PixelShader = compile ps_3_0 ps_debugdeflit();
    }
}


///////////////////////////////////////////////////////////////////////////////
