////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_LEV2_GFXENV_ENUM_H_
#define _ORK_LEV2_GFXENV_ENUM_H_

namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

enum EField
{
	EFIELD_EVEN = 0,
	EFIELD_ODD,
};

///////////////////////////////////////////////////////////////////////////////

enum EGfxTargetNiceness
{
	EGFXNICEMODE_GAME = 0,	// the target is running as fast as it can
	EGFXNICEMODE_APP		// only refresh UI when dirty (use from external apps like audio plugins, etc..)
};

///////////////////////////////////////////////////////////////////////////////

enum ETargetType
{
	ETGTTYPE_WINDOW = 0, 
	ETGTTYPE_MRT0 = 0,		// windows are always MRT 0
	ETGTTYPE_MRT1 ,
	ETGTTYPE_MRT2 ,
	ETGTTYPE_MRT3 ,
	ETGTTYPE_EXTBUFFER ,	// external buffer (not attached to a MRT)
};

///////////////////////////////////////////////////////////////////////////////

enum EUIColorMode
{
	EUICOLOR_MOD = 0,
	EUICOLOR_VTX ,
	EUICOLOR_MODVTX ,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureAddressMode
{
	ETEXADDR_CLAMP = 0,
	ETEXADDR_WRAP,
	ETEXADDR_END,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureFilterMode
{
	ETEXFILT_POINT = 0,
	ETEXFILT_LINEAR,
	ETEXFILT_ANISO,
	ETEXFILT_END,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureDest
{
	ETEXDEST_AMBIENT = 0,
	ETEXDEST_DIFFUSE ,
	ETEXDEST_SPECULAR ,
	ETEXDEST_BUMP ,
	ETEXDEST_END ,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureType
{
	ETEXTYPE_1D = 0,
	ETEXTYPE_2D ,
	ETEXTYPE_3D ,
	ETEXTYPE_CUBE ,
	ETEXTYPE_ENVSPH ,
	ETEXTYPE_END ,
};

///////////////////////////////////////////////////////////////////////////////

enum EBufferFormat
{
	EBUFFMT_RGBA32 = 0,
	EBUFFMT_RGBA64 ,
	EBUFFMT_RGBA128 ,
	EBUFFMT_F32 ,
	EBUFFMT_Z16 ,
	EBUFFMT_Z24S8 ,
	EBUFFMT_Z32,
	EBUFFMT_DEPTH,
	EBUFFMT_END
};

///////////////////////////////////////////////////////////////////////////////

enum EPrimitiveType
{
	EPRIM_NONE = 0,
	EPRIM_POINTS ,
	EPRIM_LINES ,
	EPRIM_LINESTRIP ,
	EPRIM_LINELOOP ,
	EPRIM_TRIANGLES ,
	EPRIM_QUADS ,
	EPRIM_TRIANGLESTRIP ,
	EPRIM_TRIANGLEFAN ,
	EPRIM_QUADSTRIP ,
	EPRIM_MULTI,
	EPRIM_POINTSPRITES,
	EPRIM_END
};

//////////////////////////////////////

enum EScissorTest
{
	ESCISSORTEST_OFF = 0,
	ESCISSORTEST_ON ,
};	// 1 bit

//////////////////////////////////////

enum EAlphaTest
{
	EALPHATEST_OFF = 0,
	EALPHATEST_GREATER ,
	EALPHATEST_LESS ,
};	// 1 bit

//////////////////////////////////////

enum EBlending
{
	EBLENDING_OFF = 0,
	EBLENDING_PREMA ,				// (SrcClr) + (FBClr*(1-SrcAlpha))
	EBLENDING_ALPHA ,				// (SrcClr*SrcAlpha) + (FBClr*(1-SrcAlpha))
	EBLENDING_DSTALPHA ,			// (SrcClr*FBAlpha) + (FBClr*(1-FBAlpha))
	EBLENDING_ADDITIVE ,			// (SrcClr*1) + (FBClr*1)
	EBLENDING_ALPHA_ADDITIVE ,		// (SrcClr*SrcAlpha) + (FBClr*1)
	EBLENDING_SUBTRACTIVE ,			// (SrcClr*0) + (FBClr*(1-SrcColor))
	EBLENDING_ALPHA_SUBTRACTIVE ,	// (SrcClr*0) + (FBClr*(1-SrcAlpha))
	EBLENDING_MODULATE ,			// (SrcClr*0) + (FBClr*(1-SrcAlpha))
	EBLENDING_END ,	
};	// 3 bit

//////////////////////////////////////

enum EDepthTest
{
	EDEPTHTEST_OFF = 0,
	EDEPTHTEST_LESS ,
	EDEPTHTEST_LEQUALS ,
	EDEPTHTEST_GREATER ,
	EDEPTHTEST_GEQUALS ,
	EDEPTHTEST_EQUALS ,
	EDEPTHTEST_ALWAYS ,		// is this the same as off?

};	// 3 bits

//////////////////////////////////////

enum EStencilOp
{
	ESTENCILOP_ZERO = 0,
	ESTENCILOP_KEEP ,
	ESTENCILOP_REPLACE ,
	ESTENCILOP_INCR ,
	ESTENCILOP_INCRSAT ,
	ESTENCILOP_DECR ,
	ESTENCILOP_DECRSAT ,
	ESTENCILOP_INVERT ,

};	// 3 bits

enum EStencilMode
{
	ESTENCILTEST_OFF = 0,
	ESTENCILTEST_NEVER ,
	ESTENCILTEST_LESS ,
	ESTENCILTEST_LEQUALS ,
	ESTENCILTEST_GREATER  ,
	ESTENCILTEST_GEQUALS ,
	ESTENCILTEST_EQUALS ,
	ESTENCILTEST_NOTEQUALS ,
	ESTENCILTEST_ALWAYS ,

};	// 4 bits

//////////////////////////////////////
// Interp of Per Vertex Params?

enum EShadeModel
{
	ESHADEMODEL_FLAT = 0,
	ESHADEMODEL_SMOOTH ,

};	// 1 bit

//////////////////////////////////////

enum ECullTest
{
	ECULLTEST_OFF = 0,
	ECULLTEST_PASS_FRONT ,
	ECULLTEST_PASS_BACK ,

};	// 2 bits

///////////////////////////////////////////////////////////////////////////////

enum EVtxStreamFormat
{
	EVTXSTREAMFMT_V16 = 0,		// 16 BPV	flat CVector4's
	EVTXSTREAMFMT_V4T4 ,		// 8 BPV	2D text (or textured quads) no vtxcolors
	EVTXSTREAMFMT_V4C4 ,		// 8 BPV	2D Colored
	EVTXSTREAMFMT_V4T4C4,		// 12 BPV	2D text (or textured quads) w / vtxcolors
	EVTXSTREAMFMT_V12C4T16 ,		// 20 BPV	3D Textured Colored

	EVTXSTREAMFMT_V12N6I1T4 ,	// 24 BPV	3D Textured hard skinned w/normals  (gamecube/wii basic)
	EVTXSTREAMFMT_V12N6C2T4 ,	// 24 BPV	3D Textured colored rigid w/normals (gamecube/wii basic)

	EVTXSTREAMFMT_V16T16C16,	// 48 BPV	Fat Testing Format
	EVTXSTREAMFMT_V12I4N12T8,	// 36 BPV	I4 = Bone Index (SKINNED)
	EVTXSTREAMFMT_V12C4N6I2T8,	// 32 BPV	I2 = Bone Index (SKINNED)
	EVTXSTREAMFMT_V6I2C4N3T2,   // 16 BPV	I2 = Bone Index (SKINNED)
	EVTXSTREAMFMT_V12I4N6W4T4,	// 32 BPV	I4 = Bone Index, W4 = Bone Weights

	EVTXSTREAMFMT_V12N12T8I4W4,		// 40BPV	Normals,1UV,4 bone weighting
	EVTXSTREAMFMT_V12N12B12T8,		// 44BPV	Normals,Binormals,1UV
	EVTXSTREAMFMT_V12N12T16C4,		// 44BPV	Normals,2UV,Color
	EVTXSTREAMFMT_V12N12B12T8C4,	// 48BPV	Normals,Binormals,1UV
	EVTXSTREAMFMT_V12N12B12T16,		// 52BPV	Normals,Binormals,2UV (lightmapped)
	EVTXSTREAMFMT_V12N12B12T8I4W4,	// 52BPV	Normals,Binormals,1UV,4 bone weighting

	EVTXSTREAMFMT_MODELERRIGID,	// 32 BPV	I4 = Bone Index, W4 = Bone Weights

	//////////////////////////////
	// Platform Specific

	EVTXSTREAMFMT_XP_VCNT,		// unskinned, colors, normals, UV's
	EVTXSTREAMFMT_XP_VCNTI,		// skinned, colors, normals, UV's

	//////////////////////////////

	EVTXSTREAMFMT_END,
};
///////////////////////////////////////////////////////////////////////////////
} }

#endif // !_ORK_LEV2_GFXENV_ENUM_H_
