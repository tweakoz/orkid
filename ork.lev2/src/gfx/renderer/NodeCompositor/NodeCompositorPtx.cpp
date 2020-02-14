////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPtx.h>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::PtxCompositingNode, "PtxCompositingNode");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
struct PtxImpl {
  PtxImpl(PtxCompositingNode* node)
      : _node(node) {
  }
  ~PtxImpl() {
    if (_output)
      delete _output;
  }
  void gpuInit(lev2::Context* pTARG, int iW, int iH) {
    if (_needsinit) {
      _needsinit = false;
      _blit2screenmtl.SetUserFx("orkshader://solid", "texcolor");
      _blit2screenmtl.Init(pTARG);

      _output                   = new lev2::RtGroup(pTARG, iW, iH);
      _outputbuffer             = new lev2::RtBuffer(lev2::ERTGSLOT0, lev2::EBUFFMT_RGBA16F, iW, iH);
      _outputbuffer->_debugName = FormatString("PtxCompositingNode::output");
      _output->SetMrt(0, _outputbuffer);
    }
  }
  void _recompute(CompositorDrawData& drawdata) {
    lev2::FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
    lev2::RenderContextFrameData& framedata = the_renderer.framedata();
    auto target                             = framedata.GetTarget();
    auto& templ                             = _node->_template;
    _ptexContext.SetBufferDim(_node->_bufferDim);
    _ptexContext.mTarget = target;
    _ptexContext.mdflowctx.Clear();
    _ptexContext.mCurrentTime = _time;
    _ptexContext.mWriteFrames = false;
    _ptexContext.mWritePath   = ""; // cd.GetWritePath();
    templ.compute(_ptexContext);
    _resultTexture = templ.ResultTexture();

    _time += 0.01f; // todo hook up to realtime
  }

