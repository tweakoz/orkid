////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
/// OutputCompositingNode : compositor node responsible for output to a sink
///   sinks include things like RtGroups, the screen, Vr-HMD, etc..
///////////////////////////////////////////////////////////////////////////////

using compdrawdata_fn_t = std::function<void(CompositorDrawData& drawdata)>;

class OutputCompositingNode : public ork::Object {
  DeclareAbstractX(OutputCompositingNode, ork::Object);

public:

  OutputCompositingNode();
  ~OutputCompositingNode();
  virtual void gpuInit(lev2::Context* pTARG, int w, int h) {
  }
  // Per-frame GPU update hook, invoked exactly ONCE per rendered frame from the
  //  compositing technique's assemble entry, BEFORE any eye/render work — regardless
  //  of how many eyes/views the node's assembler goes on to render. VR output nodes
  //  drive the device frame update (xrWaitFrame/xrBeginFrame/xrLocateViews) here.
  //  Default no-op — non-VR output nodes are untouched.
  virtual void onGpuUpdate(CompositorDrawData& drawdata) {
  }
  virtual void beginAssemble(CompositorDrawData& drawdata) {
  }
  virtual void endAssemble(CompositorDrawData& drawdata) {
  }
  virtual void composite(CompositorDrawData& drawdata) {
  }

  bool _flipY = true;

  compdrawdata_fn_t _onBeginAssemble = nullptr;
  compdrawdata_fn_t _onCameraChange = nullptr;
  compdrawdata_fn_t _onEndAssemble = nullptr;
  svar64_t _pyimpl_oba;
  svar64_t _pyimpl_oea;
};

///////////////////////////////////////////////////////////////////////////////
/// RenderCompositingNode : compositor node responsible for generation of
///   a frame utilizing some rendering technique such as forward, deferred, etc..
///////////////////////////////////////////////////////////////////////////////

class RenderCompositingNode : public ork::Object {
  DeclareAbstractX(RenderCompositingNode, ork::Object);

public:
  RenderCompositingNode();
  ~RenderCompositingNode();
  void gpuInit(lev2::Context* pTARG, int w, int h);
  void Render(CompositorDrawData& drawdata);
  virtual lev2::rtbuffer_ptr_t GetOutput() const {
    return nullptr;
  }
  virtual lev2::rtgroup_ptr_t GetOutputGroup() const {
    return nullptr;
  }
  // The layer-name ROLES this render node reads when assembling its frame
  // (empty = node doesn't enumerate roles). Scene::initWithParams creates
  // each as a real (empty, zero-cost) layer up front, so content can land
  // on any role without per-scene declarations — under-declared scenes
  // (findLayer asserts, nodes silently parked on sg_default) were a
  // recurring bug class.
  virtual const std::vector<std::string>& renderedLayerRoles() const {
    static const std::vector<std::string> s_none;
    return s_none;
  }

  RenderingModel _renderingmodel;
  std::string _layers;
  // GENERIC AUX CHANNELS (E2B item D): named auxiliary render targets the
  // node renders as extra passes. Each channel name X adds layer role
  // "aux_X" (drawables opt in by attaching to it; materials opt in with an
  // aux technique pair) and publishes its RTG into the CompositorDrawData
  // properties under crc("aux_X") for postfx consumption (heat distortion,
  // glow masks, velocity, ...). Set BEFORE Scene::initWithParams pre-creates
  // role layers (it reads renderedLayerRoles()). v1 LIMIT: aux RTGs carry no
  // depth attachment — aux content is not occluded by scene geometry.
  std::vector<std::string> _auxChannels;
  uint64_t _bufferKey = 0;
  int      _frameIndex = 0;
  uint64_t _debugRenderingModel = "NONE"_crcu;
  uint64_t _debugPassID = "NONE"_crcu;
  uint64_t _debugSubPassID = "NONE"_crcu;
  
private:
  virtual void doGpuInit(lev2::Context* pTARG, int w, int h) = 0;
  virtual void DoRender(CompositorDrawData& drawdata)        = 0;
};

///////////////////////////////////////////////////////////////////////////////
/// PostCompositingNode : compositor node responsible for postprocessing effects.
///////////////////////////////////////////////////////////////////////////////

class PostCompositingNode : public ork::Object {
  DeclareAbstractX(PostCompositingNode, ork::Object);

public:
  PostCompositingNode();
  ~PostCompositingNode();
  void gpuInit(lev2::Context* pTARG, int w, int h);
  void Render(CompositorDrawData& drawdata);
  virtual lev2::rtbuffer_ptr_t GetOutput() const {
    return nullptr;
  }
  virtual lev2::rtgroup_ptr_t GetOutputGroup() const {
    return nullptr;
  }

