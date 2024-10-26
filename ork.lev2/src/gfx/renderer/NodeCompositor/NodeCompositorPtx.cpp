////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPtx.h>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/registerX.inl>

#if 0
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
      _blit2screenmtl.gpuInit(pTARG);

      _output                   = new lev2::RtGroup(pTARG, iW, iH);
      _outputbuffer             = _output->createRenderTarget(lev2::EBufferFormat::RGBA16F);
      _outputbuffer->_debugName = FormatString("PtxCompositingNode::output");
      _output->SetMrt(0, _outputbuffer);
    }
  }
  void _recompute(CompositorDrawData& drawdata) {
    auto target                             = drawdata.context();
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
    auto target                        = drawdata.context();
    auto fbi                           = target->FBI();
    auto gbi                           = target->GBI();

    auto CIMPL  = drawdata._cimpl;
    auto CPD    = CIMPL->topCPD();
    auto vprect = target->mainSurfaceRectAtOrigin();
    CPD.SetDstRect(vprect);
    CPD._cameraMatrices       = nullptr;
    CPD._stereoCameraMatrices = nullptr;
    CPD._stereo1pass          = false;

    lev2::texture_ptr_t input_tex = nullptr;

    if (auto try_render = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
      auto buffer = try_render.value();
      if (buffer) {
        assert(buffer != nullptr);
        OrkAssert(false); // FIXME!
        input_tex = std::shared_ptr<Texture>(buffer->texture());
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
      auto this_buf = target->FBI()->GetThisBuffer();
      auto& mtl     = _blit2screenmtl;
      auto quadrect = vprect.asSRect();
      fvec4 color(1.0f, 1.0f, 1.0f, 1.0f);
      mtl.SetAuxMatrix(fmtx4::Identity());
      mtl.SetTexture(_resultTexture);
      mtl.SetTexture2(nullptr);
      mtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
      mtl._rasterstate.SetBlending(Blending::OFF);
      mtl._rasterstate.SetDepthTest(EDepthTest::OFF);
      this_buf->RenderMatOrthoQuad(
          vprect.asSRect(),
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
  rtbuffer_ptr_t _outputbuffer;
  Texture* _resultTexture   = nullptr;
  bool _needsinit           = true;
  float _time               = 0.0f;
};
///////////////////////////////////////////////////////////////////////////////
typedef std::set<PtxCompositingNode*> instex_set_t;
ork::LockedResource<instex_set_t> ginstexset;
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::describeX(class_t* c) {
  /*
  c->accessorProperty("ReturnTexture", &PtxCompositingNode::GetTextureAccessor, &PtxCompositingNode::SetTextureAccessor)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");

  ork::reflect::RegisterProperty("Template", &PtxCompositingNode::_accessTemplate);

  c->directProperty("BufferDim", &PtxCompositingNode::_bufferDim)
      ->annotate<ConstString>("editor.range.min", "16")
      ->annotate<ConstString>("editor.range.max", "8192");

  /////////////////////

  c->directProperty("DynTexName", &PtxCompositingNode::mDynTexPath);

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
  nodins_loader->mCheckFn = [=](const PieceString& name) { return std::string(name.c_str()).contains("nodins://"); };
  nodins_loader->mLoadFn  = [=](asset::asset_ptr_t asset) {
    auto as_tex     = std::dynamic_pointer_cast<lev2::TextureAsset>(asset);
    auto asset_name = asset->GetName().c_str();
    ginstexset.atomicOp([&](instex_set_t& dset) {
      for (auto item : dset) {
        std::string pstr("nodins://");
        pstr += item->mDynTexPath.c_str();
        printf("LOADDYNPTEX pstr<%s> anam<%s>\n", pstr.c_str(), asset_name);
        if (pstr == asset_name) {
          item->mSendTexture = as_tex.get();
        }
      }
    });
    return true;
  };

  lev2::TextureAsset::GetClassStatic()->AddLoader(nodins_loader);
  */
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
void PtxCompositingNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.getShared<PtxImpl>()->gpuInit(pTARG, iW, iH);
}
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.getShared<PtxImpl>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
lev2::rtbuffer_ptr_t PtxCompositingNode::GetOutput() const {
  auto ptximpl = _impl.getShared<PtxImpl>();
  return ptximpl->_output->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
#endif 
