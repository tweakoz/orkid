////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/math/collision_test.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork ::lev2 {
///////////////////////////////////////////////////////////////////////////////

LightProbe::LightProbe() {
}

LightProbe::~LightProbe() {
}

void LightProbe::resize(int dim) {
  _dim   = dim;
  _dirty = true;
}

///////////////////////////////////////////////////////////////////////////////

void LightProbe::exportEquirectangular(Context* ctx, const fquat& rot, const file::Path& path) {
  auto tex              = _cubeTexture;
  int w                 = 2048;
  int h                 = 1024;
  _equiRenderRTG        = std::make_shared<RtGroup>(ctx, w, h);
  _equiRenderRTG->_name = "ReflectionProbeRTG";
  auto colorbuf         = _equiRenderRTG->createRenderTarget(EBufferFormat::RGB8);
  colorbuf->_debugName  = "ReflectionProbeColorCubeMap";

  auto material = std::make_shared<FreestyleMaterial>();

  material->gpuInit(ctx, "orkshader://cube2equirectangular");

  material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  material->_rasterstate->_culltest  = ECullTest::OFF;
  material->_rasterstate->_depthtest = EDepthTest::OFF;

  auto tek_c2e = material->technique("tek_cube2equi");
  auto p_mrot   = material->param("mrot");
  auto p_cube  = material->param("cube_sampler");

  auto FBI = ctx->FBI();

  ctx->beginFrame();
  FBI->PushRtGroup(_equiRenderRTG.get());
  FBI->Clear(fvec4(1, 0, 0, 0), 1.0);

  fmtx3 mtxrot;
  mtxrot.fromQuaternion(rot);

  if (1) {
    auto RCFD = std::make_shared<RenderContextFrameData>(ctx);
    material->begin(tek_c2e, RCFD);


    material->bindParamMatrix(p_mrot, mtxrot);
    material->bindParamCTex(p_cube, _cubeTexture.get());
    //ctx->RSI()->BindRasterState(material->_rasterstate, true);
    ctx->GBI()->render2dQuadEML(); // full screen quad
    material->end(RCFD);
  }

  FBI->PopRtGroup();
  ctx->endFrame();

  FBI->capture(colorbuf.get(), path);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
