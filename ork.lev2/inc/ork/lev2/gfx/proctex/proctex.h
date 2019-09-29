///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/math/gradient.h>
#include <ork/math/multicurve.h>
#include <ork/object/Object.h>
#include <ork/rtti/RTTI.h>

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DeclareImg32OutPlug(name)                                                                                                  \
  Img32 OutDataName(name);                                                                                                         \
  ImgOutPlug OutPlugName(name);                                                                                                    \
  ork::Object* OutAccessor##name() { return &OutPlugName(name); }

#define DeclareImg64OutPlug(name)                                                                                                  \
  Img64 OutDataName(name);                                                                                                         \
  ImgOutPlug OutPlugName(name);                                                                                                    \
  ork::Object* OutAccessor##name() { return &OutPlugName(name); }

#define DeclareImgInpPlug(name)                                                                                                    \
  ImgInPlug InpPlugName(name);                                                                                                     \
  ork::Object* InpAccessor##name() { return &InpPlugName(name); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace reflect {
class ISerializer;
class IDeserializer;
}}; // namespace ork::reflect
namespace ork { namespace proctex {
class Module;

enum EPTEX_TYPE {
  EPTEXTYPE_REALTIME = 0,
  EPTEXTYPE_EXPORT,
};

///////////////////////////////////////////////////////////////////////////////
class Buffer {
public:
  static const int kx = 0;
  static const int ky = 0;
  // static const int kw = 1024;
  // static const int kh = 1024;
  Buffer(ork::lev2::EBufferFormat efmt);
  dataflow::node_hash mHash;
  lev2::RtGroup* mRtGroup;
  lev2::GfxTarget* mTarget;
  int miW, miH;

  lev2::Texture* OutputTexture();

  void PtexBegin(lev2::GfxTarget* ptgt, bool push_full_vp, bool clear_all);
  void PtexEnd(bool pop_vp);

  bool IsBuf32() const { return GetBufferFormat() == lev2::EBUFFMT_RGBA8; }
  bool IsBuf64() const { return GetBufferFormat() == lev2::EBUFFMT_RGBA16F; }

  lev2::RtGroup* GetRtGroup(lev2::GfxTarget* pt);

  virtual ork::lev2::EBufferFormat GetBufferFormat() const = 0;

  void SetBufferSize(int w, int h);
};
class Buffer32 : public Buffer {
public:
  Buffer32();
  ork::lev2::EBufferFormat GetBufferFormat() const override { return lev2::EBUFFMT_RGBA8; }
};
class Buffer64 : public Buffer {
public:
  Buffer64();
  ork::lev2::EBufferFormat GetBufferFormat() const override { return lev2::EBUFFMT_RGBA16F; }
};

///////////////////////////////////////////////////////////////////////////////

class ProcTex;
struct ProcTexContext;

struct ImgBase {
  mutable int miBufferIndex;
  ImgBase()
      : miBufferIndex(-1) {}
  virtual ork::lev2::Texture* GetTexture(ProcTex& ptex) const = 0;
  virtual Buffer& GetBuffer(ProcTex& ptex) const              = 0;
  virtual int PixelSize() const                               = 0;
};
struct Img32 : public ImgBase {
  ork::lev2::Texture* GetTexture(ProcTex& ptex) const override;
  Buffer& GetBuffer(ProcTex& ptex) const override;
  virtual int PixelSize() const override { return 32; }
};
struct Img64 : public ImgBase {
  ork::lev2::Texture* GetTexture(ProcTex& ptex) const override;
  Buffer& GetBuffer(ProcTex& ptex) const override;
  virtual int PixelSize() const override { return 64; }
};

typedef ork::dataflow::outplug<ImgBase> ImgOutPlug;
typedef ork::dataflow::inplug<ImgBase> ImgInPlug;

struct AA16Render {
  ProcTex& mPTX;
  Buffer& bufout;
  lev2::GfxMaterial3DSolid downsamplemat;
  fvec4 mOrthoBoxXYWH;

  AA16Render(ProcTex& ptx, Buffer& bo);
  void RenderAA();
  void RenderNoAA();
  virtual void DoRender(float left, float right, float top, float bot, Buffer& buf) = 0;

  void Render(bool bAA) {
    if (bAA)
      RenderAA();
    else
      RenderNoAA();
  }
};

///////////////////////////////////////////////////////////////////////////////

class Module : public ork::dataflow::dgmodule {
  RttiDeclareAbstract(Module, ork::dataflow::dgmodule);

