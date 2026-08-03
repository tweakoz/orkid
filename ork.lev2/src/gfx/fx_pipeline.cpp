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
#include <ork/lev2/gfx/material_freestyle.h>
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
  index += (uint64_t(_is_alpha) << 6);
  index += (uint64_t(_is_vertex_ssbo) << 7);
  index += (uint64_t(_instanced_matrices_only) << 8);
  index += (uint64_t(_is_impostor) << 9);
  index += (uint64_t(_is_mesh_shader) << 10);
  index += (uint64_t(_is_sun_cookie) << 11);
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
void FxPipeline::bindStorage(fxparamstorageblock_constptr_t p, varval_t v, size_t byte_offset) {
  OrkAssert(p != nullptr);
  _storages[p]        = v;
  _storage_offsets[p] = byte_offset; // sub-range bind (0 = whole)
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
void FxPipeline::_syncMaterialParams() {
  if (_material_ptr == nullptr)
    return;
  if (_bound_params_seen == _material_ptr->_bound_params_stamp)
    return;
  for (const auto& item : _material_ptr->_bound_params)
    _params[item.first] = item.second;
  _bound_params_seen = _material_ptr->_bound_params_stamp;
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

  _syncMaterialParams(); // material-level rebinds go live here (stamp-gated)

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
    auto oit                            = _storage_offsets.find(stor);
    size_t byte_offset                  = (oit != _storage_offsets.end()) ? oit->second : 0;
    _set_storage(RCID, stor, val, byte_offset);
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
void FxPipeline::_set_storage(const RenderContextInstData& RCID, fxparamstorageblock_constptr_t p, varval_t val, size_t byte_offset) {
  auto context = RCID.rcfd()->GetTarget();
  auto RCFD    = RCID.rcfd();
  auto FXI     = context->FXI();
  if (auto as_ssbo = val.tryAs<storagebufferptr_t>()) {
    FXI->bindStorageBuffer(p, as_ssbo.value(), byte_offset);
  }
  else if (auto as_crcstr = val.tryAs<crcstring_ptr_t>()) {
    const auto& crcstr = *as_crcstr.value().get();
    switch (crcstr.hashed()) {
      case "LMGR_LIGHTING_STORAGE"_crcu: {
        auto pl_buffer = PBRMaterial::lightingDataBuffer(context);
        FXI->bindStorageBuffer(p, pl_buffer, byte_offset);
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
    const fmtx4& VP = *(as_mtx4p.get());  // This is VP (View*Projection) from pick camera
    auto worldmatrix = ppc._rcid.worldMatrix();
    fmtx4 MVP = VP * worldmatrix;  // Compute full MVP = VP * M
    ppc._fxi->bindParamMatrix(param, MVP);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_TIME"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto RCFD  = ppc._rcfd;
    float time = RCFD->userPropertyAs<float>("time"_crc);
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
    auto depth_tex = RCFD->userPropertyAs<texture_ptr_t>("DEPTH_MAP"_crc);
    ppc._fxi->bindParamTexture(param, depth_tex.get());
    // OrkAssert(false);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_EYE_INDEX"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto RCFD      = ppc._rcfd;
    int eye_index = RCFD->userPropertyAs<int>("eyeindex"_crc);
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
  // 4 world-space corners of the view frustum intersected with y=0.
  // NDC corner order: (x0,y0),(x1,y0),(x1,y1),(x0,y1).
  // Used by projected-grid water / ground shaders.
  _providers["RCFD_GROUND_FRUSTUM_CORNERS"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    fvec4 corners4[4];
    if (monocams) {
      auto pts = monocams->projectedCornersOnPlane(0.0f);
      for (int i = 0; i < 4; ++i) {
        corners4[i] = fvec4(pts[i].x, pts[i].y, pts[i].z, 1.0f);
      }
    } else {
      for (int i = 0; i < 4; ++i) {
        corners4[i] = fvec4(0, 0, 0, 1);
      }
    }
    ppc._fxi->bindParamVect4Array(param, corners4, 4);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_BRDF_INTEGRATION_GGX"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    // SKYLIGHT B3: from the ACTIVE maps, so it pairs with the env textures the
    // sibling providers bind (same object as _radiance_maps in baked scenes).
    auto brdf_integration = pbrcommon->activeRadianceMaps()->_brdfIntegrationMapGGX.get();
    ppc._fxi->bindParamTexture(param, brdf_integration);
  };
  /////////////////////////////////////////////////////////////////
  // THE DIFFUSE AMBIENT (W4-S9) — nine L2 coefficients, not a map. Every
  // consumer of the shared PBR sampler set reads the ambient through these,
  // the sky probe's or the bound map set's alike (CommonStuff::envSHCoeffs).
  _providers["RCFD_PBR_ENV_SH"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon = ppc._rcfd->_pbrcommon;
    fvec4 sh[9]    = {};
    pbrcommon->envSHCoeffs(sh);
    ppc._fxi->bindParamVect4Array(param, sh, 9);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_ENV_SH_VALID"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon = ppc._rcfd->_pbrcommon;
    fvec4 sh[9]    = {};
    ppc._fxi->bindParamFloat(param, pbrcommon->envSHCoeffs(sh) ? 1.0f : 0.0f);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_SPECULAR_ENV"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    auto the_tex = pbrcommon->envSpecularTexture().get();
    ppc._fxi->bindParamTextureArray(param, the_tex);
  };
  /////////////////////////////////////////////////////////////////
  // The outgoing IBL set + its blend weight while a procedural refilter
  // crossfades. Aliases the specular bind above (weight 1) whenever no fade is
  // running, so a consumer that binds both is unchanged outside a fade window.
  // The ambient does not appear here: its crossfade is resolved on the CPU,
  // inside envSHCoeffs, so one set of coefficients covers the pair.
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_SPECULAR_ENV_PREV"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    auto the_tex = pbrcommon->envSpecularTexturePrev().get();
    ppc._fxi->bindParamTextureArray(param, the_tex);
  };
  /////////////////////////////////////////////////////////////////
  _providers["RCFD_PBR_ENV_BLEND_WEIGHT"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto pbrcommon          = ppc._rcfd->_pbrcommon;
    ppc._fxi->bindParamFloat(param, pbrcommon->envCrossfadeWeight());
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
  _providers["RCFD_Camera_IVP_NoTrans_Mono"_crcu] = [](const FxPipelineProviderContext& ppc, fxparam_constptr_t param) {
    auto monocams = ppc._topCPD._mono_cam_matrices;
    if (monocams) {
      auto V = monocams->_vmatrix;
      V.setTranslation(0, 0, 0);
      auto VP  = fmtx4::multiply_ltor(V, monocams->_pmatrix);
      ppc._fxi->bindParamMatrix(param, VP.inverse());
    } else {
      auto MTXI = ppc._rcfd->GetTarget()->MTXI();
      auto V    = MTXI->RefVMatrix();
      V.setTranslation(0, 0, 0);
      auto VP   = fmtx4::multiply_ltor(V, MTXI->RefPMatrix());
      ppc._fxi->bindParamMatrix(param, VP.inverse());
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
    auto parname = param->_name;
    //printf("FxPipeline<%p:%s> bindParamFloat<%s> val<%f>\n", (void*)this, _debugName.c_str(), parname.c_str(), val );
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
    auto name = param->_name;
    printf("bad type<%s> for param<%s>\n", val.typeName(), name.c_str() );
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
// GATE 0 NEGATIVE CONTROL 2 — "force the mono technique inside the stereo pass".
//
// A RUNTIME TECHNIQUE-SELECTION override, cache-honest by construction: it clears the
// _stereo permutation bit, so the cache hands back a genuinely different (mono) pipeline
// rather than a mislabelled one. The pass keeps its viewMask armed, so both views still
// render — they just render the same mono clip transform, and the two layers come back
// identical. That identical pair IS the control's required outcome: it proves the gate can
// see loss-of-parallax, rather than merely asserting that it would.
//
// ONE cached read, called from EVERY stereo-selection entry point, so no two sites can
// disagree about whether the control is armed — a control that is honored on one path and
// silently bypassed on the path the gate actually draws through is a false pass, which is
// the single failure mode this whole control exists to rule out.
//
// Default OFF; announces itself once when armed (a control nobody can prove was armed is
// not a control). Never referenced by production code paths.
///////////////////////////////////////////////////////////////////////////////

bool gate0ForceMonoTechnique() {
  static const bool _armed = []() -> bool {
    auto env = std::getenv("ORKID_GATE0_FORCE_MONO_TEK");
    bool on  = env and (std::string(env) == "1");
    if (on)
      printf("GATE0: ORKID_GATE0_FORCE_MONO_TEK=1 — _MO techniques FORCED inside stereo passes\n");
    return on;
  }();
  return _armed;
}

///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const RenderContextInstData& RCID) const {
  auto RCFD    = RCID.rcfd();
  auto context = RCFD->_target;
  auto fxi     = context->FXI();
  bool stereo  = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
  bool picking = RCFD->hasCPD() ? RCFD->topCPD().isPicking() : false;
  bool cookie  = RCFD->hasCPD() ? RCFD->topCPD()._sunCookiePass : false;
  // GATE 0 NC2 SITE 1 of 2 — invariant: EVERY stereo-selection entry the gate exercises
  //  honors this hook, and the gate bypasses none of them (see gate0ForceMonoTechnique).
  if (gate0ForceMonoTechnique())
    stereo = false;
  /////////////////
  FxPipelinePermutation permu;
  permu._stereo = stereo;

  permu._skinned          = RCID._isSkinned;
  permu._instanced        = RCID._isInstanced;
  permu._is_vertex_ssbo   = RCID._isSSBOSourced;
  permu._is_impostor      = RCID._isImpostor;
  permu._is_mesh_shader   = RCID._isMeshSourced;
  permu._forced_technique = RCID._forced_technique;
  permu._is_picking       = picking;
  permu._is_sun_cookie    = cookie;
  permu._rendering_model  = RCFD->_renderingmodel._modelID;
  // permu.dump();
  /////////////////
  return findPipeline(permu);
}
///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const FxPipelinePermutation& permu_in) const {
  // GATE 0 NC2 SITE 2 of 2 — same invariant as site 1. This overload is the entry a
  //  BELOW-THE-COMPOSITOR caller uses (it hands its own permutation rather than deriving
  //  one from frame data), so the hook has to bite here too or a gate drawing through it
  //  would report a control it never actually armed.
  FxPipelinePermutation permu = permu_in;
  if (gate0ForceMonoTechnique())
    permu._stereo = false;
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
}
///////////////////////////////////////////////////////////////////////////////
// E.6/2.12 gate — exercises the rebind-propagation core (stamp compare +
// _bound_params overlay) with fabricated param handles; no GPU needed. The
// full path (bindParam -> beginBlock -> uniform) is covered by the hypermesh
// paramsink demo/gates. Returns the failure count (0 = pass).
///////////////////////////////////////////////////////////////////////////////
int fxPipelineRebindSelfTest() {
  int fails  = 0;
  auto CHECK = [&](bool ok, const char* what) {
    printf("[fxpipeline rebind] %s %s\n", ok ? "PASS" : "FAIL", what);
    if (not ok)
      fails++;
  };
  auto floatIs = [](const varmap::VarMap::value_type& v, float x) -> bool {
    auto as_f = v.tryAs<float>();
    return as_f and (as_f.value() == x);
  };
  auto mtl  = std::make_shared<FreestyleMaterial>();
  auto parA = new FxShaderParam;
  auto parB = new FxShaderParam;
  parA->_name = "parA";
  parB->_name = "parB";
  // NB: explicitly the BASE bindParam (the deferred _bound_params contract this
  // test exercises — what pbrmaterial_ptr_t callers get). FreestyleMaterial
  // SHADOWS bindParam with its immediate-FXI direct-draw variant, which needs
  // an initialized shader target.
  auto bind = [&](FxShaderParam* p, float v) { mtl->GfxMaterial::bindParam(p, v); };

  bind(parA, 1.0f); // bound BEFORE the pipeline exists
  FxPipelinePermutation permu;
  auto pipe           = std::make_shared<FxPipeline>(permu);
  pipe->_material_ptr = mtl.get();

  pipe->_syncMaterialParams();
  CHECK(floatIs(pipe->_params[parA], 1.0f), "pre-creation bind overlays on first sync");

  bind(parA, 2.0f); // REBIND after the pipeline exists (the 2.12 bug)
  bind(parB, 3.0f); // and a brand-new param
  pipe->_syncMaterialParams();
  CHECK(floatIs(pipe->_params[parA], 2.0f), "rebind propagates to a cached pipeline");
  CHECK(floatIs(pipe->_params[parB], 3.0f), "new param propagates to a cached pipeline");

  pipe->_params.clear(); // clean-stamp path must NOT re-overlay (O(1) skip)
  pipe->_syncMaterialParams();
  CHECK(pipe->_params.empty(), "clean stamp skips the overlay");

  bind(parB, 4.0f); // any bind re-arms the overlay
  pipe->_syncMaterialParams();
  CHECK(floatIs(pipe->_params[parA], 2.0f) and floatIs(pipe->_params[parB], 4.0f), "stamp bump restores the full overlay");

  printf("=== fxpipeline rebind selftest %s (%d failures) ===\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
