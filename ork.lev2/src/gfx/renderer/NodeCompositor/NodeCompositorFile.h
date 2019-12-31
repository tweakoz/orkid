////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {

  ///////////////////////////////////////////////////////////////////////////////

  enum EOutputTimeStep {
    EOutputTimeStep_RealTime = 0,
    EOutputTimeStep_15fps,
    EOutputTimeStep_24fps,
    EOutputTimeStep_30fps,
    EOutputTimeStep_48fps,
    EOutputTimeStep_60fps,
    EOutputTimeStep_72fps,
    EOutputTimeStep_96fps,
    EOutputTimeStep_120fps,
    EOutputTimeStep_240fps,
  };

  enum EOutputRes {
    EOutputRes_640x480 = 0,
    EOutputRes_960x640,
    EOutputRes_1024x1024,
    EOutputRes_1280x720,
    EOutputRes_1600x1200,
    EOutputRes_1920x1080,
  };

  enum EOutputResMult {
    EOutputResMult_Quarter = 0,
    EOutputResMult_Half,
    EOutputResMult_Full,
    EOutputResMult_Double,
    EOutputResMult_Quadruple,
  };

///////////////////////////////////////////////////////////////////////////////

class FileOutputCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(FileOutputCompositingNode, OutputCompositingNode);

public:
  FileOutputCompositingNode();
  ~FileOutputCompositingNode();

  PoolString _layername;

  bool IsOutputFramesEnabled() const { return mbOutputFrames; }
  EOutputTimeStep OutputFrameRate() const { return mOutputFrameRate; }
  EOutputTimeStep currentFrameRateEnum() const;
  float currentFrameRate() const;

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;

  bool mbOutputFrames;
  EOutputRes mOutputBaseResolution;
  EOutputResMult mOutputResMult;
  EOutputTimeStep mOutputFrameRate;

};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
