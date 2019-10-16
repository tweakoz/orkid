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
  static constexpr size_t KMAXLIGHTS = 1024;
  static constexpr int KMAXNUMTILESX         = 512;
  static constexpr int KMAXNUMTILESY         = 256;
  static constexpr int KMAXTILECOUNT         = KMAXNUMTILESX * KMAXNUMTILESY;
  static constexpr size_t KMAXLIGHTSPERCHUNK = 32768 / sizeof(fvec4);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  IMPL(DeferredCompositingNode* node)
      : _camname(AddPooledString("Camera"))
      , _context(node,KMAXLIGHTS)
      , _lighttiles(KMAXTILECOUNT)\
      , _lightbuffer(nullptr){
    
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~IMPL() {}
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::GfxTarget* target) {
    _context.gpuInit(target);
    if( nullptr == _lightbuffer ) {
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
  void _render(DeferredCompositingNode* node, CompositorDrawData& drawdata) {
    //_timer.Start();
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    //////////////////////////////////////////////////////
    _context.renderUpdate(drawdata);
    auto VD = _context.computeViewData(drawdata);
    _context.update(VD);
    _context._clearColor = node->_clearColor;
    //////////////////////////////////////////////////////////////////
    // clear lighttiles
    //////////////////////////////////////////////////////////////////
    int numtiles = _context._clusterW * _context._clusterH;
    for (int i = 0; i < numtiles; i++)
      _lighttiles[i].atomicOp([](pllist_t&item){ item.clear(); });
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
    auto FXI                     = framerenderer.framedata().GetTarget()->FXI();
    auto this_buf                = framerenderer.framedata().GetTarget()->FBI()->GetThisBuffer();
    /////////////////////////////////////////////////////////////////
    _context.beginPointLighting(drawdata,VD);
    FXI->bindParamBlockBuffer(_context._lightblock, _lightbuffer);
    /////////////////////////////////////
    // float time_tile_cpa = _timer.SecsSinceStart();
    // printf( "Deferred::_render tilecpa time<%g>\n", time_tile_cpa-time_tile_in );
    /////////////////////////////////////
    _lightjobcount = 0;
    _pendingtilecounter.store(0);
    auto depthclusterbase = (const uint32_t*)_context._clustercapture._data;
    for (int iy = 0; iy <= _context._clusterH; iy++) {
      for (int ix = 0; ix <= _context._clusterW; ix++) {
        auto job = [this, ix, iy, &depthclusterbase]() {
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
                while (depthclustersample != 0 and (false == overlapZ)) {
                  bool has_bit = (depthclustersample & 1);
                  if (has_bit) {
                    float bitshiftedLO = float(1 << bitindex);
                    float bitshiftedHI = bitshiftedLO + bitshiftedLO;
                    overlapZ |= doRangesOverlap(light->_minZ, light->_maxZ, bitshiftedLO, bitshiftedHI);
                  } // if (has_bit) {
                  depthclustersample >>= 1;
                  bitindex++;
                } // while(depthsample)
                if (overlapZ) {
                  int numlintile = 0;
                  _lighttiles[tileindex].atomicOp([&](pllist_t&item){
                      item.push_back(light);
                      numlintile = item.size();
                  });
                  if (numlintile==1) {
                    _pendingtiles[_pendingtilecounter.fetch_add(1)] = tileindex;
                  }
                } // if( overlapZ )
              }   // if( overlapY )
            }     // if( overlapX) {
          }       // for (size_t lightindex = 0; lightindex < _pointlights.size(); lightindex++) {
          _lightjobcount--;
        }; // job =
        int jobindex = _lightjobcount++;
        //job();
        ParallelOpQ().push(job);
      }         // for (int ix = 0; ix <= _clusterW; ix++) {
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
    size_t num_pending_tiles = _pendingtilecounter.load();
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
      auto mapping        = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
      int chunksize       = 0;
      size_t chunk_offset = 0;
      _chunktiles_pos.clear();
      _chunktiles_uva.clear();
      _chunktiles_uvb.clear();
      /////////////////////////////////////
      while (false == chunk_done) {
        int index              = _pendingtiles[actindex];
        _lighttiles[index].atomicOp([&](pllist_t& lightlist){
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
        if (_chunktiles_pos.size())
          this_buf->Render2dQuadsEML(_chunktiles_pos.size(),
                                     _chunktiles_pos.data(),
                                     _chunktiles_uva.data(),
                                     _chunktiles_uvb.data());
      }
      /////////////////////////////////////
      num_pending_tiles = _pendingtilecounter.load() - actindex;
      numchunks++;
      /////////////////////////////////////
    } // while (num_pending_tiles) {
    // float time_tile_out = _timer.SecsSinceStart();
    // printf( "Deferred::_render tiletime<%g>\n", time_tile_out-time_tile_in );
    // printf( "numchunks<%zu>\n", numchunks );
    /////////////////////////////////////
    _context.endPointLighting(drawdata,VD);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PoolString _camname;
  typedef std::vector<const PointLight*> pllist_t;
  typedef ork::LockedResource<pllist_t> locked_pllist_t;

  DeferredContext _context;
  int _sequence = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  ork::fixedvector<locked_pllist_t, KMAXTILECOUNT> _lighttiles;
  int  _pendingtiles[KMAXTILECOUNT];
  ork::fixedvector<int, KMAXTILECOUNT> _chunktiles;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_pos;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_uva;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_uvb;
  FxShaderParamBuffer* _lightbuffer = nullptr;
  std::atomic<int> _pendingtilecounter;
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
