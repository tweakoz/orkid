////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/lev2/gfx/material_freestyle.h>

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
      : _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All"))
      , _node(node) {
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* ctx) {
    if (_needsinit) {
      _blit2screenmtl.gpuInit(ctx, "orkshader://solid");
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
      _fxtechnique2x2 = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique3x3 = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique4x4 = _blit2screenmtl.technique("downsample_4x4");
      _fxtechnique5x5 = _blit2screenmtl.technique("downsample_5x5");
      _fxtechnique6x6 = _blit2screenmtl.technique("downsample_6x6");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");
      _needsinit      = false;
    }
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    auto& ddprops                = drawdata._properties;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto DB                      = RCFD.GetDB();
    Context* targ                = drawdata.context();
    int w                        = targ->mainSurfaceWidth();
    int h                        = targ->mainSurfaceHeight();
    if (targ->hiDPI()) {
      w /= 2;
      h /= 2;
    }
    bool supersample = false; // _node->supersample()
    _width           = w;
    _height          = h;
    //////////////////////////////////////////////////////
    drawdata._properties["OutputWidth"_crcu].set<int>(_width);
    drawdata._properties["OutputHeight"_crcu].set<int>(_height);
    drawdata._properties["StereoEnable"_crcu].set<bool>(false);
    _CPD.defaultSetup(drawdata);
    CIMPL->pushCPD(_CPD);
  }
  void endAssemble(CompositorDrawData& drawdata) {
    auto CIMPL                   = drawdata._cimpl;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  FileOutputCompositingNode* _node = nullptr;
  CompositingPassData _CPD;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderTechnique* _fxtechnique2x2;
  const FxShaderTechnique* _fxtechnique3x3;
  const FxShaderTechnique* _fxtechnique4x4;
  const FxShaderTechnique* _fxtechnique5x5;
  const FxShaderTechnique* _fxtechnique6x6;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  bool _needsinit = true;
  int _width      = 0;
  int _height     = 0;
  ///////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
FileOutputCompositingNode::FileOutputCompositingNode()
    : mbOutputFrames(false)
    , mOutputBaseResolution(OutputRes::res_1280x720)
    , mOutputResMult(OutputResMult::Full)
    , mOutputFrameRate(OutputTimeStep::RealTime) {
  _impl     = std::make_shared<IMPL>(this);
  _basepath = "output";
}
FileOutputCompositingNode::~FileOutputCompositingNode() {
}
void FileOutputCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<IMPL>>()->gpuInit(pTARG);
}
void FileOutputCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  _impl.get<std::shared_ptr<IMPL>>()->beginAssemble(drawdata);
}
void FileOutputCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  _impl.get<std::shared_ptr<IMPL>>()->endAssemble(drawdata);
}
void FileOutputCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("ScreenOutputCompositingNode::composite");
  auto impl = _impl.get<std::shared_ptr<IMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer      = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = framerenderer.framedata();
  Context* context                  = framedata.GetTarget();
  auto fbi                          = context->FBI();
  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen");
        auto this_buf = context->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        switch (0) {
          case 0:
            mtl.begin(impl->_fxtechnique1x1, framedata);
            break;
          case 1:
            mtl.begin(impl->_fxtechnique2x2, framedata);
            break;
          case 2:
            mtl.begin(impl->_fxtechnique3x3, framedata);
            break;
          case 3:
            mtl.begin(impl->_fxtechnique4x4, framedata);
            break;
          case 4:
            mtl.begin(impl->_fxtechnique5x5, framedata);
            break;
          case 5:
            mtl.begin(impl->_fxtechnique6x6, framedata);
            break;
        }
        mtl._rasterstate.SetBlending(Blending::OFF);
        mtl.bindParamCTex(impl->_fxpColorMap, tex);
        mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
        this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
        mtl.end(framedata);

        drawdata.context()->debugPopGroup();

        if (_basepath.length()) {

          std::string out_path;
          if (_frameoutputindex >= 0)
            out_path = ork::FormatString("%s-%d.png", _basepath.c_str(), _frameoutputindex++);
          else
            out_path = ork::FormatString("%s.png", _basepath.c_str());

          fbi->capture(buffer, file::Path(out_path));
        }
      }
    }
  }
  drawdata.context()->debugPopGroup();
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
