////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class PostFxNodeUser : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeUser, PostCompositingNode);

public:
  PostFxNodeUser();
  ~PostFxNodeUser();

  void doGpuInit(lev2::Context* pTARG, int w, int h) final; // virtual
  void DoRender(CompositorDrawData& drawdata) final;        // virtual

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;
  texture_ptr_t getCurrentReadTexture() const;  // For double-buffered feedback

  svar256_t _impl;
  //fxpipeline_ptr_t _pipeline;
  std::unordered_map<std::string, FxPipeline::varval_t> _bindings;
  std::string _technique_name;
  std::string _shader_path;
  bool _double_buffer = false;
  bool _flip_vertical = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
