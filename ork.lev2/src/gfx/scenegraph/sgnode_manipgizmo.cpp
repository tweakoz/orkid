////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/sgnode_manipgizmo.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ManipGizmoDrawableData, "ManipGizmoDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static const char* GIZMO_SHADER = R"(
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
vertex_shader vs_gizmo : iface_vtx {
  gl_Position = mvp * position;
  frg_clr = vtxcolor * modcolor;
}
fragment_shader fs_gizmo : iface_frg {
  out_color = frg_clr;
}
state_block sb_gizmo : default {
  CullTest = OFF;
  DepthTest = OFF;
  DepthMask = false;
}
technique tek_gizmo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader = vs_gizmo;
    fragment_shader = fs_gizmo;
    state_block = sb_gizmo;
  }
}
)";

///////////////////////////////////////////////////////////////////////////////

ManipGizmoDrawableImpl::ManipGizmoDrawableImpl(const ManipGizmoDrawableData* data)
    : _data(data) {
}

ManipGizmoDrawableImpl::~ManipGizmoDrawableImpl() {
}

void ManipGizmoDrawableImpl::gpuInit(lev2::Context* ctx) {
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInitFromShaderText(ctx, "gizmo_shader", GIZMO_SHADER);
  _technique = _material->technique("tek_gizmo");
  _paramMVP = _material->param("mvp");
  _paramModColor = _material->param("modcolor");
  _material->_rasterstate->setDepthTest(EDepthTest::OFF);  // Render on top of everything
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _initted = true;
}

///////////////////////////////////////////////////////////////////////////////
// Drawing Helpers
///////////////////////////////////////////////////////////////////////////////

