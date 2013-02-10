///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include "vtxfragstructs.fxi"
#include "common.fxi"
#include "lighting.fxi"

///////////////////////////////////////////////////////////////////////////////
// changed per instance

float4x4    WVPMatrix;
float4x4    WMatrix;
float4x4    PMatrix;
float4x4    WVMatrix;
float4x4	IWMatrix;
float4x4    MatMV;
float3x3    WRotMatrix;

///////////////////////////////////////////////////////////////////////////////
// changed per view

float4x4    MatP;
float4x4    VPMatrix;
float4x4    VMatrix;

///////////////////////////////////////////////////////////////////////////////

float4x4	DiffuseMapMatrix;
float4x4	NormalMapMatrix;
float4x4	SpecularMapMatrix;
float4		WCamLoc;

float4      modcolor;
float		time;
float		SpecularPower;

///////////////////////////////////////////////////////////////////////////////

//#define PERPIXDIFFUSE

///////////////////////////////////////////////////////////////////////////////

float4		EmissiveColor;

///////////////////////////////////////////////////////////////////////////////
// Lighting Info

uniform int			NumDirectionalLights;
uniform float4		DirectionalLightPos[8];
uniform float4		DirectionalLightDir[8];
uniform float4		DirectionalLightColor[8];
uniform float4		DirectionalAttenA[8];
uniform float4		DirectionalAttenK[8];
uniform float		LightMode[8];
uniform float3		AmbientLight;

///////////////////////////////////////////////////////////////////////////////

FragClrUv3 VSTexColor( SimpleTexVertex VtxIn )
{
    FragClrUv3 FragOut;

    float4 WorldPos = mul( float4( VtxIn.Position, 1.0f ), WMatrix );
    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), WVPMatrix );
            
    FragOut.Color = VtxIn.Color.bgra*modcolor.rgba;
    FragOut.UV0 = mul( float4( VtxIn.UV0, 0.0f, 1.0f ), DiffuseMapMatrix ).xy;
    FragOut.UV1 = VtxIn.Position;
    FragOut.UV2 = VtxIn.Position;        
    FragOut.refl = float2(0.0f,0.0f);  
    FragOut.UVD = float4( 0.0f, 0.0f, FragOut.ClipPos.z, 0.0f );      
    FragOut.NrmD = float4( 0.0f,0.0f,0.0f, 0.0f ); 
    FragOut.WldP = WorldPos.xyz;
	return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

float3 PerVertexDiffuseLight( int ilight, float3 WorldNormal )
{
	float DiffuseDot = saturate(dot(-DirectionalLightDir[ilight].xyz,WorldNormal));
	return DirectionalLightColor[ilight]*float3(DiffuseDot,DiffuseDot,DiffuseDot);
}

///////////////////////////////////////////////////////////////////////////////

float3 DiffuseLight( float3 WorldPos, float3 WorldNormal )
{
	float3 DiffuseAccum = float3( 1.0f, 1.0f, 1.0f );

	int inl = 1;

	if( inl )
	{
		DiffuseAccum = float3( 0.0f, 0.0f, 0.0f );
	}
	
	//for( int i=0; i<inl; i++ )
	int i = 1;
	{
		float3 lpos = DirectionalLightPos[i].xyz;
		float3 ldir = DirectionalLightDir[i].xyz;
		///////////////////////////////////////////////////
		float3 dlpwp = (WorldPos.xyz-lpos.xyz);
		float3 PosToLight = normalize(dlpwp);	
		///////////////////////////////////////////////////
		float CosaP = dot(-PosToLight,WorldNormal);
		float CosaD = saturate(dot(WorldNormal,-ldir));
		float Cosa = lerp( CosaD, CosaP, LightMode[i] );
		float numer = max( 0.0f, 
		                DirectionalAttenA[i].x
		              + Cosa*DirectionalAttenA[i].y
		              + Cosa*Cosa*DirectionalAttenA[i].z );
		///////////////////////////////////////////////////
		float Dist = length(dlpwp);
		float denom = DirectionalAttenK[i].x
		            //+ Dist*DirectionalAttenK[i].y
		            + pow(Dist,DirectionalAttenK[i].z);
		///////////////////////////////////////////////////
		// WII emu
		// float Diffuse = saturate(dot(WorldNormal,-ldir));
		// float atten = Diffuse*numer/denom;
		///////////////////////////////////////////////////
		float atten = numer/denom;
		///////////////////////////////////////////////////
		///////////////////////////////////////////////////
		DiffuseAccum += DirectionalLightColor[i]*float3(atten,atten,atten);
	}

	return DiffuseAccum*modcolor.xyz;
}
	