  bool _disabled = false;
private:
  virtual void doGpuInit(lev2::Context* pTARG, int w, int h) = 0;
  virtual void DoRender(CompositorDrawData& drawdata)        = 0;
};

///////////////////////////////////////////////////////////////////////////////
/// PostCompositingNode : compositor node responsible for postprocessing effects.
///////////////////////////////////////////////////////////////////////////////

class LambdaPostCompositingNode : public PostCompositingNode {
  DeclareAbstractX(LambdaPostCompositingNode, PostCompositingNode);

public:
  LambdaPostCompositingNode();
  ~LambdaPostCompositingNode();
  void gpuInit(lev2::Context* pTARG, int w, int h);
  void Render(CompositorDrawData& drawdata);
  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

private:
  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata)        final;
};

///////////////////////////////////////////////////////////////////////////////
/// Op2CompositingNode : binary (2 in, 1 out) with a choice of operation
///  has scale and bias on each of the input terms (a and b)
///////////////////////////////////////////////////////////////////////////////

class Op2CompositingNode : public PostCompositingNode {
  DeclareConcreteX(Op2CompositingNode, PostCompositingNode);

public:
  Op2CompositingNode();
  ~Op2CompositingNode();

private:
  void doGpuInit(lev2::Context* pTARG, int w, int h) override; // virtual
  void DoRender(CompositorDrawData& drawdata) override;        // virtual
  void GetNodeA(ork::rtti::ICastable*& val) const;
  void SetNodeA(ork::rtti::ICastable* const& val);
  void GetNodeB(ork::rtti::ICastable*& val) const;
  void SetNodeB(ork::rtti::ICastable* const& val);
  lev2::rtbuffer_ptr_t GetOutput() const override {
    return _output;
  }
  lev2::rtgroup_ptr_t GetOutputGroup() const final {
    return _rtg;
  }

  PostCompositingNode* mSubA = nullptr;
  PostCompositingNode* mSubB = nullptr;
  CompositingMaterial mCompositingMaterial;
  lev2::rtbuffer_ptr_t _output;
  lev2::rtgroup_ptr_t _rtg;
  Op2CompositeMode mMode;
  fvec4 mLevelA;
  fvec4 mLevelB;
  fvec4 mBiasA;
  fvec4 mBiasB;
};

///////////////////////////////////////////////////////////////////////////////
/// NodeCompositingTechnique : CompositingTechnique specifically utilizing
//    a three node setup. Each node has a specific purpose in the output of a frame.
///   1. RenderCompositingNode: render a frame
///   2. PostCompositingNode: postprocessing (blur,bloom,etc..)
///   3. OutputCompositingNode: output to a sink (screen,VR, etc..)
///////////////////////////////////////////////////////////////////////////////

struct NodeCompositingTechnique final : public CompositingTechnique {
  DeclareConcreteX(NodeCompositingTechnique, CompositingTechnique);

public:
  NodeCompositingTechnique();
  ~NodeCompositingTechnique();

  template <typename T, typename... A> std::shared_ptr<T> createRenderNode(A&&... args) {
    auto rval = std::make_shared<T>(std::forward<A>(args)...);
    _renderNode = rval;
    return rval;
  }
  template <typename T, typename... A> std::shared_ptr<T> createPostFxNode(A&&... args) {
    auto rval = std::make_shared<T>(std::forward<A>(args)...);
    _postEffectNodes.push_back(rval);
    return rval;
  }
  template <typename T, typename... A> std::shared_ptr<T> createOutputNode(A&&... args) {
    auto rval = std::make_shared<T>(std::forward<A>(args)...);
    _outputNode = rval;
    return rval;
  }

  void gpuInit(lev2::Context* context, int w, int h) override;
  bool assemble(CompositorDrawData& drawdata) override;
  void composite(CompositorDrawData& drawdata) override;
  //

  template <typename T> std::shared_ptr<T> tryRenderNodeAs() {
    return std::dynamic_pointer_cast<T>(_renderNode);
  }
  template <typename T> std::shared_ptr<T> tryOutputNodeAs() {
    return std::dynamic_pointer_cast<T>(_outputNode);
  }
  void defaultAssembly();

  ork::ObjectMap mBufferMap;
  compositorrendernode_ptr_t _renderNode;
  postfx_node_chain_t _postEffectNodes;
  compositoroutnode_ptr_t _outputNode;
  compdrawdata_fn_t _assemblerFn = nullptr;
};

using outputcompositingnode_ptr_t      = std::shared_ptr<OutputCompositingNode>;
using outputcompositingnode_constptr_t = std::shared_ptr<const OutputCompositingNode>;

} // namespace ork::lev2

