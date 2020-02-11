///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/AccessorObjectPropertyObject.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/file/file.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/asset/AssetManager.h>

BEGIN_ENUM_SERIALIZER(ork::proctex, EPTEX_TYPE)
DECLARE_ENUM(EPTEXTYPE_REALTIME)
DECLARE_ENUM(EPTEXTYPE_EXPORT)
END_ENUM_SERIALIZER()

INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ProcTex, "proctex");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ImgModule, "proctex::ImgModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Img32Module, "proctex::Img32Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Img64Module, "proctex::Img64Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Module, "proctex::Module");

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::outplug<ork::proctex::ImgBase>, "proctex::OutImgPlug");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::inplug<ork::proctex::ImgBase>, "proctex::InImgPlug");

using namespace ork::lev2;

namespace ork { namespace dataflow {
template <> void outplug<ork::proctex::ImgBase>::Describe() {
}
template <> void inplug<ork::proctex::ImgBase>::Describe() {
}
template <> int MaxFanout<ork::proctex::ImgBase>() {
  return 0;
}
template <> const ork::proctex::ImgBase& outplug<ork::proctex::ImgBase>::GetInternalData() const {
  OrkAssert(mOutputData != 0);
  return *mOutputData;
}
template <> const ork::proctex::ImgBase& outplug<ork::proctex::ImgBase>::GetValue() const {
  return GetInternalData();
}
}} // namespace ork::dataflow

