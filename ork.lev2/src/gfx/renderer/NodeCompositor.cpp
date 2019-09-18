////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/compositor.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/rtti/RTTI.h>
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
#include <ork/lev2/gfx/renderer/drawable.h>

BEGIN_ENUM_SERIALIZER(ork::lev2, EOp2CompositeMode)
DECLARE_ENUM(Op2AsumB)
DECLARE_ENUM(Op2AmulB)
DECLARE_ENUM(Op2AdivB)
DECLARE_ENUM(Op2BoverA)
DECLARE_ENUM(Op2Asolo)
DECLARE_ENUM(Op2Bsolo)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::NodeCompositingTechnique, "NodeCompositingTechnique");
ImplementReflectionX(ork::lev2::CompositingNode, "CompositingNode");
ImplementReflectionX(ork::lev2::SeriesCompositingNode, "SeriesCompositingNode");
ImplementReflectionX(ork::lev2::InsertCompositingNode, "InsertCompositingNode");
ImplementReflectionX(ork::lev2::Op2CompositingNode, "Op2CompositingNode");
ImplementReflectionX(ork::lev2::PassThroughCompositingNode, "PassThroughCompositingNode");
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::describeX(class_t*c) {
  ork::reflect::RegisterProperty("RootNode", &NodeCompositingTechnique::GetRoot, &NodeCompositingTechnique::SetRoot);
  ork::reflect::AnnotatePropertyForEditor<NodeCompositingTechnique>("RootNode", "editor.factorylistbase", "CompositingNode");
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::NodeCompositingTechnique() : mpRootNode(nullptr) {}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::~NodeCompositingTechnique() {
  if (mpRootNode)
    delete mpRootNode;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::GetRoot(ork::rtti::ICastable*& val) const {
  CompositingNode* nonconst = const_cast<CompositingNode*>(mpRootNode);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::SetRoot(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mpRootNode = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Init(lev2::GfxTarget* pTARG, int w, int h) {
  if (mpRootNode) {
    mpRootNode->Init(pTARG, w, h);
    mCompositingMaterial.Init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Draw(CompositorDrawData& drawdata, CompositingImpl* pCCI) {
  if (mpRootNode) {
    mpRootNode->Render(drawdata, pCCI);
  }
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) {
  if (mpRootNode) {
    auto output = mpRootNode->GetOutput();
    if (output) {
      auto ptex = output->GetMrt(0)->GetTexture();
      auto fbi = pT->FBI();
      auto buf = fbi->GetThisBuffer();

      int w = pT->GetW();
      int h = pT->GetH();

      SRect vprect(0, 0, w - 1, h - 1);
      SRect quadrect(0, h - 1, w - 1, 0);

      mCompositingMaterial.SetTextureA(ptex);
      mCompositingMaterial.SetLevelA(fvec4(1.0f, 1.0f, 1.0f, 1.0f));
      mCompositingMaterial.SetLevelB(fvec4(0.0f, 0.0f, 0.0f, 0.0f));
      mCompositingMaterial.SetLevelC(fvec4(0.0f, 0.0f, 0.0f, 0.0f));
      mCompositingMaterial.SetTechnique("Asolo");
      buf->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::CompositingBuffer() {}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::~CompositingBuffer() {}
///////////////////////////////////////////////////////////////////////////////
void CompositingNode::describeX(class_t*c) {}
///////////////////////////////////////////////////////////////////////////////
CompositingNode::CompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
CompositingNode::~CompositingNode() {}
void CompositingNode::Init(lev2::GfxTarget* pTARG, int w, int h) { DoInit(pTARG, w, h); }
void CompositingNode::Render(CompositorDrawData& drawdata, CompositingImpl* pCCI) { DoRender(drawdata, pCCI); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::describeX(class_t*c) {
  c->accessorProperty("Group", &PassThroughCompositingNode::_readGroup, &PassThroughCompositingNode::_writeGroup)
   ->annotate<ConstString>("editor.factorylistbase", "CompositingGroup");
}
///////////////////////////////////////////////////////////////////////////////
PassThroughCompositingNode::PassThroughCompositingNode() : mFTEK(nullptr), mGroup(nullptr) {}
///////////////////////////////////////////////////////////////////////////////
PassThroughCompositingNode::~PassThroughCompositingNode() {
  if (mFTEK)
    delete mFTEK;
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::_readGroup(ork::rtti::ICastable*& val) const {
  CompositingGroup* nonconst = const_cast<CompositingGroup*>(mGroup);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::_writeGroup(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mGroup = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingGroup*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (nullptr == mFTEK) {
    mCompositingMaterial.Init(pTARG);

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) // virtual
{
  const CompositingGroup* pCG = mGroup;
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;

  CompositingPassData node;
  node.mbDrawSource = (pCG != 0);

  if (mFTEK) {
    mFTEK->mfSourceAmplitude = pCG ? 1.0f : 0.0f;
    lev2::rendervar_t passdata;
    passdata.Set<const char*>("All");
    the_renderer.framedata().setUserProperty("pass"_crc, passdata);
    node.mpGroup = pCG;
    node.mpFrameTek = mFTEK;
    node.mpCameraName = (pCG != 0) ? &pCG->GetCameraName() : 0;
    node.mpLayerName = (pCG != 0) ? &pCG->GetLayers() : 0;
    cgSTACK.push(node);
    mFTEK->Render(the_renderer);
    cgSTACK.pop();
  }
}

lev2::RtGroup* PassThroughCompositingNode::GetOutput() const {
  lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
  return pRT;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::describeX(class_t*c) {
  ork::reflect::RegisterProperty("Node", &SeriesCompositingNode::GetNode, &SeriesCompositingNode::SetNode);

  auto anno = [&](const char* p, const char* k, const char* v) {
    ork::reflect::AnnotatePropertyForEditor<SeriesCompositingNode>(p, k, v);
  };

  // anno( "Mode", "editor.class", "ged.factory.enum" );
  anno("Node", "editor.factorylistbase", "CompositingNode");
}
///////////////////////////////////////////////////////////////////////////////
SeriesCompositingNode::SeriesCompositingNode() : mFTEK(nullptr), mNode(nullptr), mOutput(nullptr) {}
SeriesCompositingNode::~SeriesCompositingNode() {
  if (mFTEK)
    delete mFTEK;
  if (mOutput)
    delete mOutput;
}
void SeriesCompositingNode::GetNode(ork::rtti::ICastable*& val) const { val = const_cast<CompositingNode*>(mNode); }
void SeriesCompositingNode::SetNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mNode = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingNode*>(ptr));
}
void SeriesCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    mOutput = new lev2::RtGroup(pTARG, iW, iH);
    mOutput->SetMrt(0, new lev2::RtBuffer(mOutput, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH));

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);

    if (mNode)
      mNode->Init(pTARG, iW, iH);
  }
}
void SeriesCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) // virtual
{
  // const ent::CompositingGroup* pCG = mGroup;
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mNode)
    mNode->Render(drawdata, pCCI);

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mNode) {
    fbi->SetAutoClear(false);
    fbi->PushRtGroup(mOutput);
    gbi->BeginFrame();

    lev2::Texture* ptex = mNode->GetOutput()->GetMrt(0)->GetTexture();

    mCompositingMaterial.SetTextureA(ptex);
    mCompositingMaterial.SetTechnique("Asolo");

    fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());

    gbi->EndFrame();
    fbi->PopRtGroup();
  }
}
lev2::RtGroup* SeriesCompositingNode::GetOutput() const {
  lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
  return pRT;
}

///////////////////////////////////////////////////////////////////////////////
typedef std::set<InsertCompositingNode*> instex_set_t;
ork::LockedResource<instex_set_t> ginstexset;
///////////////////////////////////////////////////////////////////////////////
void InsertCompositingNode::describeX(class_t*c) {
  ork::reflect::RegisterProperty("InputNode", &InsertCompositingNode::GetNode, &InsertCompositingNode::SetNode);

  auto anno = [&](const char* p, const char* k, const char* v) {
    ork::reflect::AnnotatePropertyForEditor<InsertCompositingNode>(p, k, v);
  };

  // anno( "Mode", "editor.class", "ged.factory.enum" );
  anno("InputNode", "editor.factorylistbase", "CompositingNode");

  ork::reflect::RegisterProperty("ReturnTexture", &InsertCompositingNode::GetTextureAccessor,
                                 &InsertCompositingNode::SetTextureAccessor);
  ork::reflect::AnnotatePropertyForEditor<InsertCompositingNode>("ReturnTexture", "editor.class", "ged.factory.assetlist");
  ork::reflect::AnnotatePropertyForEditor<InsertCompositingNode>("ReturnTexture", "editor.assettype", "lev2tex");
  ork::reflect::AnnotatePropertyForEditor<InsertCompositingNode>("ReturnTexture", "editor.assetclass", "lev2tex");

  /////////////////////

  ork::reflect::RegisterProperty("DynTexName", &InsertCompositingNode::mDynTexPath);

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
  nodins_loader->mLoadFn = [=](asset::Asset* asset) {
    auto asset_name = asset->GetName().c_str();
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
InsertCompositingNode::InsertCompositingNode()
    : mFTEK(nullptr), mNode(nullptr), mOutput(nullptr), mReturnTexture(nullptr), mSendTexture(nullptr) {
  ginstexset.atomicOp([&](instex_set_t& dset) { dset.insert(this); });
}
InsertCompositingNode::~InsertCompositingNode() {
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
void InsertCompositingNode::SetTextureAccessor(ork::rtti::ICastable* const& tex) {
  mReturnTexture = tex ? ork::rtti::autocast(tex) : 0;
}
///////////////////////////////////////////////////////////////////////////////
void InsertCompositingNode::GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = mReturnTexture; }
void InsertCompositingNode::GetNode(ork::rtti::ICastable*& val) const { val = const_cast<CompositingNode*>(mNode); }
void InsertCompositingNode::SetNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mNode = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingNode*>(ptr));
}
void InsertCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    mOutput = new lev2::RtGroup(pTARG, iW, iH);
    mOutput->SetMrt(0, new lev2::RtBuffer(mOutput, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH));

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);

    if (mNode)
      mNode->Init(pTARG, iW, iH);
  }
}
void InsertCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) // virtual
{
  // const ent::CompositingGroup* pCG = mGroup;
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mNode)
    mNode->Render(drawdata, pCCI);

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mNode) {
    auto node_out = mNode->GetOutput();
    if(node_out){
        lev2::Texture* send_texture = mNode->GetOutput()->GetMrt(0)->GetTexture();

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
lev2::RtGroup* InsertCompositingNode::GetOutput() const {
  lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
  return pRT;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::describeX(class_t*c) {
  ork::reflect::RegisterProperty("Mode", &Op2CompositingNode::mMode);
  ork::reflect::RegisterProperty("LevelA", &Op2CompositingNode::mLevelA);
  ork::reflect::RegisterProperty("LevelB", &Op2CompositingNode::mLevelB);
  ork::reflect::RegisterProperty("BiasA", &Op2CompositingNode::mBiasA);
  ork::reflect::RegisterProperty("BiasB", &Op2CompositingNode::mBiasB);

  ork::reflect::RegisterProperty("NodeA", &Op2CompositingNode::GetNodeA, &Op2CompositingNode::SetNodeA);
  ork::reflect::RegisterProperty("NodeB", &Op2CompositingNode::GetNodeB, &Op2CompositingNode::SetNodeB);

  auto anno = [&](const char* p, const char* k, const char* v) {
    ork::reflect::AnnotatePropertyForEditor<Op2CompositingNode>(p, k, v);
  };

  anno("Mode", "editor.class", "ged.factory.enum");
  anno("NodeA", "editor.factorylistbase", "CompositingNode");
  anno("NodeB", "editor.factorylistbase", "CompositingNode");
}
void Op2CompositingNode::GetNodeA(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<CompositingNode*>(mSubA);
  val = nonconst;
}
void Op2CompositingNode::SetNodeA(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubA = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingNode*>(ptr));
}
void Op2CompositingNode::GetNodeB(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<CompositingNode*>(mSubB);
  val = nonconst;
}
void Op2CompositingNode::SetNodeB(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubB = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
Op2CompositingNode::Op2CompositingNode()
    : mSubA(nullptr), mSubB(nullptr), mOutput(nullptr), mMode(Op2AsumB), mLevelA(1.0f, 1.0f, 1.0f, 1.0f),
      mLevelB(1.0f, 1.0f, 1.0f, 1.0f), mBiasA(0.0f, 0.0f, 0.0f, 0.0f), mBiasB(0.0f, 0.0f, 0.0f, 0.0f) {}
///////////////////////////////////////////////////////////////////////////////
Op2CompositingNode::~Op2CompositingNode() {
  if (mSubA)
    delete mSubA;
  if (mSubB)
    delete mSubB;
  if (mOutput)
    delete mOutput;
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (mSubA)
    mSubA->Init(pTARG, iW, iH);
  if (mSubB)
    mSubB->Init(pTARG, iW, iH);

  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    mOutput = new lev2::RtGroup(pTARG, iW, iH);
    mOutput->SetMrt(0, new lev2::RtBuffer(mOutput, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH));
  }
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) // virtual
{
  auto& the_renderer = drawdata.mFrameRenderer;
  auto& framedata = the_renderer.framedata();
  auto& cgSTACK = drawdata.mCompositingGroupStack;
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mSubA) {
    mSubA->Render(drawdata, pCCI);
  }
  if (mSubB) {
    mSubB->Render(drawdata, pCCI);
  }

  fbi->SetAutoClear(false);
  fbi->PushRtGroup(mOutput);
  gbi->BeginFrame();

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mSubA && mSubB) {
    lev2::Texture* ptexa = mSubA->GetOutput()->GetMrt(0)->GetTexture();
    lev2::Texture* ptexb = mSubB->GetOutput()->GetMrt(0)->GetTexture();
    mCompositingMaterial.SetTextureA(ptexa);
    mCompositingMaterial.SetTextureB(ptexb);
    mCompositingMaterial.SetBiasA(mBiasA);
    mCompositingMaterial.SetBiasB(mBiasB);
    mCompositingMaterial.SetLevelA(mLevelA);
    mCompositingMaterial.SetLevelB(mLevelB);
    mCompositingMaterial.SetLevelC(fvec4(0.0f, 0.0f, 0.0f, 0.0f));

    switch (mMode) {
      case Op2AsumB:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
      case Op2AmulB:
        mCompositingMaterial.SetTechnique("Op2AmulB");
        break;
      case Op2AdivB:
        mCompositingMaterial.SetTechnique("Op2AdivB");
        break;
      case Op2AoverB:
        mCompositingMaterial.SetTechnique("AoverBplusC");
        break;
      case Op2BoverA:
        mCompositingMaterial.SetTechnique("BoverAplusC");
        break;
      case Op2Asolo:
        mCompositingMaterial.SetTechnique("Asolo");
        break;
      case Op2Bsolo:
        mCompositingMaterial.SetTechnique("Bsolo");
        break;
    }

    fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());
  }

  gbi->EndFrame();
  fbi->PopRtGroup();
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