  ////////////////////////////////////////////////////////////
  void Compute(dataflow::workunit* wu) override {}
  void CombineWork(const dataflow::cluster* c) override {}

protected:
  Module();
};

///////////////////////////////////////////////////////////////////////////////

void RenderQuad(ork::lev2::GfxTarget* pTARG,
                float fx1,
                float fy1,
                float fx2,
                float fy2,
                float fu1 = 0.0f,
                float fv1 = 0.0f,
                float fu2 = 1.0f,
                float fv2 = 1.0f);

class ImgModule : public Module {
  RttiDeclareAbstract(ImgModule, Module);

  virtual void compute(ProcTex& ptex) = 0;

  bool IsDirty(void) const { return true; } // virtual

  bool mExport;

protected:
  void UnitTexQuad(ork::lev2::GfxTarget* pTARG);

  static Img32 gNoCon;

  ImgModule();
  Buffer32 mThumbBuffer;
  ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16> mVW;

  ////////////////////////////////////////////////////////////
  void Compute(dataflow::workunit* wu) final;
  void CombineWork(const dataflow::cluster* clus) final {}
  ////////////////////////////////////////////////////////////

  void MarkClean();

public:
  Buffer& GetWriteBuffer(ProcTex& ptex);
  Buffer& GetThumbBuffer() { return mThumbBuffer; }
  void UpdateThumb(ProcTex& ptex);
};

///////////////////////////////////////////////////////////////////////////////

class Img64Module : public ImgModule {
  RttiDeclareAbstract(Img64Module, ImgModule);

protected:
  DeclareImg64OutPlug(ImgOut);

  Img64Module();

  dataflow::outplugbase* GetOutput(int idx) final { return &mPlugOutImgOut; }
};

///////////////////////////////////////////////////////////////////////////////

class Img32Module : public ImgModule {
  RttiDeclareAbstract(Img32Module, ImgModule);

protected:
  DeclareImg32OutPlug(ImgOut);

  Img32Module();

  dataflow::outplugbase* GetOutput(int idx) final { return &mPlugOutImgOut; }
};

///////////////////////////////////////////////////////////////////////////////

class Curve1D : public Module {
  RttiDeclareConcrete(Curve1D, Module);

private:
  ork::dataflow::inplugbase* GetInput(int idx) final;
  dataflow::outplugbase* GetOutput(int idx) final;
  void Compute(dataflow::workunit* wu) final;
  void CombineWork(const dataflow::cluster* clus) final {}

  ork::Object* CurveAccessor() { return &mMultiCurve; }
  ork::Object* PlgAccessorOutput() { return &mOutput; }

  float mOutValue;
  float mInValue;
  ork::dataflow::outplug<float> mOutput;
  MultiCurve1D mMultiCurve;

  DeclareFloatXfPlug(Input);

public:
  Curve1D();
};

///////////////////////////////////////////////////////////////////////////////

class Global : public Module {
  RttiDeclareConcrete(Global, Module);

private:
  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareFloatXfPlug(TimeScale);

  dataflow::inplugbase* GetInput(int idx) final { return &mPlugInpTimeScale; }

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  DeclareFloatOutPlug(Time);
  DeclareFloatOutPlug(TimeDiv10);
  DeclareFloatOutPlug(TimeDiv100);

  dataflow::outplugbase* GetOutput(int idx) final;

  //////////////////////////////////////////////////

  void Compute(dataflow::workunit* wu) final;
  void CombineWork(const dataflow::cluster* clus) final {}

public:
  Global();
};

///////////////////////////////////////////////////////////////////////////////

struct ProcTexContext {
  static const int k64buffers = 4;
  static const int k32buffers = 16;
  dataflow::dgcontext mdflowctx;
  dataflow::dgregisterblock mFloatRegs;
  dataflow::dgregisterblock mImage32Regs;
  dataflow::dgregisterblock mImage64Regs;
  Buffer* mBuffer32[k32buffers];
  Buffer* mBuffer64[k64buffers];
  Buffer32 mTrashBuffer;
  float mCurrentTime;
  int mBufferDim;
  EPTEX_TYPE mProcTexType;
  bool mWriteFrames;
  int mWriteFrameIndex;
  ork::file::Path mWritePath;