namespace ork {

file::Path SaveFileRequester(const std::string& title, const std::string& ext);

namespace proctex {

Img32 ImgModule::gNoCon;
ork::MpMcBoundedQueue<Buffer*> ProcTexContext::gBuf32Q;
ork::MpMcBoundedQueue<Buffer*> ProcTexContext::gBuf64Q;

///////////////////////////////////////////////////////////////////////////////
Buffer::Buffer(ork::lev2::EBufferFormat efmt)
    : mRtGroup(nullptr)
    , miW(256)
    , miH(256) {
}
void Buffer::SetBufferSize(int w, int h) {
  if (w != miW && h != miH) {

    miW = w;
    miH = h;
    delete mRtGroup;
    mRtGroup = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////
void Buffer::PtexBegin(lev2::Context* ptgt, bool push_full_vp, bool clear_all) {
  mTarget = ptgt;
  mTarget->FBI()->SetAutoClear(false);
  // mTarget->FBI()->BeginFrame();
  auto rtg = GetRtGroup(ptgt);

  mTarget->FBI()->PushRtGroup(rtg);
  SRect vprect_full(0, 0, miW, miH);

  // printf( "  buffer<%p> w<%d> h<%d> rtg<%p> begin\n", this, miW,miH,rtg);

  if (push_full_vp) {
    mTarget->FBI()->PushViewport(vprect_full);
    mTarget->FBI()->PushScissor(vprect_full);
  }
  if (clear_all)
    mTarget->FBI()->Clear(CColor3::Black(), 1.0f);
}
///////////////////////////////////////////////////////////////////////////////
void Buffer::PtexEnd(bool pop_vp) {
  if (pop_vp) {
    mTarget->FBI()->PopViewport();
    mTarget->FBI()->PopScissor();
  }

  // printf( "  buffer<%p> end\n", this);

  mTarget->FBI()->PopRtGroup();
  // mTarget->FBI()->EndFrame();
  mTarget = nullptr;
}
lev2::RtGroup* Buffer::GetRtGroup(lev2::Context* ptgt) {
  if (mRtGroup == nullptr) {
    mRtGroup = new RtGroup(ptgt, miW, miH);

    auto mrt = new ork::lev2::RtBuffer(lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA8, miW, miH);

    mrt->_debugName = FormatString("ptx::Reg32");
    mrt->_mipgen    = RtBuffer::EMG_AUTOCOMPUTE;
    mRtGroup->SetMrt(0, mrt);

    ptgt->FBI()->PushRtGroup(mRtGroup);
    ptgt->FBI()->PopRtGroup();
  }
  return mRtGroup;
}

///////////////////////////////////////////////////////////////////////////////
lev2::Texture* Buffer::OutputTexture() {
  return (mRtGroup != nullptr) ? mRtGroup->GetMrt(0)->texture() : nullptr;
}

Buffer32::Buffer32()
    : Buffer(lev2::EBUFFMT_RGBA8) {
} // EBUFFMT_RGBA8
Buffer64::Buffer64()
    : Buffer(lev2::EBUFFMT_RGBA16F) {
} // EBUFFMT_RGBA16F

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ork::lev2::Texture* Img32::GetTexture(ProcTex& ptex) const {
  Buffer& buf = GetBuffer(ptex);
  buf.GetRtGroup(ptex.GetTarget());
  return buf.OutputTexture();
}
Buffer& Img32::GetBuffer(ProcTex& ptex) const {
  static Buffer32 gnone;
  return (miBufferIndex >= 0) ? ptex.GetBuffer32(miBufferIndex) : gnone;
}
ork::lev2::Texture* Img64::GetTexture(ProcTex& ptex) const {
  Buffer& buf = GetBuffer(ptex);
  buf.GetRtGroup(ptex.GetTarget());
  return buf.OutputTexture();
}
Buffer& Img64::GetBuffer(ProcTex& ptex) const {
  static Buffer64 gnone;
  return (miBufferIndex >= 0) ? ptex.GetBuffer64(miBufferIndex) : gnone;
}

///////////////////////////////////////////////////////////////////////////////
static lev2::Texture* GetImgModuleIcon(ork::dataflow::dgmodule* pmod) {
  ImgModule* pimgmod = rtti::autocast(pmod);
  auto& buffer       = pimgmod->GetThumbBuffer();
  return buffer.OutputTexture();
}

void ImgModule::Describe() {
  reflect::annotateClassForEditor<ImgModule>("dflowicon", &GetImgModuleIcon);

  auto opm = new ork::reflect::OpMap;

  opm->mLambdaMap["ExportPng"] = [=](Object* pobj) {
    Img32Module* as_module = rtti::autocast(pobj);

    printf("ExportPNG pobj<%p> as_mod<%p>\n", pobj, as_module);
    if (as_module) {
      as_module->mExport = true;
    }
  };

  reflect::annotateClassForEditor<Img32Module>("editor.object.ops", opm);
}
void Img32Module::Describe() {
  RegisterObjOutPlug(Img32Module, ImgOut);
}
void Img64Module::Describe() {
  RegisterObjOutPlug(Img64Module, ImgOut);
}
ImgModule::ImgModule()
    : mExport(false) {
}
Img32Module::Img32Module()
    : ConstructOutTypPlug(ImgOut, dataflow::EPR_UNIFORM, typeid(Img32))
    , ImgModule() {
}
Img64Module::Img64Module()
    : ConstructOutTypPlug(ImgOut, dataflow::EPR_UNIFORM, typeid(Img64))
    , ImgModule() {
}
Buffer& ImgModule::GetWriteBuffer(ProcTex& ptex) {
  ImgOutPlug* outplug = 0;
  GetTypedOutput<ImgBase>(0, outplug);
  const ImgBase& base = outplug->GetValue();
  // printf( "MOD<%p> WBI<%d>\n", this, base.miBufferIndex );
  return base.GetBuffer(ptex);
  // return ptex.GetBuffer(outplug->GetValue().miBufferIndex);
}
void ImgModule::Compute(dataflow::workunit* wu) {
  auto ptex                = wu->GetContextData().Get<ProcTex*>();
  ProcTexContext* ptex_ctx = ptex->GetPTC();

  auto pTARG = ptex->GetTarget();
  auto fbi   = pTARG->FBI();

  const RenderContextFrameData* RCFD = pTARG->topRenderContextFrameData();
  auto CIMPL                         = RCFD->_cimpl;
  CompositingPassData CPD;
  CIMPL->pushCPD(CPD);

  const_cast<ImgModule*>(this)->compute(*ptex);

  auto& wrbuf   = GetWriteBuffer(*ptex);
  auto ptexture = wrbuf.OutputTexture();
  pTARG->TXI()->generateMipMaps(ptexture);
  ptexture->TexSamplingMode().PresetPointAndClamp();
  pTARG->TXI()->ApplySamplingMode(ptexture);

  if (mExport) {

    if (nullptr == ptexture)
      return;

    auto rtg = wrbuf.GetRtGroup(pTARG);

    auto fname = SaveFileRequester("Export ProcTexImage", "DDS (*.dds)");

    if (fname.length()) {

      // SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
      if (ork::FileEnv::filespec_to_extension(fname.c_str()).length() == 0)
        fname += ".dds";
      fbi->Capture(*rtg, 0, fname);
    }

    mExport = false;
  }
  CIMPL->popCPD();
}
///////////////////////////////////////////////////////////////////////////////
void ImgModule::UnitTexQuad(
    ork::lev2::Context* pTARG) { // fmtx4 mtxortho = pTARG->MTXI()->Ortho( -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f );
  pTARG->MTXI()->PushPMatrix(fmtx4::Identity);
  pTARG->MTXI()->PushVMatrix(fmtx4::Identity);
  pTARG->MTXI()->PushMMatrix(fmtx4::Identity);
  pTARG->PushModColor(fvec3::White());
  { RenderQuad(pTARG, -1.0f, -1.0f, 1.0f, 1.0f); }
  pTARG->PopModColor();
  pTARG->MTXI()->PopPMatrix();
  pTARG->MTXI()->PopVMatrix();
  pTARG->MTXI()->PopMMatrix();
}
void ImgModule::MarkClean() {
  for (int i = 0; i < GetNumOutputs(); i++) {
    GetOutput(i)->SetDirty(false);
  }
}
///////////////////////////////////////////////////////////////////////////////
void RenderQuad(ork::lev2::Context* pTARG, float fX1, float fY1, float fX2, float fY2, float fu1, float fv1, float fu2, float fv2) {
  U32 uColor = 0xffffffff; // gGfxEnv.GetColor().GetABGRU32();

  float maxuv = 1.0f;
  float minuv = 0.0f;

  int ivcount = 6;

  lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(pTARG, &GfxEnv::GetSharedDynamicVB(), ivcount);
  float fZ = 0.0f;

  vw.AddVertex(SVtxV12C4T16(fX1, fY1, fZ, fu1, fv1, uColor));
  vw.AddVertex(SVtxV12C4T16(fX2, fY1, fZ, fu2, fv1, uColor));
  vw.AddVertex(SVtxV12C4T16(fX2, fY2, fZ, fu2, fv2, uColor));

  vw.AddVertex(SVtxV12C4T16(fX1, fY1, fZ, fu1, fv1, uColor));
  vw.AddVertex(SVtxV12C4T16(fX2, fY2, fZ, fu2, fv2, uColor));
  vw.AddVertex(SVtxV12C4T16(fX1, fY2, fZ, fu1, fv2, uColor));

  vw.UnLock(pTARG);

  pTARG->GBI()->DrawPrimitive(vw, EPRIM_TRIANGLES, ivcount);
}

///////////////////////////////////////////////////////////////////////////////
void ImgModule::UpdateThumb(ProcTex& ptex) {
  auto pTARG    = ptex.GetTarget();
  auto fbi      = pTARG->FBI();
  auto& wrbuf   = GetWriteBuffer(ptex);
  auto ptexture = wrbuf.OutputTexture();

  if (nullptr == ptexture)
    return;

  // mThumbBuffer.BeginFrame();

  fbi->PushRtGroup(mThumbBuffer.GetRtGroup(pTARG));
  GfxMaterial3DSolid gridmat(pTARG, "orkshader://proctex", "ttex");
  gridmat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
  gridmat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
  gridmat._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
  gridmat._rasterstate.SetBlending(ork::lev2::EBLENDING_OFF);
  gridmat._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
  gridmat.SetTexture(ptexture);
  gridmat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(wrbuf.miW)));
  pTARG->BindMaterial(&gridmat);
  ////////////////////////////////////////////////////////////////
  float ftexw = ptexture ? ptexture->_width : 1.0f;
  pTARG->PushModColor(ork::fvec4(ftexw, ftexw, ftexw, ftexw));
  ////////////////////////////////////////////////////////////////
  fmtx4 mtxortho = pTARG->MTXI()->Ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);
  // pTARG->MTXI()->PushPMatrix( mtxortho );
  pTARG->MTXI()->PushPMatrix(fmtx4::Identity);
  pTARG->MTXI()->PushVMatrix(fmtx4::Identity);
  pTARG->MTXI()->PushMMatrix(fmtx4::Identity);
  { RenderQuad(pTARG, -1.0f, -1.0f, 1.0f, 1.0f); }
  pTARG->MTXI()->PopPMatrix();
  pTARG->MTXI()->PopVMatrix();
  pTARG->MTXI()->PopMMatrix();
  pTARG->PopModColor();
  MarkClean();
  fbi->PopRtGroup();
}
///////////////////////////////////////////////////////////////////////////////
void ProcTex::Describe() { // ork::reflect::RegisterProperty( "Global", & ProcTex::GlobalAccessor );
                           // ork::reflect::annotatePropertyForEditor< ProcTex >("Global", "editor.visible", "false" );
                           // ork::reflect::annotatePropertyForEditor< ProcTex >("Modules", "editor.factorylistbase",
                           // "proctex::Module" );
}

///////////////////////////////////////////////////////////////////////////////
ProcTex::ProcTex()
    : mpctx(0)
    , mbTexQuality(false)
    , mpResTex(0) {
  Global::GetClassStatic();
  RotSolid::GetClassStatic();
  Colorize::GetClassStatic();
  ImgOp2::GetClassStatic();
  ImgOp3::GetClassStatic();
  Transform::GetClassStatic();
  Octaves::GetClassStatic();
  Texture::GetClassStatic();
}
///////////////////////////////////////////////////////////////////////////////
bool ProcTex::CanConnect(const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout) const // virtual
{
  bool brval = false;
  brval |= (&pin->GetDataTypeId() == &typeid(ImgBase)) && (&pout->GetDataTypeId() == &typeid(Img64));
  brval |= (&pin->GetDataTypeId() == &typeid(ImgBase)) && (&pout->GetDataTypeId() == &typeid(Img32));
  brval |= (&pin->GetDataTypeId() == &typeid(float)) && (&pout->GetDataTypeId() == &typeid(float));
  return brval;
}
///////////////////////////////////////////////////////////////////////////////
// compute result to mBuffer
///////////////////////////////////////////////////////////////////////////////
void ProcTex::compute(ProcTexContext& ptctx) {
  if (false == IsComplete())
    return;
  mpctx = &ptctx;
  // printf( "ProcTex<%p>::compute ProcTexContext<%p>\n", this, mpctx );
  //////////////////////////////////
  // build the execution graph
  //////////////////////////////////
  Clear();
  RefreshTopology(ptctx.mdflowctx);

  //////////////////////////////////

  mpResTex = nullptr;

  auto pTARG = GetTarget();
  pTARG->debugPushGroup(FormatString("ptx::compute"));

  //////////////////////////////////
  // execute df graph
  //////////////////////////////////

#if 1
  ImgModule* res_module                              = nullptr;
  const orklut<int, dataflow::dgmodule*>& TopoSorted = LockTopoSortedChildrenForRead(1);
  {
    for (orklut<int, dataflow::dgmodule*>::const_iterator it = TopoSorted.begin(); it != TopoSorted.end(); it++) {
      dataflow::dgmodule* dgmod = it->second;
      Group* pgroup             = rtti::autocast(dgmod);
      ImgModule* img_module     = ork::rtti::autocast(dgmod);
      ///////////////////////////////////
      ImgModule* img_module_updthumb = nullptr;
      ///////////////////////////////////
      if (img_module) {
        ImgOutPlug* outplug = 0;
        img_module->GetTypedOutput<ImgBase>(0, outplug);
        const ImgBase& base = outplug->GetValue();

        if (outplug->GetRegister()) {
          int ireg = outplug->GetRegister()->mIndex;
          // OrkAssert( ireg>=0 && ireg<ibufmax );
          outplug->GetValue().miBufferIndex = ireg;
          // printf( "pmod<%p> reg<%d>\n", img_module, ireg );
          auto tex = base.GetTexture(*this);
          if (tex != nullptr) {
            mpResTex            = tex;
            res_module          = img_module;
            img_module_updthumb = img_module;
          }
        } else {
          outplug->GetValue().miBufferIndex = -2;
        }

        /*if( mpProbeImage == pmodule )
        {
            outplug->GetValue().miBufferIndex = ibufmax-1;
        }*/
      }
      ///////////////////////////////////
      if (pgroup) {
      } else {
        bool bmoddirty = true; // pmod->IsDirty();
        if (bmoddirty) {
          dataflow::cluster mycluster;
          dataflow::workunit mywunit(dgmod, &mycluster, 0);
          mywunit.SetContextData(this);
          dgmod->Compute(&mywunit);
        }
      }
      ///////////////////////////////////
      if (img_module_updthumb) {
        // img_module_updthumb->UpdateThumb(*this);
      }
    }
  }
  UnLockTopoSortedChildren();
#endif

  //////////////////////////////////

  if (ptctx.mWriteFrames && res_module) {
    auto& wrbuf   = res_module->GetWriteBuffer(*this);
    auto ptexture = wrbuf.OutputTexture();

    if (nullptr == ptexture)
      return;

    auto targ = ptctx.mTarget;
    auto fbi  = targ->FBI();

    auto rtg = wrbuf.GetRtGroup(targ);

    if (ptctx.mWritePath.length()) {
      file::DecomposedPath dpath;
      ptctx.mWritePath.DeCompose(dpath);
      fxstring<64> fidstr;
      fidstr.format("_%04d", ptctx.mWriteFrameIndex);
      dpath.mFile += fidstr.c_str();
      ork::file::Path indexed_path;
      indexed_path.Compose(dpath);
      printf("indexed_path<%s>\n", indexed_path.c_str());

      fbi->Capture(*rtg, 0, indexed_path);
    }
  }
  pTARG->debugPopGroup();

  //////////////////////////////////
  // mpctx = 0;
}
///////////////////////////////////////////////////////////////////////////////
void Module::Describe() {
}
Module::Module() {
}
///////////////////////////////////////////////////////////////////////////////
ork::lev2::Texture* ProcTex::ResultTexture() {
  return mpResTex;
}
Buffer& ProcTexContext::GetBuffer32(int edest) {
  if (edest < 0)
    return mTrashBuffer;
  OrkAssert(edest < k32buffers);
  return *mBuffer32[edest];
}
Buffer& ProcTexContext::GetBuffer64(int edest) {
  if (edest < 0)
    return mTrashBuffer;
  OrkAssert(edest < k64buffers);
  return *mBuffer64[edest];
}
ProcTexContext::ProcTexContext()
    : mdflowctx()
    , mFloatRegs("ptex_float", 4)
    , mImage32Regs("ptex_img32", k32buffers)
    , mImage64Regs("ptex_img64", k64buffers)
    , mTrashBuffer()
    , mCurrentTime(0.0f)
    , mTarget(nullptr)
    , mBufferDim(0)
    , mProcTexType(EPTEXTYPE_REALTIME)
    , mWriteFrames(false)
    , mWriteFrameIndex(0)
    , mWritePath("ptex_out.png") {
  mdflowctx.SetRegisters<float>(&mFloatRegs);
  mdflowctx.SetRegisters<Img32>(&mImage32Regs);
  mdflowctx.SetRegisters<Img64>(&mImage64Regs);

  for (int i = 0; i < k64buffers; i++)
    mBuffer64[i] = AllocBuffer64();

  for (int i = 0; i < k32buffers; i++)
    mBuffer32[i] = AllocBuffer64();
}
ProcTexContext::~ProcTexContext() {
  for (int i = 0; i < k64buffers; i++)
    ReturnBuffer(mBuffer64[i]);

  for (int i = 0; i < k32buffers; i++)
    ReturnBuffer(mBuffer32[i]);
}
void ProcTexContext::SetBufferDim(int idim) {
  mBufferDim = idim;
  for (int i = 0; i < k64buffers; i++)
    mBuffer64[i]->SetBufferSize(idim, idim);

  for (int i = 0; i < k32buffers; i++)
    mBuffer32[i]->SetBufferSize(idim, idim);
}
///////////////////////////////////////////////////////////////////////////////
ProcTex* ProcTex::Load(const ork::file::Path& pth) {
  ProcTex* rval        = 0;
  ork::file::Path path = pth;
  path.SetExtension("ptx");
  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
  stream::FileInputStream istream(path.c_str());
  reflect::serialize::XMLDeserializer iser(istream);
  rtti::ICastable* pcastable = 0;
  bool bOK                   = iser.Deserialize(pcastable);
  if (bOK) {
    ork::asset::AssetManager<ork::lev2::TextureAsset>::AutoLoad();
    rval = rtti::safe_downcast<ProcTex*>(pcastable);
  }
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  return rval;
}

Buffer* ProcTexContext::AllocBuffer32() {
  Buffer* rval = nullptr;
  if (false == gBuf32Q.try_pop(rval)) {
    rval = new Buffer32;
  }
  return rval;
}
Buffer* ProcTexContext::AllocBuffer64() {
  Buffer* rval = nullptr;
  if (false == gBuf64Q.try_pop(rval)) {
    rval = new Buffer64;
  }
  return rval;
}
void ProcTexContext::ReturnBuffer(Buffer* pbuf) {
  assert(pbuf != nullptr);
  if (pbuf->IsBuf32())
    gBuf32Q.push(pbuf);
  else
    gBuf64Q.push(pbuf);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AA16Render::AA16Render(ProcTex& ptx, Buffer& bo)
    : mPTX(ptx)
    , bufout(bo)
    , downsamplemat(ork::lev2::GfxEnv::GetRef().loadingContext(), "orkshader://proctex", "downsample16") {
  downsamplemat.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
  downsamplemat._rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
  downsamplemat._rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
  downsamplemat._rasterstate.SetBlending(ork::lev2::EBLENDING_ADDITIVE);
  downsamplemat._rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_ALWAYS);
  downsamplemat.SetUser0(fvec4(0.0f, 0.0f, 0.0f, float(bo.miW)));
}

///////////////////////////////////////////////////////////////////////////////

struct quad {
  float fx0;
  float fy0;
  float fx1;
  float fy1;
};

void AA16Render::RenderAA() {
  auto target = mPTX.GetTarget();
  auto fbi    = target->FBI();
  auto mtxi   = target->MTXI();
  auto txi    = target->TXI();

  fmtx4 mtxortho;

  float boxx = mOrthoBoxXYWH.GetX();
  float boxy = mOrthoBoxXYWH.GetY();
  float boxw = mOrthoBoxXYWH.GetZ();
  float boxh = mOrthoBoxXYWH.GetW();

  float xa = boxx + (boxw * 0.0f);
  float xb = boxx + (boxw * 0.25f);
  float xc = boxx + (boxw * 0.5f);
  float xd = boxx + (boxw * 0.75f);
  float xe = boxx + (boxw * 1.0f);

  float ya = boxy + (boxh * 0.0f);
  float yb = boxy + (boxh * 0.25f);
  float yc = boxy + (boxh * 0.5f);
  float yd = boxy + (boxh * 0.75f);
  float ye = boxy + (boxh * 1.0f);

  quad quads[16] = {
      {xa, ya, xb, yb},
      {xb, ya, xc, yb},
      {xc, ya, xd, yb},
      {xd, ya, xe, yb},
      {xa, yb, xb, yc},
      {xb, yb, xc, yc},
      {xc, yb, xd, yc},
      {xd, yb, xe, yc},
      {xa, yc, xb, yd},
      {xb, yc, xc, yd},
      {xc, yc, xd, yd},
      {xd, yc, xe, yd},
      {xa, yd, xb, ye},
      {xb, yd, xc, ye},
      {xc, yd, xd, ye},
      {xd, yd, xe, ye},
  };

  float ua = 0.0f;
  float ub = 0.25f;
  float uc = 0.5f;
  float ud = 0.75f;
  float ue = 1.0f;

  float va = 0.0f;
  float vb = 0.25f;
  float vc = 0.5f;
  float vd = 0.75f;
  float ve = 1.0f;

  quad quadsUV[16] = {
      {ua, va, ub, vb},
      {ub, va, uc, vb},
      {uc, va, ud, vb},
      {ud, va, ue, vb},
      {ua, vb, ub, vc},
      {ub, vb, uc, vc},
      {uc, vb, ud, vc},
      {ud, vb, ue, vc},
      {ua, vc, ub, vd},
      {ub, vc, uc, vd},
      {uc, vc, ud, vd},
      {ud, vc, ue, vd},
      {ua, vd, ub, ve},
      {ub, vd, uc, ve},
      {uc, vd, ud, ve},
      {ud, vd, ue, ve},
  };

  auto temp_buffer = bufout.IsBuf32() ? ProcTexContext::AllocBuffer32() : ProcTexContext::AllocBuffer64();

  for (int i = 0; i < 16; i++) {
    const quad& q  = quads[i];
    const quad& uq = quadsUV[i];
    float left     = q.fx0;
    float right    = q.fx1;
    float top      = q.fy0;
    float bottom   = q.fy1;

    //////////////////////////////////////////////////////
    // Render subsection to BufTA
    //////////////////////////////////////////////////////

    {
      temp_buffer->PtexBegin(target, true, true);
      fmtx4 mtxortho = mtxi->Ortho(left, right, top, bottom, 0.0f, 1.0f);
      mtxi->PushMMatrix(fmtx4::Identity);
      mtxi->PushVMatrix(fmtx4::Identity);
      mtxi->PushPMatrix(mtxortho);
      DoRender(left, right, top, bottom, *temp_buffer);
      mtxi->PopPMatrix();
      mtxi->PopVMatrix();
      mtxi->PopMMatrix();
      temp_buffer->PtexEnd(true);
    }

    //////////////////////////////////////////////////////
    // Resolve to output buffer
    //////////////////////////////////////////////////////

    bufout.PtexBegin(target, true, (i == 0));
    {
      float l = boxx;
      float r = boxx + boxw;
      float t = boxy;
      float b = boxy + boxh;

      auto tex = temp_buffer->OutputTexture();
      downsamplemat.SetTexture(tex);
      tex->TexSamplingMode().PresetPointAndClamp();
      txi->ApplySamplingMode(tex);

      fmtx4 mtxortho = mtxi->Ortho(l, r, t, b, 0.0f, 1.0f);
      mtxi->PushMMatrix(fmtx4::Identity);
      mtxi->PushVMatrix(fmtx4::Identity);
      mtxi->PushPMatrix(mtxortho);
      target->PushMaterial(&downsamplemat);
      RenderQuad(target, q.fx0, q.fy1, q.fx1, q.fy0, 0.0f, 0.0f, 1.0f, 1.0f);
      target->PopMaterial();
      mtxi->PopPMatrix();
      mtxi->PopVMatrix();
      mtxi->PopMMatrix();
    }
    bufout.PtexEnd(true);

    //////////////////////////////////////////////////////
  }
  ProcTexContext::ReturnBuffer(temp_buffer);
  fbi->SetAutoClear(true);
}

///////////////////////////////////////////////////////////////////////////////

void AA16Render::RenderNoAA() {
  auto target = mPTX.GetTarget();
  auto mtxi   = target->MTXI();
  auto fbi    = target->FBI();

  float x = mOrthoBoxXYWH.GetX();
  float y = mOrthoBoxXYWH.GetY();
  float w = mOrthoBoxXYWH.GetZ();
  float h = mOrthoBoxXYWH.GetW();
  float l = x;
  float r = x + w;
  float t = y;
  float b = y + h;

  bufout.PtexBegin(target, true, true);
  {
    fmtx4 mtxortho = mtxi->Ortho(l, r, t, b, 0.0f, 1.0f);
    mtxi->PushMMatrix(fmtx4::Identity);
    mtxi->PushVMatrix(fmtx4::Identity);
    mtxi->PushPMatrix(mtxortho);
    DoRender(l, r, t, b, bufout);
    mtxi->PopPMatrix();
    mtxi->PopVMatrix();
    mtxi->PopMMatrix();
  }
  bufout.PtexEnd(true);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace proctex
} // namespace ork