///////////////////////////////////////////////////////////////////////////////

const float dofpower = 1.0f;

FragClrUv3 VSLambertTex( VertexNrmTex VtxIn )
{
    FragClrUv3 FragOut;

    float4 WorldPos = mul( float4( VtxIn.Position, 1.0f ), WMatrix );
    float3 WorldNormal = mul( VtxIn.Normal, WRotMatrix );
    
    float4 ClipPos = mul( float4( VtxIn.Position, 1.0f ), WVPMatrix );
    FragOut.ClipPos = ClipPos;
            	
   // float3 worldEye  = transpose(VMatrix)[3].xyz;
    float3 worldEye     = float3( VMatrix[0].x, VMatrix[0].y, VMatrix[0].z ) ;

#ifdef PERPIXDIFFUSE
    FragOut.Color = float4( VtxIn.Color.rgb*modcolor.rgb, modcolor.a ); //;
#else
	float3 DiffuseAccum = DiffuseLight( WorldPos, WorldNormal );
    FragOut.Color = float4( VtxIn.Color.rgb*modcolor.rgb*DiffuseAccum, modcolor.a ); //;
    //FragOut.Color = float4( modcolor.rgb*DiffuseAccum, modcolor.a ); //;
#endif
 	 
    FragOut.UV0 = mul( float4( VtxIn.UV0, 0.0f, 1.0f ), DiffuseMapMatrix ).xy;
    FragOut.UV1 = WorldPos;
    FragOut.UV2 = VtxIn.Normal;        
    FragOut.refl = float2(0.0f,0.0f);        
    FragOut.UVD = float4( 0.0f, 0.0f, FragOut.ClipPos.z, 0.0f );
    FragOut.NrmD = float4( WorldNormal, ClipPos.w ); 
    FragOut.WldP = WorldPos;
    
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

FragClr vs_wnormal( VertexNrmTex VtxIn )
{
    FragClr FragOut;
    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), WVPMatrix );
	float3 wnorm = mul( VtxIn.Normal.xyz, WRotMatrix );
    FragOut.Color = float4(normalize(wnorm.xyz),1.0f);
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