  Buffer& GetBuffer32(int edest);
  Buffer& GetBuffer64(int edest);
  lev2::GfxTarget* mTarget;
  static ork::MpMcBoundedQueue<Buffer*> gBuf32Q;
  static ork::MpMcBoundedQueue<Buffer*> gBuf64Q;

  static Buffer* AllocBuffer32();
  static Buffer* AllocBuffer64();
  static void ReturnBuffer(Buffer* pbuf);

  void SetBufferDim(int buffer_dim);

  ProcTexContext();
  ~ProcTexContext();
};

///////////////////////////////////////////////////////////////////////////////

class ProcTex : public ork::dataflow::graph_inst {
  RttiDeclareConcrete(ProcTex, ork::dataflow::graph_inst);

public:
  ProcTex();
  void compute(ProcTexContext& ptctx);

  Buffer& GetBuffer32(int edest) {
    OrkAssert(mpctx);
    return mpctx->GetBuffer32(edest);
  }
  Buffer& GetBuffer64(int edest) {
    OrkAssert(mpctx);
    return mpctx->GetBuffer64(edest);
  }

  bool GetTexQuality() const { return mbTexQuality; }

  ProcTexContext* GetPTC() { return mpctx; }
  lev2::GfxTarget* GetTarget() { return (mpctx != nullptr) ? mpctx->mTarget : nullptr; }

  ork::lev2::Texture* ResultTexture();

  static ProcTex* Load(const ork::file::Path& pth);

private:
  bool CanConnect(const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout) const final;

  ProcTexContext* mpctx;
  bool mbTexQuality;
  ork::lev2::Texture* mpResTex;
};

///////////////////////////////////////////////////////////////////////////////

enum EPeriodicShape {
  ESH_SAW = 0,
  ESH_SIN,
  ESH_COS,
  ESH_SQU,
};

class Periodic : public Module {
  RttiDeclareAbstract(Periodic, Module);

public:
  Periodic();

  float compute(float unitphase);

  void Hash(dataflow::node_hash& hash);

private:
  ////////////////////////////////////////////
  ork::dataflow::inplugbase* GetInput(int idx) final;
  ////////////////////////////////////////////

  DeclareFloatXfPlug(PhaseOffset);
  DeclareFloatXfPlug(Frequency);
  DeclareFloatXfPlug(Amplitude);
  DeclareFloatXfPlug(Bias);

  EPeriodicShape meShape;

  ////////////////////////////////////////////
  ////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
class RotSolid : public Img32Module {
  RttiDeclareConcrete(RotSolid, Img32Module);

  dataflow::node_hash mVBHash;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareFloatXfPlug(PhaseOffset);

  ork::dataflow::inplugbase* GetInput(int idx) final { return &mPlugInpPhaseOffset; }

  /////////////////////////////////////////

  int miNumSides;
  ork::lev2::EBlending meBlendMode;

  Periodic mRadiusFunc;
  Periodic mIntensFunc;

  bool mbAA;

  ork::Object* RadiusAccessor() { return &mRadiusFunc; }
  ork::Object* IntensAccessor() { return &mIntensFunc; }

  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;

  void ComputeVB(lev2::GfxTarget* tgt);

  void compute(ProcTex& ptex) final;

public:
  RotSolid();
};

///////////////////////////////////////////////////////////////////////////////
enum EColorizeType {
  ECT_1D = 0,
  ECT_2D,
};
///////////////////////////////////////////////////////////////////////////////
class Colorize : public Img32Module {
  RttiDeclareConcrete(Colorize, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  EColorizeType meColorizeType;
  bool mbAA;

public:
  Colorize();
};
///////////////////////////////////////////////////////////////////////////////
class UvMap : public Img32Module {
  RttiDeclareConcrete(UvMap, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA;

public:
  UvMap();
};
///////////////////////////////////////////////////////////////////////////////
class SphMap : public Img32Module {
  RttiDeclareConcrete(SphMap, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputN);
  DeclareImgInpPlug(InputR);
  DeclareFloatXfPlug(Directionality);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA;

public:
  SphMap();
};
///////////////////////////////////////////////////////////////////////////////
class SphRefract : public Img32Module {
  RttiDeclareConcrete(SphRefract, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);
  DeclareFloatXfPlug(Directionality);
  DeclareFloatXfPlug(IOR);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA;

public:
  SphRefract();
};
///////////////////////////////////////////////////////////////////////////////
class SolidColor : public Img32Module {
  RttiDeclareConcrete(SolidColor, Img32Module);

