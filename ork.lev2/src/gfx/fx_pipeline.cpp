////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/util/logger.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

namespace ork::lev2 {
static logchannel_ptr_t logchan_fxcache = logger()->configureChannel("fxcache", fvec3(0.7, 0.7, 0.5), false);
///////////////////////////////////////////////////////////////////////////////
uint64_t FxPipelinePermutation::genIndex() const {
  uint64_t index = 0;
  index += (uint64_t(_stereo) << 1);
  index += (uint64_t(_instanced) << 2);
  index += (uint64_t(_skinned) << 3);
  index += (uint64_t(_is_picking) << 4);
  index += (uint64_t(_has_vtxcolors) << 5);
  index += (uint64_t(_rendering_model) << 16);

  auto tekovr = uint64_t((const void*)_forced_technique);
  index += tekovr;
  return index;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipelinePermutation::dump() const {
  std::string rmodel = FormatString("0x%zx", uint64_t(_rendering_model));

  switch (_rendering_model) {
    case "FORWARD_PBR"_crcu:
      rmodel = "FORWARD_PBR";
      break;
    default:
      break;
  }
  printf(
      "configdump: rendering_model<0x%zx> stereo<%d> instanced<%d> skinned<%d> picking<%d>\n",
      size_t(_rendering_model),
      int(_stereo),
      int(_instanced),
      int(_skinned),
      int(_is_picking));
}
///////////////////////////////////////////////////////////////////////////////
void FxPipelinePermutationSet::add(fxpipelinepermutation_constptr_t perm) {
  __permutations.insert(perm);
}
/////////////////////////////////////////////////////////////////////////
FxPipeline::FxPipeline(const FxPipelinePermutation& config)
    : __permutation(config) {
  _vars = std::make_shared<varmap::VarMap>();
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::bindParam(fxparam_constptr_t p, varval_t v) {
  OrkAssert(p != nullptr);
  _params[p] = v;
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::bindStorage(fxparamstorageblock_constptr_t p, varval_t v) {
  OrkAssert(p != nullptr);
  _storages[p] = v;
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::bindUniformBuffer(fxuniformblock_constptr_t p, varval_t v) {
  OrkAssert(p != nullptr);
  _uniformbuffers[p] = v;
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall) {
  if (_debugBreak) {
    OrkBreak();
  }
  int inumpasses = beginBlock(RCID);
  if (_debugPrint) {
    printf("FxPipeline<%p:%s> wrappedDrawCall inumpasses<%d>\n", this, _debugName.c_str(), inumpasses);
  }
  drawcall();
  endBlock(RCID);
}
///////////////////////////////////////////////////////////////////////////////
int FxPipeline::beginBlock(const RenderContextInstData& RCID) {
  auto context            = RCID.rcfd()->GetTarget();
  auto FXI                = context->FXI();
  auto RCFD               = RCID.rcfd();
  const auto& CPD         = RCFD->topCPD();
  auto CIMPL              = RCFD->topCompositor();
  auto PBRC               = RCFD->_pbrcommon;
  lightmanager_ptr_t LMGR = CIMPL                       //
                                ? CIMPL->lightManager() //
                                : nullptr;              //

  int rval = FXI->BeginBlock(_technique, RCID);

  if (_debugBreak) {
    OrkBreak();
  }

  FXI->_debugDrawCall = _debugPrint;
  // bool OK = FXI->BindPass(ipass);
  FXI->_debugDrawCall = false;
  // if (not OK)
  // return OK;

  ///////////////////////////////
  // run state lambdas
  ///////////////////////////////

  if (_debugPrint) {
    printf("FxPipeline<%p:%s>::beginBlock num_statelambdas<%zu>\n", this, _debugName.c_str(), _statelambdas.size());
  }

  for (auto& item : _statelambdas) {
    item(RCID);
  }

  ///////////////////////////////
  // run individual state items
  ///////////////////////////////

  if (_debugPrint) {
    printf("FxPipeline<%p:%s>::beginBlock num_params<%zu>\n", this, _debugName.c_str(), _params.size());
  }

  for (auto item : _params) {
    fxparam_constptr_t param = item.first;
    const auto& val          = item.second;
    _set_typed_param(RCID, param, val);
  }

  ///////////////////////////////

  for (auto item : _storages) {
    fxparamstorageblock_constptr_t stor = item.first;
    const auto& val                     = item.second;
    _set_storage(RCID, stor, val);
  }

  ///////////////////////////////
  // apply raster state
  ///////////////////////////////
  if (_rasterstate) {
    FXI->applyRasterState(*_rasterstate);
  }

  ///////////////////////////////

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::_set_storage(const RenderContextInstData& RCID, fxparamstorageblock_constptr_t p, varval_t val) {
  auto context = RCID.rcfd()->GetTarget();
  auto RCFD    = RCID.rcfd();
  auto FXI     = context->FXI();
  if (auto as_ssbo = val.tryAs<storagebufferptr_t>()) {
    FXI->bindStorageBuffer(p, as_ssbo.value());
  }
  else if (auto as_crcstr = val.tryAs<crcstring_ptr_t>()) {
    const auto& crcstr = *as_crcstr.value().get();
    switch (crcstr.hashed()) {
      case "LMGR_LIGHTING_STORAGE"_crcu: {
        auto pl_buffer = PBRMaterial::lightingDataBuffer(context);
        FXI->bindStorageBuffer(p, pl_buffer);
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
FxPipelineProviderContext::FxPipelineProviderContext(
    const RenderContextInstData& rcid,
    const CompositingPassData& topCPD,
    FxInterface* fxi)          //
    : _rcid(rcid)              //
    , _rcfd(rcid.rcfd().get()) //
    , _fxi(fxi)                //
    , _topCPD(topCPD) {        //
}
///////////////////////////////////////////////////////////////////////////////
FxPipelineNamedParamProviders::FxPipelineNamedParamProviders() {
  /////////////////////////////////////////////////////////////////
  _providers["RCID_PickID"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    const auto& RCFDPROPS = ppc._rcfd->userProperties();
    auto itpfc            = RCFDPROPS.find("pixel_fetch_context"_crc);
    OrkAssert(itpfc != RCFDPROPS.end());
    auto as_pfc = itpfc->second.get<pixelfetchctx_ptr_t>();
    auto as_u32 = as_pfc->encodeVariant(ppc._rcid._pickID);
    // printf( "PICKID: RGBA<%g %g %g %g>\n", as_rgba.x, as_rgba.y, as_rgba.z, as_rgba.w );
    ppc._fxi->bindParamU32(param, as_u32);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_Pick"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    const auto& RCFDPROPS = ppc._rcfd->userProperties();
    auto it               = RCFDPROPS.find("pickbufferMvpMatrix"_crc);
    OrkAssert(it != RCFDPROPS.end());
    auto as_mtx4p    = it->second.get<fmtx4_ptr_t>();
    const fmtx4& MVP = *(as_mtx4p.get());
    // MVP.dump("pickbufferMvpMatrix");
    ppc._fxi->bindParamMatrix(param, MVP);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_Pick"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    const auto& RCFDPROPS = ppc._rcfd->userProperties();
    auto it               = RCFDPROPS.find("pickbufferMvpMatrix"_crc);
    OrkAssert(it != RCFDPROPS.end());
    auto as_mtx4p    = it->second.get<fmtx4_ptr_t>();
    const fmtx4& MVP = *(as_mtx4p.get());
    // MVP.dump("pickbufferMvpMatrix");
    ppc._fxi->bindParamMatrix(param, MVP);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_TIME"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto RCFD  = ppc._rcfd;
    float time = RCFD->getUserProperty("time"_crc).get<float>();
    ppc._fxi->bindParamFloat(param, time);
  };
  /////////////////////////////////////////////////////////////////
  _providers["CPD_Rtg_Dim"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    const auto& CPD = ppc._topCPD;
    int W           = CPD._width;
    int H           = CPD._height;
    ppc._fxi->bindParamVect2(param, fvec2(W, H));
  };
  /////////////////////////////////////////////////////////////////
  _providers["CPD_Rtg_InvDim"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    const auto& CPD = ppc._topCPD;
    int W           = CPD._width;
    int H           = CPD._height;
    fvec2 invdim(1.0f / float(W), 1.0f / float(H));
    ppc._fxi->bindParamVect2(param, invdim);
  };
  /////////////////////////////////////////////////////////////////
  _providers["FBI_RTG_DIM"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto context = ppc._rcfd->GetTarget();
    auto rtg     = context->FBI()->_active_rtgroup;
    int fbiw     = rtg->miW;
    int fbih     = rtg->miH;
    ppc._fxi->bindParamVect2(param, fvec2(fbiw, fbih));
  };
  /////////////////////////////////////////////////////////////////
  _providers["FBI_RTG_INVDIM"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto context = ppc._rcfd->GetTarget();
    auto rtg     = context->FBI()->_active_rtgroup;
    int fbiw     = rtg->miW;
    int fbih     = rtg->miH;
    ppc._fxi->bindParamVect2(param, fvec2(1.0 / fbiw, 1.0 / fbih));
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_MODCOLOR"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto context  = ppc._rcfd->GetTarget();
    auto modcolor = context->RefModColor();
    ppc._fxi->bindParamVect4(param, modcolor);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_M"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto context     = ppc._rcfd->GetTarget();
    auto mtxi        = context->MTXI();
    auto worldmatrix = ppc._rcid.worldMatrix();
    ppc._fxi->bindParamMatrix(param, worldmatrix);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_DEPTH_MAP"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto RCFD      = ppc._rcfd;
    auto depth_tex = RCFD->getUserProperty("DEPTH_MAP"_crc).get<texture_ptr_t>();
    ppc._fxi->bindParamTexture(param, depth_tex.get());
    // OrkAssert(false);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_EYE_INDEX"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto RCFD      = ppc._rcfd;
    int eye_index = RCFD->getUserProperty("eyeindex"_crc).get<int>();
    ppc._fxi->bindParamInt(param, eye_index);
    // OrkAssert(false);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_EYE_POSITION"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    fmtx4 V;
    if (monocams) {
      V = monocams->_vmatrix;
    }
    auto eyepos = V.inverse().translation();
    ppc._fxi->bindParamVect3(param, eyepos);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_BRDF_INTEGRATION_GGX"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    auto brdf_integration = pbrcommon->_radiance_maps->_brdfIntegrationMapGGX.get();
    ppc._fxi->bindParamTexture(param, brdf_integration);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_DIFFUSE_ENV"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    auto the_tex = pbrcommon->envDiffuseTexture().get();
    ppc._fxi->bindParamTexture(param, the_tex);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_SPECULAR_ENV"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    auto the_tex = pbrcommon->envSpecularTexture().get();
    ppc._fxi->bindParamTextureArray(param, the_tex);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_BLACK_2DMAP"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamTexture(param, pbrcommon->_texBlack.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_WHITE_2DMAP"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamTexture(param, pbrcommon->_texWhite.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_WHITE_LIGHTMAP_ARRAY"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamTextureArray(param, pbrcommon->_texWhiteLightMapArray.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_BLACK_LIGHTMAP_ARRAY"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamTextureArray(param, pbrcommon->_texBlackLightMapArray.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_LIGHTMAP_COLORS"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    static fvec3 lightmap_colors[8] = {
        fvec3(1.0f, 1.0f, 1.0f), // white
        fvec3(0.5f, 0.5f, 0.5f), // gray
        fvec3(1.0f, 0.5f, 0.5f), // red
        fvec3(0.5f, 1.0f, 0.5f), // green
        fvec3(0.5f, 0.5f, 1.0f), // blue
        fvec3(1.0f, 1.0f, 0.5f), // yellow
        fvec3(1.0f, 0.5f, 1.0f), // magenta
        fvec3(0.5f, 1.0f, 1.0f)  // cyan
    };
    ppc._fxi->bindParamVect3Array(param, lightmap_colors, 8);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_BLACK_CUBEMAP"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamTexture(param, pbrcommon->_texCubeBlack.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_WHITE_CUBEMAP"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamTexture(param, pbrcommon->_texCubeWhite.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_MONOCAM_NEAR_FAR"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    float near = monocams->_camdat.mNear;
    float far  = monocams->_camdat.mFar;
    ppc._fxi->bindParamVect2(param, fvec2(near, far));
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_MVP_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      // printf( "RCFD_Camera_MVP_Mono: monocams<%p>\n", (void*)monocams );
      ppc._fxi->bindParamMatrix(param, monocams->MVPMONO(worldmatrix));
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
      ppc._fxi->bindParamMatrix(param, MVP);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_MV_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      // printf( "RCFD_Camera_MVP_Mono: monocams<%p>\n", (void*)monocams );
      ppc._fxi->bindParamMatrix(param, monocams->_vmatrix * worldmatrix);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MV = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVMatrix());
      ppc._fxi->bindParamMatrix(param, MV);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_V_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    if (monocams) {
      ppc._fxi->bindParamMatrix(param, monocams->_vmatrix);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      ppc._fxi->bindParamMatrix(param, MTXI->RefVMatrix());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_P_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    if (monocams) {
      ppc._fxi->bindParamMatrix(param, monocams->_pmatrix);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      ppc._fxi->bindParamMatrix(param, MTXI->RefPMatrix());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_VP_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      fmtx4 vp = monocams->VPMONO();
      // vp.dump("monocams->VPMONO()");
      ppc._fxi->bindParamMatrix(param, vp);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
      ppc._fxi->bindParamMatrix(param, MVP);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IM_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    ppc._fxi->bindParamMatrix(param, worldmatrix.inverse());
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IV_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      ppc._fxi->bindParamMatrix(param, monocams->GetIVMatrix());
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVMatrix().inverse());
      ppc._fxi->bindParamMatrix(param, MVP);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IP_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      ppc._fxi->bindParamMatrix(param, monocams->_pmatrix.inverse());
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVMatrix().inverse());
      ppc._fxi->bindParamMatrix(param, MVP);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IMV_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      // printf( "RCFD_Camera_MVP_Mono: monocams<%p>\n", (void*)monocams );
      ppc._fxi->bindParamMatrix(param, (monocams->_vmatrix * worldmatrix).inverse());
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MV = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVMatrix());
      ppc._fxi->bindParamMatrix(param, MV.inverse());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IVP_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      auto VP  = monocams->VPMONO();
      auto IVP = VP.inverse();
      // IVP.dump("IVP");
      ppc._fxi->bindParamMatrix(param, IVP);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix().inverse());
      ppc._fxi->bindParamMatrix(param, MVP);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IMVP_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      auto MVP  = monocams->MVPMONO(worldmatrix);
      auto IMVP = MVP.inverse();
      // IVP.dump("IVP");
      ppc._fxi->bindParamMatrix(param, IMVP);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
      ppc._fxi->bindParamMatrix(param, MVP.inverse());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_ZNORMAL_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    auto worldmatrix = ppc._rcid.worldMatrix();
    if (monocams) {
      auto VP      = monocams->VPMONO();
      auto IVP     = VP.inverse();
      fvec3 raydir = IVP.zNormal().normalized();
      // IVP.dump("IVP");
      // printf("raydir1<%g %g %g>\n", raydir.x, raydir.y, raydir.z);
      ppc._fxi->bindParamVect4(param, raydir);
    } else {
      auto MTXI        = ppc._rcfd->GetTarget()->MTXI();
      auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix().inverse());
      // printf("raydir2<%g %g %g>\n", raydir.x, raydir.y, raydir.z);
      ppc._fxi->bindParamMatrix(param, MVP);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_VP_Left"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto stereocams = ppc._topCPD._stereo_cam_matrices;
    bool is_stereo = ppc._topCPD.isSinglePassStereo();
    if (is_stereo and stereocams) {
      ppc._fxi->bindParamMatrix(param, stereocams->VPL());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_VP_Right"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto stereocams = ppc._topCPD._stereo_cam_matrices;
    bool is_stereo = ppc._topCPD.isSinglePassStereo();
    if (is_stereo and stereocams) {
      ppc._fxi->bindParamMatrix(param, stereocams->VPR());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IVP_Left"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto stereocams = ppc._topCPD._stereo_cam_matrices;
    bool is_stereo = ppc._topCPD.isSinglePassStereo();
    if (is_stereo and stereocams) {
      auto m = stereocams->VPL().inverse();
      ppc._fxi->bindParamMatrix(param, m);
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_IVP_Right"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto stereocams = ppc._topCPD._stereo_cam_matrices;
    bool is_stereo = ppc._topCPD.isSinglePassStereo();
    if (is_stereo and stereocams) {
      ppc._fxi->bindParamMatrix(param, stereocams->VPR().inverse());
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_MVP_Left"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto stereocams = ppc._topCPD._stereo_cam_matrices;
    bool is_stereo = ppc._topCPD.isSinglePassStereo();
    if (is_stereo and stereocams) {
      auto worldmatrix = ppc._rcid.worldMatrix();
      ppc._fxi->bindParamMatrix(param, stereocams->MVPL(worldmatrix));
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Camera_MVP_Right"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto stereocams = ppc._topCPD._stereo_cam_matrices;
    bool is_stereo = ppc._topCPD.isSinglePassStereo();
    if (is_stereo and stereocams) {
      auto worldmatrix = ppc._rcid.worldMatrix();
      ppc._fxi->bindParamMatrix(param, stereocams->MVPR(worldmatrix));
    }
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_Model_Rot"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto worldmatrix = ppc._rcid.worldMatrix();
    auto rotmtx = worldmatrix.rotMatrix33();
    ppc._fxi->bindParamMatrix(param, rotmtx);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_DPP_ZBIAS"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamFloat(param, pbrcommon->_dppZbias);
  };
  /////////////////////////////////////////////////////////////////
  _providers["LMGR_ACTIVE_UNTEXTURED_POINTLIGHT_COUNT"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto enumlights = ppc._rcfd->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    ppc._fxi->bindParamInt(param, enumlights->_num_active_untextured_pointlights);
  };
  /////////////////////////////////////////////////////////////////
  _providers["LMGR_ACTIVE_TEXTURED_SPOTLIGHT_COUNT"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto enumlights = ppc._rcfd->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    ppc._fxi->bindParamInt(param, enumlights->_num_active_texspotlights);
  };
  /////////////////////////////////////////////////////////////////
  _providers["LMGR_ACTIVE_TEXTURED_SPOTLIGHT_COLOR_COOKIES"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto CIMPL              = ppc._rcfd->topCompositor();
    lightmanager_ptr_t LMGR = CIMPL->lightManager();
    ppc._fxi->bindParamTextureArray(param, LMGR->_cookies_spot_color.get());
  };
  /////////////////////////////////////////////////////////////////
  _providers["LMGR_ACTIVE_TEXTURED_SPOTLIGHT_DEPTH_COOKIES"_crcu] = [this](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto CIMPL              = ppc._rcfd->topCompositor();
    lightmanager_ptr_t LMGR = CIMPL->lightManager();
    ppc._fxi->bindParamTextureArray(param, LMGR->_cookies_spot_depth.get());
  };
  /////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
fxpipelinenamedparamproviders_ptr_t FxPipelineNamedParamProviders::instance() {
  static fxpipelinenamedparamproviders_ptr_t inst = std::make_shared<FxPipelineNamedParamProviders>();
  return inst;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::_set_typed_param(const RenderContextInstData& RCID, fxparam_constptr_t param, varval_t val) {
  auto context            = RCID.rcfd()->GetTarget();
  auto RCFD               = RCID.rcfd();
  auto FXI                = context->FXI();
  auto worldmatrix        = RCID.worldMatrix();
  const auto& CPD         = RCID.rcfd()->topCPD();
  int W                   = CPD._width;
  int H                   = CPD._height;
  auto MTXI               = context->MTXI();
  const auto& RCFDPROPS   = RCID.rcfd()->userProperties();
  bool is_picking         = CPD.isPicking();
  bool is_stereo          = CPD.isSinglePassStereo();
  auto pbrcommon          = RCID.rcfd()->_pbrcommon;
  auto modcolor           = context->RefModColor();
  auto CIMPL              = RCFD->topCompositor();
  auto named_providers    = FxPipelineNamedParamProviders::instance();
  lightmanager_ptr_t LMGR = CIMPL                       //
                                ? CIMPL->lightManager() //
                                : nullptr;              //

  ////////////////////////////////////////////////////////////
  // amortize lookups
  ////////////////////////////////////////////////////////////
  FxPipelineProviderContext PPC(RCID, CPD, FXI);
  ////////////////////////////////////////////////////////////
  // try to order these by commonalitiy
  //  or find a quicker dispatch method
  ////////////////////////////////////////////////////////////
  if (auto as_mtx4 = val.tryAs<fmtx4>()) {
    FXI->bindParamMatrix(param, as_mtx4.value());
  } else if (auto as_mtx4ptr = val.tryAs<fmtx4_ptr_t>()) {
    FXI->bindParamMatrix(param, *as_mtx4ptr.value().get());
  } else if (auto as_texture = val.tryAs<Texture*>()) {
    auto texture = as_texture.value();
    FXI->bindParamTexture(param, texture);
  } else if (auto as_texture = val.tryAs<texture_ptr_t>()) {
    auto texture = as_texture.value();
    FXI->bindParamTexture(param, texture.get());
  } else if (auto as_texture_array = val.tryAs<texturearray_ptr_t>()) {
    auto texture_array = as_texture_array.value();
    FXI->bindParamTextureArray(param, texture_array.get());
  } else if (auto as_bool_ = val.tryAs<bool>()) {
    FXI->bindParamBool(param, as_bool_.value());
  } else if (auto as_int_ = val.tryAs<int>()) {
    FXI->bindParamInt(param, as_int_.value());
  } else if (auto as_float_ = val.tryAs<float>()) {
    float val = as_float_.value();
    // auto parname = param->_name;
    // printf("FxPipeline<%p:%s> bindParamFloat<%s> val<%f>\n", (void*)this, _debugName.c_str(), param->_name.c_str(), val );
    FXI->bindParamFloat(param, val);
  } else if (auto as_fvec4_ = val.tryAs<fvec4>()) {
    FXI->bindParamVect4(param, as_fvec4_.value());
  } else if (auto as_fvec3 = val.tryAs<fvec3>()) {
    FXI->bindParamVect3(param, as_fvec3.value());
  } else if (auto as_fvec2 = val.tryAs<fvec2>()) {
    FXI->bindParamVect2(param, as_fvec2.value());
  } else if (auto as_fmtx3 = val.tryAs<fmtx3>()) {
    FXI->bindParamMatrix(param, as_fmtx3.value());
  } else if (auto as_instancedata_ = val.tryAs<instanceddrawinstancedata_ptr_t>()) {
    OrkAssert(false);
  } else if (auto as_fquat = val.tryAs<fquat_ptr_t>()) {
    const auto& Q = *as_fquat.value().get();
    fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
    FXI->bindParamVect4(param, as_vec4);
  } else if (auto as_fplane3 = val.tryAs<fplane3_ptr_t>()) {
    const auto& P = *as_fplane3.value().get();
    fvec4 as_vec4(P.n, P.d);
    FXI->bindParamVect4(param, as_vec4);
  }
  ///////////////////////////////////////////////////////////////////
  else if (auto as_varval_generator = val.tryAs<varval_generator_t>()) {
    auto gen = as_varval_generator.value();
    _set_typed_param(RCID, param, gen());
  }
  ///////////////////////////////////////////////////////////////////
  else if (auto as_storage = val.tryAs<FxShaderStorageBuffer*>()) {
    auto storage = as_storage.value();
    // FXI->bindParamStorageBuffer(param, storage);
    OrkAssert(false);
  }
  ///////////////////////////////////////////////////////////////////
  else if (auto as_crcstr = val.tryAs<crcstring_ptr_t>()) {
    const auto& crcstr = *as_crcstr.value().get();
    auto it = named_providers->_providers.find(crcstr.hashed());
    if (it != named_providers->_providers.end()) {
      auto func = it->second;
      func(PPC, param);
    } else {
      OrkAssert(false);
    }
  } else {
    OrkAssert(false);
  }
}
void FxPipeline::dump() const {
  printf("FxPipeline<%p:%s>\n", (void*)this, _debugName.c_str());
  __permutation.dump();
  printf("  debugtext<%s>\n", _debugText.c_str());
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::endBlock(const RenderContextInstData& RCID) {
  auto context = RCID.rcfd()->GetTarget();
  context->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const RenderContextInstData& RCID) const {
  auto RCFD    = RCID.rcfd();
  auto context = RCFD->_target;
  auto fxi     = context->FXI();
  bool stereo  = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
  bool picking = RCFD->hasCPD() ? RCFD->topCPD().isPicking() : false;
  /////////////////
  FxPipelinePermutation permu;
  permu._stereo = stereo;
#if defined(__APPLE__)
  permu._stereo  = stereo;
  permu._vr_mono = stereo;
#endif

  permu._skinned          = RCID._isSkinned;
  permu._instanced        = RCID._isInstanced;
  permu._forced_technique = RCID._forced_technique;
  permu._is_picking       = picking;
  permu._rendering_model  = RCFD->_renderingmodel._modelID;
  // permu.dump();
  /////////////////
  return findPipeline(permu);
}
///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  uint64_t index = permu.genIndex();
  auto it        = _lut.find(index);
  if (it != _lut.end()) {
    pipeline = it->second;
  } else { // miss
    OrkAssert(_on_miss);
    logchan_fxcache->log("fxlut<%p> findPipeline onmiss index<%zu>", this, index);
    pipeline    = _on_miss(permu);
    _lut[index] = pipeline;
  }
  return pipeline;
} ///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
