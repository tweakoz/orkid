////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////

struct InstancedIndexedPrimitive {

  using instance_t         = SVtxVU32Inst;
  using instance_vb_t      = DynamicVertexBuffer<instance_t>;
  using instance_vb_ptr_t  = std::shared_ptr<instance_vb_t>;

  //////////////////////////////////////////////////////////////////////////////

  int _num_instances;
  int _capacity;
  PrimitiveType _prim_type;
  instance_vb_ptr_t _instance_vb;
  idxbufferbase_ptr_t _base_ib;
  fxpipeline_ptr_t _pipeline;
  instance_t* _locked_data = nullptr;
  //////////////////////////////////////////////////////////////////////////////

  inline InstancedIndexedPrimitive(
    Context* ctx,
    std::vector<uint16_t> baseIndices,
    PrimitiveType prim_type,
    int max_instances
  ) {
    _num_instances = max_instances;
    _prim_type = prim_type;
    _capacity  = max_instances;
    _instance_vb = std::make_shared<instance_vb_t>(max_instances, 0);

    auto gbi = ctx->GBI();

    // Create base index buffer from provided indices
    size_t base_count = baseIndices.size();
    _base_ib = std::make_shared<StaticIndexBuffer<uint16_t>>(base_count);
    auto dst_indices = (uint16_t*) ctx->GBI()->LockIB(*_base_ib, 0, base_count);

    memcpy(dst_indices, baseIndices.data(), base_count * sizeof(uint16_t));
    ctx->GBI()->UnLockIB(*_base_ib);

    // Create instance vertex buffer
    _instance_vb = std::make_shared<instance_vb_t>(max_instances, 0);
    auto dst_instances = (instance_t*) ctx->GBI()->LockVB(*_instance_vb, 0, max_instances);

    for(uint32_t i = 0; i < max_instances; i++) {
      dst_instances[i]._instanceId = i;
    }
    ctx->GBI()->UnLockVB(*_instance_vb);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline instance_t* lock(Context* context, int num_instances=0) {
    if(0==num_instances){
      _num_instances = _capacity;
      num_instances = _capacity;
    }
    else{
      _num_instances = num_instances;
      OrkAssert(num_instances<=_capacity);
    }
    _locked_data = (instance_t*) context->GBI()->LockVB(*_instance_vb, 0, _num_instances);
    return _locked_data;
  }

  inline void unlock(Context* context) {
    _locked_data = nullptr;
    context->GBI()->UnLockVB(*_instance_vb);
  }

  //////////////////////////////////////////////////////////////////////////////

  void renderEML(Context* ctx) {
    auto gbi = ctx->GBI();

    gbi->DrawInstancedIndexedPrimitiveEML(
      *_instance_vb,
      *_base_ib,
      _prim_type,
      _num_instances);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline scenegraph::drawable_node_ptr_t createNode(
    std::string named, //
    scenegraph::layer_ptr_t layer,
    fxpipeline_ptr_t pipeline
  ) {

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
};

//////////////////////////////////////////////////////////////////////////////

using instanced_indexed_primitive_ptr_t = std::shared_ptr<InstancedIndexedPrimitive>;

} // namespace ork::lev2::primitives