  void compute(ProcTex& ptex) final;

  float mfr, mfg, mfb, mfa;
  lev2::GfxMaterial3DSolid* mMaterial;

public:
  SolidColor();
};
///////////////////////////////////////////////////////////////////////////////
struct CellVert {
  fvec3 pos;
  void Lerp(const CellVert& va, const CellVert& vb, float flerp) { pos.Lerp(va.pos, vb.pos, flerp); }
  const fvec3& Pos() const { return pos; }
  CellVert(const fvec3& tp)
      : pos(tp) {}
  CellVert()
      : pos() {}
};
class CellPoly {
  orkvector<CellVert> mverts;

public:
  typedef CellVert VertexType;
  void AddVertex(const CellVert& v) { mverts.push_back(v); }
  int GetNumVertices() const { return mverts.size(); }
  const CellVert& GetVertex(int idx) const { return mverts[idx]; }
  CellPoly() {}
  void SetDefault() { mverts.clear(); }
};
class Cells : public Img32Module {
  RttiDeclareConcrete(Cells, Img32Module);

  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;

  int miSeedA;
  int miSeedB;
  int miDimU;
  int miDimV;
  int miDiv;
  int miSmoothing;
  orkvector<fvec3> mSitesA;
  orkvector<fvec3> mSitesB;
  orkvector<CellPoly> mPolys;
  bool mbAA;
  dataflow::node_hash mVBHash;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareFloatXfPlug(Dispersion);
  DeclareFloatXfPlug(SeedLerp);
  DeclareFloatXfPlug(SmoothingRadius);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  int site_index(int ix, int iy) { return (iy * miDimU) + ix; }
  void ComputeVB(lev2::GfxTarget* tgt);

public:
  Cells();
};
///////////////////////////////////////////////////////////////////////////////
enum EKaledMode {
  EKM_4SQU = 0,
  ESH_8TRI,
  ESH_24TRI,
};
class Kaled : public Img32Module {
  RttiDeclareConcrete(Kaled, Img32Module);

  typedef ork::lev2::SVtxV12C4T16 vtxt;

  EKaledMode meMode;
  dataflow::node_hash mVBHash;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);
  DeclareFloatXfPlug(Size);
  DeclareFloatXfPlug(OffsetX);
  DeclareFloatXfPlug(OffsetY);

  ork::dataflow::inplugbase* GetInput(int idx) final;

  //////////////////////////////////////////////////

  void compute(ProcTex& ptex) final;
  void ComputeVB(lev2::GfxBuffer& buffer);
  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;
  void addvtx(float fx, float fy, float fu, float fv);

public:
  Kaled();
};
///////////////////////////////////////////////////////////////////////////////
enum EIMGOP2 {
  EIO2_ADD = 0,
  EIO2_MUL,
  EIO2_AMINUSB,
  EIO2_BMINUSA,
};
class ImgOp2 : public Img32Module {
  RttiDeclareConcrete(ImgOp2, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  EIMGOP2 meOp;

public:
  ImgOp2();
};
///////////////////////////////////////////////////////////////////////////////
enum EIMGOP3 {
  EIO3_LERP = 0,
  EIO3_ADDW,
  EIO3_SUBW,
  EIO3_MUL3,
};
enum EIMGOP3CHAN {
  EIO3_CH_R = 0,
  EIO3_CH_A,
  EIO3_CH_RGB,
  EIO3_CH_RGBA,
};
class ImgOp3 : public Img32Module {
  RttiDeclareConcrete(ImgOp3, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);
  DeclareImgInpPlug(InputM);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  EIMGOP3 meOp;
  EIMGOP3CHAN meChanCtrl;
  lev2::GfxMaterial3DSolid* mMtlLerp;
  lev2::GfxMaterial3DSolid* mMtlAddw;
  lev2::GfxMaterial3DSolid* mMtlSubw;
  lev2::GfxMaterial3DSolid* mMtlMul3;

public:
  ImgOp3();
};
///////////////////////////////////////////////////////////////////////////////
class Transform : public Img32Module {
  RttiDeclareConcrete(Transform, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);
  DeclareFloatXfPlug(ScaleX);
  DeclareFloatXfPlug(ScaleY);
  DeclareFloatXfPlug(OffsetX);
  DeclareFloatXfPlug(OffsetY);
  DeclareFloatXfPlug(Rotate);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  lev2::GfxMaterial3DSolid* mMaterial;

public:
  Transform();
};
///////////////////////////////////////////////////////////////////////////////
class H2N : public Img64Module {
  RttiDeclareConcrete(H2N, Img64Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);
  DeclareFloatXfPlug(ScaleY);

