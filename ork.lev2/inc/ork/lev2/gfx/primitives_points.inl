////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/kernel/datablock.h>

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////
struct PointsData;
using pointsdata_ptr_t = std::shared_ptr<PointsData>;

struct PointsData {
  PointsData(datablock_ptr_t db, int num_points, EVtxStreamFormat format);
  datablock_ptr_t _datablock;
  int _num_points = 0;
  EVtxStreamFormat _format = EVtxStreamFormat::NONE;
  void transformInPlace(const fmtx4& mtx);
  pointsdata_ptr_t transformed(const fmtx4& mtx) const;
  pointsdata_ptr_t convertToV12C4(image_ptr_t image) const;
  pointsdata_ptr_t depthClamped(float min_depth, float max_depth) const;
};

//////////////////////////////////////////////////////////////////////////////

template <typename VertexType>
struct PointsPrimitive {

  using vtx_t = VertexType;
  using vtx_buf_t = DynamicVertexBuffer<vtx_t>;

  //////////////////////////////////////////////////////////////////////////////

  inline PointsPrimitive(int maxpoints){
    _numpoints = maxpoints;
    _capacity = maxpoints;
    _vertexBuffer = std::make_shared<vtx_buf_t>(maxpoints,0);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline vtx_t* lock(Context* context, int num_points=0) {
    if(0==num_points){
      _numpoints = _capacity;
      num_points = _capacity;
    }
    else{
      _numpoints = num_points;
      OrkAssert(num_points<=_capacity);
    }
    return (vtx_t*) context->GBI()->LockVB(*_vertexBuffer,0,_numpoints);
  }

  inline void unlock(Context* context) {
    context->GBI()->UnLockVB(*_vertexBuffer);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void renderEML(Context* context) {
    auto gbi = context->GBI();
    gbi->DrawPrimitiveEML(*_vertexBuffer, PrimitiveType::POINTS,0,_numpoints);
  }

  //////////////////////////////////////////////////////////////////////////////
  inline scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      fxpipeline_ptr_t pipeline) {

    OrkAssert(pipeline);

    _pipeline = pipeline;

    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      _pipeline->wrappedDrawCall(RCID, //
                                 [this, context]() { //
                                  this->renderEML(context); //
                                });
    });
    return layer->createDrawableNode(named, drw);
  }
  //////////////////////////////////////////////////////////////////////////////

  int _numpoints = 0;
  int _capacity = 0;
  fxpipeline_ptr_t _pipeline;
  std::shared_ptr<vtx_buf_t> _vertexBuffer;
};

//////////////////////////////////////////////////////////////////////////////

template <typename VertexType>
struct TiledPointsPrimitive {

  using vtx_t = VertexType;
  using vtx_buf_t = DynamicVertexBuffer<vtx_t>;
  using vtx_buf_ptr_t = std::shared_ptr<vtx_buf_t>;

  //////////////////////////////////////////////////////////////////////////////

  struct Tile {
    int _numpoints = 0;
    int _capacity = 0;
    int _version = -2;
    int _update_priority = 0;
    svarp_t _userdata;

    vtx_buf_ptr_t _vertexBuffer;
  };

  using tile_ptr_t = std::shared_ptr<Tile>;

  //////////////////////////////////////////////////////////////////////////////

  inline void renderEML(Context* context) {
    auto gbi = context->GBI();
    for( auto item : _tiles ){
      size_t num_points = item.second->_numpoints;
      auto vtxbuf = item.second->_vertexBuffer;
      gbi->DrawPrimitiveEML(*vtxbuf, PrimitiveType::POINTS,0,num_points);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  inline scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      fxpipeline_ptr_t pipeline) {

    OrkAssert(pipeline);

    _pipeline = pipeline;

    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      _pipeline->wrappedDrawCall(RCID, //
                                 [this, context]() { //
                                  this->renderEML(context); //
                                });
    });
    return layer->createDrawableNode(named, drw);
  }
  //////////////////////////////////////////////////////////////////////////////

  fxpipeline_ptr_t _pipeline;
  std::unordered_map<uint64_t,tile_ptr_t> _tiles;
  using tileptr_list = std::vector<tile_ptr_t>;
  int _max_tile_update_rate = 10;

};

///////////////////////////////////////////////////////////////////////////////

using tiled_points_v12c4_t = TiledPointsPrimitive<VtxV12C4>;
using tiled_points_v12c4_ptr_t = std::shared_ptr<tiled_points_v12c4_t>;
using points_v12c4_ptr_t = std::shared_ptr<PointsPrimitive<VtxV12C4>>;
using points_v12t8_ptr_t = std::shared_ptr<PointsPrimitive<VtxV12T8>>;

} // namespace ork::lev2::primitives
