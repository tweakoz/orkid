////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>
#include <boost/filesystem.hpp>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>

namespace ork::lev2 {
using namespace boost::filesystem;
using namespace std::literals;

///////////////////////////////////////////////////////////////////////////////

class PBRMaterial;
using pbrmaterial_ptr_t      = std::shared_ptr<PBRMaterial>;
using pbrmaterial_constptr_t = std::shared_ptr<const PBRMaterial>;

struct PbrMatrixBlockApplicator : public MaterialInstApplicator {
  MaterialInstItemMatrixBlock* _matrixblock = nullptr;
  const PBRMaterial* _pbrmaterial           = nullptr;
  void ApplyToTarget(Context* pTARG) final;
  static PbrMatrixBlockApplicator* getApplicator();
};

///////////////////////////////////////////////////////////////////////////////

struct FilteredEnvMap {
  std::shared_ptr<RtGroup> _rtgroup;
  std::shared_ptr<RtBuffer> _rtbuffer;
  Texture* _texture = nullptr;
};
typedef std::shared_ptr<FilteredEnvMap> filtenvmapptr_t;
///////////////////////////////////////////////////////////////////////////////

class PBRMaterial final : public GfxMaterial {

  DeclareConcreteX(PBRMaterial, GfxMaterial);

public:
  PBRMaterial(Context* targ);
  PBRMaterial();
  ~PBRMaterial();

  void setTextureBaseName(std::string basename) {
    _textureBaseName = basename;
  }

  ////////////////////////////////////////////

  static Texture* brdfIntegrationMap(Context* targ);
  static Texture* filterSpecularEnvMap(Texture* rawenvmap, Context* targ);
  static Texture* filterDiffuseEnvMap(Texture* rawenvmap, Context* targ);

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(Context* targ, int iPass = 0) final;
  void EndPass(Context* targ) final;
  int BeginBlock(Context* targ, const RenderContextInstData& RCID) final;
  void EndBlock(Context* targ) final;
  void Init(Context* targ) final;
  void Update() final;
  void BindMaterialInstItem(MaterialInstItem* pitem) const override;
  void UnBindMaterialInstItem(MaterialInstItem* pitem) const override;

  ////////////////////////////////////////////
  void setupCamera(const RenderContextFrameData& RCFD);
  ////////////////////////////////////////////

  FxShader* _shader                        = nullptr;
  Context* _initialTarget                  = nullptr;
  const FxShaderParam* _paramMVP           = nullptr;
  const FxShaderParam* _paramMVPL          = nullptr;
  const FxShaderParam* _paramMVPR          = nullptr;
  const FxShaderParam* _paramMV            = nullptr;
  const FxShaderParam* _paramMROT          = nullptr;
  const FxShaderParam* _paramMapColor      = nullptr;
  const FxShaderParam* _paramMapNormal     = nullptr;
  const FxShaderParam* _paramMapMtlRuf     = nullptr;
  const FxShaderParam* _parInvViewSize     = nullptr;
  const FxShaderParam* _parMetallicFactor  = nullptr;
  const FxShaderParam* _parRoughnessFactor = nullptr;
  const FxShaderParam* _parModColor        = nullptr;
  const FxShaderParam* _parBoneMatrices    = nullptr;
  ///////////////////////////////////////////
  // instancing (via texture)
  const FxShaderParam* _paramInstanceMatrixMap = nullptr; // 1k*1k texture containing instance matrices
  ///////////////////////////////////////////
  Texture* _texColor  = nullptr;
  Texture* _texNormal = nullptr;
  Texture* _texMtlRuf = nullptr;
  std::string _textureBaseName;

  const FxShaderTechnique* _tekRigidGBUFFER              = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_SKINNED_N    = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_N            = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_N_STEREO     = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_N_TEX_STEREO = nullptr;
  const FxShaderTechnique* _tekRigidPICKING              = nullptr;

  std::string _colorMapName;
  std::string _normalMapName;
  std::string _mtlRufMapName;
  std::string _amboccMapName;
  std::string _emissiveMapName;

  float _metallicFactor  = 0.0f;
  float _roughnessFactor = 1.0f;
  fvec4 _baseColor;

  bool _stereoVtex = false;
};

material_ptr_t default3DMaterial();

} // namespace ork::lev2
