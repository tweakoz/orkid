///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/math/gradient.h>
#include <ork/math/multicurve.h>
#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DeclareImg32OutPlug(name)                                                                                                  \
  Img32 OutDataName(name);                                                                                                         \
  mutable ImgOutPlug OutPlugName(name);                                                                                            \
  ork::Object* OutAccessor##name() {                                                                                               \
    return &OutPlugName(name);                                                                                                     \
  }

#define DeclareImg64OutPlug(name)                                                                                                  \
  Img64 OutDataName(name);                                                                                                         \
  mutable ImgOutPlug OutPlugName(name);                                                                                            \
  ork::Object* OutAccessor##name() {                                                                                               \
    return &OutPlugName(name);                                                                                                     \
  }

#define DeclareImgInpPlug(name)                                                                                                    \
  mutable ImgInPlug InpPlugName(name);                                                                                             \
  ork::Object* InpAccessor##name() {                                                                                               \
    return &InpPlugName(name);                                                                                                     \
  }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace reflect {
class ISerializer;
class IDeserializer;
}}; // namespace ork::reflect
namespace ork { namespace proctex {
class Module;

enum class ProcTexType {
  REALTIME = 0,
  EXPORT,
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
  lev2::Context* mTarget;
  std::string _basename;
  int miW, miH;

  lev2::Texture* OutputTexture();

  void PtexBegin(lev2::Context* ptgt, bool push_full_vp, bool clear_all);
  void PtexEnd(bool pop_vp);

  bool IsBuf32() const {
    return format() == lev2::EBufferFormat::RGBA8;
  }
  bool IsBuf64() const {
    return format() == lev2::EBufferFormat::RGBA16F;
  }

  lev2::RtGroup* GetRtGroup(lev2::Context* pt);

  virtual ork::lev2::EBufferFormat format() const = 0;

  void SetBufferSize(int w, int h);
};
class Buffer32 : public Buffer {
public:
  Buffer32();
  ork::lev2::EBufferFormat format() const override {
    return lev2::EBufferFormat::RGBA8;
  }
};
class Buffer64 : public Buffer {
public:
  Buffer64();
  ork::lev2::EBufferFormat format() const override {
    return lev2::EBufferFormat::RGBA16F;
  }
};

///////////////////////////////////////////////////////////////////////////////

class ProcTex;
struct ProcTexContext;

struct ImgBase {
  mutable int miBufferIndex;
  ImgBase()
      : miBufferIndex(-1) {
  }
  virtual ork::lev2::Texture* GetTexture(ProcTex& ptex) const = 0;
  virtual Buffer& GetBuffer(ProcTex& ptex) const              = 0;
  virtual int PixelSize() const                               = 0;
};
struct Img32 : public ImgBase {
  ork::lev2::Texture* GetTexture(ProcTex& ptex) const override;
  Buffer& GetBuffer(ProcTex& ptex) const override;
  virtual int PixelSize() const override {
    return 32;
  }
};
struct Img64 : public ImgBase {
  ork::lev2::Texture* GetTexture(ProcTex& ptex) const override;
  Buffer& GetBuffer(ProcTex& ptex) const override;
  virtual int PixelSize() const override {
    return 64;
  }
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
  DeclareAbstractX(Module, ork::dataflow::dgmodule);

  ////////////////////////////////////////////////////////////
  void Compute(dataflow::workunit* wu) override {
  }
  void CombineWork(const dataflow::cluster* c) override {
  }

protected:
  Module();
};

///////////////////////////////////////////////////////////////////////////////

void RenderQuad(
    lev2::GfxMaterial* mtl,
    lev2::Context* pTARG,
    float fx1,
    float fy1,
    float fx2,
    float fy2,
    float fu1 = 0.0f,
    float fv1 = 0.0f,
    float fu2 = 1.0f,
    float fv2 = 1.0f);

class ImgModule : public Module {
  DeclareAbstractX(ImgModule, Module);

  virtual void compute(ProcTex& ptex) = 0;

  bool IsDirty(void) const final {
    return true;
  }

  bool mExport;

protected:
  void UnitTexQuad(lev2::GfxMaterial* mtl, lev2::Context* pTARG);