  ork::dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA;
  lev2::GfxMaterial3DSolid mMTL;

public:
  H2N();
};
///////////////////////////////////////////////////////////////////////////////

class Octaves : public Img32Module {
  RttiDeclareConcrete(Octaves, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);

  DeclareFloatXfPlug(BaseOffsetX);
  DeclareFloatXfPlug(BaseOffsetY);
  DeclareFloatXfPlug(ScalOffsetX);
  DeclareFloatXfPlug(ScalOffsetY);
  DeclareFloatXfPlug(BaseFreq);
  DeclareFloatXfPlug(ScalFreq);
  DeclareFloatXfPlug(BaseAmp);
  DeclareFloatXfPlug(ScalAmp);

  //////////////////////////

  dataflow::inplugbase* GetInput(int idx) final;
  void compute(ProcTex& ptex) final;

  //////////////////////////

  ork::lev2::GfxMaterial3DSolid mOctMaterial;
  int miNumOctaves;

public:
  Octaves();
};
///////////////////////////////////////////////////////////////////////////////
class Texture : public Img32Module {
  RttiDeclareConcrete(Texture, Img32Module);

  ork::lev2::TextureAsset* mpTexture;
  void SetTextureAccessor(ork::rtti::ICastable* const& tex) { mpTexture = tex ? ork::rtti::autocast(tex) : 0; }
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = mpTexture; }

  void compute(ProcTex& ptex) final;

  ork::lev2::Texture* GetTexture() { return (0 == mpTexture) ? 0 : mpTexture->GetTexture(); }

public:
  Texture();
  bool _flipy = false;
};
///////////////////////////////////////////////////////////////////////////////
class ShaderQuad : public Img32Module {
  RttiDeclareConcrete(ShaderQuad, Img32Module);

  void SetTextureAccessor(ork::rtti::ICastable* const& tex) { mpTexture = tex ? ork::rtti::autocast(tex) : 0; }
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = mpTexture; }

  void compute(ProcTex& ptex) final;

  ork::lev2::Texture* GetTexture() { return (0 == mpTexture) ? 0 : mpTexture->GetTexture(); }

  dataflow::inplugbase* GetInput(int idx) final;

  ork::file::Path mShaderPath;
  lev2::GfxMaterial3DSolid* mShader;
  ork::lev2::TextureAsset* mpTexture;

  DeclareImgInpPlug(ImgInput0);
  DeclareFloatXfPlug(User0X);
  DeclareFloatXfPlug(User0Y);
  DeclareFloatXfPlug(User0Z);

public:
  ShaderQuad();
};
///////////////////////////////////////////////////////////////////////////////
class Group : public Img32Module {
  RttiDeclareConcrete(Group, Img32Module);

  ProcTex* mpProcTex;

  void compute(ProcTex& ptex) final;

  void SetTextureAccessor(ork::rtti::ICastable* const& tex) { mpProcTex = tex ? ork::rtti::autocast(tex) : 0; }
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = mpProcTex; }
  ork::dataflow::graph_inst* GetChildGraph() const final { return mpProcTex; }

public:
  Group();
  ProcTex* GetProcTex() const { return mpProcTex; }
  void SetProcTex(ProcTex* ptex) { mpProcTex = ptex; }
};
///////////////////////////////////////////////////////////////////////////////
enum EGradientRepeatMode {
  EGS_REPEAT,
  EGS_PINGPONG,
};
enum EGradientType {
  EGT_HORIZONTAL = 0,
  EGT_VERTICAL,
  EGT_RADIAL,
  EGT_CONICAL,
};
class Gradient : public Img32Module {
  RttiDeclareConcrete(Gradient, Img32Module);

  ork::lev2::Texture* mpTexture;
  ork::Gradient<ork::fvec4> mGradient;
  int miRepeat;
  EGradientRepeatMode meRepeatMode;
  EGradientType meGradientType;
  lev2::GfxMaterial3DSolid* mMtl;

  ork::Object* GradientAccessor() { return &mGradient; }

  void compute(ProcTex& ptex) final;

  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;

  bool mbAA;

public:
  Gradient();
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::proctex
