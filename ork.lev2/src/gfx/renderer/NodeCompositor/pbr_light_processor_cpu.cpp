////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/datacache.h>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
//#include <ork/gfx/image.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_cpu.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr::deferrednode {
////////////////////////////////////////////////////////////////

CpuLightProcessor::CpuLightProcessor(DeferredContext& defctx, DeferredCompositingNodePbr* compnode)
    : _lighttiles(KMAXTILECOUNT)
    , _deferredContext(defctx)
    , _defcompnode(compnode) {
}
void CpuLightProcessor::_gpuInit(lev2::Context* target) {
  if (nullptr == _lightbuffer) {
    _lightbuffer = target->FXI()->createParamBuffer(65536);
    auto mapped  = target->FXI()->mapParamBuffer(_lightbuffer);
    size_t base  = 0;
    for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
      mapped->ref<fvec3>(base + i * sizeof(fvec4)) = fvec3(0, 0, 0);
    base += KMAXLIGHTSPERCHUNK * sizeof(fvec4);
    for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
      mapped->ref<fvec4>(base + i * sizeof(fvec4)) = fvec4();
    mapped->unmap();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CpuLightProcessor::_clearFrameLighting() {
  _pointlights.clear();
  int numtiles = _deferredContext._clusterW * _deferredContext._clusterH;
  for (int i = 0; i < numtiles; i++)
    _lighttiles[i].atomicOp([](pllist_t& item) { item.clear(); });
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CpuLightProcessor::render(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights) {
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto context                 = RCFD.GetTarget();
  _gpuInit(context);
  _clearFrameLighting();
  _depthClusterBase = _deferredContext.captureDepthClusters(drawdata, VD);
  _renderUnshadowedUnTexturedPointLights(drawdata, VD, enumlights);
  _depthClusterBase = nullptr;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CpuLightProcessor::_renderUnshadowedUnTexturedPointLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    enumeratedlights_constptr_t enumlights) {
  bool is_stereo = VD._isStereo;
  /////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  const auto& RCFD             = framerenderer.framedata();
  auto gfxctx                  = RCFD.GetTarget();
  auto FXI                     = gfxctx->FXI();
  //auto RSI                     = gfxctx->RSI();
  auto this_buf                = gfxctx->FBI()->GetThisBuffer();

  /////////////////////////////////////
  // convert enumerated scenelights to deferred format
  /////////////////////////////////////

  const auto& scene_lights = enumlights->_alllights;

  const int KTILEMAXX = _deferredContext._clusterW - 1;
  const int KTILEMAXY = _deferredContext._clusterH - 1;

  pbr::PointLight deferred_pointlight;

  for (auto l : scene_lights) {

    if (l->isShadowCaster())
      continue;

    fvec3 color = l->color()*l->intensity();

    if (auto as_point = dynamic_cast<lev2::PointLight*>(l)) {
      float radius = as_point->radius();
      fvec3 pos    = as_point->worldPosition();
      float faloff = as_point->falloff();

      deferred_pointlight._radius = radius;
      deferred_pointlight._pos    = as_point->worldPosition();
      deferred_pointlight._color  = color;

      Sphere sph(pos, radius);
      deferred_pointlight._aabox   = sph.projectedBounds(VD.VPL);
      const auto& boxmin           = deferred_pointlight._aabox.Min();
      const auto& boxmax           = deferred_pointlight._aabox.Max();
      deferred_pointlight._aamin   = ((boxmin + fvec3(1, 1, 1)) * 0.5);
      deferred_pointlight._aamax   = ((boxmax + fvec3(1, 1, 1)) * 0.5);
      deferred_pointlight._minX    = int(floor(deferred_pointlight._aamin.x * KTILEMAXX));
      deferred_pointlight._maxX    = int(ceil(deferred_pointlight._aamax.x * KTILEMAXX));
      deferred_pointlight._minY    = int(floor(deferred_pointlight._aamin.y * KTILEMAXY));
      deferred_pointlight._maxY    = int(ceil(deferred_pointlight._aamax.y * KTILEMAXY));
      deferred_pointlight.dist2cam = (deferred_pointlight._pos - VD._camposmono).magnitude();
      deferred_pointlight._minZ    = deferred_pointlight.dist2cam -
                                  deferred_pointlight._radius; // Zndc2eye.x / (deferred_pointlight._aabox.Min().z - Zndc2eye.y);
      deferred_pointlight._maxZ =
          deferred_pointlight.dist2cam + deferred_pointlight._radius; // Zndc2eye.x / (pl->_aabox.Max().z - Zndc2eye.y);

      _pointlights.push_back(deferred_pointlight);
    }
  }

  /////////////////////////////////////
  // light culling
  /////////////////////////////////////

  _lightjobcount = 0;
  _pendingtilecounter.store(0);
  for (int iy = 0; iy <= _deferredContext._clusterH; iy++) {
    for (int ix = 0; ix <= _deferredContext._clusterW; ix++) {
      auto job = [this, ix, iy]() {
        int tileindex                     = iy * _deferredContext._clusterW + ix;
        const uint32_t depthclustersample = _depthClusterBase[tileindex];
        for (size_t lightindex = 0; lightindex < _pointlights.size(); lightindex++) {
          auto light    = _pointlights[lightindex];
          bool overlapx = doRangesOverlap(ix, ix, light._minX, light._maxX);
          if (overlapx) {
            bool overlapy = doRangesOverlap(iy, iy, light._minY, light._maxY);
            if (overlapy) {
              uint32_t depthcluster = depthclustersample;
              uint32_t bitindex     = 0;
              bool overlapZ         = false;
              while (depthcluster != 0 and (false == overlapZ)) {
                bool has_bit = (depthcluster & 1);
                if (has_bit) {
                  float bitshiftedLO = float(1 << bitindex);
                  float bitshiftedHI = bitshiftedLO + bitshiftedLO;
                  overlapZ |= doRangesOverlap(light._minZ, light._maxZ, bitshiftedLO, bitshiftedHI);
                } // if (has_bit) {
                depthcluster >>= 1;
                bitindex++;
              } // while(depthsample)
              if (overlapZ) {
                int numlintile = 0;
                _lighttiles[tileindex].atomicOp([&](pllist_t& item) {
                  item.push_back(light);
                  numlintile = item.size();
                });
                if (numlintile == 1) {
                  _pendingtiles[_pendingtilecounter.fetch_add(1)] = tileindex;
                }
              } // if( overlapZ )
            }   // if( overlapY )
          }     // if( overlapX) {
        }       // for (size_t lightindex = 0; lightindex < _pointlights.size(); lightindex++) {
        _lightjobcount--;
      }; // job =
      int jobindex = _lightjobcount++;
      // job();
      opq::concurrentQueue()->enqueue(job);
    } // for (int ix = 0; ix <= _clusterW; ix++) {
  }   // for (int iy = 0; iy <= _clusterH; iy++) {

  /////////////////////////////////////
  // synchronize with lighting tile computation
  /////////////////////////////////////

  while (_lightjobcount) {
    opq::concurrentQueue()->sync();
  }

  /////////////////////////////////////
  // render culled pointlights
  /////////////////////////////////////

  const float KTILESIZX    = 2.0f / float(_deferredContext._clusterW);
  const float KTILESIZY    = 2.0f / float(_deferredContext._clusterH);
  size_t num_pending_tiles = _pendingtilecounter.load();
  size_t actindex          = 0;
  /////////////////////////////////////
  // float time_tile_cpc = _timer.SecsSinceStart();
  // printf( "Deferred::_render tilecpc time<%g>\n", time_tile_cpc-time_tile_cpb );
  /////////////////////////////////////
  size_t numchunks = 0;
  size_t lidxbase  = 0;
  /////////////////////////////////////
  auto& lightmtl = _deferredContext._lightingmtl;
  _deferredContext.beginPointLighting(_defcompnode,drawdata, VD, nullptr);
  FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
  while (num_pending_tiles) {
    /////////////////////////////////////
    // process a chunk
    /////////////////////////////////////
    bool chunk_done     = false;
    auto mapping        = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
    int chunksize       = 0;
    size_t chunk_offset = 0;
    _chunktiles_pos.clear();
    _chunktiles_uva.clear();
    _chunktiles_uvb.clear();
    /////////////////////////////////////
    while (false == chunk_done) {
      int index = _pendingtiles[actindex];
      _lighttiles[index].atomicOp([&](pllist_t& lightlist) {
        int iy                 = index / _deferredContext._clusterW;
        int ix                 = index % _deferredContext._clusterW;
        float T                = float(iy) * KTILESIZY - 1.0f;
        float L                = float(ix) * KTILESIZX - 1.0f;
        size_t numlightsintile = lightlist.size();
        size_t remainingintile = numlightsintile - lidxbase;
        size_t countthisiter   = remainingintile;
        /////////////////////////////////////////////////////////
        // clamp number of lights to that which will
        //  fit into the current chunk
        /////////////////////////////////////////////////////////
        if ((countthisiter + chunksize) > KMAXLIGHTSPERCHUNK) {
          countthisiter = KMAXLIGHTSPERCHUNK - chunksize;
        }
        /////////////////////////////////////////////////////////
        // add item to current chunk
        /////////////////////////////////////////////////////////
        _chunktiles_pos.push_back(fvec4(L, T, KTILESIZX, KTILESIZY));
        _chunktiles_uva.push_back(fvec4(0, 0, 1, 1));
        _chunktiles_uvb.push_back(fvec4(chunksize, countthisiter, 0, 0));
        /////////////////////////////////////////////////////////
        constexpr size_t KPOSPASE = KMAXLIGHTSPERCHUNK * sizeof(fvec4);
        for (size_t lidx = 0; lidx < countthisiter; lidx++) {
          const auto& light = lightlist[lidxbase + lidx];
          /////////////////////////////////////////////////////////
          // embed chunk's lights into lighting UBO
          /////////////////////////////////////////////////////////
          mapping->ref<fvec4>(chunk_offset)            = fvec4(light._color, light.dist2cam);
          mapping->ref<fvec4>(KPOSPASE + chunk_offset) = fvec4(light._pos, light._radius);
          chunk_offset += sizeof(fvec4);
          // printf("tile<%d %d,%d> light_color<%g %g %g>\n", index, ix, iy, light._color.x, light._color.y, light._color.z);
        }
        /////////////////////////////////////////////////////////
        chunksize += countthisiter;
        chunk_done = chunksize >= KMAXLIGHTSPERCHUNK;
        /////////////////////////////////////////////////////////
        // advance tile ?
        /////////////////////////////////////////////////////////
        lidxbase += countthisiter;
        if (lidxbase == lightlist.size()) {
          actindex++;
          lidxbase = 0;
          chunk_done |= (actindex >= _pendingtilecounter.load());
        }
      }); // _lighttiles[index].atomicOp
    }
    /////////////////////////////////////
    // chunk ready, fire it off..
    /////////////////////////////////////
    FXI->unmapParamBuffer(mapping.get());
    //////////////////////////////////////////////////
    // set number of lights for tile
    //////////////////////////////////////////////////
    _deferredContext._lightingmtl->bindParamInt(_deferredContext._parNumLights, chunksize);
    _deferredContext._lightingmtl->commit();
    //////////////////////////////////////////////////
    // accumulate light for tile
    //////////////////////////////////////////////////

     printf("numlighttiles<%zu>\n", _chunktiles_pos.size());
    if (VD._isStereo) {
      // float L = (float(ix) / float(_clusterW));
      // this_buf->Render2dQuadEML(fvec4(L - 1.0f, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
      // this_buf->Render2dQuadEML(fvec4(L, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
    } else {
      if (_chunktiles_pos.size())
        this_buf->Render2dQuadsEML(_chunktiles_pos.size(), _chunktiles_pos.data(), _chunktiles_uva.data(), _chunktiles_uvb.data());
    }
    /////////////////////////////////////
    num_pending_tiles = _pendingtilecounter.load() - actindex;
    numchunks++;
    /////////////////////////////////////
  } // while (num_pending_tiles) {
  _deferredContext.endPointLighting(drawdata, VD);
}
/////////////////////////////////////
} // namespace ork::lev2::deferrednode
