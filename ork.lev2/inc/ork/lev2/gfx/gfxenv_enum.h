////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/util/crc.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

enum class MsaaSamples {
  MSAA_1X = 1,
  MSAA_4X = 4,
  MSAA_8X = 8,
  MSAA_9X = 9,
  MSAA_16X = 16,
  MSAA_25X = 25,
  MSAA_36X = 36,
};

int msaaEnumToInt( const MsaaSamples& samples );
MsaaSamples intToMsaaEnum( int ival );

///////////////////////////////////////////////////////////////////////////////

enum class TargetType {
  NONE = 0,
  LOADING,
  OFFSCREEN,
  WINDOW,
};

enum class RtgSlot {
  Slot0 = 0,
  Slot1 = 1,
  Slot2 = 2,
  Slot3 = 3,
};

///////////////////////////////////////////////////////////////////////////////

enum class UiColorMode {
  MOD = 0,
  VTX,
  MODVTX,
};

///////////////////////////////////////////////////////////////////////////////

enum class TextureAddressMode {
  CLAMP = 0,
  WRAP,
  END,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureFilterMode {
  ETEXFILT_POINT = 0,
  ETEXFILT_LINEAR,
  ETEXFILT_ANISO,
  ETEXFILT_END,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureDest {
  ETEXDEST_AMBIENT = 0,
  ETEXDEST_DIFFUSE,
  ETEXDEST_SPECULAR,
  ETEXDEST_BUMP,
  ETEXDEST_END,
};

///////////////////////////////////////////////////////////////////////////////

enum ETextureType {
  ETEXTYPE_1D = 0,
  ETEXTYPE_2D,
  ETEXTYPE_3D,
  ETEXTYPE_CUBE,
  ETEXTYPE_ENVSPH,
  ETEXTYPE_END,
};

///////////////////////////////////////////////////////////////////////////////
enum struct EBufferFormat : crc_enum_t {
  CrcEnum(R8),
  CrcEnum(R16),
  CrcEnum(BGR5A1),
  CrcEnum(BGR8),
  CrcEnum(RGB8),
  CrcEnum(BGRA8),
  CrcEnum(RGBA8),
  CrcEnum(RG16F),
  CrcEnum(RG32F),
  CrcEnum(RGBA16F),
  CrcEnum(RGBA16UI),
  CrcEnum(RGBA32F),
  CrcEnum(RGB10A2),
  CrcEnum(RGB32UI),
  CrcEnum(RGBA32UI),
  CrcEnum(R32F),
  CrcEnum(R32UI),
  CrcEnum(NV12),
  CrcEnum(YUV420P),
  CrcEnum(Z16),
  CrcEnum(Z24S8),
  CrcEnum(Z32),
  CrcEnum(DEPTH),
  CrcEnum(RGBA_BPTC_UNORM),
  CrcEnum(SRGB_ALPHA_BPTC_UNORM),
  CrcEnum(RGBA_ASTC_4X4),
  CrcEnum(SRGB_ASTC_4X4),
  CrcEnum(S3TC_DXT1),
  CrcEnum(S3TC_DXT3),
  CrcEnum(S3TC_DXT5),
  CrcEnum(NONE)
};

std::string EBufferFormatToName(EBufferFormat fmt);
///////////////////////////////////////////////////////////////////////////////

enum struct PrimitiveType : crc_enum_t {
  CrcEnum(NONE),
  CrcEnum(POINTS),
  CrcEnum(LINES),
  CrcEnum(LINESTRIP),
  CrcEnum(LINELOOP),
  CrcEnum(TRIANGLES),
  CrcEnum(QUADS),
  CrcEnum(TRIANGLESTRIP),
  CrcEnum(TRIANGLEFAN),
  CrcEnum(QUADSTRIP),
  CrcEnum(MULTI),
  CrcEnum(PATCHES),
  CrcEnum(END)
};

//////////////////////////////////////

enum EScissorTest {
  ESCISSORTEST_OFF = 0,
  ESCISSORTEST_ON,
}; // 1 bit

//////////////////////////////////////

enum EAlphaTest {
  EALPHATEST_OFF = 0,
  EALPHATEST_GREATER,
  EALPHATEST_LESS,
}; // 1 bit

//////////////////////////////////////

enum struct Blending : crc_enum_t {
  CrcEnum(OFF),
  CrcEnum(PREMA),             // (SrcClr) + (FBClr*(1-SrcAlpha))
  CrcEnum(ALPHA),             // (SrcClr*SrcAlpha) + (FBClr*(1-SrcAlpha))
  CrcEnum(DSTALPHA),          // (SrcClr*FBAlpha) + (FBClr*(1-FBAlpha))
  CrcEnum(ADDITIVE),          // (SrcClr*1) + (FBClr*1)
  CrcEnum(ALPHA_ADDITIVE),    // (SrcClr*SrcAlpha) + (FBClr*1)
  CrcEnum(SUBTRACTIVE),       // (SrcClr*0) + (FBClr*(1-SrcColor))
  CrcEnum(ALPHA_SUBTRACTIVE), // (SrcClr*0) + (FBClr*(1-SrcAlpha))
  CrcEnum(MODULATE),          // (SrcClr*0) + (FBClr*(1-SrcAlpha))
}; 

//////////////////////////////////////

enum struct EDepthTest : crc_enum_t {
  CrcEnum(OFF),
  CrcEnum(LESS),
  CrcEnum(LEQUALS),
  CrcEnum(GREATER),
  CrcEnum(GEQUALS),
  CrcEnum(EQUALS),
  CrcEnum(ALWAYS), // is this the same as off?

}; 

//////////////////////////////////////

enum EStencilOp {
  ESTENCILOP_ZERO = 0,
  ESTENCILOP_KEEP,
  ESTENCILOP_REPLACE,
  ESTENCILOP_INCR,
  ESTENCILOP_INCRSAT,
  ESTENCILOP_DECR,
  ESTENCILOP_DECRSAT,
  ESTENCILOP_INVERT,

}; // 3 bits

enum EStencilMode {
  ESTENCILTEST_OFF = 0,
  ESTENCILTEST_NEVER,
  ESTENCILTEST_LESS,
  ESTENCILTEST_LEQUALS,
  ESTENCILTEST_GREATER,
  ESTENCILTEST_GEQUALS,
  ESTENCILTEST_EQUALS,
  ESTENCILTEST_NOTEQUALS,
  ESTENCILTEST_ALWAYS,

}; // 4 bits

//////////////////////////////////////
// Interp of Per Vertex Params?

enum EShadeModel {
  ESHADEMODEL_FLAT = 0,
  ESHADEMODEL_SMOOTH,

}; // 1 bit

//////////////////////////////////////

enum struct ECullTest : crc_enum_t {
  CrcEnum(OFF),
  CrcEnum(PASS_FRONT),
  CrcEnum(PASS_BACK),
}; // 2 bits

///////////////////////////////////////////////////////////////////////////////

enum struct EVtxStreamFormat : crc_enum_t {
  CrcEnum(V12),      // 12 BPV	flat fvec3's
  CrcEnum(V12C4),    // 16 BPV  flat fvec3's
  CrcEnum(V12T8),    // 20 BPV	flat fvec3's
  CrcEnum(V16),      // 16 BPV	flat fvec4's
  CrcEnum(V4T4),     // 8 BPV	2D text (or textured quads) no vtxcolors
  CrcEnum(V4C4),     // 8 BPV	2D Colored
  CrcEnum(V4T4C4),   // 12 BPV	2D text (or textured quads) w / vtxcolors
  CrcEnum(V12C4T16), // 20 BPV	3D Textured Colored

  CrcEnum(V12N6I1T4), // 24 BPV	3D Textured hard skinned w/normals  (gamecube/wii basic)
  CrcEnum(V12N6C2T4), // 24 BPV	3D Textured colored rigid w/normals (gamecube/wii basic)

  CrcEnum(V16T16C16),   // 48 BPV	Fat Testing Format
  CrcEnum(V12I4N12T8),  // 36 BPV	I4 = Bone Index (SKINNED)
  CrcEnum(V12C4N6I2T8), // 32 BPV	I2 = Bone Index (SKINNED)
  CrcEnum(V6I2C4N3T2),  // 16 BPV	I2 = Bone Index (SKINNED)
  CrcEnum(V12I4N6W4T4), // 32 BPV	I4 = Bone Index, W4 = Bone Weights

  CrcEnum(V12N12T8I4W4),    // 40BPV	Normals,1UV,4 bone weighting
  CrcEnum(V12N12B12T8),     // 44BPV	Normals,Binormals,1UV
  CrcEnum(V12N12T16C4),     // 44BPV	Normals,2UV,Color
  CrcEnum(V12N12B12T8C4),   // 48BPV	Normals,Binormals,1UV
  CrcEnum(V12N12B12T16),    // 52BPV	Normals,Binormals,2UV (lightmapped)
  CrcEnum(V12N12B12T8I4W4), // 52BPV	Normals,Binormals,1UV,4 bone weighting

  CrcEnum(MODELERRIGID), // 32 BPV	I4 = Bone Index, W4 = Bone Weights

  //////////////////////////////
  // Platform Specific

  CrcEnum(XP_VCNT),  // unskinned, colors, normals, UV's
  CrcEnum(XP_VCNTI), // skinned, colors, normals, UV's

  //////////////////////////////

  CrcEnum(NONE)
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
