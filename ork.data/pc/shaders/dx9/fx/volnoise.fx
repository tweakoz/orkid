///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "tanspace.fxi"

///////////////////////////////////////////////////////////////////////////////

uniform float time : reltime;
uniform float4		ModColor : modcolor;

///////////////////////////////////////////////////////////////////////////////
// artist parameters

uniform float		UvScaleClr	;
uniform float		UvScaleNseA	;
uniform float		UvScaleNseB	;
uniform float		UvScaleNseC	;

uniform float		UvNseSpeedA	;
uniform float		UvNseSpeedB	;
uniform float		UvNseSpeedC	;

uniform float		AlphaThresh	;

///////////////////////////////////////////////////////////////////////////////

uniform texture2D ColorMap;
uniform texture3D VolumeMap;

sampler2D ColorMapSampler = sampler_state
{
	Texture = <ColorMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
	//WrapS = Repeat;
    //WrapT = Repeat;
};

sampler3D VolumeMapSampler = sampler_state
{
	Texture = <VolumeMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
    MipFilter = LINEAR;
    //MagFilter = Linear;
    //MinFilter = Nearest;
	//WrapS = Repeat;
    //WrapT = Repeat;
    //WrapR = Repeat;
};

///////////////////////////////////////////////////////////////////////////////

struct Fragment
{
    float4 ClipPos : POSITION;  // in clip space
    float4 Color : COLOR;

    float3 UVW0             : TEXCOORD0;
    float3 UVW1             : TEXCOORD1;
    float3 UVW2             : TEXCOORD2;
    
    float2 UVC             : TEXCOORD3;
};

struct Vertex
{
    float4 Position : POSITION;   // in object space
    float4 TexCoord : TEXCOORD0;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR0;
};

///////////////////////////////////////////////////////////////////////////////

Fragment vs_volnoise( Vertex VtxIn )
{
    Fragment FragOut;
	
	float ftime = fmod( time, 10000 );

    //////////////////////////////////////

	float fscale = 1.0f; //600.0f;

	float3 ObjPos = VtxIn.Position.xyz;

    float3 camPos = mul(VtxIn.Position,WorldView).xyz;
    float3 worldPos = mul(VtxIn.Position,World).xyz;
                
    //////////////////////////////////////
        
    float3 worldNormal  = normalize( mul( World, float4( VtxIn.Normal,0.0f ) ).xyz );
    float3 worldEye     = ViewInverseTranspose[3].xyz;
    float3 worldToEye   = normalize( worldEye - worldPos );
    float3 worldToLyt   = worldToEye; //normalize( - SunDir );

    /////////////////////////////////////

	float3 OffsetX = float3( 1.0f, 0.0f, 0.0f );
	float3 OffsetY = float3( 0.0f, 1.0f, 0.0f );
	float3 OffsetZ = float3( 0.0f, 0.0f, 1.0f );

    FragOut.UVW0.xyz = float3( ObjPos.xyz*UvScaleNseA*fscale+(OffsetX*ftime*UvNseSpeedA) );
    FragOut.UVW1.xyz = float3( ObjPos.xyz*UvScaleNseB*fscale+(OffsetY*ftime*UvNseSpeedB) );
    FragOut.UVW2.xyz = float3( ObjPos.xyz*UvScaleNseC*fscale+(OffsetZ*ftime*UvNseSpeedC) );

    FragOut.UVC.xy = VtxIn.TexCoord.xy;

    /////////////////////////////////////

    FragOut.Color = float4(1.0f,1.0f,1.0f,1.0f);

    FragOut.ClipPos = mul(VtxIn.Position,WorldViewProjection);

    /////////////////////////////////////

    return FragOut;
}
    
///////////////////////////////////////////////////////////////////////////////

float4 ps_volnoise( Fragment FragIn ) : COLOR
{	
	float2 UvA = FragIn.UVW0.xy*UvScaleClr;
	
	float3 rgb = tex3D( VolumeMapSampler, FragIn.UVW0.xyz ).xyz;
	       rgb += tex3D( VolumeMapSampler, FragIn.UVW1.xyz ).xyz;
	       rgb += tex3D( VolumeMapSampler, FragIn.UVW2.xyz ).xyz;
	
	rgb *= 0.333f;
	
	
	float3 tclr = tex2D(ColorMapSampler, float2(rgb.r*rgb.r,0.5f) ).xyz;
	
	float4 mrtout = float4( tclr, rgb.r );

	//mrtout += float4( 0.1f, 0.0f, 0.0f, 1.0f );
    return mrtout;
	
}

///////////////////////////////////////////////////////////////////////////////

technique volnoise_objspace
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_volnoise();
		PixelShader = compile ps_3_0 ps_volnoise();

        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
		AlphaBlendEnable = true;

		CullMode = NONE;
		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
		//ZWriteEnable = true;
		AlphaTestEnable = TRUE;
        AlphaRef = (255*AlphaThresh);
        AlphaFunc = GREATER;
		
	}
}

technique volnoise_additive_objspace
{
	pass p0
	{
		VertexShader = compile vs_3_0 vs_volnoise();
		PixelShader = compile ps_3_0 ps_volnoise();

        SrcBlend = ONE;
        DestBlend = ONE;
		AlphaBlendEnable = true;

		CullMode = NONE;
		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
		//ZWriteEnable = true;
		AlphaTestEnable = TRUE;
        AlphaRef = (255*AlphaThresh);
        AlphaFunc = GREATER;
		
	}
}


///////////////////////////////////////////////////////////////////////////////

#include "pick.fxi"

///////////////////////////////////////////////////////////////////////////////
