////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

enum class OutputTimeStep {
  RealTime = 0,
  fps15,
  fps24,
  fps30,
  fps48,
  fps60,
  fps72,
  fps96,
  fps120,
  fps240,
};

enum class OutputRes {
  res_640x480 = 0,
  res_960x640,
  res_1024x1024,
  res_1280x720,
  res_1600x1200,
  res_1920x1080,
};

enum class OutputResMult {
  Quarter = 0,
  Half,
  Full,
  Double,
  Quadruple,
};

///////////////////////////////////////////////////////////////////////////////
/// FileOutputCompositingNode : OutputCompositingNode rendering to an image file
///////////////////////////////////////////////////////////////////////////////

class FileOutputCompositingNode : public OutputCompositingNode {
  DeclareConcreteX(FileOutputCompositingNode, OutputCompositingNode);

public:
  FileOutputCompositingNode();
  ~FileOutputCompositingNode();

  PoolString _layername;

  bool IsOutputFramesEnabled() const {
    return mbOutputFrames;
  }
  OutputTimeStep OutputFrameRate() const {
    return mOutputFrameRate;
  }
  OutputTimeStep currentFrameRateEnum() const;
  float currentFrameRate() const;

private:
  void gpuInit(lev2::Context* pTARG, int w, int h) final;
  void beginAssemble(CompositorDrawData& drawdata) final;
  void endAssemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;

  svar256_t _impl;

  bool mbOutputFrames;
  OutputRes mOutputBaseResolution;
  OutputResMult mOutputResMult;
  OutputTimeStep mOutputFrameRate;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
