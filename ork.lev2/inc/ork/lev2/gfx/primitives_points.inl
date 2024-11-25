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

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////

template <typename VertexType>
struct PointsPrimitive {

  using vtx_t = VertexType;
  using vtx_buf_t = DynamicVertexBuffer<vtx_t>;

  //////////////////////////////////////////////////////////////////////////////

  inline PointsPrimitive(int numpoints){
    _numpoints = numpoints;
    _vertexBuffer = std::make_shared<vtx_buf_t>(numpoints,0);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline vtx_t* lock(Context* context) {
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
  fxpipeline_ptr_t _pipeline;
  std::shared_ptr<vtx_buf_t> _vertexBuffer;
};

///////////////////////////////////////////////////////////////////////////////

using points_v12c4_ptr_t = std::shared_ptr<PointsPrimitive<VtxV12C4>>;
using points_v12t8_ptr_t = std::shared_ptr<PointsPrimitive<VtxV12T8>>;

} // namespace ork::lev2::primitives