void ManipGizmoDrawableImpl::_drawAxis(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                                        const fvec3& dir, const fvec4& color,
                                        float length, float thickness) {
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;

  // Build cylinder around the axis
  const int segments = 8;
  const int numVerts = segments * 6;  // 2 triangles per segment
  vw.Lock(ctx, &VB, numVerts);

  fvec4 uv(0, 0, 0, 0);

  // Get perpendicular vectors for the cylinder
  fvec3 perp1, perp2;
  if (fabs(dir.y) < 0.9f) {
    perp1 = dir.crossWith(fvec3(0, 1, 0)).normalized();
  } else {
    perp1 = dir.crossWith(fvec3(1, 0, 0)).normalized();
  }
  perp2 = dir.crossWith(perp1).normalized();

  fvec3 endPos = pos + dir * length;

  for (int i = 0; i < segments; i++) {
    float a0 = (float(i) / segments) * 2.0f * PI;
    float a1 = (float(i + 1) / segments) * 2.0f * PI;

    fvec3 offset0 = (perp1 * cos(a0) + perp2 * sin(a0)) * thickness;
    fvec3 offset1 = (perp1 * cos(a1) + perp2 * sin(a1)) * thickness;

    fvec3 v0 = pos + offset0;
    fvec3 v1 = pos + offset1;
    fvec3 v2 = endPos + offset0;
    fvec3 v3 = endPos + offset1;

    // Two triangles for each segment
    vw.AddVertex(vtx_t(v0, uv, color));
    vw.AddVertex(vtx_t(v2, uv, color));
    vw.AddVertex(vtx_t(v1, uv, color));

    vw.AddVertex(vtx_t(v1, uv, color));
    vw.AddVertex(vtx_t(v2, uv, color));
    vw.AddVertex(vtx_t(v3, uv, color));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, VP);
  _material->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawCone(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                                        const fvec3& dir, const fvec4& color,
                                        float radius, float height) {
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;

  const int segments = 12;
  const int numVerts = segments * 3 + segments * 3;  // cone side + base
  vw.Lock(ctx, &VB, numVerts);

  fvec4 uv(0, 0, 0, 0);

  // Get perpendicular vectors
  fvec3 perp1, perp2;
  if (fabs(dir.y) < 0.9f) {
    perp1 = dir.crossWith(fvec3(0, 1, 0)).normalized();
  } else {
    perp1 = dir.crossWith(fvec3(1, 0, 0)).normalized();
  }
  perp2 = dir.crossWith(perp1).normalized();

  fvec3 tip = pos + dir * height;

  for (int i = 0; i < segments; i++) {
    float a0 = (float(i) / segments) * 2.0f * PI;
    float a1 = (float(i + 1) / segments) * 2.0f * PI;

    fvec3 b0 = pos + (perp1 * cos(a0) + perp2 * sin(a0)) * radius;
    fvec3 b1 = pos + (perp1 * cos(a1) + perp2 * sin(a1)) * radius;

    // Cone side triangle
    vw.AddVertex(vtx_t(tip, uv, color));
    vw.AddVertex(vtx_t(b1, uv, color));
    vw.AddVertex(vtx_t(b0, uv, color));

    // Base triangle
    vw.AddVertex(vtx_t(pos, uv, color));
    vw.AddVertex(vtx_t(b0, uv, color));
    vw.AddVertex(vtx_t(b1, uv, color));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, VP);
  _material->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawRing(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& center,
                                        const fvec3& normal, const fvec4& color,
                                        float radius, float thickness) {
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;

  const int segments = 32;
  const int numVerts = segments * 6;  // 2 triangles per segment
  vw.Lock(ctx, &VB, numVerts);

  fvec4 uv(0, 0, 0, 0);

  // Get perpendicular vectors in the ring's plane
  fvec3 perp1, perp2;
  if (fabs(normal.y) < 0.9f) {
    perp1 = normal.crossWith(fvec3(0, 1, 0)).normalized();
  } else {
    perp1 = normal.crossWith(fvec3(1, 0, 0)).normalized();
  }
  perp2 = normal.crossWith(perp1).normalized();

  float innerRadius = radius - thickness * 0.5f;
  float outerRadius = radius + thickness * 0.5f;

  for (int i = 0; i < segments; i++) {
    float a0 = (float(i) / segments) * 2.0f * PI;
    float a1 = (float(i + 1) / segments) * 2.0f * PI;

    fvec3 dir0 = perp1 * cos(a0) + perp2 * sin(a0);
    fvec3 dir1 = perp1 * cos(a1) + perp2 * sin(a1);

    fvec3 inner0 = center + dir0 * innerRadius;
    fvec3 outer0 = center + dir0 * outerRadius;
    fvec3 inner1 = center + dir1 * innerRadius;
    fvec3 outer1 = center + dir1 * outerRadius;

    vw.AddVertex(vtx_t(inner0, uv, color));
    vw.AddVertex(vtx_t(outer0, uv, color));
    vw.AddVertex(vtx_t(inner1, uv, color));

    vw.AddVertex(vtx_t(inner1, uv, color));
    vw.AddVertex(vtx_t(outer0, uv, color));
    vw.AddVertex(vtx_t(outer1, uv, color));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, VP);
  _material->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawCube(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                                        const fvec4& color, float size) {
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;

  const int numVerts = 36;  // 6 faces * 2 triangles * 3 verts
  vw.Lock(ctx, &VB, numVerts);

  fvec4 uv(0, 0, 0, 0);
  float h = size * 0.5f;

  // 8 corners of the cube
  fvec3 corners[8] = {
    pos + fvec3(-h, -h, -h),
    pos + fvec3(+h, -h, -h),
    pos + fvec3(+h, +h, -h),
    pos + fvec3(-h, +h, -h),
    pos + fvec3(-h, -h, +h),
    pos + fvec3(+h, -h, +h),
    pos + fvec3(+h, +h, +h),
    pos + fvec3(-h, +h, +h),
  };

  // 6 faces (2 triangles each)
  int faces[6][4] = {
    {0, 1, 2, 3},  // front
    {5, 4, 7, 6},  // back
    {4, 0, 3, 7},  // left
    {1, 5, 6, 2},  // right
    {3, 2, 6, 7},  // top
    {4, 5, 1, 0},  // bottom
  };

  for (int f = 0; f < 6; f++) {
    fvec3 v0 = corners[faces[f][0]];
    fvec3 v1 = corners[faces[f][1]];
    fvec3 v2 = corners[faces[f][2]];
    fvec3 v3 = corners[faces[f][3]];

    vw.AddVertex(vtx_t(v0, uv, color));
    vw.AddVertex(vtx_t(v1, uv, color));
    vw.AddVertex(vtx_t(v2, uv, color));

    vw.AddVertex(vtx_t(v0, uv, color));
    vw.AddVertex(vtx_t(v2, uv, color));
    vw.AddVertex(vtx_t(v3, uv, color));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, VP);
  _material->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawPlaneHandle(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
                                               const fvec3& pos,
                                               const fvec3& axis1, const fvec3& axis2,
                                               const fvec4& color, float offset, float size) {
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;

  vw.Lock(ctx, &VB, 6);

  fvec4 uv(0, 0, 0, 0);

  fvec3 center = pos + axis1 * offset + axis2 * offset;
  float h = size * 0.5f;

  fvec3 v0 = center - axis1 * h - axis2 * h;
  fvec3 v1 = center + axis1 * h - axis2 * h;
  fvec3 v2 = center + axis1 * h + axis2 * h;
  fvec3 v3 = center - axis1 * h + axis2 * h;

  vw.AddVertex(vtx_t(v0, uv, color));
  vw.AddVertex(vtx_t(v1, uv, color));
  vw.AddVertex(vtx_t(v2, uv, color));

  vw.AddVertex(vtx_t(v0, uv, color));
  vw.AddVertex(vtx_t(v2, uv, color));
  vw.AddVertex(vtx_t(v3, uv, color));

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, VP);
  _material->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

///////////////////////////////////////////////////////////////////////////////
// Gizmo Rendering
///////////////////////////////////////////////////////////////////////////////

void ManipGizmoDrawableImpl::_drawTranslateGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
                                                   const fvec3& pos, float scale) {
  auto controller = _data->_controller;
  if (!controller || !controller->target()) return;

  auto hovered = controller->hoveredAxis();
  auto active = controller->activeAxis();
  bool dragging = controller->isDragging();

  // Get axes based on space mode
  fvec3 axisX(1, 0, 0), axisY(0, 1, 0), axisZ(0, 0, 1);
  if (controller->space() == editor::ManipSpace::LOCAL) {
    fquat targetRot = controller->target()->getWorldRotation();
    axisX = targetRot.transform(fvec3(1, 0, 0));
    axisY = targetRot.transform(fvec3(0, 1, 0));
    axisZ = targetRot.transform(fvec3(0, 0, 1));
  }

  float axisLen = _data->_axisLength * scale;
  float thickness = _data->_axisThickness * scale;
  float coneH = axisLen * 0.15f;
  float coneR = coneH * 0.577f;  // tan(30 degrees) for sharper cone
  float planeOffset = axisLen * 0.5f;
  float planeSize = axisLen * 0.15f;

  auto getColor = [&](editor::ManipAxis axis, const fvec4& baseColor) -> fvec4 {
    if (dragging && active == axis) return _data->_colorActive;
    if (hovered == axis) return _data->_colorHighlight;
    return baseColor;
  };

  // X axis
  fvec4 colorX = getColor(editor::ManipAxis::X, _data->_colorX);
  _drawAxis(ctx, RCFD, VP, pos, axisX, colorX, axisLen - coneH, thickness);
  _drawCone(ctx, RCFD, VP, pos + axisX * (axisLen - coneH), axisX, colorX, coneR, coneH);

  // Y axis
  fvec4 colorY = getColor(editor::ManipAxis::Y, _data->_colorY);
  _drawAxis(ctx, RCFD, VP, pos, axisY, colorY, axisLen - coneH, thickness);
  _drawCone(ctx, RCFD, VP, pos + axisY * (axisLen - coneH), axisY, colorY, coneR, coneH);

  // Z axis
  fvec4 colorZ = getColor(editor::ManipAxis::Z, _data->_colorZ);
  _drawAxis(ctx, RCFD, VP, pos, axisZ, colorZ, axisLen - coneH, thickness);
  _drawCone(ctx, RCFD, VP, pos + axisZ * (axisLen - coneH), axisZ, colorZ, coneR, coneH);

  // Plane handles (XY, XZ, YZ)
  fvec4 colorXY = getColor(editor::ManipAxis::XY, fvec4(1, 1, 0.2f, 0.6f));
  fvec4 colorXZ = getColor(editor::ManipAxis::XZ, fvec4(1, 0.2f, 1, 0.6f));
  fvec4 colorYZ = getColor(editor::ManipAxis::YZ, fvec4(0.2f, 1, 1, 0.6f));

  _drawPlaneHandle(ctx, RCFD, VP, pos, axisX, axisY, colorXY, planeOffset, planeSize);
  _drawPlaneHandle(ctx, RCFD, VP, pos, axisX, axisZ, colorXZ, planeOffset, planeSize);
  _drawPlaneHandle(ctx, RCFD, VP, pos, axisY, axisZ, colorYZ, planeOffset, planeSize);
}

void ManipGizmoDrawableImpl::_drawRotateGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
                                                const fvec3& pos, float scale) {
  auto controller = _data->_controller;
  if (!controller || !controller->target()) return;

  auto hovered = controller->hoveredAxis();
  auto active = controller->activeAxis();
  bool dragging = controller->isDragging();

  float radius = _data->_ringRadius * scale;
  float thick = _data->_ringTubeRadius * scale;

  // Get target rotation for local-space rings
  fquat targetRot = controller->target()->getWorldRotation();
  fvec3 localX = targetRot.transform(fvec3(1, 0, 0));
  fvec3 localY = targetRot.transform(fvec3(0, 1, 0));
  fvec3 localZ = targetRot.transform(fvec3(0, 0, 1));

  // Get camera direction from RCFD
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fvec3 camDir = cmtcs->_vmatrix.inverse().column(2).xyz().normalized() * -1.0f;

  // Compute dimming factor based on view angle (edge-on rings are dimmed)
  auto getDimFactor = [&](const fvec3& ringNormal) -> float {
    float dotProduct = fabs(ringNormal.dotWith(camDir));
    float angleDegrees = 90.0f - (acos(dotProduct) * 180.0f / PI);
    if (angleDegrees >= controller->_minRingElevationDegrees) {
      return 1.0f;  // Full brightness
    } else {
      // Fade from 1.0 at threshold to 0.3 at 0 degrees (edge-on)
      float t = angleDegrees / controller->_minRingElevationDegrees;
      return 0.3f + 0.7f * t;
    }
  };

  auto getColor = [&](editor::ManipAxis axis, const fvec4& baseColor, float dimFactor) -> fvec4 {
    fvec4 color;
    if (dragging && active == axis) {
      color = _data->_colorActive;
    } else if (hovered == axis) {
      color = _data->_colorHighlight;
    } else {
      color = baseColor;
    }
    // Apply dimming to RGB, reduce alpha for edge-on rings
    color.x *= dimFactor;
    color.y *= dimFactor;
    color.z *= dimFactor;
    color.w *= (0.5f + 0.5f * dimFactor);  // 50% transparent when fully dimmed
    return color;
  };

  // X ring (rotates around local X axis)
  float dimX = getDimFactor(localX);
  fvec4 colorX = getColor(editor::ManipAxis::X, _data->_colorX, dimX);
  _drawRing(ctx, RCFD, VP, pos, localX, colorX, radius, thick);

  // Y ring (rotates around local Y axis)
  float dimY = getDimFactor(localY);
  fvec4 colorY = getColor(editor::ManipAxis::Y, _data->_colorY, dimY);
  _drawRing(ctx, RCFD, VP, pos, localY, colorY, radius, thick);

  // Z ring (rotates around local Z axis)
  float dimZ = getDimFactor(localZ);
  fvec4 colorZ = getColor(editor::ManipAxis::Z, _data->_colorZ, dimZ);
  _drawRing(ctx, RCFD, VP, pos, localZ, colorZ, radius, thick);
}

void ManipGizmoDrawableImpl::_drawScaleGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
                                               const fvec3& pos, float scale) {
  auto controller = _data->_controller;
  if (!controller || !controller->target()) return;

  auto hovered = controller->hoveredAxis();
  auto active = controller->activeAxis();
  bool dragging = controller->isDragging();

  float axisLen = _data->_axisLength * scale;
  float thickness = _data->_axisThickness * scale;
  float cubeSize = axisLen * 0.1f;

  auto getColor = [&](editor::ManipAxis axis, const fvec4& baseColor) -> fvec4 {
    if (dragging && active == axis) return _data->_colorActive;
    if (hovered == axis) return _data->_colorHighlight;
    return baseColor;
  };

  // X axis with cube
  fvec4 colorX = getColor(editor::ManipAxis::X, _data->_colorX);
  _drawAxis(ctx, RCFD, VP, pos, fvec3(1, 0, 0), colorX, axisLen - cubeSize, thickness);
  _drawCube(ctx, RCFD, VP, pos + fvec3(axisLen, 0, 0), colorX, cubeSize);

  // Y axis with cube
  fvec4 colorY = getColor(editor::ManipAxis::Y, _data->_colorY);
  _drawAxis(ctx, RCFD, VP, pos, fvec3(0, 1, 0), colorY, axisLen - cubeSize, thickness);
  _drawCube(ctx, RCFD, VP, pos + fvec3(0, axisLen, 0), colorY, cubeSize);

  // Z axis with cube
  fvec4 colorZ = getColor(editor::ManipAxis::Z, _data->_colorZ);
  _drawAxis(ctx, RCFD, VP, pos, fvec3(0, 0, 1), colorZ, axisLen - cubeSize, thickness);
  _drawCube(ctx, RCFD, VP, pos + fvec3(0, 0, axisLen), colorZ, cubeSize);

  // Center cube for uniform scale
  fvec4 colorFree = getColor(editor::ManipAxis::FREE, fvec4(0.8f, 0.8f, 0.8f, 1.0f));
  _drawCube(ctx, RCFD, VP, pos, colorFree, cubeSize * 1.5f);
}

///////////////////////////////////////////////////////////////////////////////

void ManipGizmoDrawableImpl::_render(const RenderContextInstData& RCID) {
  auto context = RCID.context();

  if (!_initted) {
    gpuInit(context);
  }

  auto controller = _data->_controller;
  if (!controller) {
    printf("ManipGizmo: no controller\n");
    return;
  }
  if (!controller->target()) {
    printf("ManipGizmo: no target\n");
    return;
  }
  auto RCFD = RCID.rcfd();
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fmtx4 VP = cmtcs->_pmatrix * cmtcs->_vmatrix;  // P * V for correct MVP ordering

  fvec3 gizmoPos = controller->target()->getWorldPosition();

  // Compute scale based on distance from camera
  fvec3 camPos = cmtcs->_camdat.mEye;
  float dist = (gizmoPos - camPos).length();
  float scale = dist * 0.1f;  // Keep gizmo at consistent screen size

  auto fxi = context->FXI();
  fxi->pushRasterState(_material->_rasterstate);

  switch (controller->mode()) {
    case editor::ManipMode::TRANSLATE:
      _drawTranslateGizmo(context, RCFD, VP, gizmoPos, scale);
      break;
    case editor::ManipMode::ROTATE:
      _drawRotateGizmo(context, RCFD, VP, gizmoPos, scale);
      break;
    case editor::ManipMode::SCALE:
      _drawScaleGizmo(context, RCFD, VP, gizmoPos, scale);
      break;
  }

  fxi->popRasterState();
}

///////////////////////////////////////////////////////////////////////////////

void ManipGizmoDrawableImpl::renderGizmo(RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto drawable = renderable->_drawable;
  drawable->_implA.getShared<ManipGizmoDrawableImpl>()->_render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void ManipGizmoDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ManipGizmoDrawableData::createDrawable() const {
  auto drw = std::make_shared<CallbackDrawable>(nullptr);
  auto impl = drw->_implA.makeShared<ManipGizmoDrawableImpl>(this);
  drw->_sortkey = 999;  // Render late (on top of scene)
  drw->SetRenderCallback(ManipGizmoDrawableImpl::renderGizmo);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

ManipGizmoDrawableData::ManipGizmoDrawableData() {
  _ringRadius = 1.2f;
}

///////////////////////////////////////////////////////////////////////////////

ManipGizmoDrawableData::~ManipGizmoDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