#if 1
FragClrUv3 VSLambertTexSkinned( VertexNrmTexSkinned VtxIn )
{
    FragClrUv3 FragOut;
		
	float3 ObjectVertex = SkinPosition( VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Position );
	float3 ObjectNormal = SkinNormal( VtxIn.BoneIndices, VtxIn.BoneWeights, VtxIn.Normal );
	float3 WorldNormal = mul( ObjectNormal, WRotMatrix );
    
    
    float3 ViewNormal = normalize( mul( float4( WorldNormal, 0.0f ), VMatrix ).xyz );
    
	float4 WorldPos = mul( float4( ObjectVertex.xyz, 1.0f ), WMatrix );
    FragOut.ClipPos = mul( WorldPos, VPMatrix );

	float3 wEyePos	= WCamLoc.xyz;
	float3 wVtx2Eye	= normalize( wEyePos-WorldPos.xyz );

	float3 DiffuseAccum = float3( 1.0f, 1.0f, 1.0f );
	
	int inl = max( NumDirectionalLights, 1 );

	if( inl )
	{
		DiffuseAccum = float3( 0.0f, 0.0f, 0.0f );
	}

	// aattn is the cosine of the angle between the light direction
	//   and the vector from the light position to the vertex%
	
	for( int i=0; i<inl; i++ )
	{
		float3 lpos = DirectionalLightPos[i].xyz;
		float3 ldir = DirectionalLightDir[i].xyz;
		///////////////////////////////////////////////////
		float3 dlpwp = (WorldPos.xyz-lpos.xyz);
		float3 PosToLight = normalize(dlpwp);	
		///////////////////////////////////////////////////
		float Cosa = saturate(dot(PosToLight,ldir));
		float numer = max( 0.0f, 
		                DirectionalAttenA[i].x
		              + Cosa*DirectionalAttenA[i].y
		              + Cosa*Cosa*DirectionalAttenA[i].z );
		///////////////////////////////////////////////////
		float Dist = length(dlpwp);
		float denom = DirectionalAttenK[i].x
		            + Dist*DirectionalAttenK[i].y
		            + Dist*Dist*DirectionalAttenK[i].z;
		///////////////////////////////////////////////////
		float Diffuse = saturate(dot(WorldNormal,-ldir));
		float atten = Diffuse*numer/denom;
		///////////////////////////////////////////////////
		DiffuseAccum += saturate(normalize(DirectionalLightColor[i]))*float3(atten,atten,atten);
	}
                  
    FragOut.UV0 = mul( float4( VtxIn.UV0, 0.0f, 1.0f ), DiffuseMapMatrix ).xy;
	FragOut.UV1 = FragOut.UV0;
	FragOut.UV2 = FragOut.UV0;
    //FragOut.UV0 *= float2( 1.0f, -1.0f );
    
    //DualParaboloidEnv( WorldNormal, wVtx2Eye, FragOut.UV1, FragOut.UV2, FragOut.refl.x );

	FragOut.refl.x = 0.0f;
	FragOut.refl.y = (1.0f - pow( abs(dot( ViewNormal, float3(0.0f,0.0f,1.0f) )), 2.0f ));

	float headlight = dot( ViewNormal, float3(0.0f,0.0f,-1.0f) );

	float dist = pow( abs(FragOut.ClipPos.z/FragOut.ClipPos.w), dofpower );

    FragOut.Color = saturate( float4( saturate(DiffuseAccum)*modcolor.rgb, modcolor.a ) );
    FragOut.UVD = float4( 0.0f, 0.0f, FragOut.ClipPos.z, dist );      
    FragOut.NrmD = float4( WorldNormal, dist ); 
	FragOut.WldP = WorldPos.xyz;
	
    //float4 ClipPos : Position;
   // float4 Color   : Color;
    //float2 UV0     : TEXCOORD0;
    //float2 UV1     : TEXCOORD1;
    //float2 UV2     : TEXCOORD2;
    //float2 refl    : TEXCOORD3;
    //float4 UVD     : TEXCOORD4;
    //float4 NrmD    : TEXCOORD5;
    //float3 WldP    : TEXCOORD6;


    return FragOut;
}
#endif

///////////////////////////////////////////////////////////////////////////////

MrtFxPixel PSLambertTex( FragClrUv3 FragIn )
{
    MrtFxPixel PixOut;
		
#ifdef PERPIXDIFFUSE
		
	float3 WorldPos = FragIn.WldP.xyz;
	float3 WorldNrm = FragIn.NrmD.xyz;
		
	float3 DiffuseAccum = DiffuseLight( WorldPos, WorldNrm );
    
#else
	float3 DiffuseAccum = FragIn.Color.xyz;
#endif    

    float4 texA = tex2D( DiffuseMapSampler, FragIn.UV0.xy );
	float3 color = (texA.xyz*DiffuseAccum);//+(reflection.xyz);

	float falpha = texA.w;

	//if( falpha < 0.5f )
	//{
	//	discard;
	//}	

	//PixOut.DiffuseBuffer =  float4( 1.0f,1.0f,1.0f,1.0f );
	PixOut.DiffuseBuffer =  float4( color, texA.w );
	PixOut.SpecularBuffer =  float4( 0.0f, 0.0f, FragIn.UVD.z, texA.w );
	PixOut.NormalDepthBuffer =  float4( FragIn.NrmD );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

FragClrUv3 VSModColor( VertexNrmTex VtxIn )
{
    FragClrUv3 FragOut;

	float4 ObjNormal = normalize( float4( VtxIn.Normal, 0.0f ) );
	float3 WldNormal = normalize( mul( ObjNormal, WMatrix ).xyz );
	
    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), WVPMatrix );
    
    float4 WldPos = mul( float4( VtxIn.Position, 1.0f ), WMatrix );
    
    FragOut.Color = modcolor;
    FragOut.UV0 = mul( float4( VtxIn.UV0, 0.0f, 1.0f ), DiffuseMapMatrix ).xy;
    FragOut.UV1 = FragOut.ClipPos.xy;
    FragOut.UV2 = FragOut.ClipPos.xy;
    FragOut.refl = float2(0.0f,0.0f);        
    FragOut.UVD = float4( WldNormal.xy, FragOut.ClipPos.z, WldNormal.z );      
    FragOut.NrmD = float4( WldNormal, 0.0f ); 
    FragOut.WldP = WldPos; 
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

