////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>

ImplementReflectionX(ork::lev2::pbr::deferrednode::DeferredCompositingNode, "DeferredCompositingNode");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::describeX(class_t* c) {
  c->directProperty("ClearColor", &DeferredCompositingNode::_clearColor);
  c->directProperty("FogColor", &DeferredCompositingNode::_fogColor);
}
///////////////////////////////////////////////////////////////////////////////
struct CpuNodeImpl {
  static constexpr size_t KMAXLIGHTS         = 512;
  static constexpr int KMAXNUMTILESX         = 512;
  static constexpr int KMAXNUMTILESY         = 256;
  static constexpr int KMAXTILECOUNT         = KMAXNUMTILESX * KMAXNUMTILESY;
  static constexpr size_t KMAXLIGHTSPERCHUNK = 32768 / sizeof(fvec4);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  CpuNodeImpl(DeferredCompositingNode* node)
      : _node(node)
      , _context(node, "orkshader://deferred", KMAXLIGHTS)
      , _camname(AddPooledString("Camera"))
      , _lighttiles(KMAXTILECOUNT){
    for (int i = 0; i < KMAXTILECOUNT; i++)
      _lighttiles[i] = new locked_pllist_t;
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~CpuNodeImpl() {
    for (locked_pllist_t* item : _lighttiles) {
      delete item;
    }
    _lighttiles.clear();
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::Context* target) {
    _context.gpuInit(target);
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
  void _render(CompositorDrawData& drawdata) {
    //_timer.Start();
    auto targ                    = drawdata.context();
    //////////////////////////////////////////////////////
    _context.renderUpdate(_node, drawdata);
    auto VD = drawdata.computeViewData();
    _context.updateDebugLights(VD);
    _context._clearColor = _node->_clearColor;
    //////////////////////////////////////////////////////////////////
    // clear lighttiles
    //////////////////////////////////////////////////////////////////
    int numtiles = _context._clusterW * _context._clusterH;
    for (int i = 0; i < numtiles; i++)
      _lighttiles[i]->atomicOp([](pllist_t& item) { item.clear(); });
    /////////////////////////////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
    _context.renderGbuffer(_node, drawdata, VD);
    auto depthclusterbase = _context.captureDepthClusters(drawdata, VD);
    targ->debugPushGroup("Deferred::LightAccum");
    printf("WTF1\n");
    _context.renderBaseLighting(_node, drawdata, VD);
    this->renderPointLights(drawdata, VD);
    targ->debugPopGroup(); // "Deferred::LightAccum"
    targ->debugPopGroup(); // "Deferred::render"
    // float totaltime = _timer.SecsSinceStart();
    // printf( "Deferred::_render totaltime<%g>\n", totaltime );
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void renderPointLights(CompositorDrawData& drawdata, const ViewData& VD) {
    /////////////////////////////////////////////////////////////////
    auto context  = drawdata.context();
    auto FXI      = context->FXI();
    auto this_buf = context->FBI()->GetThisBuffer();
    /////////////////////////////////////////////////////////////////
    _context.beginPointLighting(_node, drawdata, VD, nullptr);
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
          int tileindex                     = iy * _context._clusterW + ix;
          const uint32_t depthclustersample = depthclusterbase[tileindex];
          for (size_t lightindex = 0; lightindex < _context._pointlights.size(); lightindex++) {
            auto light    = _context._pointlights[lightindex];
            bool overlapx = doRangesOverlap(ix, ix, light->_minX, light->_maxX);
            if (overlapx) {
              bool overlapy = doRangesOverlap(iy, iy, light->_minY, light->_maxY);
              if (overlapy) {
                uint32_t depthcluster = depthclustersample;
                uint32_t bitindex     = 0;
                bool overlapZ         = false;
                while (depthcluster != 0 and (false == overlapZ)) {
                  bool has_bit = (depthcluster & 1);
                  if (has_bit) {
                    float bitshiftedLO = float(1 << bitindex);
                    float bitshiftedHI = bitshiftedLO + bitshiftedLO;
                    overlapZ |= doRangesOverlap(light->_minZ, light->_maxZ, bitshiftedLO, bitshiftedHI);
                  } // if (has_bit) {
                  depthcluster >>= 1;
                  bitindex++;
                } // while(depthsample)
                if (overlapZ) {
                  int numlintile = 0;
                  _lighttiles[tileindex]->atomicOp([&](pllist_t& item) {
                    item.push_back(light);
                    numlintile = item.size();
                  });
                  if (numlintile == 1) {
                    _pendingtiles[_pendingtilecounter.fetch_add(1)] = tileindex;
                  }
                } // if( overlapZ )
              } // if( overlapY )
            } // if( overlapX) {
          } // for (size_t lightindex = 0; lightindex < _pointlights.size(); lightindex++) {
          _lightjobcount--;
        }; // job =
        int jobindex = _lightjobcount++;
        // job();
        opq::concurrentQueue()->enqueue(job);
      } // for (int ix = 0; ix <= _clusterW; ix++) {
    } // for (int iy = 0; iy <= _clusterH; iy++) {
    /////////////////////////////////////
    // float time_tile_cpb = _timer.SecsSinceStart();
    // printf( "Deferred::_render tilecpb time<%g>\n", time_tile_cpb-time_tile_cpa );
    /////////////////////////////////////
    while (_lightjobcount) {
      opq::concurrentQueue()->sync();
    }
    const float KTILESIZX    = 2.0f / float(_context._clusterW);
    const float KTILESIZY    = 2.0f / float(_context._clusterH);
    size_t num_pending_tiles = _pendingtilecounter.load();
    size_t actindex          = 0;
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
        int index = _pendingtiles[actindex];
        _lighttiles[index]->atomicOp([&](pllist_t& lightlist) {
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
      _context._lightingmtl->bindParamInt(_context._parNumLights, chunksize);
      _context._lightingmtl->commit();
      //////////////////////////////////////////////////
      // accumulate light for tile
      //////////////////////////////////////////////////
      if (VD._isStereo) {
        // float L = (float(ix) / float(_clusterW));
        // this_buf->Render2dQuadEML(fvec4(L - 1.0f, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
        // this_buf->Render2dQuadEML(fvec4(L, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
      } else {
        if (_chunktiles_pos.size())
          this_buf->Render2dQuadsEML(
              _chunktiles_pos.size(), _chunktiles_pos.data(), _chunktiles_uva.data(), _chunktiles_uvb.data());
      }
      /////////////////////////////////////
      num_pending_tiles = _pendingtilecounter.load() - actindex;
      numchunks++;
      /////////////////////////////////////
    } // while (num_pending_tiles) {
    // float time_tile_out = _timer.SecsSinceStart();
    // printf( "Deferred::_render tiletime<%g>\n", time_tile_out-time_tile_in );
    printf("numchunks<%zu>\n", numchunks);
    /////////////////////////////////////
    _context.endPointLighting(drawdata, VD);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  using pllist_t = std::vector<const PointLight*>;
  using locked_pllist_t = ork::LockedResource<pllist_t>;

  DeferredCompositingNode* _node = nullptr;
  DeferredContext _context;
  PoolString _camname;
  ork::fixedvector<locked_pllist_t*, KMAXTILECOUNT> _lighttiles;


  int _sequence = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  int _pendingtiles[KMAXTILECOUNT];
  ork::fixedvector<int, KMAXTILECOUNT> _chunktiles;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_pos;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_uva;
  ork::fixedvector<fvec4, KMAXTILECOUNT> _chunktiles_uvb;
  FxShaderParamBuffer* _lightbuffer = nullptr;
  std::atomic<int> _pendingtilecounter;
}; // CpuNodeImpl

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::DeferredCompositingNode() {
  _impl.makeShared<CpuNodeImpl>(this);
}
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNode::~DeferredCompositingNode() {
  _impl = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<CpuNodeImpl>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<CpuNodeImpl>>();
  impl->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t DeferredCompositingNode::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.get<std::shared_ptr<CpuNodeImpl>>()->_context._rtgs_laccum->fetch(_bufferKey)->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr::deferrednode
