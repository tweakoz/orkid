////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/sgnode_cursor.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::CursorDrawableData, "CursorDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

// Simple solid color shader for 3D cursor rendering
// Uses SVtxV16T16C16 vertex format (position, texcoord, color)
static const char* CURSOR_SHADER = R"(
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_set ublock {
  mat4 mvp;
  vec4 modcolor;
}
vertex_interface iface_vtx : ublock {
  inputs {
    vec4 position : POSITION;
    vec2 uv : TEXCOORD0;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    vec4 frg_clr;
  }
}
fragment_interface iface_frg : ublock {
  inputs {
    vec4 frg_clr;
  }
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
vertex_shader vs_cursor : iface_vtx {
  gl_Position = mvp * position;
  frg_clr = vtxcolor * modcolor;
}
fragment_shader fs_cursor : iface_frg {
  out_color = frg_clr;
}
state_block sb_cursor : default {
  CullTest = OFF;
}
technique tek_cursor {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader = vs_cursor;
    fragment_shader = fs_cursor;
    state_block = sb_cursor;
  }
}
)";

///////////////////////////////////////////////////////////////////////////////

CursorDrawableImpl::CursorDrawableImpl(const CursorDrawableData* data)
    : _data(data) {
}

CursorDrawableImpl::~CursorDrawableImpl() {
}

void CursorDrawableImpl::gpuInit(lev2::Context* ctx) {
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInitFromShaderText(ctx, "cursor_shader", CURSOR_SHADER);
  _technique = _material->technique("tek_cursor");
  _paramMVP = _material->param("mvp");
  _paramModColor = _material->param("modcolor");
  _material->_rasterstate->setBlendingMacro(BlendingMacro::ALPHA);
  _material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _initted = true;
}