  static Img32 gNoCon;

  ImgModule();
  Buffer32 mThumbBuffer;
  ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16> mVW;

  ////////////////////////////////////////////////////////////
  void Compute(dataflow::workunit* wu) final;
  void CombineWork(const dataflow::cluster* clus) final {
  }
  ////////////////////////////////////////////////////////////

  void MarkClean();

public:
  ProcTex* _ptex = nullptr;
  Buffer& GetWriteBuffer(ProcTex& ptex);
  Buffer& GetThumbBuffer() {
    return mThumbBuffer;
  }
  void UpdateThumb(ProcTex& ptex);
};

///////////////////////////////////////////////////////////////////////////////

class Img64Module : public ImgModule {
  DeclareAbstractX(Img64Module, ImgModule);

protected:
  DeclareImg64OutPlug(ImgOut);

  Img64Module();

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutImgOut;
  }
};

///////////////////////////////////////////////////////////////////////////////

class Img32Module : public ImgModule {
  DeclareAbstractX(Img32Module, ImgModule);

protected:
  DeclareImg32OutPlug(ImgOut);

  Img32Module();

  dataflow::outplugbase* GetOutput(int idx) const final {
    return &mPlugOutImgOut;
  }
};

///////////////////////////////////////////////////////////////////////////////

class Curve1D : public Module {
  DeclareConcreteX(Curve1D, Module);

private:
  ork::dataflow::inplugbase* GetInput(int idx) const final;
  dataflow::outplugbase* GetOutput(int idx) const final;
  void Compute(dataflow::workunit* wu) final;
  void CombineWork(const dataflow::cluster* clus) final {
  }

  ork::Object* CurveAccessor() {
    return &mMultiCurve;
  }
  ork::Object* PlgAccessorOutput() {
    return &mOutput;
  }

  float mInValue;
  DeclareFloatXfPlug(Input);

  float mOutValue;
  mutable ork::dataflow::outplug<float> mOutput;
  MultiCurve1D mMultiCurve;

public:
  Curve1D();
};

///////////////////////////////////////////////////////////////////////////////

class Global : public Module {
  DeclareConcreteX(Global, Module);

private:
  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareFloatXfPlug(TimeScale);

  dataflow::inplugbase* GetInput(int idx) const final {
    return &mPlugInpTimeScale;
  }

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  DeclareFloatOutPlug(Time);
  DeclareFloatOutPlug(TimeDiv10);
  DeclareFloatOutPlug(TimeDiv100);

  dataflow::outplugbase* GetOutput(int idx) const final;

  //////////////////////////////////////////////////

  void Compute(dataflow::workunit* wu) final;
  void CombineWork(const dataflow::cluster* clus) final {
  }

public:
  Global();
};

///////////////////////////////////////////////////////////////////////////////

struct ProcTexContext {

  static const int k64buffers = 4;
  static const int k32buffers = 16;

  static ork::MpMcBoundedQueue<Buffer*> gBuf32Q;
  static ork::MpMcBoundedQueue<Buffer*> gBuf64Q;

  static Buffer* AllocBuffer32();
  static Buffer* AllocBuffer64();
  static void ReturnBuffer(Buffer* pbuf);

  ///////////////////////////////////

  ProcTexContext();
  ~ProcTexContext();

  void SetBufferDim(int buffer_dim);

  Buffer& GetBuffer32(int edest);
  Buffer& GetBuffer64(int edest);

  ///////////////////////////////////

  Buffer* mBuffer32[k32buffers];
  Buffer* mBuffer64[k64buffers];
  lev2::Context* mTarget = nullptr;

  float mCurrentTime       = 0.0f;
  int mBufferDim           = 0;
  ProcTexType mProcTexType = ProcTexType::REALTIME;
  bool mWriteFrames        = false;
  int mWriteFrameIndex     = 0;

  dataflow::dgcontext mdflowctx;
  dataflow::dgregisterblock mFloatRegs;
  dataflow::dgregisterblock mImage32Regs;
  dataflow::dgregisterblock mImage64Regs;
  ork::file::Path mWritePath;
  Buffer32 mTrashBuffer;
};

///////////////////////////////////////////////////////////////////////////////

class ProcTex : public ork::dataflow::graph_inst {
  DeclareConcreteX(ProcTex, ork::dataflow::graph_inst);

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

