////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct SVtxV4T4 // 8 BPV	PreXF 2D (all on top)
{
  S16 miX; // 2
  S16 miY; // 4
  S16 miU; // 6
  S16 miV; // 8

  SVtxV4T4(S16 iX, S16 iY, S16 iU, S16 iV)
      : miX(iX)
      , miY(iY)
      , miU(iU)
      , miV(iV) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V4T4;
};

///////////////////////////////////////////////////////////////////////////////
// UI Vertex Format for lines/quads (non Textured, colored)
///////////////////////////////////////////////////////////////////////////////

struct SVtxV4C4 // 8 BPV
{
  S16 miX;     // 2
  S16 miY;     // 4
  U32 muColor; // 8

  SVtxV4C4(S16 iX, S16 iY, U32 uColor)
      : miX(iX)
      , miY(iY)
      , muColor(uColor) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V4C4;
};

///////////////////////////////////////////////////////////////////////////////

struct VtxV12 {
  F32 x, y, z; // 12

  VtxV12()
      : x(0.0f)
      , y(0.0f)
      , z(0.0f) {
  }

  VtxV12(F32 X, F32 Y, F32 Z)
      : x(X)
      , y(Y)
      , z(Z) {
  }

  void EndianSwap() {
    swapbytes_dynamic(x);
    swapbytes_dynamic(y);
    swapbytes_dynamic(z);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV4T4C4 // 8 BPV	PreXF 2D (all on top)
{
  S16 miX; // 2
  S16 miY; // 4
  S16 miU; // 6
  S16 miV; // 8
  U32 muColor;

  SVtxV4T4C4(S16 iX, S16 iY, S16 iU, S16 iV, U32 uColor)
      : miX(iX)
      , miY(iY)
      , miU(iU)
      , miV(iV)
      , muColor(uColor) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V4T4C4;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct _VtxV12C4 {
  float x, y, z; 
  uint32_t color;   
};

struct VtxV12C4 { // 16BPV
  F32 x, y, z; // 12
  U32 color;   // 20

  VtxV12C4()
      : x(0.0f)
      , y(0.0f)
      , z(0.0f)
      , color(0xffffffff) {
  }

  VtxV12C4(F32 X, F32 Y, F32 Z)
      : x(X)
      , y(Y)
      , z(Z)
      , color(0xffffffff) {
  }
  VtxV12C4(F32 X, F32 Y, F32 Z, U32 c)
      : x(X)
      , y(Y)
      , z(Z)
      , color(c) {
  }

  void EndianSwap() {
    swapbytes_dynamic(x);
    swapbytes_dynamic(y);
    swapbytes_dynamic(z);
    swapbytes_dynamic(color);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12C4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV6I2C4N3T2 { // 18BPV
  S16 mX, mY, mZ;
  U16 mIDX;
  U32 mColor;
  S8 mNX, mNY, mNZ;
  U8 mU, mV, _pad;

  SVtxV6I2C4N3T2(S16 X, S16 Y, S16 Z, U16 idx, U32 color, S8 NX, S8 NY, S8 NZ, U8 U, U8 V)
      : mX(X)
      , mY(Y)
      , mZ(Z)
      , mIDX(idx)
      , mColor(color)
      , mNX(NX)
      , mNY(NY)
      , mNZ(NZ)
      , mU(U)
      , mV(V)
      , _pad(0) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V6I2C4N3T2;
};

///////////////////////////////////////////////////////////////////////////////

struct _VtxV12T8 { // 20BPV
  float x, y, z;
  float u,v ;
};

struct VtxV12T8 {
  fvec3 pos; // 12
  fvec2 uv0; // 20

  VtxV12T8(F32 X, F32 Y, F32 Z)
      : pos(X,Y,Z){
  }
  VtxV12T8(F32 X, F32 Y, F32 Z, F32 U, F32 V)
      : pos(X,Y,Z)
      , uv0(U,V) {
  }

  void EndianSwap() {
    swapbytes_dynamic(pos.x);
    swapbytes_dynamic(pos.y);
    swapbytes_dynamic(pos.z);
    swapbytes_dynamic(uv0.x);
    swapbytes_dynamic(uv0.y);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12T8;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N6I1T4 { // 24BPV
  F32 mX, mY, mZ;    // 12
  S16 mNX, mNY, mNZ; // 18
  U8 mBone;          // 19
  U8 mPad;           // 20
  S16 mU, mV;        // 24

  SVtxV12N6I1T4()
      : mX(0.0f)
      , mY(0.0f)
      , mZ(0.0f)
      , mNX(0)
      , mNY(0)
      , mNZ(0)
      , mBone(0)
      , mPad(0)
      , mU(0)
      , mV(0) {
  }

  SVtxV12N6I1T4(F32 X, F32 Y, F32 Z, S16 NX, S16 NY, S16 NZ, int ibone, S16 U, S16 V)
      : mX(X)
      , mY(Y)
      , mZ(Z)
      , mNX(NX)
      , mNY(NY)
      , mNZ(NZ)
      , mBone(U8(ibone))
      , mPad(0)
      , mU(U)
      , mV(V) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mX);
    swapbytes_dynamic(mY);
    swapbytes_dynamic(mZ);
    swapbytes_dynamic(mNZ);
    swapbytes_dynamic(mNX);
    swapbytes_dynamic(mNY);
    swapbytes_dynamic(mU);
    swapbytes_dynamic(mV);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N6I1T4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N6C2T4 // 24BPV WII rigid format
{
  F32 mX, mY, mZ;    // 0  12
  S16 mNX, mNY, mNZ; // 12 18
  U16 mColor;        // 18 20
  S16 mU, mV;        // 20 24

  SVtxV12N6C2T4()
      : mX(0.0f)
      , mY(0.0f)
      , mZ(0.0f)
      , mNX(0)
      , mNY(0)
      , mNZ(0)
      , mColor(0)
      , mU(0)
      , mV(0) {
  }

  SVtxV12N6C2T4(F32 X, F32 Y, F32 Z, S16 NX, S16 NY, S16 NZ, U16 color, S16 U, S16 V)
      : mX(X)
      , mY(Y)
      , mZ(Z)
      , mNX(NX)
      , mNY(NY)
      , mNZ(NZ)
      , mColor(color)
      , mU(U)
      , mV(V) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mX);
    swapbytes_dynamic(mY);
    swapbytes_dynamic(mZ);
    swapbytes_dynamic(mNZ);
    swapbytes_dynamic(mNX);
    swapbytes_dynamic(mNY);
    swapbytes_dynamic(mColor);
    swapbytes_dynamic(mU);
    swapbytes_dynamic(mV);
    // orkprintf( "wii: SVtxV12N6C2T4 <u %08x> <v %08x>\n", int(mU), int(mV) );
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N6C2T4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12C4T16 { // 32 BPV{
  SVtxV12C4T16(F32 iX = 0.0f, F32 iY = 0.0f, F32 iZ = 0.0f, F32 fU = 0.0f, F32 fV = 0.0f, U32 uColor = 0xffffffff)
      : _position(iX, iY, iZ)
      , _color(uColor)
      , _uv0(fU, fV) {
  }

  SVtxV12C4T16(F32 iX, F32 iY, F32 iZ, F32 fU, F32 fV, F32 fU2, F32 fV2, U32 uColor = 0xffffffff)
      : _position(iX, iY, iZ)
      , _color(uColor)
      , _uv0(fU, fV)
      , _uv1(fU2, fV2) {
  }

  SVtxV12C4T16(const fvec3& pos, const fvec2& uv, U32 uColor = 0xffffffff)
      : _position(pos)
      , _color(uColor)
      , _uv0(uv)
      , _uv1(uv) {
  }

  SVtxV12C4T16(const fvec3& pos, const fvec2& uv, const fvec2& uv2, U32 uColor = 0xffffffff)
      : _position(pos)
      , _color(uColor)
      , _uv0(uv)
      , _uv1(uv2) {
  }

  void EndianSwap() {
  }

  fvec3 _position; // 12
  U32 _color;      // 16
  fvec2 _uv0;      // 24
  fvec2 _uv1;      // 32

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12C4T16;
}; // namespace lev2

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12C4N6I2T8 { // 32BPV
  F32 mX, mY, mZ;
  U32 mColor;
  S16 mNX, mNY, mNZ;
  U16 mIDX;
  F32 mU, mV;

  SVtxV12C4N6I2T8(F32 X, F32 Y, F32 Z, U16 idx, U32 color, S16 NX, S16 NY, S16 NZ, F32 U, F32 V)
      : mX(X)
      , mY(Y)
      , mZ(Z)
      , mColor(color)
      , mNX(NX)
      , mNY(NY)
      , mNZ(NZ)
      , mIDX(idx)
      , mU(U)
      , mV(V) {
  }

  SVtxV12C4N6I2T8()
      : mX(0.0f)
      , mY(0.0f)
      , mZ(0.0f)
      , mColor(0xffffffff)
      , mNX(0)
      , mNY(0)
      , mNZ(0)
      , mIDX(0)
      , mU(0.0f)
      , mV(0.0f) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12C4N6I2T8;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12I4N12T8 { // 36BPV
  fvec3 mPosition;
  U32 mIDX;
  fvec3 mNormal;
  fvec2 mUV0;

  SVtxV12I4N12T8(const fvec3& pos = fvec3(), const fvec3& nrm = fvec3(), const fvec2& uv = fvec2(), U32 idx = 0)
      : mPosition(pos)
      , mIDX(idx)
      , mNormal(nrm)
      , mUV0(uv) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12I4N12T8;
};


///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12T8I4W4 { // 40BPV
  fvec3 mPosition;
  fvec3 mNormal;
  fvec2 mUV0;
  U32 mBoneIndices;
  U32 mBoneWeights;

  SVtxV12N12T8I4W4(
      const fvec3& pos = fvec3(),
      const fvec3& nrm = fvec3(),
      const fvec2& uv  = fvec2(),
      U32 BoneIndices  = 0,
      U32 BoneWeights  = 0)
      : mPosition(pos)
      , mNormal(nrm)
      , mUV0(uv)
      , mBoneIndices(BoneIndices)
      , mBoneWeights(BoneWeights) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mPosition[0]);
    swapbytes_dynamic(mPosition[1]);
    swapbytes_dynamic(mPosition[2]);

    swapbytes_dynamic(mNormal[0]);
    swapbytes_dynamic(mNormal[1]);
    swapbytes_dynamic(mNormal[2]);

    swapbytes_dynamic(mBoneIndices);
    swapbytes_dynamic(mBoneWeights);

    swapbytes_dynamic(mUV0[0]);
    swapbytes_dynamic(mUV0[1]);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12T8I4W4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12T16 { // 40BPV
  fvec3 mPosition;
  fvec3 mNormal;
  fvec4 mUV;

  SVtxV12N12T16(
      const fvec3& pos   = fvec3(),
      const fvec3& nrm   = fvec3(),
      const fvec4& uv   = fvec4())
      : mPosition(pos)
      , mNormal(nrm)
      , mUV(uv) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mPosition[0]);
    swapbytes_dynamic(mPosition[1]);
    swapbytes_dynamic(mPosition[2]);

    swapbytes_dynamic(mNormal[0]);
    swapbytes_dynamic(mNormal[1]);
    swapbytes_dynamic(mNormal[2]);

    swapbytes_dynamic(mUV[0]);
    swapbytes_dynamic(mUV[1]);
    swapbytes_dynamic(mUV[2]);
    swapbytes_dynamic(mUV[3]);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12T16;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12B12T8 { // 44BPV
  fvec3 mPosition;
  fvec3 mNormal;
  fvec3 mBiNormal;
  fvec2 mUV0;

  SVtxV12N12B12T8(const fvec3& pos = fvec3(), const fvec3& nrm = fvec3(), const fvec3& binrm = fvec3(), const fvec2& uv = fvec2())
      : mPosition(pos)
      , mNormal(nrm)
      , mBiNormal(binrm)
      , mUV0(uv) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12B12T8;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12T16C4 { // 44BPV
  fvec3 mPosition;
  fvec3 mNormal;
  fvec2 mUV0;
  fvec2 mUV1;
  U32 mColor;

  SVtxV12N12T16C4(
      const fvec3& pos = fvec3(),
      const fvec3& nrm = fvec3(),
      const fvec2& uv0 = fvec2(),
      const fvec2& uv1 = fvec2(),
      const U32 clr    = 0xffffffff)
      : mPosition(pos)
      , mNormal(nrm)
      , mUV0(uv0)
      , mUV1(uv1)
      , mColor(clr) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mPosition[0]);
    swapbytes_dynamic(mPosition[1]);
    swapbytes_dynamic(mPosition[2]);

    swapbytes_dynamic(mNormal[0]);
    swapbytes_dynamic(mNormal[1]);
    swapbytes_dynamic(mNormal[2]);

    swapbytes_dynamic(mUV0[0]);
    swapbytes_dynamic(mUV0[1]);

    swapbytes_dynamic(mUV1[0]);
    swapbytes_dynamic(mUV1[1]);

    swapbytes_dynamic(mColor);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12T16C4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12B12T8C4 { // 48BPV
  fvec3 _position;
  fvec3 _normal;
  fvec3 _binormal;
  fvec2 _uv;
  U32 _color;

  SVtxV12N12B12T8C4(
      const fvec3& pos   = fvec3(),
      const fvec3& nrm   = fvec3(),
      const fvec3& binrm = fvec3(),
      const fvec2& uv    = fvec2(),
      const U32 clr      = 0xffffffff)
      : _position(pos)
      , _normal(nrm)
      , _binormal(binrm)
      , _uv(uv)
      , _color(clr) {
  }

  void EndianSwap() {
    swapbytes_dynamic(_position[0]);
    swapbytes_dynamic(_position[1]);
    swapbytes_dynamic(_position[2]);

    swapbytes_dynamic(_normal[0]);
    swapbytes_dynamic(_normal[1]);
    swapbytes_dynamic(_normal[2]);

    swapbytes_dynamic(_binormal[0]);
    swapbytes_dynamic(_binormal[1]);
    swapbytes_dynamic(_binormal[2]);

    swapbytes_dynamic(_uv[0]);
    swapbytes_dynamic(_uv[1]);

    swapbytes_dynamic(_color);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12B12T8C4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12T8DF12C4 { // 48BPV
  fvec3 _position;
  fvec3 _normal;
  fvec2 _uv;
  fvec3 _data;
  uint32_t _color;

  SVtxV12N12T8DF12C4(
      const fvec3& pos   = fvec3(),
      const fvec3& nrm   = fvec3(),
      const fvec2& uv    = fvec2(),
      const fvec3& d     = fvec3(),
      const uint32_t clr = 0xffffffff)
      : _position(pos)
      , _normal(nrm)
      , _uv(uv)
      , _data(d)
      , _color(clr) {
  }

  void EndianSwap() {
    swapbytes_dynamic(_position[0]);
    swapbytes_dynamic(_position[1]);
    swapbytes_dynamic(_position[2]);

    swapbytes_dynamic(_normal[0]);
    swapbytes_dynamic(_normal[1]);
    swapbytes_dynamic(_normal[2]);

    swapbytes_dynamic(_uv[0]);
    swapbytes_dynamic(_uv[1]);

    swapbytes_dynamic(_data[0]);
    swapbytes_dynamic(_data[1]);
    swapbytes_dynamic(_data[2]);

    swapbytes_dynamic(_color);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12T8DF12C4;
};

///////////////////////////////////////////////////////////////////////////////
// fat testing format

struct SVtxV16T16C16 // 48 BPV
{
  float miPX;
  float miPY;
  float miPZ;
  float miPW;
  float miTU;
  float miTV;
  float miTW;
  float miTQ;
  float miCR;
  float miCG;
  float miCB;
  float miCA;

  explicit SVtxV16T16C16(
      float px,
      float py,
      float pz,
      float pw, //
      float tu,
      float tv,
      float tw,
      float tq, //
      float cr,
      float cg,
      float cb,
      float ca) //
      : miPX(px)
      , miPY(py)
      , miPZ(pz)
      , miPW(pw) //
      , miTU(tu)
      , miTV(tv)
      , miTW(tw)
      , miTQ(tq) //
      , miCR(cr)
      , miCG(cg)
      , miCB(cb)
      , miCA(ca) {
  }

  explicit SVtxV16T16C16(
      fvec4 pos,  //
      fvec4 uvwq, //
      fvec4 rgba) //
      : miPX(pos.x)
      , miPY(pos.y)
      , miPZ(pos.z)
      , miPW(pos.w) //
      , miTU(uvwq.x)
      , miTV(uvwq.y)
      , miTW(uvwq.z)
      , miTQ(uvwq.w) //
      , miCR(rgba.x)
      , miCG(rgba.y)
      , miCB(rgba.z)
      , miCA(rgba.w) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V16T16C16;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12B12T16 { // 52BPV
  fvec3 mPosition;
  fvec3 mNormal;
  fvec3 mBiNormal;
  fvec2 mUV0;
  fvec2 mUV1;

  SVtxV12N12B12T16(
      const fvec3& pos   = fvec3(),
      const fvec3& nrm   = fvec3(),
      const fvec3& binrm = fvec3(),
      const fvec2& uv0   = fvec2(),
      const fvec2& uv1   = fvec2())
      : mPosition(pos)
      , mNormal(nrm)
      , mBiNormal(binrm)
      , mUV0(uv0)
      , mUV1(uv1) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mPosition[0]);
    swapbytes_dynamic(mPosition[1]);
    swapbytes_dynamic(mPosition[2]);

    swapbytes_dynamic(mNormal[0]);
    swapbytes_dynamic(mNormal[1]);
    swapbytes_dynamic(mNormal[2]);

    swapbytes_dynamic(mBiNormal[0]);
    swapbytes_dynamic(mBiNormal[1]);
    swapbytes_dynamic(mBiNormal[2]);

    swapbytes_dynamic(mUV0[0]);
    swapbytes_dynamic(mUV0[1]);

    swapbytes_dynamic(mUV1[0]);
    swapbytes_dynamic(mUV1[1]);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12B12T16;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxV12N12B12T8I4W4 { // 52BPV
  fvec3 mPosition;
  fvec3 mNormal;
  fvec3 mBiNormal;
  fvec2 mUV0;
  U32 mBoneIndices;
  U32 mBoneWeights;

  SVtxV12N12B12T8I4W4(
      const fvec3& pos   = fvec3(),
      const fvec3& nrm   = fvec3(),
      const fvec3& binrm = fvec3(),
      const fvec2& uv    = fvec2(),
      U32 BoneIndices    = 0,
      U32 BoneWeights    = 0)
      : mPosition(pos)
      , mNormal(nrm)
      , mBiNormal(binrm)
      , mUV0(uv)
      , mBoneIndices(BoneIndices)
      , mBoneWeights(BoneWeights) {
  }

  void EndianSwap() {
    swapbytes_dynamic(mPosition[0]);
    swapbytes_dynamic(mPosition[1]);
    swapbytes_dynamic(mPosition[2]);

    swapbytes_dynamic(mNormal[0]);
    swapbytes_dynamic(mNormal[1]);
    swapbytes_dynamic(mNormal[2]);

    swapbytes_dynamic(mBiNormal[0]);
    swapbytes_dynamic(mBiNormal[1]);
    swapbytes_dynamic(mBiNormal[2]);

    swapbytes_dynamic(mBoneIndices);
    swapbytes_dynamic(mBoneWeights);

    swapbytes_dynamic(mUV0[0]);
    swapbytes_dynamic(mUV0[1]);
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::V12N12B12T8I4W4;
};

///////////////////////////////////////////////////////////////////////////////

struct SVtxMODELERRIGID { // 72BPV
  F32 mX, mY, mZ;        // 12
  U32 mObjectID;         // 16
  U32 mFaceID;           // 20
  U32 mVertexID;         // 24
  U32 mColor;            // 28
  S16 mNX, mNY, mNZ, mS; // 36
  S16 mBX, mBY, mBZ, mT; // 44
  S16 mU0, mV0;          // 48
  S16 mU1, mV1;          // 52
  S16 mU2, mV2;          // 56
  S16 mU3, mV3;          // 60
  S16 mU4, mV4;          // 64
  f32 fU, fV;            // 72

  SVtxMODELERRIGID() {
  }

  SVtxMODELERRIGID(F32 X, F32 Y, F32 Z, U32 obj, U32 cmp, U32 clr, S16 NX, S16 NY, S16 BX, S16 BY, S16 u0, S16 v0, S16 u1, S16 v1)
      : mX(X)
      , mY(Y)
      , mZ(Z)
      , mObjectID(obj)
      , mFaceID(0)
      , mVertexID(0)
      , mColor(clr)
      , mNX(NX)
      , mNY(NY)
      , mNZ(0)
      , mS(0)
      , mBX(BX)
      , mBY(BY)
      , mBZ(0)
      , mT(0)
      , mU0(u0)
      , mV0(v0)
      , mU1(u1)
      , mV1(v1)
      , mU2(0)
      , mV2(0)
      , mU3(0)
      , mV3(0)
      , mU4(0)
      , mV4(0) {
  }

  SVtxMODELERRIGID(const fvec4& Pos, U32 obj, U32 cmp, U32 clr, S16 NX, S16 NY, S16 BX, S16 BY, S16 u0, S16 v0, S16 u1, S16 v1)
      : mObjectID(obj)
      , mFaceID(0)
      , mVertexID(0)
      , mColor(clr)
      , mNX(NX)
      , mNY(NY)
      , mNZ(0)
      , mS(0)
      , mBX(BX)
      , mBY(BY)
      , mBZ(0)
      , mT(0)
      , mU0(u0)
      , mV0(v0)
      , mU1(u1)
      , mV1(v1)
      , mU2(0)
      , mV2(0)
      , mU3(0)
      , mV3(0)
      , mU4(0)
      , mV4(0) {
  }

  void EndianSwap() {
  }

  constexpr static EVtxStreamFormat meFormat = EVtxStreamFormat::MODELERRIGID;
};

} //namespace ork::lev2 {
