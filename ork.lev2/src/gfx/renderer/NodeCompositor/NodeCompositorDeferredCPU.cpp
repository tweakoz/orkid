////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.inl>

#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::deferrednode::DeferredCompositingNode, "DeferredCompositingNode");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::describeX(class_t* c) {
  c->memberProperty("ClearColor", &DeferredCompositingNode::_clearColor);
  c->memberProperty("FogColor", &DeferredCompositingNode::_fogColor);
}
///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  IMPL(DeferredCompositingNode* node)
      : _camname(AddPooledString("Camera"))
      , _actlmutex("activelightsmutex")
      , _context(node) {}
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~IMPL() {}
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) { _context.gpuInit(pTARG); }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(DeferredCompositingNode* node, CompositorDrawData& drawdata) {
    //_timer.Start();
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto targ                    = RCFD.GetTarget();
    auto FBI                     = targ->FBI();
    auto FXI                     = targ->FXI();
    auto TXI                     = targ->TXI();
    auto RSI                     = targ->RSI();
    auto GBI                     = targ->GBI();
    auto& ddprops                = drawdata._properties;
    auto this_buf                = FBI->GetThisBuffer();
    //////////////////////////////////////////////////////
    _context.renderUpdate(drawdata);
    auto VD = _context.computeViewData(drawdata);
    _context.update(VD);
    _context._clearColor = node->_clearColor;
    /////////////////////////////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
      _context.renderGbuffer(drawdata, VD);
      auto depthclusterbase = _context.captureDepthClusters(drawdata, VD);
      targ->debugPushGroup("Deferred::LightAccum");
        _context.renderBaseLighting(drawdata, VD);
        this->renderPointLights(drawdata, VD);
      targ->debugPopGroup(); // "Deferred::LightAccum"
    targ->debugPopGroup(); // "Deferred::render"
    // float totaltime = _timer.SecsSinceStart();
    // printf( "Deferred::_render totaltime<%g>\n", totaltime );
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void renderPointLights(CompositorDrawData& drawdata,
                         const ViewData& VD){
    /////////////////////////////////////////////////////////////////
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto targ                    = RCFD.GetTarget();
    auto FBI                     = targ->FBI();
    auto FXI                     = targ->FXI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = targ->RSI();
    /////////////////////////////////////////////////////////////////
     targ->debugPushGroup("Deferred::PointLighting");
    CIMPL->pushCPD(_context._accumCPD);
    FBI->SetAutoClear(false);
    FBI->PushRtGroup(_context._rtgLaccum);
    targ->BeginFrame();
    _context._lightingmtl.bindTechnique(VD._isStereo ? _context._tekPointLightingStereo : _context._tekPointLighting);
    _context._lightingmtl.begin(RCFD);
    //////////////////////////////////////////////////////
    FXI->bindParamBlockBuffer(_context._lightblock, _context._lightbuffer);
    //////////////////////////////////////////////////////
    _context._lightingmtl.bindParamMatrixArray(_context._parMatIVPArray, VD._ivp, 2);
    _context._lightingmtl.bindParamMatrixArray(_context._parMatVArray, VD._v, 2);
    _context._lightingmtl.bindParamMatrixArray(_context._parMatPArray, VD._p, 2);
    _context._lightingmtl.bindParamCTex(_context._parMapGBufAlbAo, _context._rtgGbuffer->GetMrt(0)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapGBufNrmL, _context._rtgGbuffer->GetMrt(1)->GetTexture());
    _context._lightingmtl.bindParamCTex(_context._parMapDepth, _context._rtgGbuffer->_depthTexture);
    _context._lightingmtl.bindParamCTex(_context._parMapDepthCluster, _context._rtgDepthCluster->GetMrt(0)->GetTexture());
    _context._lightingmtl.bindParamVec2(_context._parNearFar, fvec2(DeferredContext::KNEAR, DeferredContext::KFAR));
    _context._lightingmtl.bindParamVec2(_context._parZndc2eye, VD._zndc2eye);
    _context._lightingmtl.bindParamVec2(_context._parInvViewSize,
                                        fvec2(1.0 / float(_context._width), 1.0f / float(_context._height)));
    //////////////////////////////////////////////////
    _context._lightingmtl.mRasterState.SetCullTest(ECULLTEST_OFF);
    _context._lightingmtl.mRasterState.SetBlending(EBLENDING_ADDITIVE);
    //_context._lightingmtl.mRasterState.SetBlending(EBLENDING_OFF);
    _context._lightingmtl.mRasterState.SetDepthTest(EDEPTHTEST_OFF);
    RSI->BindRasterState(_context._lightingmtl.mRasterState);
    constexpr size_t KPOSPASE = DeferredContext::KMAXLIGHTSPERCHUNK * sizeof(fvec4);
    /////////////////////////////////////
    // float time_tile_cpa = _timer.SecsSinceStart();
    // printf( "Deferred::_render tilecpa time<%g>\n", time_tile_cpa-time_tile_in );
    /////////////////////////////////////
    _lightjobcount = 0;
    size_t numltiles = 0;
    auto depthclusterbase = (const uint32_t*)_context._clustercapture._data;
    for (int iy = 0; iy <= _context._clusterH; iy++) {
      auto job = [this, iy, &depthclusterbase, &numltiles]() {
        for (int ix = 0; ix <= _context._clusterW; ix++) {
          int tileindex = iy * _context._clusterW + ix;
          for (size_t lightindex = 0; lightindex < _context._pointlights.size(); lightindex++) {
            auto light    = _context._pointlights[lightindex];
            bool overlapx = doRangesOverlap(ix, ix, light->_minX, light->_maxX);
            if (overlapx) {
              bool overlapy = doRangesOverlap(iy, iy, light->_minY, light->_maxY);
              if (overlapy) {
                uint32_t depthclustersample = depthclusterbase[tileindex];
                uint32_t bitindex    = 0;
                bool overlapZ        = false;
                int highest_bit      = 0;
                while (depthclustersample != 0 and (false == overlapZ)) {
                  bool has_bit = (depthclustersample & 1);
                  if (has_bit) {
                    float bitshiftedLO = float(1 << bitindex);
                    float bitshiftedHI = bitshiftedLO + bitshiftedLO;
                    overlapZ |= doRangesOverlap(light->_minZ, light->_maxZ, bitshiftedLO, bitshiftedHI);
                    highest_bit = bitindex;
                  } // if (have_bit) {
                  depthclustersample >>= 1;
                  bitindex++;
                } // while(depthsample)
                if (overlapZ) {
                  _actlmutex.Lock();
                  auto& lt = _context._lighttiles[tileindex];
                  lt.push_back(light);
                  if (lt.size() == 1)
                    _context._pendingtiles.push_back(tileindex);
                  _actlmutex.UnLock();
                  numltiles++;
                } // if( overlapZ )
              }   // if( overlapY )
            }     // if( overlapX) {
          }       // for (size_t lightindex = 0; lightindex < _pointlights.size(); lightindex++) {
        }         // for (int ix = 0; ix <= _clusterW; ix++) {
        _lightjobcount--;
      }; // job =
      int jobindex = _lightjobcount++;
      // job();
      ParallelOpQ().push(job);
    } // for (int iy = 0; iy <= _clusterH; iy++) {
    /////////////////////////////////////
    // float time_tile_cpb = _timer.SecsSinceStart();
    // printf( "Deferred::_render tilecpb time<%g>\n", time_tile_cpb-time_tile_cpa );
    /////////////////////////////////////
    while (_lightjobcount) {
      ParallelOpQ().sync();
    }
    const float KTILESIZX = 2.0f / float(_context._clusterW);
    const float KTILESIZY = 2.0f / float(_context._clusterH);
    size_t num_pending_tiles = _context._pendingtiles.size();
    size_t actindex  = 0;
    /////////////////////////////////////
    // float time_tile_cpc = _timer.SecsSinceStart();
    // printf( "Deferred::_render tilecpc time<%g>\n", time_tile_cpc-time_tile_cpb );
    /////////////////////////////////////
    size_t numchunks = 0;
    size_t lidxbase  = 0;
    /////////////////////////////////////
    while (num_pending_tiles) {
      /////////////////////////////////////
      // process a chunk
      /////////////////////////////////////
      bool chunk_done     = false;
      auto mapping        = FXI->mapParamBuffer(_context._lightbuffer, 0, 65536);
      int chunksize       = 0;
      size_t chunk_offset = 0;
      _context._chunktiles_pos.clear();
      _context._chunktiles_uva.clear();
      _context._chunktiles_uvb.clear();
      /////////////////////////////////////
      while (false == chunk_done) {
        int index              = _context._pendingtiles[actindex];
        auto& lightlist        = _context._lighttiles[index];
        int iy                 = index / _context._clusterW;
        int ix                 = index % _context._clusterW;
        float T                = float(iy) * KTILESIZY - 1.0f;
        float L                = float(ix) * KTILESIZX - 1.0f;
        size_t numlightsintile = lightlist.size();
        size_t remainingintile = numlightsintile - lidxbase;
        size_t countthisiter   = remainingintile;
        /////////////////////////////////////////////////////////
        // clamp number of lights to that which will
        //  fit into the current chunk
        /////////////////////////////////////////////////////////
        if ((countthisiter + chunksize) > DeferredContext::KMAXLIGHTSPERCHUNK) {
          countthisiter = DeferredContext::KMAXLIGHTSPERCHUNK - chunksize;
        }
        /////////////////////////////////////////////////////////
        // add item to current chunk
        /////////////////////////////////////////////////////////
        _context._chunktiles_pos.push_back(fvec4(L, T, KTILESIZX, KTILESIZY));
        _context._chunktiles_uva.push_back(fvec4(0, 0, 1, 1));
        _context._chunktiles_uvb.push_back(fvec4(chunksize, countthisiter, 0, 0));
        /////////////////////////////////////////////////////////
        for (size_t lidx = 0; lidx < countthisiter; lidx++) {
          const auto light = lightlist[lidxbase + lidx];
          /////////////////////////////////////////////////////////
          // embed chunk's lights into lighting UBO
          /////////////////////////////////////////////////////////
          mapping->ref<fvec4>(chunk_offset)            = fvec4(light->_color, light->dist2cam);
          mapping->ref<fvec4>(KPOSPASE + chunk_offset) = fvec4(light->_pos, light->_radius);
          chunk_offset += sizeof(fvec4);
        }
        /////////////////////////////////////////////////////////
        chunksize += countthisiter;
        chunk_done = chunksize >= DeferredContext::KMAXLIGHTSPERCHUNK;
        /////////////////////////////////////////////////////////
        // advance tile ?
        /////////////////////////////////////////////////////////
        lidxbase += countthisiter;
        if (lidxbase == lightlist.size()) {
          actindex++;
          lidxbase = 0;
          chunk_done |= (actindex >= _context._pendingtiles.size());
        }
      }
      /////////////////////////////////////
      // chunk ready, fire it off..
      /////////////////////////////////////
      FXI->unmapParamBuffer(mapping.get());
      //////////////////////////////////////////////////
      // set number of lights for tile
      //////////////////////////////////////////////////
      _context._lightingmtl.bindParamInt(_context._parNumLights, chunksize);
      _context._lightingmtl.commit();
      //////////////////////////////////////////////////
      // accumulate light for tile
      //////////////////////////////////////////////////
      if (VD._isStereo) {
        // float L = (float(ix) / float(_clusterW));
        // this_buf->Render2dQuadEML(fvec4(L - 1.0f, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
        // this_buf->Render2dQuadEML(fvec4(L, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
      } else {
        if (_context._chunktiles_pos.size())
          this_buf->Render2dQuadsEML(_context._chunktiles_pos.size(),
                                     _context._chunktiles_pos.data(),
                                     _context._chunktiles_uva.data(),
                                     _context._chunktiles_uvb.data());
      }
      /////////////////////////////////////
      num_pending_tiles = _context._pendingtiles.size() - actindex;
      numchunks++;
      /////////////////////////////////////
    } // while (num_pending_tiles) {
    // float time_tile_out = _timer.SecsSinceStart();
    // printf( "Deferred::_render tiletime<%g>\n", time_tile_out-time_tile_in );
    // printf( "numchunks<%zu>\n", numchunks );
    /////////////////////////////////////
    _context._lightingmtl.end(RCFD);
    CIMPL->popCPD();
    targ->debugPopGroup(); // PointLighting
    targ->EndFrame();
    FBI->PopRtGroup();     // deferredRtg
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PoolString _camname;

  DeferredContext _context;
  int _sequence = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  ork::mutex _actlmutex;
}; // IMPL

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::DeferredCompositingNode() { _impl = std::make_shared<IMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::~DeferredCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) { _impl.Get<std::shared_ptr<IMPL>>()->init(pTARG); }
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* DeferredCompositingNode::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.Get<std::shared_ptr<IMPL>>()->_context._rtgLaccum->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