  bool GetTexQuality() const {
    return mbTexQuality;
  }

  ProcTexContext* GetPTC() {
    return mpctx;
  }
  lev2::Context* GetTarget() {
    return (mpctx != nullptr) ? mpctx->mTarget : nullptr;
  }

  ork::lev2::Texture* ResultTexture();

  static ProcTex* Load(const ork::file::Path& pth);

  bool _dogpuinit = true;
  lev2::FreestyleMaterial _thumbmtl;

private:
  bool CanConnect(const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout) const final;

  ProcTexContext* mpctx;
  bool mbTexQuality;
  ork::lev2::Texture* mpResTex;
};

///////////////////////////////////////////////////////////////////////////////

enum PeriodicShape {
  SAW = 0,
  SIN,
  COS,
  SQU,
};

struct Periodic : public Module {
  DeclareAbstractX(Periodic, Module);

public:
  Periodic();

  float compute(float unitphase);

  void Hash(dataflow::node_hash& hash);

private:
  ////////////////////////////////////////////
  ork::dataflow::inplugbase* GetInput(int idx) const final;
  ////////////////////////////////////////////

  DeclareFloatXfPlug(Frequency);
  DeclareFloatXfPlug(Amplitude);
  DeclareFloatXfPlug(PhaseOffset);
  DeclareFloatXfPlug(Bias);

  PeriodicShape meShape = PeriodicShape::SQU;

  ////////////////////////////////////////////
  ////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
struct RotSolid : public Img32Module {

  DeclareConcreteX(RotSolid, Img32Module);

public:
  RotSolid();

  ork::Object* RadiusAccessor() {
    return &mRadiusFunc;
  }
  ork::Object* IntensAccessor() {
    return &mIntensFunc;
  }

  void ComputeVB(lev2::Context* tgt);

  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareFloatXfPlug(PhaseOffset);

  ork::dataflow::inplugbase* GetInput(int idx) const final {
    return &mPlugInpPhaseOffset;
  }

  /////////////////////////////////////////

  int miNumSides                  = 3;
  ork::lev2::Blending meBlendMode = lev2::Blending::OFF;
  bool mbAA                       = false;

  Periodic mRadiusFunc;
  Periodic mIntensFunc;
  dataflow::node_hash mVBHash;
  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;
};

///////////////////////////////////////////////////////////////////////////////
enum EColorizeType {
  ECT_1D = 0,
  ECT_2D,
};
///////////////////////////////////////////////////////////////////////////////
struct Colorize : public Img32Module {
  DeclareConcreteX(Colorize, Img32Module);

public:
  Colorize();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  EColorizeType meColorizeType = EColorizeType::ECT_1D;
  bool mbAA                    = false;
};
///////////////////////////////////////////////////////////////////////////////
struct UvMap : public Img32Module {
  DeclareConcreteX(UvMap, Img32Module);

public:
  UvMap();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA = false;
};
///////////////////////////////////////////////////////////////////////////////
struct SphMap : public Img32Module {
  DeclareConcreteX(SphMap, Img32Module);

public:
  SphMap();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputN);
  DeclareImgInpPlug(InputR);
  DeclareFloatXfPlug(Directionality);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA = false;
};
///////////////////////////////////////////////////////////////////////////////
struct SphRefract : public Img32Module {
  DeclareConcreteX(SphRefract, Img32Module);

public:
  SphRefract();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);
  DeclareFloatXfPlug(Directionality);
  DeclareFloatXfPlug(IOR);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA = false;
};
///////////////////////////////////////////////////////////////////////////////
class SolidColor : public Img32Module {
  DeclareConcreteX(SolidColor, Img32Module);

  void compute(ProcTex& ptex) final;