  void _render(CompositorDrawData& drawdata) {
    lev2::FrameRenderer& the_renderer  = drawdata.mFrameRenderer;
    lev2::RenderContextFrameData& RCFD = the_renderer.framedata();
    auto target                        = RCFD.GetTarget();
    auto fbi                           = target->FBI();
    auto gbi                           = target->GBI();

    auto CIMPL   = drawdata._cimpl;
    auto CPD     = CIMPL->topCPD();
    SRect vprect = target->mainSurfaceRectAtOrigin();
    CPD.SetDstRect(vprect);
    CPD._cameraMatrices       = nullptr;
    CPD._stereoCameraMatrices = nullptr;
    CPD._stereo1pass          = false;

    lev2::Texture* input_tex = nullptr;

    if (auto try_render = drawdata._properties["render_out"_crcu].TryAs<RtBuffer*>()) {
      auto buffer = try_render.value();
      if (buffer) {
        assert(buffer != nullptr);
        input_tex = buffer->texture();
      }
    }

    if (nullptr == input_tex)
      return;

    if (_output) {

      CIMPL->pushCPD(CPD);

      /////////////////////////////////////////////
      // send texture
      /////////////////////////////////////////////

      if (_node->mSendTexture && input_tex)
        _node->mSendTexture->SetTexture(input_tex);

      /////////////////////////////////////////////

      _recompute(drawdata);

      /////////////////////////////////////////////
      // return
      /////////////////////////////////////////////
      target->debugPushGroup("PtxCompositingNode::to_output");
      target->FBI()->SetAutoClear(true);
      target->FBI()->PushRtGroup(_output);
      target->beginFrame();
      RtGroupRenderTarget rt(_output);
      auto this_buf  = target->FBI()->GetThisBuffer();
      auto& mtl      = _blit2screenmtl;
      SRect quadrect = target->mainSurfaceRectAtOrigin();
      fvec4 color(1.0f, 1.0f, 1.0f, 1.0f);
      mtl.SetAuxMatrix(fmtx4::Identity);
      mtl.SetTexture(_resultTexture);
      mtl.SetTexture2(nullptr);
      mtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
      mtl._rasterstate.SetBlending(EBLENDING_OFF);
      mtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
      this_buf->RenderMatOrthoQuad(
          vprect,
          quadrect,
          &mtl,
          0.0f,
          0.0f, // u0 v0
          1.0f,
          1.0f, // u1 v1
          nullptr,
          color);
      target->endFrame();
      target->FBI()->PopRtGroup();
      target->debugPopGroup();

      CIMPL->popCPD();
    }
  }
  ork::lev2::GfxMaterial3DSolid _blit2screenmtl;
  proctex::ProcTexContext _ptexContext;
  PtxCompositingNode* _node = nullptr;
  RtGroup* _output          = nullptr;
  RtBuffer* _outputbuffer   = nullptr;
  Texture* _resultTexture   = nullptr;
  bool _needsinit           = true;
  float _time               = 0.0f;
};
///////////////////////////////////////////////////////////////////////////////
typedef std::set<PtxCompositingNode*> instex_set_t;
ork::LockedResource<instex_set_t> ginstexset;
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::describeX(class_t* c) {
  c->accessorProperty("ReturnTexture", &PtxCompositingNode::GetTextureAccessor, &PtxCompositingNode::SetTextureAccessor)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");

  ork::reflect::RegisterProperty("Template", &PtxCompositingNode::_accessTemplate);

  c->memberProperty("BufferDim", &PtxCompositingNode::_bufferDim)
      ->annotate<ConstString>("editor.range.min", "16")
      ->annotate<ConstString>("editor.range.max", "8192");

  /////////////////////

  c->memberProperty("DynTexName", &PtxCompositingNode::mDynTexPath);

  auto nodins_loader = new asset::DynamicAssetLoader;

  nodins_loader->mEnumFn = [=]() {
    std::set<file::Path> rval;
    ginstexset.atomicOp([&](instex_set_t& dset) {
      for (auto item : dset) {
        std::string pstr("nodins://");
        pstr += item->mDynTexPath.c_str();
        file::Path p = pstr.c_str();
        rval.insert(p);
      }
    });
    return rval;
  };
  nodins_loader->mCheckFn = [=](const PieceString& name) { return ork::IsSubStringPresent("nodins://", name.c_str()); };
  nodins_loader->mLoadFn  = [=](asset::Asset* asset) {
    auto asset_name            = asset->GetName().c_str();
    lev2::TextureAsset* as_tex = rtti::autocast(asset);
    ginstexset.atomicOp([&](instex_set_t& dset) {
      for (auto item : dset) {
        std::string pstr("nodins://");
        pstr += item->mDynTexPath.c_str();

        printf("LOADDYNPTEX pstr<%s> anam<%s>\n", pstr.c_str(), asset_name);
        if (pstr == asset_name) {
          item->mSendTexture = rtti::autocast(asset);
        }
      }
    });
    return true;
  };

  lev2::TextureAsset::GetClassStatic()->AddLoader(nodins_loader);
}
///////////////////////////////////////////////////////////////////////////////
PtxCompositingNode::PtxCompositingNode()
    : mReturnTexture(nullptr)
    , mSendTexture(nullptr) {
  _impl.makeShared<PtxImpl>(this);
  ginstexset.atomicOp([&](instex_set_t& dset) { dset.insert(this); });
}
///////////////////////////////////////////////////////////////////////////////
PtxCompositingNode::~PtxCompositingNode() {
  ginstexset.atomicOp([&](instex_set_t& dset) {
    auto it = dset.find(this);
    dset.erase(it);
  });
}
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::SetTextureAccessor(ork::rtti::ICastable* const& tex) {
  mReturnTexture = tex ? ork::rtti::autocast(tex) : 0;
}
void PtxCompositingNode::GetTextureAccessor(ork::rtti::ICastable*& tex) const {
  tex = mReturnTexture;
}
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::DoInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.getShared<PtxImpl>()->gpuInit(pTARG, iW, iH);
}
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.getShared<PtxImpl>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtBuffer* PtxCompositingNode::GetOutput() const {
  auto ptximpl = _impl.getShared<PtxImpl>();
  return ptximpl->_output->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
