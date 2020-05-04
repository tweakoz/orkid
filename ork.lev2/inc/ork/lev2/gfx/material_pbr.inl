////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/fxstate_instance.h>
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
  fxinstance_ptr_t createFxStateInstance(FxStateInstanceConfig& cfg) const override;
  ////////////////////////////////////////////
  void setupCamera(const RenderContextFrameData& RCFD);
  ////////////////////////////////////////////

  FxShader* _shader                      = nullptr;
  Context* _initialTarget                = nullptr;
  fxparam_constptr_t _paramMVP           = nullptr;
  fxparam_constptr_t _paramMVPL          = nullptr;
  fxparam_constptr_t _paramMVPR          = nullptr;
  fxparam_constptr_t _paramMV            = nullptr;
  fxparam_constptr_t _paramMROT          = nullptr;
  fxparam_constptr_t _paramMapColor      = nullptr;
  fxparam_constptr_t _paramMapNormal     = nullptr;
  fxparam_constptr_t _paramMapMtlRuf     = nullptr;
  fxparam_constptr_t _parInvViewSize     = nullptr;
  fxparam_constptr_t _parMetallicFactor  = nullptr;
  fxparam_constptr_t _parRoughnessFactor = nullptr;
  fxparam_constptr_t _parModColor        = nullptr;
  fxparam_constptr_t _parBoneMatrices    = nullptr;
  ///////////////////////////////////////////
  // instancing (via texture)
  fxparam_constptr_t _paramInstanceMatrixMap = nullptr; // 1k*1k texture containing instance matrices
  ///////////////////////////////////////////
  Texture* _texColor  = nullptr;
  Texture* _texNormal = nullptr;
  Texture* _texMtlRuf = nullptr;
  std::string _textureBaseName;

  fxtechnique_constptr_t _tekRigidGBUFFER              = nullptr;
  fxtechnique_constptr_t _tekRigidGBUFFER_SKINNED_N    = nullptr;
  fxtechnique_constptr_t _tekRigidGBUFFER_N            = nullptr;
  fxtechnique_constptr_t _tekRigidGBUFFER_N_STEREO     = nullptr;
  fxtechnique_constptr_t _tekRigidGBUFFER_N_TEX_STEREO = nullptr;
  fxtechnique_constptr_t _tekRigidPICKING              = nullptr;

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