  float mfr, mfg, mfb, mfa;
  lev2::GfxMaterial3DSolid* mMaterial;

public:
  SolidColor();
};
///////////////////////////////////////////////////////////////////////////////
struct CellVert {
  fvec3 pos;
  void lerp(const CellVert& va, const CellVert& vb, float flerp) {
    pos.lerp(va.pos, vb.pos, flerp);
  }
  const fvec3& Pos() const {
    return pos;
  }
  CellVert(const fvec3& tp)
      : pos(tp) {
  }
  CellVert()
      : pos() {
  }
};
class CellPoly {
  orkvector<CellVert> mverts;

public:
  typedef CellVert VertexType;
  void AddVertex(const CellVert& v) {
    mverts.push_back(v);
  }
  int GetNumVertices() const {
    return mverts.size();
  }
  const CellVert& GetVertex(int idx) const {
    return mverts[idx];
  }
  CellPoly() {
  }
  void SetDefault() {
    mverts.clear();
  }
};
struct Cells : public Img32Module {
  DeclareConcreteX(Cells, Img32Module);

public:
  Cells();
  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  int site_index(int ix, int iy) {
    return (iy * miDimU) + ix;
  }
  void ComputeVB(lev2::Context* tgt);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareFloatXfPlug(Dispersion);
  DeclareFloatXfPlug(SeedLerp);
  DeclareFloatXfPlug(SmoothingRadius);

  //////////////////////////////////////////////////

  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;

  int miSeedA     = 0;
  int miSeedB     = 0;
  int miDimU      = 2;
  int miDimV      = 2;
  int miDiv       = 1;
  int miSmoothing = 0;
  bool mbAA       = false;

  orkvector<fvec3> mSitesA;
  orkvector<fvec3> mSitesB;
  orkvector<CellPoly> mPolys;
  dataflow::node_hash mVBHash;
};
///////////////////////////////////////////////////////////////////////////////
enum class KaledMode {
  SQU4 = 0,
  TRI8,
  TRI24,
};
///////////////////////////////////////////////////////////////////////////////
struct Kaled : public Img32Module {
  DeclareConcreteX(Kaled, Img32Module);

public:
  Kaled();
  void compute(ProcTex& ptex) final;
  void ComputeVB(lev2::OffscreenBuffer& buffer);
  void addvtx(float fx, float fy, float fu, float fv);

  typedef ork::lev2::SVtxV12C4T16 vtxt;

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);
  DeclareFloatXfPlug(Size);
  DeclareFloatXfPlug(OffsetX);
  DeclareFloatXfPlug(OffsetY);

  ork::dataflow::inplugbase* GetInput(int idx) const final;

  //////////////////////////////////////////////////

  KaledMode meMode = KaledMode::SQU4;
  dataflow::node_hash mVBHash;
  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;
};
///////////////////////////////////////////////////////////////////////////////
enum class ImageOp2 {
  ADD = 0,
  MUL,
  AMINUSB,
  BMINUSA,
};
class ImgOp2 : public Img32Module {
  DeclareConcreteX(ImgOp2, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  ImageOp2 meOp;

public:
  ImgOp2();
};
///////////////////////////////////////////////////////////////////////////////
enum ImageOp3 {
  LERP = 0,
  ADDW,
  SUBW,
  MUL3,
};
enum ImageOp3Channel {
  R = 0,
  A,
  RGB,
  RGBA,
};
class ImgOp3 : public Img32Module {
  DeclareConcreteX(ImgOp3, Img32Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(InputA);
  DeclareImgInpPlug(InputB);
  DeclareImgInpPlug(InputM);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  ImageOp3 meOp;
  ImageOp3Channel meChanCtrl;
  lev2::GfxMaterial3DSolid* mMtlLerp;
  lev2::GfxMaterial3DSolid* mMtlAddw;
  lev2::GfxMaterial3DSolid* mMtlSubw;
  lev2::GfxMaterial3DSolid* mMtlMul3;

public:
  ImgOp3();
};
///////////////////////////////////////////////////////////////////////////////
struct Transform : public Img32Module {
  DeclareConcreteX(Transform, Img32Module);

public:
  Transform();

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);
  DeclareFloatXfPlug(ScaleX);
  DeclareFloatXfPlug(ScaleY);
  DeclareFloatXfPlug(OffsetX);
  DeclareFloatXfPlug(OffsetY);
  DeclareFloatXfPlug(Rotate);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  lev2::GfxMaterial3DSolid* mMaterial = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
class H2N : public Img64Module {
  DeclareConcreteX(H2N, Img64Module);