void CursorDrawableImpl::_render(const RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto context = RCID.context();

  if (not _initted) {
    gpuInit(context);
  }

  float cursorNdcX = _cursorX;
  float cursorNdcY = _cursorY;

  // If autopos, get cursor position from context base (works with GLFW, DRM, etc.)
  if (_data->_autopos) {
    auto ctxbase = context->mCtxBase;
    if (ctxbase && ctxbase->fsMouseMode()) {
      auto uiev = ctxbase->uievent();
      int w = context->mainSurfaceWidth();
      int h = context->mainSurfaceHeight();
      cursorNdcX = (2.0f * uiev->miX / float(w)) - 1.0f;
      cursorNdcY = (2.0f * uiev->miY / float(h)) - 1.0f;
      //printf("Cursor NDC: (%f, %f)\n", cursorNdcX, cursorNdcY);
    } else {
      // Not in fullscreen mouse mode, don't render
      return;
    }
  }

  auto RCFD = RCID.rcfd();
  auto try_cdd = RCFD->getUserProperty("cdd"_crc).tryAs<std::shared_ptr<CompositorDrawData>>();
  auto cdd = try_cdd.value_or(nullptr);
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();

  // P_unrotated: projection without stereo rotation (for NDC->view conversion)
  // V_center: center eye view matrix (for view->world conversion)
  // These ensure cursor position is computed from the center viewpoint
  fmtx4 P_unrotated = cmtcs->_pmatrix;
  fmtx4 V_center = cmtcs->_vmatrix;
  fmtx4 P_rotated = cmtcs->_pmatrix;
  if(cdd){
    auto dcc = cdd->_properties["defcammtx"_crcu].get<cameramatrices_ptr_t>();
    auto ccc = cdd->_properties["centercam"_crcu].get<cameramatrices_ptr_t>();
    // Use center camera for both P and V - it has no stereo rotation
    P_unrotated = ccc->_pmatrix;
    V_center = ccc->_vmatrix;
    P_rotated = dcc->_pmatrix;
  }

  // Get camera matrices
  fmtx4 invV = V_center.inverse();

  // Convert NDC cursor position to view space at fixed depth
  // Using unrotated projection so cursor appears at correct screen position
  float viewZ = -_data->_depth;
  float x_view = cursorNdcX * (-viewZ) / P_unrotated.elemXY(0, 0);
  float y_view = cursorNdcY * (-viewZ) / P_unrotated.elemXY(1, 1);

  // Transform view-space cursor position to world space
  fvec3 cursorViewPos(x_view, y_view, viewZ);
  fvec3 cursorWorldPos = cursorViewPos.transform(invV);

  // Get camera right/up vectors from inverse view matrix for billboard orientation
  fvec3 camRight(invV.elemXY(0, 0), invV.elemXY(0, 1), invV.elemXY(0, 2));
  fvec3 camUp(invV.elemXY(1, 0), invV.elemXY(1, 1), invV.elemXY(1, 2));

  float cursorSize = _data->_size;
  float cursorThickness = _data->_thickness;

  //printf("_depth: (%f)\n", _data->_depth);
  //printf("Cursor World Pos: (%f, %f, %f)\n", cursorWorldPos.x, cursorWorldPos.y, cursorWorldPos.z);
  // Build vertex data in world space (billboard facing camera)
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;
  vw.Lock(context, &VB, 12);  // 2 quads * 6 verts each

  fvec4 white(1, 1, 1, 1);
  fvec4 uv(0, 0, 0, 0);  // unused

  // Horizontal bar quad (2 triangles) - in world space
  fvec3 hv0 = cursorWorldPos + camRight * (-cursorSize) + camUp * (-cursorThickness / 2);
  fvec3 hv1 = cursorWorldPos + camRight * (cursorSize) + camUp * (-cursorThickness / 2);
  fvec3 hv2 = cursorWorldPos + camRight * (cursorSize) + camUp * (cursorThickness / 2);
  fvec3 hv3 = cursorWorldPos + camRight * (-cursorSize) + camUp * (cursorThickness / 2);
  vw.AddVertex(vtx_t(hv0, uv, white));
  vw.AddVertex(vtx_t(hv2, uv, white));
  vw.AddVertex(vtx_t(hv1, uv, white));
  vw.AddVertex(vtx_t(hv0, uv, white));
  vw.AddVertex(vtx_t(hv3, uv, white));
  vw.AddVertex(vtx_t(hv2, uv, white));

  // Vertical bar quad (2 triangles) - in world space
  fvec3 vv0 = cursorWorldPos + camRight * (-cursorThickness / 2) + camUp * (-cursorSize);
  fvec3 vv1 = cursorWorldPos + camRight * (cursorThickness / 2) + camUp * (-cursorSize);
  fvec3 vv2 = cursorWorldPos + camRight * (cursorThickness / 2) + camUp * (cursorSize);
  fvec3 vv3 = cursorWorldPos + camRight * (-cursorThickness / 2) + camUp * (cursorSize);
  vw.AddVertex(vtx_t(vv0, uv, white));
  vw.AddVertex(vtx_t(vv2, uv, white));
  vw.AddVertex(vtx_t(vv1, uv, white));
  vw.AddVertex(vtx_t(vv0, uv, white));
  vw.AddVertex(vtx_t(vv3, uv, white));
  vw.AddVertex(vtx_t(vv2, uv, white));

  vw.UnLock(context);

  // Draw with freestyle material - use full VP since verts are in world space
  auto fxi = context->FXI();
  auto gbi = context->GBI();
  fmtx4 VP = P_rotated * V_center;

  fxi->pushRasterState(_material->_rasterstate);
  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, VP);
  _material->bindParamVec4(_paramModColor, _data->_color);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
  fxi->popRasterState();
}

///////////////////////////////////////////////////////////////////////////////

void CursorDrawableImpl::renderCursor(RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto drawable = renderable->_drawable;
  drawable->_implA.getShared<CursorDrawableImpl>()->_render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void CursorDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t CursorDrawableData::createDrawable() const {
  auto drw = std::make_shared<CallbackDrawable>(nullptr);
  auto impl = drw->_implA.makeShared<CursorDrawableImpl>(this);
  drw->_sortkey = 1000;  // render late (on top)
  drw->SetRenderCallback(CursorDrawableImpl::renderCursor);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

CursorDrawableData::CursorDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

CursorDrawableData::~CursorDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
