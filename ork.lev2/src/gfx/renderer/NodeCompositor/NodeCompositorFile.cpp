////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorFile.h"

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>

ImplementReflectionX(ork::lev2::FileOutputCompositingNode, "FileOutputCompositingNode");

///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::lev2, EOutputRes)
DECLARE_ENUM(EOutputRes_640x480)
DECLARE_ENUM(EOutputRes_960x640)
DECLARE_ENUM(EOutputRes_1024x1024)
DECLARE_ENUM(EOutputRes_1280x720)
DECLARE_ENUM(EOutputRes_1600x1200)
DECLARE_ENUM(EOutputRes_1920x1080)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::lev2, EOutputResMult)
DECLARE_ENUM(EOutputResMult_Quarter)
DECLARE_ENUM(EOutputResMult_Half)
DECLARE_ENUM(EOutputResMult_Full)
DECLARE_ENUM(EOutputResMult_Double)
DECLARE_ENUM(EOutputResMult_Quadruple)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::lev2, EOutputTimeStep)
DECLARE_ENUM(EOutputTimeStep_RealTime)
DECLARE_ENUM(EOutputTimeStep_15fps)
DECLARE_ENUM(EOutputTimeStep_24fps)
DECLARE_ENUM(EOutputTimeStep_30fps)
DECLARE_ENUM(EOutputTimeStep_48fps)
DECLARE_ENUM(EOutputTimeStep_60fps)
DECLARE_ENUM(EOutputTimeStep_72fps)
DECLARE_ENUM(EOutputTimeStep_96fps)
DECLARE_ENUM(EOutputTimeStep_120fps)
DECLARE_ENUM(EOutputTimeStep_240fps)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void FileOutputCompositingNode::describeX(class_t* c) {
  c->memberProperty("Layer", &FileOutputCompositingNode::_layername);
  c->memberProperty("OutputFrames", &FileOutputCompositingNode::mbOutputFrames);
  c->memberProperty("OutputResBase", &FileOutputCompositingNode::mOutputBaseResolution)
      ->annotate("editor.class", "ged.factory.enum");
  c->memberProperty("OutputResMult", &FileOutputCompositingNode::mOutputResMult)->annotate("editor.class", "ged.factory.enum");
  c->memberProperty("OutputFrameRate", &FileOutputCompositingNode::mOutputFrameRate)->annotate("editor.class", "ged.factory.enum");
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
    drawdata.target()->debugMarker("File::endFrame");
  }
  void composite(CompositorDrawData& drawdata) {
    auto final = drawdata._properties["final"_crcu].Get<RtGroup*>();
    drawdata.target()->debugMarker("File::endFrame");
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  FileOutputCompositingNode* _node = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
FileOutputCompositingNode::FileOutputCompositingNode()
    : mbOutputFrames(false)
    , mOutputFrameRate(EOutputTimeStep_RealTime)
    , mOutputBaseResolution(EOutputRes_1280x720)
    , mOutputResMult(EOutputResMult_Full) {
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

EOutputTimeStep FileOutputCompositingNode::currentFrameRateEnum() const {
  return mOutputFrameRate;
}

///////////////////////////////////////////////////////////////////////////////

float FileOutputCompositingNode::currentFrameRate() const {
  EOutputTimeStep time_step = currentFrameRateEnum();
  float framerate           = 0.0f;
  switch (time_step) {
    case EOutputTimeStep_15fps:
      framerate = 1.0f / 15.0f;
      break;
    case EOutputTimeStep_24fps:
      framerate = 24.0f;
      break;
    case EOutputTimeStep_30fps:
      framerate = 30.0f;
      break;
    case EOutputTimeStep_48fps:
      framerate = 48.0f;
      break;
    case EOutputTimeStep_60fps:
      framerate = 60.0f;
      break;
    case EOutputTimeStep_72fps:
      framerate = 72.0f;
      break;
    case EOutputTimeStep_96fps:
      framerate = 96.0f;
      break;
    case EOutputTimeStep_120fps:
      framerate = 120.0f;
      break;
    case EOutputTimeStep_240fps:
      framerate = 240.0f;
      break;
    case EOutputTimeStep_RealTime:
    default:
      break;
  }

  return framerate;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