  //////////////////////////////////////////////////
  // inputs
  //////////////////////////////////////////////////

  DeclareImgInpPlug(Input);
  DeclareFloatXfPlug(ScaleY);

  ork::dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////////////////////////////

  bool mbAA;
  lev2::GfxMaterial3DSolid mMTL;

public:
  H2N();
};
///////////////////////////////////////////////////////////////////////////////

struct Octaves : public Img32Module {
  DeclareConcreteX(Octaves, Img32Module);

public:
  Octaves();

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

  dataflow::inplugbase* GetInput(int idx) const final;
  void compute(ProcTex& ptex) final;

  //////////////////////////

  ork::lev2::GfxMaterial3DSolid mOctMaterial;
  int miNumOctaves = 1;
};
///////////////////////////////////////////////////////////////////////////////
class Texture : public Img32Module {
  DeclareConcreteX(Texture, Img32Module);

  ork::lev2::TextureAsset* mpTexture;
  void SetTextureAccessor(ork::rtti::ICastable* const& tex) {
    mpTexture = tex ? ork::rtti::autocast(tex) : 0;
  }
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const {
    tex = mpTexture;
  }

  void compute(ProcTex& ptex) final;

  ork::lev2::Texture* GetTexture() {
    return (0 == mpTexture) ? 0 : mpTexture->GetTexture().get();
  }

public:
  Texture();
  bool _flipy = false;
};
///////////////////////////////////////////////////////////////////////////////
struct ShaderQuad : public Img32Module {
  DeclareConcreteX(ShaderQuad, Img32Module);

public:
  ShaderQuad();

  void SetTextureAccessor(ork::rtti::ICastable* const& tex) {
    mpTexture = tex ? ork::rtti::autocast(tex) : 0;
  }
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const {
    tex = mpTexture;
  }

  void compute(ProcTex& ptex) final;

  ork::lev2::Texture* GetTexture() {
    return (0 == mpTexture) ? 0 : mpTexture->GetTexture().get();
  }

  dataflow::inplugbase* GetInput(int idx) const final;

  /////////////////////////////////

  DeclareImgInpPlug(ImgInput0);
  DeclareFloatXfPlug(User0X);
  DeclareFloatXfPlug(User0Y);
  DeclareFloatXfPlug(User0Z);

  /////////////////////////////////

  lev2::GfxMaterial3DSolid* mShader = nullptr;
  lev2::TextureAsset* mpTexture     = nullptr;

  file::Path mShaderPath;
};
///////////////////////////////////////////////////////////////////////////////
class Group : public Img32Module {
  DeclareConcreteX(Group, Img32Module);

  ProcTex* mpProcTex;

  void compute(ProcTex& ptex) final;

  void SetTextureAccessor(ork::rtti::ICastable* const& tex) {
    mpProcTex = tex ? ork::rtti::autocast(tex) : 0;
  }
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const {
    tex = mpProcTex;
  }
  ork::dataflow::graph_inst* GetChildGraph() const final {
    return mpProcTex;
  }

public:
  Group();
  ProcTex* GetProcTex() const {
    return mpProcTex;
  }
  void SetProcTex(ProcTex* ptex) {
    mpProcTex = ptex;
  }
};
///////////////////////////////////////////////////////////////////////////////
enum class GradientRepeatMode {
  REPEAT,
  PINGPONG,
};
enum class GradientType {
  HORIZONTAL = 0,
  VERTICAL,
  RADIAL,
  CONICAL,
};

struct Gradient : public Img32Module {
  DeclareConcreteX(Gradient, Img32Module);

public:
  Gradient();

  ork::Object* GradientAccessor() {
    return &mGradient;
  }

  void compute(ProcTex& ptex) final;

  ork::lev2::Texture* mpTexture   = nullptr;
  lev2::GfxMaterial3DSolid* mMtl  = nullptr;
  int miRepeat                    = 1;
  bool mbAA                       = false;
  GradientRepeatMode meRepeatMode = GradientRepeatMode::REPEAT;
  GradientType meGradientType     = GradientType::HORIZONTAL;

  ork::Gradient<ork::fvec4> mGradient;

  ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> mVertexBuffer;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::proctex
