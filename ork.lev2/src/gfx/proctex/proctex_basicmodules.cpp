///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/pch.h>

#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/register.h>

#include <ork/file/file.h>
#include <ork/math/plane.hpp>
#include <ork/math/polar.h>
#include <ork/reflect/enum_serializer.inl>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Periodic, "proctex::periodic");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::RotSolid, "proctex::RotSolid");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Colorize, "proctex::Colorize");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::SolidColor, "proctex::SolidColor");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ImgOp2, "proctex::ImgOp2");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ImgOp3, "proctex::ImgOp3");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Transform, "proctex::Transform");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Texture, "proctex::Texture");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ShaderQuad, "proctex::ShaderQuad");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Gradient, "proctex::Gradient");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Curve1D, "proctex::Curve1D");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Global, "proctex::Global");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Group, "proctex::Group");

///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::proctex, EPeriodicShape)
DECLARE_ENUM(ESH_SAW)
DECLARE_ENUM(ESH_SIN)
DECLARE_ENUM(ESH_COS)
DECLARE_ENUM(ESH_SQU)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::proctex, EIMGOP2)
DECLARE_ENUM(EIO2_ADD)
DECLARE_ENUM(EIO2_MUL)
DECLARE_ENUM(EIO2_AMINUSB)
DECLARE_ENUM(EIO2_BMINUSA)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::proctex, EIMGOP3)
DECLARE_ENUM(EIO3_LERP)
DECLARE_ENUM(EIO3_ADDW)
DECLARE_ENUM(EIO3_SUBW)
DECLARE_ENUM(EIO3_MUL3)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::proctex, EIMGOP3CHAN)
DECLARE_ENUM(EIO3_CH_R)
DECLARE_ENUM(EIO3_CH_A)
DECLARE_ENUM(EIO3_CH_RGB)
DECLARE_ENUM(EIO3_CH_RGBA)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::proctex, EGradientRepeatMode)
DECLARE_ENUM(EGS_REPEAT)
DECLARE_ENUM(EGS_PINGPONG)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::proctex, EGradientType)
DECLARE_ENUM(EGT_HORIZONTAL)
DECLARE_ENUM(EGT_VERTICAL)
DECLARE_ENUM(EGT_RADIAL)
DECLARE_ENUM(EGT_CONICAL)
END_ENUM_SERIALIZER()

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace proctex {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Periodic::Describe() {
  RegisterFloatXfPlug(Periodic, Frequency, 0.0f, 36.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Periodic, Amplitude, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Periodic, PhaseOffset, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Periodic, Bias, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);

  ork::reflect::RegisterProperty("Shape", &Periodic::meShape);
  ork::reflect::annotatePropertyForEditor<Periodic>("Shape", "editor.class", "ged.factory.enum");
}
dataflow::inplugbase* Periodic::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpFrequency;
      break;
    case 1:
      rval = &mPlugInpAmplitude;
      break;
    case 2:
      rval = &mPlugInpBias;
      break;
    case 3:
      rval = &mPlugInpPhaseOffset;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
Periodic::Periodic()
    : mfFrequency(1.0f)
    , mfAmplitude(0.0f)
    , mfBias(1.0f)
    , mfPhaseOffset(0.0f)
    , meShape(ESH_SQU)
    , mPlugInpFrequency(this, dataflow::EPR_UNIFORM, mfFrequency, "frq")
    , mPlugInpAmplitude(this, dataflow::EPR_UNIFORM, mfAmplitude, "amp")
    , mPlugInpPhaseOffset(this, dataflow::EPR_UNIFORM, mfPhaseOffset, "pho")
    , mPlugInpBias(this, dataflow::EPR_UNIFORM, mfBias, "bia") {
}
void Periodic::Hash(dataflow::node_hash& hash) {
  hash.Hash(mPlugInpFrequency.GetValue());
  hash.Hash(mPlugInpAmplitude.GetValue());
  hash.Hash(mPlugInpBias.GetValue());
  hash.Hash(mPlugInpPhaseOffset.GetValue());
  hash.Hash(meShape);
}
///////////////////////////////////////////////////////////////////////////////
float Periodic::compute(float unitphase) {
  float rval    = 0.0f;
  float inphase = mPlugInpPhaseOffset.GetValue() + (unitphase * mPlugInpFrequency.GetValue());
  switch (meShape) {
    case ESH_SAW:
      rval = (2.0f * fmod(inphase, 1.0f)) - 1.0f;
      break;
    case ESH_SQU:
      rval = fmod(inphase, 1.0f) < 0.5f ? 0.0f : 1.0f;
      break;
    case ESH_COS:
      rval = cosf(inphase * PI2);
      break;
    case ESH_SIN:
      rval = sinf(inphase * PI2);
      break;
    default:
      break;
  }
  rval = mPlugInpBias.GetValue() + (rval * mPlugInpAmplitude.GetValue());
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Global::Describe() {
}
Global::Global()
    : ConstructOutPlug(Time, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(TimeDiv10, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(TimeDiv100, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(TimeScale, dataflow::EPR_UNIFORM, mfTimeScale)
    , mfTimeScale(1.0f)
    , mOutDataTime(0.0f)
    , mOutDataTimeDiv10(0.0f)
    , mOutDataTimeDiv100(0.0f) {
}
void Global::Compute(dataflow::workunit* wu) // virtual
{
  auto ptex = wu->GetContextData().Get<ProcTex*>();
  auto ctx  = ptex->GetPTC();

  float ftime                                   = ctx->mCurrentTime;
  const_cast<Global*>(this)->mOutDataTime       = ftime;
  const_cast<Global*>(this)->mOutDataTimeDiv10  = ftime * 0.1f;
  const_cast<Global*>(this)->mOutDataTimeDiv100 = ftime * 0.01f;
}
dataflow::outplugbase* Global::GetOutput(int idx) {
  dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugOutTime;
      break;
    case 1:
      rval = &mPlugOutTimeDiv10;
      break;
    case 2:
      rval = &mPlugOutTimeDiv100;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Curve1D::Describe() {
  RegisterFloatXfPlug(Curve1D, Input, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  ork::reflect::RegisterProperty("curve", &Curve1D::CurveAccessor);
  // ork::reflect::annotatePropertyForEditor<Curve1D>( "curve", "editor.class", "ged.factory.curve1d" );
}
Curve1D::Curve1D()
    : mOutput(this, dataflow::EPR_UNIFORM, &mOutValue, "Output")
    , mPlugInpInput(this, dataflow::EPR_UNIFORM, mInValue, "Input")
    , mOutValue(0.0f)
    , mInValue(0.0f) {
}
void Curve1D::Compute(dataflow::workunit* wu) // virtual
{
  float finp = fmod(mPlugInpInput.GetValue(), 1.0f);
  mOutValue  = mMultiCurve.Sample(finp);
}
dataflow::outplugbase* Curve1D::GetOutput(int idx) {
  dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mOutput;
      break;
  }
  return rval;
}
dataflow::inplugbase* Curve1D::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RotSolid::Describe() {
  ork::reflect::RegisterProperty("NumSides", &RotSolid::miNumSides);
  ork::reflect::RegisterProperty("BlendMode", &RotSolid::meBlendMode);
  ork::reflect::RegisterProperty("Radius", &RotSolid::RadiusAccessor);
  ork::reflect::RegisterProperty("Intens", &RotSolid::IntensAccessor);
  ork::reflect::RegisterProperty("AntiAlias", &RotSolid::mbAA);

  RegisterFloatXfPlug(RotSolid, PhaseOffset, 0.0f, 360.0f, ged::OutPlugChoiceDelegate);

  ork::reflect::annotatePropertyForEditor<RotSolid>("NumSides", "editor.range.min", "3");
  ork::reflect::annotatePropertyForEditor<RotSolid>("NumSides", "editor.range.max", "360");
  ork::reflect::annotatePropertyForEditor<RotSolid>("BlendMode", "editor.class", "ged.factory.enum");

  static const char* EdGrpStr = "grp://Basic AntiAlias NumSides BlendMode "
                                "grp://Plugs PhaseOffset Radius Intens ";

  reflect::annotateClassForEditor<RotSolid>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
RotSolid::RotSolid()
    : miNumSides(3)
    , mVertexBuffer(2048, 0, ork::lev2::PrimitiveType::TRIANGLES)
    , meBlendMode(ork::lev2::Blending::OFF)
    , mVBHash()
    , mfPhaseOffset(0.0f)
    , mPlugInpPhaseOffset(this, dataflow::EPR_UNIFORM, mfPhaseOffset, "pho")
    , mbAA(false) {
}
///////////////////////////////////////////////////////////////////////////////
void RotSolid::ComputeVB(ork::lev2::Context* pTARG) {
  int inumv = miNumSides * 3;
  mVertexBuffer.Reset();
  ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16> vw;
  vw.Lock(pTARG, &mVertexBuffer, inumv);
  ////////////////////////////////////////
  const float kfZ = 0.0f;
  ork::lev2::SVtxV12C4T16 ctrvertex, vertexa, vertexb;
  ctrvertex._position = fvec3(0.0f, 0.0f, kfZ);
  ctrvertex._color    = 0xff000000;
  vertexa._color      = 0xffff0000;
  vertexb._color      = 0xffffff00;
  ////////////////////////////////////////
  for (int i = 0; i < miNumSides; i++) {
    float fiI = float(i) / float(miNumSides - 1);
    float fiA = float(i) / float(miNumSides);
    float fiB = float((i + 1)) / float(miNumSides);

    float fphA = (fiA * PI2) + (mPlugInpPhaseOffset.GetValue() * PI2 / 360.0f);
    float fphB = (fiB * PI2) + (mPlugInpPhaseOffset.GetValue() * PI2 / 360.0f);

    float fradA = mRadiusFunc.compute(fiA);
    float fradB = mRadiusFunc.compute(fiB);
    float fclrA = mIntensFunc.compute(fiI);

    u8 uca      = u8(fclrA * 255.0f);
    u32 ucolora = (0xff << 24) + (uca << 16) + (uca << 8) + uca;

    float fsinA = sinf(fphA) * fradA;
    float fcosA = cosf(fphA) * fradA;
    float fsinB = sinf(fphB) * fradB;
    float fcosB = cosf(fphB) * fradB;

    vertexa._color   = ucolora;
    vertexb._color   = ucolora;
    ctrvertex._color = ucolora;

    vertexa._position = fvec3(fsinA, fcosA, kfZ);
    vertexb._position = fvec3(fsinB, fcosB, kfZ);

    vw.AddVertex(vertexa);
    vw.AddVertex(vertexb);
    vw.AddVertex(ctrvertex);
  }
  vw.UnLock(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void RotSolid::compute(ProcTex& ptex) {
  Buffer& buffer = GetWriteBuffer(ptex);
  // printf( "rotsolid wrbuf<%p> wrtex<%p>\n", & buffer, buffer.OutputTexture() );

  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::RotSolid::compute"));

  //////////////////////////////////////
  dataflow::node_hash testhash;
  mRadiusFunc.Hash(testhash);
  mIntensFunc.Hash(testhash);
  testhash.Hash(miNumSides);
  testhash.Hash(mPlugInpPhaseOffset.GetValue());
  //////////////////////////////////////
  if (testhash != mVBHash) {
    ComputeVB(pTARG);
    mVBHash = testhash;
  }
  ////////////////////////////////////////////////////////////////
  // fmtx4 mtxortho = pTARG->Ortho( -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f );
  ////////////////////////////////////////////////////////////////

  struct AA16RenderRot : public AA16Render {
    virtual void DoRender(float left, float right, float top, float bot, Buffer& buf) {
      auto targ = mPTX.GetTarget();
      targ->GBI()->DrawPrimitive(&stdmat, mVB, ork::lev2::PrimitiveType::TRIANGLES, 0, mVB.GetNumVertices());
    }

    lev2::GfxMaterial3DSolid stdmat;
    lev2::VertexBufferBase& mVB;

    AA16RenderRot(ProcTex& ptx, Buffer& bo, lev2::Blending ebm, lev2::VertexBufferBase& vb)
        : AA16Render(ptx, bo)
        , stdmat(ptx.GetTarget())
        , mVB(vb) {
      stdmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
      stdmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      stdmat._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      stdmat._rasterstate.SetBlending(ebm);
      stdmat._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
      stdmat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(bo.miW)));

      mOrthoBoxXYWH = fvec4(-1.0f, -1.0f, 2.0f, 2.0f);
    }
  };

  ////////////////////////////////////////////////////////////////

  AA16RenderRot renderer(ptex, buffer, meBlendMode, mVertexBuffer);
  renderer.Render(mbAA);

  ////////////////////////////////////////////////////////////////

  // buffer.mHash = testhash;
  MarkClean();
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ImgOp2::Describe() {
  RegisterObjInpPlug(ImgOp2, InputA);
  RegisterObjInpPlug(ImgOp2, InputB);
  ork::reflect::RegisterProperty("Op", &ImgOp2::meOp);
  ork::reflect::annotatePropertyForEditor<ImgOp2>("Op", "editor.class", "ged.factory.enum");
}
ork::dataflow::inplugbase* ImgOp2::GetInput(int idx) {
  ork::dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInputA;
      break;
    case 1:
      rval = &mPlugInpInputB;
      break;
  }
  return rval;
}
ImgOp2::ImgOp2()
    : ConstructInpPlug(InputA, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(InputB, dataflow::EPR_UNIFORM, gNoCon)
    , meOp(EIO2_ADD) {
}
void ImgOp2::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::ImgOp2::compute"));

  Buffer& buffer             = GetWriteBuffer(ptex);
  const ImgOutPlug* conplugA = rtti::autocast(mPlugInpInputA.GetExternalOutput());
  const ImgOutPlug* conplugB = rtti::autocast(mPlugInpInputB.GetExternalOutput());
  if (conplugA && conplugB) {

    buffer.PtexBegin(pTARG, true, false);

    const char* pop = 0;
    switch (meOp) {
      case EIO2_ADD:
        pop = "imgop2_add";
        break;
      case EIO2_MUL:
        pop = "imgop2_mul";
        break;
      case EIO2_AMINUSB:
        pop = "imgop2_aminusb";
        break;
      case EIO2_BMINUSA:
        pop = "imgop2_bminusa";
        break;
    }
    lev2::GfxMaterial3DSolid gridmat(pTARG, "orkshader://proctex", pop);
    gridmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
    gridmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    gridmat._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
    gridmat._rasterstate.SetBlending(ork::lev2::Blending::OFF);
    gridmat._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

    auto texa = conplugA->GetValue().GetTexture(ptex);
    auto texb = conplugB->GetValue().GetTexture(ptex);
    gridmat.SetTexture(texa);
    gridmat.SetTexture2(texb);
    gridmat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(buffer.miW)));
    ////////////////////////////////////////////////////////////////
    UnitTexQuad(&gridmat, pTARG);
    buffer.PtexEnd(true);
  }
  MarkClean();
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ImgOp3::Describe() {
  RegisterObjInpPlug(ImgOp3, InputA);
  RegisterObjInpPlug(ImgOp3, InputB);
  RegisterObjInpPlug(ImgOp3, InputM);
  ork::reflect::RegisterProperty("Op", &ImgOp3::meOp);
  ork::reflect::annotatePropertyForEditor<ImgOp3>("Op", "editor.class", "ged.factory.enum");
  ork::reflect::RegisterProperty("ChanCtrl", &ImgOp3::meChanCtrl);
  ork::reflect::annotatePropertyForEditor<ImgOp3>("ChanCtrl", "editor.class", "ged.factory.enum");
}
ork::dataflow::inplugbase* ImgOp3::GetInput(int idx) {
  ork::dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInputA;
      break;
    case 1:
      rval = &mPlugInpInputB;
      break;
    case 2:
      rval = &mPlugInpInputM;
      break;
  }
  return rval;
}
ImgOp3::ImgOp3()
    : ConstructInpPlug(InputA, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(InputB, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(InputM, dataflow::EPR_UNIFORM, gNoCon)
    , meOp(EIO3_LERP)
    , meChanCtrl(EIO3_CH_RGB)
    , mMtlLerp(nullptr)
    , mMtlAddw(nullptr)
    , mMtlSubw(nullptr)
    , mMtlMul3(nullptr) {
}
void ImgOp3::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::ImgOp3::compute"));

  Buffer& buffer             = GetWriteBuffer(ptex);
  const ImgOutPlug* conplugA = rtti::autocast(mPlugInpInputA.GetExternalOutput());
  const ImgOutPlug* conplugB = rtti::autocast(mPlugInpInputB.GetExternalOutput());
  const ImgOutPlug* conplugM = rtti::autocast(mPlugInpInputM.GetExternalOutput());

  const ImgOutPlug* output_plug = &mPlugOutImgOut;

  auto& plugbuf      = output_plug->GetValue().GetBuffer(ptex);
  const auto& conrtg = plugbuf.GetRtGroup(pTARG);
  auto plug_name     = output_plug->GetName();
  auto con_mod       = output_plug->GetModule();
  auto mod_nam       = con_mod->GetName();

  // printf( "RENDERING ImgOp3<%p> output mod<%p:%s> buf<%p> plug<%p:%s> rtg<%p>\n", this, con_mod, mod_nam.c_str(), & plugbuf,
  // output_plug, plug_name.c_str(), conrtg );

  if (conplugA && conplugB && conplugM) {
    if (nullptr == mMtlLerp) {
      mMtlLerp = new lev2::GfxMaterial3DSolid(pTARG, "orkshader://proctex", "imgop3_lerp");
      mMtlAddw = new lev2::GfxMaterial3DSolid(pTARG, "orkshader://proctex", "imgop3_addw");
      mMtlSubw = new lev2::GfxMaterial3DSolid(pTARG, "orkshader://proctex", "imgop3_subw");
      mMtlMul3 = new lev2::GfxMaterial3DSolid(pTARG, "orkshader://proctex", "imgop3_mul3");

      mMtlLerp->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
      mMtlLerp->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      mMtlLerp->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      mMtlLerp->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
      mMtlLerp->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

      mMtlAddw->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
      mMtlAddw->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      mMtlAddw->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      mMtlAddw->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
      mMtlAddw->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

      mMtlSubw->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
      mMtlSubw->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      mMtlSubw->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      mMtlSubw->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
      mMtlSubw->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

      mMtlMul3->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
      mMtlMul3->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      mMtlMul3->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      mMtlMul3->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
      mMtlMul3->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
    }

    lev2::GfxMaterial3DSolid* cur_mtl = nullptr;
    switch (meOp) {
      case EIO3_LERP:
        cur_mtl = mMtlLerp;
        break;
      case EIO3_ADDW:
        cur_mtl = mMtlAddw;
        break;
      case EIO3_SUBW:
        cur_mtl = mMtlSubw;
        break;
      case EIO3_MUL3:
        cur_mtl = mMtlMul3;
        break;
    }
    cur_mtl->SetTexture(conplugA->GetValue().GetTexture(ptex));
    cur_mtl->SetTexture2(conplugB->GetValue().GetTexture(ptex));
    cur_mtl->SetTexture3(conplugM->GetValue().GetTexture(ptex));
    cur_mtl->SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(buffer.miW)));

    ////////////////////////////////////////////////////////////////
    buffer.PtexBegin(pTARG, true, true);
    UnitTexQuad(cur_mtl, pTARG);
    buffer.PtexEnd(true);
    ////////////////////////////////////////////////////////////////
    MarkClean();
    pTARG->debugPopGroup();
  }
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Transform::Describe() {
  RegisterObjInpPlug(Transform, Input);
  RegisterFloatXfPlug(Transform, ScaleX, -16.0f, 16.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Transform, ScaleY, -16.0f, 16.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Transform, OffsetX, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Transform, OffsetY, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(Transform, Rotate, -360.0f, 360.0f, ged::OutPlugChoiceDelegate);
}
ork::dataflow::inplugbase* Transform::GetInput(int idx) {
  ork::dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpScaleX;
      break;
    case 2:
      rval = &mPlugInpScaleY;
      break;
    case 3:
      rval = &mPlugInpOffsetX;
      break;
    case 4:
      rval = &mPlugInpOffsetY;
      break;
    case 5:
      rval = &mPlugInpRotate;
      break;
  }
  return rval;
}
Transform::Transform()
    : ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(ScaleX, dataflow::EPR_UNIFORM, mfScaleX)
    , ConstructInpPlug(ScaleY, dataflow::EPR_UNIFORM, mfScaleY)
    , ConstructInpPlug(OffsetX, dataflow::EPR_UNIFORM, mfOffsetX)
    , ConstructInpPlug(OffsetY, dataflow::EPR_UNIFORM, mfOffsetY)
    , ConstructInpPlug(Rotate, dataflow::EPR_UNIFORM, mfRotate)
    , mfScaleX(1.0f)
    , mfScaleY(1.0f)
    , mfOffsetX(0.0f)
    , mfOffsetY(0.0f)
    , mfRotate(0.0f)
    , mMaterial(nullptr) {
}
void Transform::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::Transform::compute"));

  Buffer& buffer            = GetWriteBuffer(ptex);
  const ImgOutPlug* conplug = rtti::autocast(mPlugInpInput.GetExternalOutput());
  if (conplug) {
    buffer.PtexBegin(pTARG, true, false);

    if (nullptr == mMaterial) {
      mMaterial = new lev2::GfxMaterial3DSolid(pTARG, "orkshader://proctex", "transform");
      mMaterial->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
      mMaterial->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
      mMaterial->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
      mMaterial->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
      mMaterial->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
    }
    ////////////////////////////////////////////////////////////////
    float rot = PI2 * mPlugInpRotate.GetValue() / 360.0f;
    ////////////////////////////////////////////////////////////////
    fmtx4 mtxR, mtxS, mtxT, mtxTO1, mtxTO2;
    mtxS.Scale(mPlugInpScaleX.GetValue(), mPlugInpScaleY.GetValue(), 1.0f);
    mtxTO1.SetTranslation(-0.5f, -0.5f, 0.0f);
    mtxTO2.SetTranslation(+0.5f, +0.5f, 0.0f);
    mtxT.SetTranslation(mPlugInpOffsetX.GetValue(), mPlugInpOffsetY.GetValue(), 0.0f);
    mtxR.SetRotateZ(rot);
    float sina = sinf(rot);
    float cosa = cosf(rot);
    ////////////////////////////////////////////////////////////////
    mMaterial->SetTexture(conplug->GetValue().GetTexture(ptex));
    mMaterial->SetAuxMatrix((mtxTO1 * (mtxR * mtxS) * mtxTO2) * mtxT);
    mMaterial->SetUser0(fvec4(sina, cosa, 0.0f, float(buffer.miW)));
    ////////////////////////////////////////////////////////////////
    UnitTexQuad(mMaterial, pTARG);
    buffer.PtexEnd(true);
  }
  MarkClean();
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Texture::Describe() {
  ork::reflect::RegisterProperty("Input", &Texture::GetTextureAccessor, &Texture::SetTextureAccessor);
  ork::reflect::RegisterProperty("FlipY", &Texture::_flipy);
  ork::reflect::annotatePropertyForEditor<Texture>("Input", "editor.class", "ged.factory.assetlist");
  ork::reflect::annotatePropertyForEditor<Texture>("Input", "editor.assettype", "lev2tex");
  ork::reflect::annotatePropertyForEditor<Texture>("Input", "editor.assetclass", "lev2tex");
}
Texture::Texture()
    : mpTexture(0) {
}
void Texture::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();

  Buffer& buffer = GetWriteBuffer(ptex);
  buffer.PtexBegin(pTARG, true, false);

  pTARG->debugPushGroup(FormatString("ptx::Texture::compute"));
  lev2::GfxMaterial3DSolid gridmat(pTARG, "orkshader://proctex", "ttex");
  gridmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
  gridmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
  gridmat._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
  gridmat._rasterstate.SetBlending(ork::lev2::Blending::OFF);
  gridmat._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
  gridmat.SetTexture(GetTexture());
  gridmat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(buffer.miW)));
  ////////////////////////////////////////////////////////////////
  float ftexw = GetTexture() ? GetTexture()->_width : 1.0f;
  pTARG->PushModColor(ork::fvec4(ftexw, ftexw, ftexw, ftexw));
  ////////////////////////////////////////////////////////////////
  fmtx4 mtxortho = pTARG->MTXI()->Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);
  pTARG->MTXI()->PushPMatrix(mtxortho);
  pTARG->MTXI()->PushVMatrix(fmtx4::Identity());
  pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
  // pTARG->PushModColor( fvec3::White() );
  {
    if (_flipy) {
      RenderQuad(&gridmat, pTARG, -1, -1, 1, 1);

    } else {
      RenderQuad(&gridmat, pTARG, -1, 1, 1, -1);
    }
  }
  pTARG->MTXI()->PopPMatrix();
  pTARG->MTXI()->PopVMatrix();
  pTARG->MTXI()->PopMMatrix();
  pTARG->PopModColor();
  UnitTexQuad(&gridmat, pTARG);
  MarkClean();
  buffer.PtexEnd(true);
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ShaderQuad::Describe() {
  ork::reflect::RegisterProperty("ShaderFile", &ShaderQuad::mShaderPath);
  ork::reflect::annotatePropertyForEditor<ShaderQuad>("ShaderFile", "editor.class", "ged.factory.filelist");
  ork::reflect::annotatePropertyForEditor<ShaderQuad>("ShaderFile", "editor.filetype", "glfx");
  ork::reflect::annotatePropertyForEditor<ShaderQuad>("ShaderFile", "editor.filebase", "orkshader://");

  ork::reflect::RegisterProperty("Texture0", &ShaderQuad::GetTextureAccessor, &ShaderQuad::SetTextureAccessor);
  ork::reflect::annotatePropertyForEditor<ShaderQuad>("Texture0", "editor.class", "ged.factory.assetlist");
  ork::reflect::annotatePropertyForEditor<ShaderQuad>("Texture0", "editor.assettype", "lev2tex");
  ork::reflect::annotatePropertyForEditor<ShaderQuad>("Texture0", "editor.assetclass", "lev2tex");

  RegisterObjInpPlug(ShaderQuad, ImgInput0);
  RegisterFloatXfPlug(ShaderQuad, User0X, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ShaderQuad, User0Y, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ShaderQuad, User0Z, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);

  ////////////////////////////////////////
  // ops map
  ////////////////////////////////////////

  auto opm                        = new ork::reflect::OpMap;
  opm->mLambdaMap["ReloadShader"] = [=](Object* pobj) {
    ShaderQuad* as_shader = rtti::autocast(pobj);
    if (as_shader) {
      if (as_shader->mShader)
        delete as_shader->mShader;
      as_shader->mShader = nullptr;
    }
  };

  reflect::annotateClassForEditor<ShaderQuad>("editor.object.ops", opm);

  ////////////////////////////////////////
}
ShaderQuad::ShaderQuad()
    : mShader(nullptr)
    , ConstructInpPlug(ImgInput0, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(User0X, dataflow::EPR_UNIFORM, mfUser0X)
    , ConstructInpPlug(User0Y, dataflow::EPR_UNIFORM, mfUser0Y)
    , ConstructInpPlug(User0Z, dataflow::EPR_UNIFORM, mfUser0Z)
    , mfUser0X(0.0f)
    , mfUser0Y(0.0f)
    , mfUser0Z(0.0f)
    , mpTexture(nullptr) {
}
ork::dataflow::inplugbase* ShaderQuad::GetInput(int idx) {
  ork::dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpUser0X;
      break;
    case 1:
      rval = &mPlugInpUser0Y;
      break;
    case 2:
      rval = &mPlugInpUser0Z;
      break;
    case 3:
      rval = &mPlugInpImgInput0;
      break;
  }
  return rval;
}
void ShaderQuad::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  // printf( "RENDERING ShaderQuad<%p>\n", this );

  if (nullptr == mShader) {
    mShader = new lev2::GfxMaterial3DSolid(pTARG, mShaderPath.c_str(), "shaderquad", true);
    printf("ShaderQuad<%p> loading shader<%s>\n", this, mShaderPath.c_str());
  }

  if (mShader->IsUserFxOk()) {
    // float ftime = proc_ctx->mCurrentTime;

    const ImgOutPlug* conplug = rtti::autocast(mPlugInpImgInput0.GetExternalOutput());

    Buffer& buffer = GetWriteBuffer(ptex);

    mShader->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
    mShader->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    mShader->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
    mShader->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
    mShader->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);

    if (conplug) {
      auto& plugbuf      = conplug->GetValue().GetBuffer(ptex);
      const auto& conrtg = plugbuf.GetRtGroup(pTARG);
      auto plug_name     = conplug->GetName();
      auto con_mod       = conplug->GetModule();
      auto mod_nam       = con_mod->GetName();

      printf(
          " ShaderQuad<%p> input con_mod<%p:%s> conplug<%p:%s> rtg<%p>\n",
          this,
          con_mod,
          mod_nam.c_str(),
          conplug,
          plug_name.c_str(),
          conrtg);

      mShader->SetTexture(conplug->GetValue().GetTexture(ptex));
    } else
      mShader->SetTexture(GetTexture());

    mShader->SetUser0(fvec4(mPlugInpUser0X.GetValue(), mPlugInpUser0Y.GetValue(), mPlugInpUser0Z.GetValue(), float(buffer.miW)));
    ////////////////////////////////////////////////////////////////
    // float ftexw = GetTexture() ? GetTexture()->width() : 1.0f;
    // pTARG->PushModColor( ork::fvec4( ftexw, ftexw, ftexw, ftexw ) );
    ////////////////////////////////////////////////////////////////
    buffer.PtexBegin(pTARG, true, false);
    pTARG->PushModColor(fvec3::White());
    { UnitTexQuad(mShader, pTARG); }
    pTARG->PopModColor();
    buffer.PtexEnd(true);
    ////////////////////////////////////////////////////////////////
    MarkClean();
  }
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SolidColor::Describe() {
  ork::reflect::RegisterProperty("Red", &SolidColor::mfr);
  ork::reflect::RegisterProperty("Green", &SolidColor::mfg);
  ork::reflect::RegisterProperty("Blue", &SolidColor::mfb);
  ork::reflect::RegisterProperty("Alpha", &SolidColor::mfa);

  ork::reflect::annotatePropertyForEditor<SolidColor>("Red", "editor.range.min", "-8.0f");
  ork::reflect::annotatePropertyForEditor<SolidColor>("Red", "editor.range.max", "8.0f");

  ork::reflect::annotatePropertyForEditor<SolidColor>("Green", "editor.range.min", "-8.0f");
  ork::reflect::annotatePropertyForEditor<SolidColor>("Green", "editor.range.max", "8.0f");

  ork::reflect::annotatePropertyForEditor<SolidColor>("Blue", "editor.range.min", "-8.0f");
  ork::reflect::annotatePropertyForEditor<SolidColor>("Blue", "editor.range.max", "8.0f");

  ork::reflect::annotatePropertyForEditor<SolidColor>("Alpha", "editor.range.min", "-8.0f");
  ork::reflect::annotatePropertyForEditor<SolidColor>("Alpha", "editor.range.max", "8.0f");

  static const char* EdGrpStr = "grp://RGBA Red Green Blue Alpha";

  reflect::annotateClassForEditor<SolidColor>("editor.prop.groups", EdGrpStr);
}
SolidColor::SolidColor()
    : mfr(1.0f)
    , mfg(1.0f)
    , mfb(1.0f)
    , mfa(1.0f)
    , mMaterial(nullptr) {
}
void SolidColor::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::SolidColor::compute"));

  Buffer& buffer = GetWriteBuffer(ptex);
  buffer.PtexBegin(pTARG, true, false);

  if (nullptr == mMaterial) {
    mMaterial = new lev2::GfxMaterial3DSolid(pTARG);
    mMaterial->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR);
    mMaterial->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    mMaterial->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
    mMaterial->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
    mMaterial->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
    mMaterial->SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(buffer.miW)));
  }

  ////////////////////////////////////////////////////////////////
  pTARG->PushModColor(ork::fvec4(mfr, mfg, mfb, mfa));
  ////////////////////////////////////////////////////////////////
  pTARG->MTXI()->PushPMatrix(fmtx4::Identity());
  pTARG->MTXI()->PushVMatrix(fmtx4::Identity());
  pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
  { RenderQuad(mMaterial, pTARG, -1, -1, 1, 1); }
  pTARG->MTXI()->PopPMatrix();
  pTARG->MTXI()->PopVMatrix();
  pTARG->MTXI()->PopMMatrix();
  pTARG->PopModColor();
  MarkClean();
  buffer.PtexEnd(true);
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Gradient::Describe() {
  ork::reflect::RegisterProperty("gradient", &Gradient::GradientAccessor);
  ork::reflect::annotatePropertyForEditor<Gradient>("gradient", "editor.class", "ged.factory.gradient");

  ork::reflect::RegisterProperty("Type", &Gradient::meGradientType);

  ork::reflect::RegisterProperty("Repeat", &Gradient::miRepeat);
  ork::reflect::RegisterProperty("RepeatMode", &Gradient::meRepeatMode);

  ork::reflect::annotatePropertyForEditor<Gradient>("Repeat", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<Gradient>("Repeat", "editor.range.max", "16");
  ork::reflect::annotatePropertyForEditor<Gradient>("RepeatMode", "editor.class", "ged.factory.enum");

  ork::reflect::annotatePropertyForEditor<Gradient>("Type", "editor.class", "ged.factory.enum");

  ork::reflect::RegisterProperty("AntiAlias", &Gradient::mbAA);

  static const char* EdGrpStr = "grp://Basic AntiAlias Type Repeat RepeatMode gradient";

  reflect::annotateClassForEditor<Gradient>("editor.prop.groups", EdGrpStr);
}
Gradient::Gradient()
    : mpTexture(0)
    , mVertexBuffer(1 << 20, 0, ork::lev2::PrimitiveType::TRIANGLES)
    , miRepeat(1)
    , meRepeatMode(EGS_REPEAT)
    , meGradientType(EGT_HORIZONTAL)
    , mbAA(false)
    , mMtl(nullptr) {
}
void Gradient::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::Gradient::compute"));

  Buffer& buffer = GetWriteBuffer(ptex);

  const orklut<float, ork::fvec4>& data = mGradient.Data();
  float frw                             = 1.0f / float(miRepeat);
  bool bpingpong                        = (meRepeatMode == EGS_PINGPONG);
  const int knumpoints                  = data.size();
  const int ksegs                       = knumpoints - 1;
  const float kz                        = 0.0f;

  fvec2 uv;
  mVertexBuffer.Reset();

  if (nullptr == mMtl) {
    mMtl = new lev2::GfxMaterial3DSolid(pTARG);

    mMtl->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
    mMtl->_rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    mMtl->_rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
    mMtl->_rasterstate.SetBlending(ork::lev2::Blending::OFF);
    mMtl->_rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
    mMtl->_rasterstate.SetShadeModel(ork::lev2::ESHADEMODEL_SMOOTH);
  }

  mMtl->SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(buffer.miW)));

  /////////////////////////////////////////
  // compute vertex count
  /////////////////////////////////////////

  int ivtxcount = 0;
  for (int ir = 0; ir < miRepeat; ir++) {
    float fleft = float(ir) * frw;
    float frght = fleft + frw;
    bool bppalt = bpingpong & (ir & 1);
    for (int i = 0; i < ksegs; i++) {
      std::pair<float, ork::fvec4> data_a = data.GetItemAtIndex(i);
      std::pair<float, ork::fvec4> data_b = data.GetItemAtIndex(i + 1);
      float fia                           = data_a.first;
      float fib                           = data_b.first;
      float fx0                           = bppalt ? frght - (fia * frw) : fleft + (fia * frw);
      float fx1                           = bppalt ? frght - (fib * frw) : fleft + (fib * frw);
      switch (meGradientType) {
        case EGT_VERTICAL:
        case EGT_HORIZONTAL:
          ivtxcount += 6;
          break;
        case EGT_CONICAL: {
          float fmx0        = bppalt ? fx1 : fx0;
          float fmx1        = bppalt ? fx0 : fx1;
          const int ksegdiv = int((fmx1 - fmx0) * 360.0f);
          ivtxcount += ksegdiv * 3;
          break;
        }
        case EGT_RADIAL:
          ivtxcount += 360 * 6;
          break;
        default:
          OrkAssert(false);
      }
    }
  }

  /////////////////////////////////////////

  lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(pTARG, &mVertexBuffer, ivtxcount);

  for (int ir = 0; ir < miRepeat; ir++) {
    float fleft = float(ir) * frw;
    float frght = fleft + frw;

    bool bppalt = bpingpong & (ir & 1);

    for (int i = 0; i < ksegs; i++) {

      std::pair<float, ork::fvec4> data_a = data.GetItemAtIndex(i);
      std::pair<float, ork::fvec4> data_b = data.GetItemAtIndex(i + 1);

      float fia = data_a.first;
      float fib = data_b.first;

      float fx0 = bppalt ? frght - (fia * frw) : fleft + (fia * frw);
      float fx1 = bppalt ? frght - (fib * frw) : fleft + (fib * frw);
      float fy0 = 0.0f;
      float fy1 = 1.0f;

      fvec4 c0 = data_a.second;
      fvec4 c1 = data_b.second;

      switch (meGradientType) {
        case EGT_VERTICAL: {
          lev2::SVtxV12C4T16 v0(fvec3(fy0, fx0, kz), uv, c0.GetVtxColorAsU32());
          lev2::SVtxV12C4T16 v1(fvec3(fy0, fx1, kz), uv, c1.GetVtxColorAsU32());
          lev2::SVtxV12C4T16 v2(fvec3(fy1, fx1, kz), uv, c1.GetVtxColorAsU32());
          lev2::SVtxV12C4T16 v3(fvec3(fy1, fx0, kz), uv, c0.GetVtxColorAsU32());
          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v2);
          vw.AddVertex(v0);
          vw.AddVertex(v2);
          vw.AddVertex(v3);
          break;
        }
        case EGT_HORIZONTAL: {
          lev2::SVtxV12C4T16 v0(fvec3(fx0, fy0, kz), uv, c0.GetVtxColorAsU32());
          lev2::SVtxV12C4T16 v1(fvec3(fx1, fy0, kz), uv, c1.GetVtxColorAsU32());
          lev2::SVtxV12C4T16 v2(fvec3(fx1, fy1, kz), uv, c1.GetVtxColorAsU32());
          lev2::SVtxV12C4T16 v3(fvec3(fx0, fy1, kz), uv, c0.GetVtxColorAsU32());
          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v2);
          vw.AddVertex(v0);
          vw.AddVertex(v2);
          vw.AddVertex(v3);
          break;
        }
        case EGT_CONICAL: {
          float fmx0 = bppalt ? fx1 : fx0;
          float fmx1 = bppalt ? fx0 : fx1;

          if (bppalt)
            std::swap(c0, c1);

          float fph0 = fmx0 * PI2;
          float fph1 = fmx1 * PI2;
          float fphd = fph1 - fph0;

          const int ksegdiv     = int((fmx1 - fmx0) * 360.0f);
          const float kfisegdiv = 1.0f / float(ksegdiv);

          for (int issg = 0; issg < ksegdiv; issg++) {
            float fi   = float(issg) * kfisegdiv;
            float fphA = fph0 + (fi * fphd);
            float fphB = fph0 + ((fi + kfisegdiv) * fphd);
            fvec4 cA;
            cA.Lerp(c0, c1, fi);
            fvec4 cB;
            cB.Lerp(c0, c1, fi + kfisegdiv);

            float fxa = pol2rect_x(fphA, 0.0f) + 0.5f;
            float fya = pol2rect_y(fphA, 0.0f) + 0.5f;
            float fxb = pol2rect_x(fphA, 1.5f) + 0.5f;
            float fyb = pol2rect_y(fphA, 1.5f) + 0.5f;
            float fxc = pol2rect_x(fphB, 1.5f) + 0.5f;
            float fyc = pol2rect_y(fphB, 1.5f) + 0.5f;

            lev2::SVtxV12C4T16 v0(fvec3(fxa, fya, kz), uv, cA.GetVtxColorAsU32());
            lev2::SVtxV12C4T16 v1(fvec3(fxb, fyb, kz), uv, cA.GetVtxColorAsU32());
            lev2::SVtxV12C4T16 v2(fvec3(fxc, fyc, kz), uv, cB.GetVtxColorAsU32());
            vw.AddVertex(v0);
            vw.AddVertex(v1);
            vw.AddVertex(v2);
          }
          break;
        }
        case EGT_RADIAL: {
          float fmx0 = bppalt ? fx1 : fx0;
          float fmx1 = bppalt ? fx0 : fx1;

          if (bppalt)
            std::swap(c0, c1);

          const int ksegdiv     = 360;
          const float kfisegdiv = 1.0f / float(ksegdiv);

          for (int issg = 0; issg < ksegdiv; issg++) {
            float fi   = float(issg) * kfisegdiv;
            float fphA = fi * PI2;
            float fphB = (fi + kfisegdiv) * PI2;
            fvec4 cA;
            cA.Lerp(c0, c1, fi);
            fvec4 cB;
            cB.Lerp(c0, c1, fi + kfisegdiv);

            float fxa = pol2rect_x(fphA, fmx0 * 0.5f) + 0.5f;
            float fya = pol2rect_y(fphA, fmx0 * 0.5f) + 0.5f;
            float fxb = pol2rect_x(fphA, fmx1 * 0.5f) + 0.5f;
            float fyb = pol2rect_y(fphA, fmx1 * 0.5f) + 0.5f;
            float fxc = pol2rect_x(fphB, fmx1 * 0.5f) + 0.5f;
            float fyc = pol2rect_y(fphB, fmx1 * 0.5f) + 0.5f;
            float fxd = pol2rect_x(fphB, fmx0 * 0.5f) + 0.5f;
            float fyd = pol2rect_y(fphB, fmx0 * 0.5f) + 0.5f;

            lev2::SVtxV12C4T16 v0(fvec3(fxa, fya, kz), uv, c0.GetVtxColorAsU32());
            lev2::SVtxV12C4T16 v1(fvec3(fxb, fyb, kz), uv, c1.GetVtxColorAsU32());
            lev2::SVtxV12C4T16 v2(fvec3(fxc, fyc, kz), uv, c1.GetVtxColorAsU32());
            lev2::SVtxV12C4T16 v3(fvec3(fxd, fyd, kz), uv, c0.GetVtxColorAsU32());
            vw.AddVertex(v0);
            vw.AddVertex(v1);
            vw.AddVertex(v2);
            vw.AddVertex(v0);
            vw.AddVertex(v2);
            vw.AddVertex(v3);
          }
          break;
        }
      }
    }
  }
  vw.UnLock(pTARG);
  struct AA16RenderGrad : public AA16Render {
    void DoRender(float left, float right, float top, float bot, Buffer& buf) override {
      fmtx4 mtxortho = mPTX.GetTarget()->MTXI()->Ortho(left, right, top, bot, 0.0f, 1.0f);
      mPTX.GetTarget()->PushModColor(fvec3::White());
      mPTX.GetTarget()->MTXI()->PushPMatrix(mtxortho);
      mPTX.GetTarget()->GBI()->DrawPrimitive(
          mMtl, mVertexBuffer, ork::lev2::PrimitiveType::TRIANGLES, 0, mVertexBuffer.GetNumVertices());
      mPTX.GetTarget()->MTXI()->PopPMatrix();
      mPTX.GetTarget()->PopModColor();
    }
    lev2::GfxMaterial3DSolid* mMtl;
    ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& mVertexBuffer;
    AA16RenderGrad(
        ProcTex& ptex,
        Buffer& bo,
        ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vb,
        lev2::GfxMaterial3DSolid* mtl)
        : AA16Render(ptex, bo)
        , mMtl(mtl)
        , mVertexBuffer(vb) {
      mOrthoBoxXYWH = fvec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
  };

  AA16RenderGrad renderer(ptex, buffer, mVertexBuffer, mMtl);
  renderer.Render(mbAA);

  pTARG->debugPopGroup();

  ////////////////////////////////////////////////////////////////
  // F32 fVPW = (F32) pTARG->GetVPW();
  // F32 fVPH = (F32) pTARG->GetVPH();
  // if( 0.0f == fVPW ) fVPW = 1.0f;
  // if( 0.0f == fVPH ) fVPH = 1.0f;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Group::Describe() {
  ork::reflect::RegisterProperty("ProcTex", &Group::GetTextureAccessor, &Group::SetTextureAccessor);
  ork::reflect::annotatePropertyForEditor<Group>("ProcTex", "editor.visible", "false");
  ork::reflect::annotateClassForEditor<Group>("editor.object.ops", ConstString("load:proctexgroupload save:proctexgroupsave"));
}
Group::Group()
    : mpProcTex(0) {
}
void Group::compute(ProcTex& ptex) {
  auto proc_ctx = ptex.GetPTC();
  auto pTARG    = ptex.GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::Group::compute"));

  Buffer& computebuffer = GetWriteBuffer(ptex);
  if (mpProcTex) {
    mpProcTex->compute(*ptex.GetPTC());
    lev2::Texture* ptexture = mpProcTex->ResultTexture();
    //////////////////////////
    // put into our buffer
    //////////////////////////
    // lev2::Texture* ptexture = ptex.GetResultTexture(*ptex.GetPTC());
    computebuffer.PtexBegin(pTARG, true, false);

    lev2::GfxMaterial3DSolid gridmat(pTARG, "orkshader://proctex", "ttex");
    gridmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
    gridmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    gridmat._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
    gridmat._rasterstate.SetBlending(ork::lev2::Blending::OFF);
    gridmat._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
    gridmat.SetTexture(ptexture);
    gridmat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(computebuffer.miW)));
    ////////////////////////////////////////////////////////////////
    float ftexw = ptexture ? ptexture->_width : 1.0f;
    pTARG->PushModColor(ork::fvec4(ftexw, ftexw, ftexw, ftexw));
    ////////////////////////////////////////////////////////////////
    fmtx4 mtxortho = pTARG->MTXI()->Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);
    pTARG->MTXI()->PushPMatrix(mtxortho);
    pTARG->MTXI()->PushVMatrix(fmtx4::Identity());
    pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
    { RenderQuad(&gridmat, pTARG, -1, -1, 1, 1); }
    pTARG->MTXI()->PopPMatrix();
    pTARG->MTXI()->PopVMatrix();
    pTARG->MTXI()->PopMMatrix();
    pTARG->PopModColor();
    MarkClean();
    computebuffer.PtexEnd(true);
    //////////////////////////
  }
  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::proctex
