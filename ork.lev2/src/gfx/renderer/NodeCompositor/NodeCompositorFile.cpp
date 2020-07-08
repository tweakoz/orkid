////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorFile.h>

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

ImplementReflectionX(ork::lev2::FileOutputCompositingNode, "FileOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
BeginEnumRegistration(OutputRes);
RegisterEnum(OutputRes, res_640x480);
RegisterEnum(OutputRes, res_960x640);
RegisterEnum(OutputRes, res_1024x1024);
RegisterEnum(OutputRes, res_1280x720);
RegisterEnum(OutputRes, res_1600x1200);
RegisterEnum(OutputRes, res_1920x1080);
EndEnumRegistration();
///////////////////////////////////////////////////////////////////////////////
BeginEnumRegistration(OutputResMult);
RegisterEnum(OutputResMult, Quarter);
RegisterEnum(OutputResMult, Half);
RegisterEnum(OutputResMult, Full);
RegisterEnum(OutputResMult, Double);
RegisterEnum(OutputResMult, Quadruple);
EndEnumRegistration();
///////////////////////////////////////////////////////////////////////////////
BeginEnumRegistration(OutputTimeStep);
RegisterEnum(OutputTimeStep, RealTime);
RegisterEnum(OutputTimeStep, fps15);
RegisterEnum(OutputTimeStep, fps24);
RegisterEnum(OutputTimeStep, fps30);
RegisterEnum(OutputTimeStep, fps48);
RegisterEnum(OutputTimeStep, fps60);
RegisterEnum(OutputTimeStep, fps72);
RegisterEnum(OutputTimeStep, fps96);
RegisterEnum(OutputTimeStep, fps120);
RegisterEnum(OutputTimeStep, fps240);
EndEnumRegistration();
///////////////////////////////////////////////////////////////////////////////
void FileOutputCompositingNode::describeX(class_t* c) {
  /*c->directProperty("Layer", &FileOutputCompositingNode::_layername);
  c->directProperty("OutputFrames", &FileOutputCompositingNode::mbOutputFrames);
  c->directProperty("OutputResBase", &FileOutputCompositingNode::mOutputBaseResolution)
      ->annotate("editor.class", "ged.factory.enum");
  c->directProperty("OutputResMult", &FileOutputCompositingNode::mOutputResMult) //
      ->annotate("editor.class", "ged.factory.enum");
  c->directProperty("OutputFrameRate", &FileOutputCompositingNode::mOutputFrameRate) //
      ->annotate("editor.class", "ged.factory.enum");*/
}
///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  ///////////////////////////////////////
  IMPL(FileOutputCompositingNode* node)
      : _node(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* pTARG) {
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    FrameRenderer& fr_renderer        = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = fr_renderer.framedata();
    auto targ                         = framedata.GetTarget();
    // framedata.setLayerName(_node->_layername.c_str());
    // targ->debugMarker("File::beginFrame");
    drawdata._properties["OutputWidth"_crcu].Set<int>(targ->mainSurfaceWidth());
    drawdata._properties["OutputHeight"_crcu].Set<int>(targ->mainSurfaceHeight());
  }
  void endAssemble(CompositorDrawData& drawdata) {
    drawdata.context()->debugMarker("File::endFrame");
  }
  void composite(CompositorDrawData& drawdata) {
    auto final = drawdata._properties["final"_crcu].Get<RtGroup*>();
    drawdata.context()->debugMarker("File::endFrame");
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  FileOutputCompositingNode* _node = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
FileOutputCompositingNode::FileOutputCompositingNode()
    : mbOutputFrames(false)
    , mOutputFrameRate(OutputTimeStep::RealTime)
    , mOutputBaseResolution(OutputRes::res_1280x720)
    , mOutputResMult(OutputResMult::Full) {
  _impl = std::make_shared<IMPL>(this);
}
FileOutputCompositingNode::~FileOutputCompositingNode() {
}
void FileOutputCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<IMPL>>()->gpuInit(pTARG);
}
void FileOutputCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<IMPL>>()->beginAssemble(drawdata);
}
void FileOutputCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<IMPL>>()->endAssemble(drawdata);
}
void FileOutputCompositingNode::composite(CompositorDrawData& drawdata) {
  _impl.Get<std::shared_ptr<IMPL>>()->composite(drawdata);
}
///////////////////////////////////////////////////////////////////////////////

OutputTimeStep FileOutputCompositingNode::currentFrameRateEnum() const {
  return mOutputFrameRate;
}

///////////////////////////////////////////////////////////////////////////////

float FileOutputCompositingNode::currentFrameRate() const {
  OutputTimeStep time_step = currentFrameRateEnum();
  float framerate          = 0.0f;
  switch (time_step) {
    case OutputTimeStep::fps15:
      framerate = 1.0f / 15.0f;
      break;
    case OutputTimeStep::fps24:
      framerate = 24.0f;
      break;
    case OutputTimeStep::fps30:
      framerate = 30.0f;
      break;
    case OutputTimeStep::fps48:
      framerate = 48.0f;
      break;
    case OutputTimeStep::fps60:
      framerate = 60.0f;
      break;
    case OutputTimeStep::fps72:
      framerate = 72.0f;
      break;
    case OutputTimeStep::fps96:
      framerate = 96.0f;
      break;
    case OutputTimeStep::fps120:
      framerate = 120.0f;
      break;
    case OutputTimeStep::fps240:
      framerate = 240.0f;
      break;
    case OutputTimeStep::RealTime:
    default:
      break;
  }

  return framerate;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
