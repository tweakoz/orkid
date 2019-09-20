////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorPtx.h"
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::PtxCompositingNode, "PtxCompositingNode");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
typedef std::set<PtxCompositingNode*> instex_set_t;
ork::LockedResource<instex_set_t> ginstexset;
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::describeX(class_t* c) {
  c->accessorProperty("InputNode", &PtxCompositingNode::GetNode, &PtxCompositingNode::SetNode)
   ->annotate<ConstString>( "editor.factorylistbase", "PostCompositingNode");

  c->accessorProperty("ReturnTexture", &PtxCompositingNode::GetTextureAccessor, &PtxCompositingNode::SetTextureAccessor)
   ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
   ->annotate<ConstString>("editor.assettype", "lev2tex")
   ->annotate<ConstString>("editor.assetclass", "lev2tex");

  /////////////////////

  ork::reflect::RegisterProperty("DynTexName", &PtxCompositingNode::mDynTexPath);

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
  nodins_loader->mCheckFn = [=](const PieceString& name) { return ork::IsSubStringPresent("nodins://", name.data()); };
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
    : mFTEK(nullptr)
    , mNode(nullptr)
    , mOutput(nullptr)
    , mReturnTexture(nullptr)
    , mSendTexture(nullptr) {
  ginstexset.atomicOp([&](instex_set_t& dset) { dset.insert(this); });
}
PtxCompositingNode::~PtxCompositingNode() {
  ginstexset.atomicOp([&](instex_set_t& dset) {
    auto it = dset.find(this);
    dset.erase(it);
  });

  if (mFTEK)
    delete mFTEK;
  if (mOutput)
    delete mOutput;
}
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::SetTextureAccessor(ork::rtti::ICastable* const& tex) {
  mReturnTexture = tex ? ork::rtti::autocast(tex) : 0;
}
///////////////////////////////////////////////////////////////////////////////
void PtxCompositingNode::GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = mReturnTexture; }
void PtxCompositingNode::GetNode(ork::rtti::ICastable*& val) const { val = const_cast<PostCompositingNode*>(mNode); }
void PtxCompositingNode::SetNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mNode                     = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
void PtxCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    mOutput         = new lev2::RtGroup(pTARG, iW, iH);
    auto buf        = new lev2::RtBuffer(mOutput, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH);
    buf->_debugName = FormatString("PtxCompositingNode::sendtex");
    mOutput->SetMrt(0, buf);

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);

    if (mNode)
      mNode->Init(pTARG, iW, iH);
  }
}
void PtxCompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  lev2::FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  orkstack<CompositingPassData>& cgSTACK  = drawdata.mCompositingGroupStack;
  auto target                             = framedata.GetTarget();
  auto fbi                                = target->FBI();
  auto gbi                                = target->GBI();
  int iw                                  = target->GetW();
  int ih                                  = target->GetH();

  if (mNode)
    mNode->Render(drawdata);

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mNode) {
    auto node_out = mNode->GetOutput();
    if (node_out) {
      lev2::Texture* send_texture = node_out->GetMrt(0)->GetTexture();

      /////////////////////////////////////////////

      if (mSendTexture && send_texture)
        mSendTexture->SetTexture(send_texture);

      /////////////////////////////////////////////

      fbi->SetAutoClear(false);
      fbi->PushRtGroup(mOutput);
      gbi->BeginFrame();

      auto ptex = (mReturnTexture != nullptr) ? mReturnTexture->GetTexture() : (lev2::Texture*)nullptr;

      mCompositingMaterial.SetTextureA(ptex);
      mCompositingMaterial.SetTechnique("Asolo");

      fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());

      gbi->EndFrame();
      fbi->PopRtGroup();
    }
  }
}
lev2::RtGroup* PtxCompositingNode::GetOutput() const {
  lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
  return pRT;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
