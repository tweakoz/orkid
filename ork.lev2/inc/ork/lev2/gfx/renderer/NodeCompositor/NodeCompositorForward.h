////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include "PBRCommon.inl"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class ForwardCompositingNode : public RenderCompositingNode {
  DeclareConcreteX(ForwardCompositingNode, RenderCompositingNode);

public:
  ForwardCompositingNode();
  ~ForwardCompositingNode();

  std::string _layername;
  fvec4 _clearColor;

private:
  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct ForwardCompositingNodePbr : public RenderCompositingNode {
  DeclareConcreteX(ForwardCompositingNodePbr, RenderCompositingNode);

public:
  ForwardCompositingNodePbr();
  ~ForwardCompositingNodePbr();

  lev2::texture_ptr_t envSpecularTexture() const;
  lev2::texture_ptr_t envDiffuseTexture() const;

  float environmentIntensity() const {
    return _environmentIntensity;
  }
  float environmentMipBias() const {
    return _environmentMipBias;
  }
  float environmentMipScale() const {
    return _environmentMipScale;
  }
  float diffuseLevel() const {
    return _diffuseLevel;
  }
  float specularLevel() const {
    return _specularLevel;
  }
  fvec3 ambientLevel() const {
    return _ambientLevel;
  }
  float skyboxLevel() const {
    return _skyboxLevel;
  }
  float depthFogDistance() const {
    return _depthFogDistance;
  }
  float depthFogPower() const {
    return _depthFogPower;
  }

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;
  void _readEnvTexture(asset::asset_ptr_t& tex) const;
  void _writeEnvTexture(asset::asset_ptr_t const& tex);

  void setEnvTexturePath(file::Path path);

  asset::asset_ptr_t _environmentTextureAsset;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

  svar256_t _impl;
  float _environmentIntensity = 1.0f;
  float _environmentMipBias   = 0.0f;
  float _environmentMipScale  = 0.0f;
  float _diffuseLevel         = 1.0f;
  float _specularLevel        = 1.0f;
  float _skyboxLevel          = 1.0f;
  float _depthFogDistance     = 1000.0f;
  float _depthFogPower        = 1.0f;
  fvec3 _ambientLevel;
  lev2::texture_ptr_t _filtenvSpecularMap;
  lev2::texture_ptr_t _filtenvDiffuseMap;
  fvec4 _clearColor;

  asset::vars_ptr_t _texAssetVarMap;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