FragClrUv3 VSVtxModColor( VertexNrmTex VtxIn )
{
    FragClrUv3 FragOut;

	float4 ObjNormal = normalize( float4( VtxIn.Normal, 0.0f ) );
	float3 WldNormal = normalize( mul( ObjNormal, WMatrix ).xyz );
	
    FragOut.ClipPos = mul( float4( VtxIn.Position, 1.0f ), WVPMatrix );
    
    float4 WldPos = mul( float4( VtxIn.Position, 1.0f ), WMatrix );
    
    FragOut.Color = modcolor*VtxIn.Color;
    FragOut.UV0 = mul( float4( VtxIn.UV0, 0.0f, 1.0f ), DiffuseMapMatrix ).xy;
    FragOut.UV1 = FragOut.ClipPos.xy;
    FragOut.UV2 = FragOut.ClipPos.xy;
    FragOut.refl = float2(0.0f,0.0f);        
    FragOut.UVD = float4( WldNormal.xy, FragOut.ClipPos.z, WldNormal.z );      
    FragOut.NrmD = float4( WldNormal, 0.0f ); 
    FragOut.WldP = WldPos; 
    return FragOut;
}

///////////////////////////////////////////////////////////////////////////////

MrtPickPixel SelectorPS( FragClrUv3 FragIn )
{
    MrtPickPixel PixOut;
    float4 texA = tex2D( DiffuseMapSampler, FragIn.UV0.xy );
    PixOut.Color = modcolor;
    PixOut.UVD = FragIn.UVD;
    clip(texA.w-0.6f);
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 ps_fragcolor( FragClr FragIn ) : COLOR
{
    float4 PixOut = FragIn.Color.xyzw;
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

float4 SelectorColorPS( FragClrUv3 FragIn ) : COLOR
{
    float4 PixOut = float4( 0,0,0,0 );
    return PixOut;
}

///////////////////////////////////////////////////////////////////////////////

technique tek_lamberttex
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSLambertTex();
        PixelShader = compile ps_3_0 PSLambertTex();
        //CullMode = CCW; // NONE CW CCW
    }
};

///////////////////////////////////////////////////////////////////////////////

technique tek_lamberttex_skinned
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSLambertTexSkinned();
        PixelShader = compile ps_3_0 PSLambertTex();
    }
};

///////////////////////////////////////////////////////////////////////////////

technique tek_modcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSModColor();
        PixelShader = compile ps_3_0 SelectorPS();

//        CullMode = NONE; // NONE CW CCW
    //    ZEnable = true;
      //  AlphaBlendEnable = FALSE;
       // AlphaTestEnable = FALSE;
    }
};

///////////////////////////////////////////////////////////////////////////////

technique tek_wnormal
{
    pass p0
    {
        VertexShader = compile vs_3_0 vs_wnormal();
        PixelShader = compile ps_3_0 ps_fragcolor();
        //CullMode = CCW; // NONE CW CCW
    }
};

///////////////////////////////////////////////////////////////////////////////

technique tek_vtxmodcolor
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSVtxModColor();
        PixelShader = compile ps_3_0 ps_fragcolor();
    }
};

///////////////////////////////////////////////////////////////////////////////

technique tek_selector_color
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSTexColor();
        PixelShader = compile ps_3_0 SelectorColorPS();

        CullMode = NONE; // NONE CW CCW
        //ZEnable = true;
        //AlphaBlendEnable = TRUE;
        //AlphaTestEnable = TRUE;
        //AlphaFunc = GREATER;
        //AlphaRef = 0.7f;
		//SrcBlend  = SRCALPHA;       
		//DestBlend = ONE;
    
    }
};

///////////////////////////////////////////////////////////////////////////////
/*
technique tek_modcolor_skinned
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSLambertTexSkinned();
        PixelShader = compile ps_3_0 SelectorPS();
    }
};*/

///////////////////////////////////////////////////////////////////////////////

technique tek_selector_pick
{
    pass p0
    {
        VertexShader = compile vs_2_0 VSModColor();
        PixelShader = compile ps_2_0 SelectorPS();

        //CullMode = NONE; // NONE CW CCW
        //AlphaBlendEnable = FALSE;
    
    }
};
