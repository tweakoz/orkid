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

struct CubePrimitive {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context) {

    using namespace meshutil;

    submesh submeshQuads;
    submesh submeshTris;

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    submeshQuads.addQuad(
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec3(P, P, P),
        fvec3(N, P, P), //
        fvec2(0.0f, 0.0f),
        fvec2(0.25f, 0.5f),
        fvec2(0.0f, 0.0f),
        fvec2(0.25f, 0.5f),
        _colorFront);

    submeshQuads.addQuad(
        fvec3(P, N, P),
        fvec3(P, N, N),
        fvec3(P, P, N),
        fvec3(P, P, P), //
        fvec2(0.25f, 0.0f),
        fvec2(0.5f, 0.5f),
        fvec2(0.25f, 0.0f),
        fvec2(0.5f, 0.5f),
        _colorRight);

    submeshQuads.addQuad(
        fvec3(P, N, N),
        fvec3(N, N, N),
        fvec3(N, P, N),
        fvec3(P, P, N), //
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorBack);

    submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(N, N, P),
        fvec3(N, P, P),
        fvec3(N, P, N), //
        fvec2(0.75f, 0.0f),
        fvec2(1.0f, 0.5f),
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorLeft);

    submeshQuads.addQuad(
        fvec3(N, P, N),
        fvec3(N, P, P),
        fvec3(P, P, P),
        fvec3(P, P, N), //
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorTop);

    submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(P, N, N), //
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorBottom);

    submeshTriangulate(submeshQuads, submeshTris);

    _primitive.fromSubMesh(submeshTris, context);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void renderEML(Context* context) {
    _primitive.renderEML(context);
  }

  //////////////////////////////////////////////////////////////////////////////
  inline scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      fxpipeline_ptr_t material_inst) {
    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      material_inst->wrappedDrawCall(RCID, //
                                     [this, context]() { //
                                        this->renderEML(context); //
                                    });
    });
    return layer->createDrawableNode(named, drw);
  }
  //////////////////////////////////////////////////////////////////////////////

  float _size = 0.0f;

  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorFront;
  fvec4 _colorBack;
  fvec4 _colorLeft;
  fvec4 _colorRight;

  using rigidprim_t = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  rigidprim_t _primitive;
};
using cube_ptr_t = std::shared_ptr<CubePrimitive>;

} // namespace ork::lev2::primitives
