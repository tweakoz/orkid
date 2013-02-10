///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include "tanspace.fxi"

uniform float4		ModColor : modcolor;

///////////////////////////////////////////////////////////////////////////////
// matrices

uniform float3 SunDir = float3( 0.0f, 1.0f, 0.0f );
uniform float2 AnIsoAng = float2( 1.0f, 0.0f );

texture2D ColorMap : Color;
texture2D SpecuMap : Specular;
texture2D AngleMap : Angle;

uniform float ColorUvScale;
uniform float SpecuUvScale;
uniform float AngleUvScale;
uniform float SpecularPower;
uniform float AngleMapScale;
uniform float AngleMapBias;

///////////////////////////////////////////////////////////////////////////////
// water parameters

uniform float time : reltime;

///////////////////////////////////////////////////////////////////////////////

struct Vertex {
    float4 Position : POSITION;   // in object space
    float4 TexCoord : TEXCOORD0;
    float3 Normal   : NORMAL;
    float3 Binormal : BINORMAL;
    float4 Color    : COLOR0;
};

struct Fragment {

    float4 ClipPos : POSITION;  // in clip space
    float4 Color : COLOR;

    float3 UVW0             : TEXCOORD0;
    float3 UVW1             : TEXCOORD1;
    float3 UVW2             : TEXCOORD2;

    float3 TanLight         : TEXCOORD3;
    float3 TanHalf          : TEXCOORD4;
    float3 TanEye           : TEXCOORD5;
    float3 VtxTangent       : TEXCOORD6;
    float3 VtxPosition      : TEXCOORD7;
};

////////////////////////////////////////////////////////////////////////////////

sampler2D ColorMapSampler = sampler_state
{
    Texture = <ColorMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
};

sampler2D SpecuMapSampler = sampler_state
{
    Texture = <SpecuMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
};

sampler2D AngleMapSampler = sampler_state
{
    Texture = <AngleMap>;
    MagFilter = LINEAR;
    MinFilter = LINEAR;
};

////////////////////////////////////////////////////////////////////////////////

Fragment AnIsoVS( Vertex VtxIn )
{
    Fragment FragOut;

    float ftime = fmod( time, 10000 );

    //////////////////////////////////////

    float3 camPos = mul(WorldView, VtxIn.Position ).xyz;
    float3 worldPos = mul(World,VtxIn.Position).xyz;

    //////////////////////////////////////

    float3 worldNormal  = normalize( mul( float4( VtxIn.Normal,0.0f ), World ).xyz );
    float3 worldEye     = transpose(ViewIT)[3].xyz;
    float3 worldToEye   = normalize( worldEye - worldPos );
    float3 worldToLyt   = worldToEye; //normalize( - SunDir );

    /////////////////////////////////////
    // ( Tangent Space Lighting )

	float3 nbin = normalize( VtxIn.Binormal );
	float3 nnrm = normalize( VtxIn.Normal );

	float3 Tangent = ( cross( nbin, nnrm ) );
	TanLytData tldata = GenTanLytData(	World,
										worldToLyt, worldToEye,
										VtxIn.Normal, VtxIn.Binormal, Tangent
									 );

    /////////////////////////////////////

    FragOut.TanLight = tldata.TanLyt;
    FragOut.TanHalf = tldata.TanHalf;
    FragOut.TanEye = tldata.TanEye;

    FragOut.VtxPosition = VtxIn.Position.xyz;
    FragOut.VtxTangent = Tangent.xyz;

    /////////////////////////////////////

    FragOut.UVW0.x = VtxIn.TexCoord.x*ColorUvScale;
    FragOut.UVW0.y = VtxIn.TexCoord.y*ColorUvScale;
    FragOut.UVW0.z = 0.0f;

    FragOut.UVW1.x = VtxIn.TexCoord.x*SpecuUvScale;
    FragOut.UVW1.y = VtxIn.TexCoord.y*SpecuUvScale;
    FragOut.UVW1.z = 0.0f;

    FragOut.UVW2.x = VtxIn.TexCoord.x*AngleUvScale;
    FragOut.UVW2.y = VtxIn.TexCoord.y*AngleUvScale;
    FragOut.UVW2.z = 0.0f;

    /////////////////////////////////////

    float diffuse = saturate( dot( worldNormal, worldToLyt.xyz ) );

    /////////////////////////////////////

    FragOut.Color.xyz = diffuse;
    //FragOut.Color.xyz = tldata.TanLyt.xyz;
    FragOut.Color.w = 1.0f;
    FragOut.ClipPos = mul(VtxIn.Position,WorldViewProjection);
    return FragOut;
}

////////////////////////////////////////////////////////////////////////////////

float4 AnIsoPS( Fragment FragIn ) : COLOR
{
    float4 PixelOut;

    float3 E = ( FragIn.TanEye  );
    float3 T = ( FragIn.VtxTangent );

    float3 texColor = tex2D( ColorMapSampler,  FragIn.UVW0.xy ).rgb;
    float3 texSpecu = tex2D( SpecuMapSampler,  FragIn.UVW1.xy ).rgb;
    float3 texAngle = tex2D( AngleMapSampler,  FragIn.UVW2.xy ).rgb;

    float fang = (AngleMapScale*texAngle.x)+AngleMapBias;
    float fx = sin(fang)+AnIsoAng.x;
    float fy = cos(fang)+AnIsoAng.y;

    float3 tang = normalize( float3( fx, fy, 0.0f ) );

    float cl = dot( - E, tang );
    float sl = sqrt(1 - cl * cl);
    float Specular = pow(saturate(sl), SpecularPower );

	float U = fmod( FragIn.UVW0.x, 1.0f );
	float V = fmod( FragIn.UVW0.y, 1.0f );

    PixelOut.rgb = (texColor*FragIn.Color).rgb+(texSpecu*Specular).rgb;
    PixelOut.a = 1.0f;
    return PixelOut;
}

///////////////////////////////////////////////////////////////////////////////

technique AnIsoLod0
{
    pass p0
    {
        VertexShader = compile vs_3_0 AnIsoVS( );
        PixelShader = compile ps_3_0 AnIsoPS( );

		//CullMode = CW; // CCW
		//ZFunc = LESS;
		//ZEnable = TRUE;
		//ZWriteEnable = true;
        //Zenable = true;
        //ZWriteEnable = true;
        //FrontFace = CCW; // ccw
        //FogEnable = false;
        //BlendEnable = false;
        //BlendFunc = int2( SrcAlpha, OneMinusSrcAlpha );
       // BlendFunc = int2(SrcAlpha,SrcColor        );
    }
}

///////////////////////////////////////////////////////////////////////////////

#include "pick.fxi"
