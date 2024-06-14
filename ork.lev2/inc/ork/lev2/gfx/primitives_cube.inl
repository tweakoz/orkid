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

    // Front face (already CCW)
    if (1)
      submeshQuads.addQuad(
          dvec3(N, N, P),
          dvec3(P, N, P),
          dvec3(P, P, P),
          dvec3(N, P, P),
          dvec2(0.0f, 0.0f),
          dvec2(1.0f, 0.0f),
          dvec2(1.0f, 1.0f),
          dvec2(0.0f, 1.0f),
          _colorFront);

    // Right face (already CCW)
    if (1)
      submeshQuads.addQuad(
          dvec3(P, N, P),
          dvec3(P, N, N),
          dvec3(P, P, N),
          dvec3(P, P, P),
          dvec2(0.0f, 0.0f),
          dvec2(1.0f, 0.0f),
          dvec2(1.0f, 1.0f),
          dvec2(0.0f, 1.0f),
          _colorRight);

    // Back face (needs to be reversed to be CCW)
    if (1)
      submeshQuads.addQuad(
          dvec3(P, N, N),
          dvec3(N, N, N),
          dvec3(N, P, N),
          dvec3(P, P, N),
          dvec2(1.0f, 0.0f),
          dvec2(0.0f, 0.0f),
          dvec2(0.0f, 1.0f),
          dvec2(1.0f, 1.0f),
          _colorBack);

    // Left face (needs to be reversed to be CCW)
    if (1)
      submeshQuads.addQuad(
          dvec3(N, P, N),
          dvec3(N, N, N),
          dvec3(N, N, P),
          dvec3(N, P, P),
          dvec2(1.0f, 0.0f),
          dvec2(0.0f, 0.0f),
          dvec2(0.0f, 1.0f),
          dvec2(1.0f, 1.0f),
          _colorLeft);

    // Top face (needs to be corrected for CCW)
    if (1)
      submeshQuads.addQuad(
          dvec3(N, P, P),
          dvec3(P, P, P),
          dvec3(P, P, N),
          dvec3(N, P, N),
          dvec2(0.0f, 1.0f),
          dvec2(1.0f, 1.0f),
          dvec2(1.0f, 0.0f),
          dvec2(0.0f, 0.0f),
          _colorTop);

    // Bottom face (needs to be corrected for CCW)
    if (1)
      submeshQuads.addQuad(
          dvec3(N, N, N),
          dvec3(P, N, N),
          dvec3(P, N, P),
          dvec3(N, N, P),
          dvec2(0.0f, 0.0f),
          dvec2(1.0f, 0.0f),
          dvec2(1.0f, 1.0f),
          dvec2(0.0f, 1.0f),
          _colorBottom);

    submeshTriangulate(submeshQuads, submeshTris);

    _primitive.fromSubMesh(submeshTris, context);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void renderEML(Context* context) {
    _primitive.renderEML(context);
  }

  //////////////////////////////////////////////////////////////////////////////
  inline drawable_ptr_t createDrawable(fxpipeline_ptr_t material_inst) {
    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      material_inst->wrappedDrawCall(
          RCID,                       //
          [this, context]() {         //
            this->renderEML(context); //
          });
    });
    return drw;
  }
  //////////////////////////////////////////////////////////////////////////////
  inline callback_drawabledata_ptr_t createDrawableData(fxpipeline_ptr_t material_inst) {
    auto drwdata = std::make_shared<CallbackDrawableData>();

    drwdata->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      material_inst->wrappedDrawCall(
          RCID,                       //
          [this, context]() {         //
            this->renderEML(context); //
          });
    });
    return drwdata;
  }
  //////////////////////////////////////////////////////////////////////////////
  inline scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      fxpipeline_ptr_t material_inst) {
    auto drw = createDrawable(material_inst);
    return layer->createDrawableNode(named, drw);
  }
  //////////////////////////////////////////////////////////////////////////////

  float _size = 0.0f;

  dvec4 _colorTop;
  dvec4 _colorBottom;
  dvec4 _colorFront;
  dvec4 _colorBack;
  dvec4 _colorLeft;
  dvec4 _colorRight;

  using rigidprim_t = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  rigidprim_t _primitive;
};
using cube_ptr_t = std::shared_ptr<CubePrimitive>;

} // namespace ork::lev2::primitives
