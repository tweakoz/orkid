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
  vec3 lightdir;  // Headlight direction (camera forward)
  float planesize;  // For edge alpha computation
  float unlit;  // 1.0 = unlit, 0.0 = lit
}
vertex_interface iface_vtx : ublock {
  inputs {
    vec4 position : POSITION;
    vec2 uv : TEXCOORD0;
    vec4 vtxcolor : COLOR0;
    vec3 normal : NORMAL;
  }
  outputs {
    vec4 frg_clr;
    vec2 frg_uv;
    vec3 frg_nrm;
  }
}
fragment_interface iface_frg : ublock {
  inputs {
    vec4 frg_clr;
    vec2 frg_uv;
    vec3 frg_nrm;
  }
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
vertex_shader vs_gizmo : iface_vtx {
  gl_Position = mvp * position;
  frg_clr = vtxcolor * modcolor;
  frg_uv = uv;
  frg_nrm = normal;
}
fragment_shader fs_gizmo : iface_frg {
  // Lighting (skip if unlit flag is set)
  float lighting = 1.0;
  if (unlit < 0.5) {
    vec3 N = normalize(frg_nrm);
    vec3 L = normalize(-lightdir);  // Light direction (camera forward)
    float NdotL = max(dot(N, L), 0.0);

    // Ambient + diffuse lighting
    float ambient = 0.4;
    float diffuse = 0.6 * NdotL;
    lighting = ambient + diffuse;
  }

  // Compute distance from edge using UV (0-1 range)
  vec2 uv = frg_uv;

  // Distance from each edge in UV space (0-1)
  float distFromEdgeU = min(uv.x, 1.0 - uv.x);
  float distFromEdgeV = min(uv.y, 1.0 - uv.y);
  float distFromEdge = min(distFromEdgeU, distFromEdgeV);

  // Use derivatives to convert UV space to screen-space pixels
  vec2 pixelsPerUV = vec2(length(vec2(dFdx(uv.x), dFdy(uv.x))),
                           length(vec2(dFdx(uv.y), dFdy(uv.y))));

  // Convert distance from edge (in UV space) to pixels
  float distFromEdgePixels = distFromEdge / max(pixelsPerUV.x, pixelsPerUV.y);

  // Border width in pixels
  float borderWidthPixels = 3.0;

  // Checkerboard pattern for planes (8 pixels per square)
  float checkerboard = 1.0;
  if (planesize > 0.0) {
    // Convert UV to pixel coordinates
    vec2 uvPixels = uv / pixelsPerUV;

    // 8-pixel checkerboard squares
    float checkerSize = 8.0;
    vec2 checker = floor(uvPixels / checkerSize);
    float checkerPattern = mod(checker.x + checker.y, 2.0);

    // Antialiased checkerboard using smoothstep
    vec2 checkerUV = fract(uvPixels / checkerSize);
    float edgeWidth = 0.5 / checkerSize; // Half pixel antialiasing
    float edge = min(
      min(smoothstep(0.0, edgeWidth, checkerUV.x), smoothstep(1.0, 1.0 - edgeWidth, checkerUV.x)),
      min(smoothstep(0.0, edgeWidth, checkerUV.y), smoothstep(1.0, 1.0 - edgeWidth, checkerUV.y))
    );

    // Blend between dark and light based on checker pattern
    checkerboard = mix(0.7, 1.0, checkerPattern);
  }

  // Alpha: 1.0 if within border, 0.5 otherwise (only for planes with valid size)
  float alpha = (planesize > 0.0 && distFromEdgePixels < borderWidthPixels) ? 1.0 :
                (planesize > 0.0) ? 0.5 : 1.0;

  out_color = vec4(frg_clr.rgb * lighting * checkerboard, frg_clr.a * alpha);
}
state_block sb_gizmo : default {
  CullTest = OFF;
  DepthTest = OFF;
  DepthMask = false;
  BlendMode = ALPHA;
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
  _param_mvp = _material->param("mvp");
  _param_modcolor = _material->param("modcolor");
  _param_lightdir = _material->param("lightdir");
  _param_planesize = _material->param("planesize");
  _param_unlit = _material->param("unlit");
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
  using vtx_t = SVtxV12N12B12T8C4;
  auto VB = GfxEnv::GetSharedDynamicVB2();
  VtxWriter<vtx_t> vw;

  // Build cylinder around the axis
  const int segments = 8;
  const int numVerts = segments * 6;  // 2 triangles per segment
  vw.Lock(ctx, VB.get(), numVerts);

  fvec2 uv(0, 0);
  U32 clr = ((U32)(color.w * 255) << 24) | ((U32)(color.z * 255) << 16) |
            ((U32)(color.y * 255) << 8) | (U32)(color.x * 255);

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

    // Normals point radially outward
    fvec3 normal0 = offset0.normalized();
    fvec3 normal1 = offset1.normalized();

    fvec3 v0 = pos + offset0;
    fvec3 v1 = pos + offset1;
    fvec3 v2 = endPos + offset0;
    fvec3 v3 = endPos + offset1;

    // Two triangles for each segment (CCW winding for outward-facing normals)
    vw.AddVertex(vtx_t(v0, normal0, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v1, normal1, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v2, normal0, fvec3(), uv, clr));

    vw.AddVertex(vtx_t(v1, normal1, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v3, normal1, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v2, normal0, fvec3(), uv, clr));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  // Get camera direction for headlight
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fvec3 lightDir = cmtcs->_vmatrix.inverse().column(2).xyz().normalized() * -1.0f;

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, VP);
  _material->bindParamVec4(_param_modcolor, fvec4(1, 1, 1, 1));
  _material->bindParamVec3(_param_lightdir, lightDir);
  _material->bindParamFloat(_param_planesize, 0.0f);
  _material->bindParamFloat(_param_unlit, 0.0f);

  // Two-pass rendering: backfaces first, then frontfaces
  _material->_rasterstate->_priority = 1 << 20;
  _material->_rasterstate->setFrontFace(EFrontFace::COUNTER_CLOCKWISE);
  _material->_rasterstate->setCullTest(ECullTest::PASS_BACK);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->_rasterstate->setCullTest(ECullTest::PASS_FRONT);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawCone(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                                        const fvec3& dir, const fvec4& color,
                                        float radius, float height) {
  using vtx_t = SVtxV12N12B12T8C4;
  auto VB = GfxEnv::GetSharedDynamicVB2();
  VtxWriter<vtx_t> vw;

  const int segments = 12;
  const int numVerts = segments * 3 + segments * 3;  // cone side + base
  vw.Lock(ctx, VB.get(), numVerts);

  fvec2 uv(0, 0);
  U32 clr = ((U32)(color.w * 255) << 24) | ((U32)(color.z * 255) << 16) |
            ((U32)(color.y * 255) << 8) | (U32)(color.x * 255);

  // Get perpendicular vectors
  fvec3 perp1, perp2;
  if (fabs(dir.y) < 0.9f) {
    perp1 = dir.crossWith(fvec3(0, 1, 0)).normalized();
  } else {
    perp1 = dir.crossWith(fvec3(1, 0, 0)).normalized();
  }
  perp2 = dir.crossWith(perp1).normalized();

  fvec3 tip = pos + dir * height;

  // Cone surface normal: blend of radial and axial
  float slopeAngle = atan2(radius, height);
  float normalRadial = sin(slopeAngle);
  float normalAxial = cos(slopeAngle);

  for (int i = 0; i < segments; i++) {
    float a0 = (float(i) / segments) * 2.0f * PI;
    float a1 = (float(i + 1) / segments) * 2.0f * PI;

    fvec3 radial0 = (perp1 * cos(a0) + perp2 * sin(a0));
    fvec3 radial1 = (perp1 * cos(a1) + perp2 * sin(a1));

    fvec3 b0 = pos + radial0 * radius;
    fvec3 b1 = pos + radial1 * radius;

    // Normals for cone surface (perpendicular to sloped surface)
    fvec3 normal0 = (radial0 * normalRadial + dir * normalAxial).normalized();
    fvec3 normal1 = (radial1 * normalRadial + dir * normalAxial).normalized();

    // Cone side triangle (CCW winding for outward-facing normals)
    vw.AddVertex(vtx_t(tip, normal0, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(b0, normal0, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(b1, normal1, fvec3(), uv, clr));

    // Base triangle (normal points opposite to dir, CCW winding)
    fvec3 baseNormal = -dir;
    vw.AddVertex(vtx_t(pos, baseNormal, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(b1, baseNormal, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(b0, baseNormal, fvec3(), uv, clr));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  // Get camera direction for headlight
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fvec3 lightDir = cmtcs->_vmatrix.inverse().column(2).xyz().normalized() * -1.0f;

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, VP);
  _material->bindParamVec4(_param_modcolor, fvec4(1, 1, 1, 1));
  _material->bindParamVec3(_param_lightdir, lightDir);
  _material->bindParamFloat(_param_planesize, 0.0f);
  _material->bindParamFloat(_param_unlit, 0.0f);

  // Two-pass rendering: backfaces first, then frontfaces
  _material->_rasterstate->_priority = 1 << 20;
  _material->_rasterstate->setFrontFace(EFrontFace::COUNTER_CLOCKWISE);
  _material->_rasterstate->setCullTest(ECullTest::PASS_BACK);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->_rasterstate->setCullTest(ECullTest::PASS_FRONT);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawRing(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& center,
                                        const fvec3& normal, const fvec3& perp1, const fvec3& perp2,
                                        const fvec4& color,
                                        float majorRadius, float minorRadius) {
  using vtx_t = SVtxV12N12B12T8C4;
  auto VB = GfxEnv::GetSharedDynamicVB2();
  VtxWriter<vtx_t> vw;

  // Torus geometry parameters
  const int majorSegments = 48;  // Around the main ring
  const int minorSegments = 12;  // Around the tube
  const int numVerts = majorSegments * minorSegments * 6;  // 2 triangles per quad

  vw.Lock(ctx, VB.get(), numVerts);

  fvec2 uv(0, 0);

  // Get band size from controller if available
  float bandDegrees = 30.0f;  // Default
  if (_data->_controller) {
    bandDegrees = _data->_controller->_ring_band_degrees;
  }
  float bandsPerRing = 360.0f / bandDegrees;

  // Generate torus vertices
  for (int i = 0; i < majorSegments; i++) {
    float majorAngle0 = (float(i) / majorSegments) * 2.0f * PI;
    float majorAngle1 = (float(i + 1) / majorSegments) * 2.0f * PI;

    // Determine intensity for this major segment (alternating bands)
    float majorAngleDeg = majorAngle0 * 180.0f / PI;
    int bandIndex = (int)(majorAngleDeg / bandDegrees);
    float intensity = (bandIndex % 2 == 0) ? 1.0f : 0.75f;

    fvec4 bandColor = color;
    bandColor.x *= intensity;
    bandColor.y *= intensity;
    bandColor.z *= intensity;

    U32 clr = ((U32)(bandColor.w * 255) << 24) | ((U32)(bandColor.z * 255) << 16) |
              ((U32)(bandColor.y * 255) << 8) | (U32)(bandColor.x * 255);

    // Tube center positions for this major segment
    fvec3 dir0 = perp1 * cos(majorAngle0) + perp2 * sin(majorAngle0);
    fvec3 dir1 = perp1 * cos(majorAngle1) + perp2 * sin(majorAngle1);
    fvec3 tubeCenter0 = center + dir0 * majorRadius;
    fvec3 tubeCenter1 = center + dir1 * majorRadius;

    for (int j = 0; j < minorSegments; j++) {
      float minorAngle0 = (float(j) / minorSegments) * 2.0f * PI;
      float minorAngle1 = (float(j + 1) / minorSegments) * 2.0f * PI;

      // Compute positions and normals around the tube
      // For a torus, the tube normal at angle θ around the tube is:
      // radial_dir * cos(θ) + ring_normal * sin(θ)
      fvec3 tubeOffset00 = (dir0 * cos(minorAngle0) + normal * sin(minorAngle0)) * minorRadius;
      fvec3 tubeOffset01 = (dir0 * cos(minorAngle1) + normal * sin(minorAngle1)) * minorRadius;
      fvec3 tubeOffset10 = (dir1 * cos(minorAngle0) + normal * sin(minorAngle0)) * minorRadius;
      fvec3 tubeOffset11 = (dir1 * cos(minorAngle1) + normal * sin(minorAngle1)) * minorRadius;

      // Normals are the offsets normalized
      fvec3 normal00 = tubeOffset00.normalized();
      fvec3 normal01 = tubeOffset01.normalized();
      fvec3 normal10 = tubeOffset10.normalized();
      fvec3 normal11 = tubeOffset11.normalized();

      fvec3 v00 = tubeCenter0 + tubeOffset00;
      fvec3 v01 = tubeCenter0 + tubeOffset01;
      fvec3 v10 = tubeCenter1 + tubeOffset10;
      fvec3 v11 = tubeCenter1 + tubeOffset11;

      // First triangle (original winding)
      vw.AddVertex(vtx_t(v00, normal00, fvec3(), uv, clr));
      vw.AddVertex(vtx_t(v10, normal10, fvec3(), uv, clr));
      vw.AddVertex(vtx_t(v01, normal01, fvec3(), uv, clr));

      // Second triangle
      vw.AddVertex(vtx_t(v01, normal01, fvec3(), uv, clr));
      vw.AddVertex(vtx_t(v10, normal10, fvec3(), uv, clr));
      vw.AddVertex(vtx_t(v11, normal11, fvec3(), uv, clr));
    }
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  // Get camera direction for headlight
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fvec3 lightDir = cmtcs->_vmatrix.inverse().column(2).xyz().normalized() * -1.0f;

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, VP);
  _material->bindParamVec4(_param_modcolor, fvec4(1, 1, 1, 1));
  _material->bindParamVec3(_param_lightdir, lightDir);
  _material->bindParamFloat(_param_planesize, 0.0f);
  _material->bindParamFloat(_param_unlit, 0.0f);

  // Two-pass rendering: backfaces first, then frontfaces
  _material->_rasterstate->_priority = 1 << 20;
  _material->_rasterstate->setFrontFace(EFrontFace::COUNTER_CLOCKWISE);
  _material->_rasterstate->setCullTest(ECullTest::PASS_BACK);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->_rasterstate->setCullTest(ECullTest::PASS_FRONT);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawCube(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP, const fvec3& pos,
                                        const fvec4& color, float size) {
  using vtx_t = SVtxV12N12B12T8C4;
  auto VB = GfxEnv::GetSharedDynamicVB2();
  VtxWriter<vtx_t> vw;

  const int numVerts = 36;  // 6 faces * 2 triangles * 3 verts
  vw.Lock(ctx, VB.get(), numVerts);

  fvec2 uv(0, 0);
  float h = size * 0.5f;

  U32 clr = ((U32)(color.w * 255) << 24) | ((U32)(color.z * 255) << 16) |
            ((U32)(color.y * 255) << 8) | (U32)(color.x * 255);

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

  // 6 faces (2 triangles each) with normals
  int faces[6][4] = {
    {0, 1, 2, 3},  // -Z front
    {5, 4, 7, 6},  // +Z back
    {4, 0, 3, 7},  // -X left
    {1, 5, 6, 2},  // +X right
    {3, 2, 6, 7},  // +Y top
    {4, 5, 1, 0},  // -Y bottom
  };

  fvec3 normals[6] = {
    fvec3(0, 0, -1),  // front
    fvec3(0, 0, +1),  // back
    fvec3(-1, 0, 0),  // left
    fvec3(+1, 0, 0),  // right
    fvec3(0, +1, 0),  // top
    fvec3(0, -1, 0),  // bottom
  };

  for (int f = 0; f < 6; f++) {
    fvec3 v0 = corners[faces[f][0]];
    fvec3 v1 = corners[faces[f][1]];
    fvec3 v2 = corners[faces[f][2]];
    fvec3 v3 = corners[faces[f][3]];
    fvec3 normal = normals[f];

    vw.AddVertex(vtx_t(v0, normal, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v1, normal, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v2, normal, fvec3(), uv, clr));

    vw.AddVertex(vtx_t(v0, normal, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v2, normal, fvec3(), uv, clr));
    vw.AddVertex(vtx_t(v3, normal, fvec3(), uv, clr));
  }

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  // Get camera direction for headlight
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fvec3 lightDir = cmtcs->_vmatrix.inverse().column(2).xyz().normalized() * -1.0f;

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, VP);
  _material->bindParamVec4(_param_modcolor, fvec4(1, 1, 1, 1));
  _material->bindParamVec3(_param_lightdir, lightDir);
  _material->bindParamFloat(_param_planesize, 0.0f);
  _material->bindParamFloat(_param_unlit, 0.0f);

  // Two-pass rendering: backfaces first, then frontfaces
  _material->_rasterstate->_priority = 1 << 20;
  _material->_rasterstate->setFrontFace(EFrontFace::COUNTER_CLOCKWISE);
  _material->_rasterstate->setCullTest(ECullTest::PASS_BACK);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->_rasterstate->setCullTest(ECullTest::PASS_FRONT);
  fxi->pushRasterState(_material->_rasterstate);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  fxi->popRasterState();

  _material->end(RCFD);
}

void ManipGizmoDrawableImpl::_drawPlaneHandle(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
                                               const fvec3& pos,
                                               const fvec3& axis1, const fvec3& axis2,
                                               const fvec4& color, float sign1, float sign2, float size) {
  using vtx_t = SVtxV12N12B12T8C4;
  auto VB = GfxEnv::GetSharedDynamicVB2();
  VtxWriter<vtx_t> vw;

  vw.Lock(ctx, VB.get(), 12);  // 6 verts per side, 2 sides for two-pass rendering

  // Plane is cornered at origin, extends in sign1*axis1 and sign2*axis2 directions
  fvec3 v0 = pos;                                    // Origin corner
  fvec3 v1 = pos + axis1 * sign1 * size;            // Along axis1
  fvec3 v2 = pos + axis1 * sign1 * size + axis2 * sign2 * size;  // Opposite corner
  fvec3 v3 = pos + axis2 * sign2 * size;            // Along axis2

  // Compute plane normal
  fvec3 planeNormal = axis1.crossWith(axis2).normalized();
  if (sign1 * sign2 < 0) planeNormal = -planeNormal;  // Flip if handedness changes

  // Set up UVs for fragment shader edge detection
  fvec2 uv0(0, 0);
  fvec2 uv1(1, 0);
  fvec2 uv2(1, 1);
  fvec2 uv3(0, 1);

  U32 clr = ((U32)(color.w * 255) << 24) | ((U32)(color.z * 255) << 16) |
            ((U32)(color.y * 255) << 8) | (U32)(color.x * 255);

  // Backface (reverse winding, inverted normal)
  vw.AddVertex(vtx_t(v0, -planeNormal, fvec3(), uv0, clr));
  vw.AddVertex(vtx_t(v2, -planeNormal, fvec3(), uv2, clr));
  vw.AddVertex(vtx_t(v1, -planeNormal, fvec3(), uv1, clr));

  vw.AddVertex(vtx_t(v0, -planeNormal, fvec3(), uv0, clr));
  vw.AddVertex(vtx_t(v3, -planeNormal, fvec3(), uv3, clr));
  vw.AddVertex(vtx_t(v2, -planeNormal, fvec3(), uv2, clr));

  // Frontface (normal winding)
  vw.AddVertex(vtx_t(v0, planeNormal, fvec3(), uv0, clr));
  vw.AddVertex(vtx_t(v1, planeNormal, fvec3(), uv1, clr));
  vw.AddVertex(vtx_t(v2, planeNormal, fvec3(), uv2, clr));

  vw.AddVertex(vtx_t(v0, planeNormal, fvec3(), uv0, clr));
  vw.AddVertex(vtx_t(v2, planeNormal, fvec3(), uv2, clr));
  vw.AddVertex(vtx_t(v3, planeNormal, fvec3(), uv3, clr));

  vw.UnLock(ctx);

  auto fxi = ctx->FXI();
  auto gbi = ctx->GBI();

  // Get camera direction for headlight (even though unlit, still need to bind)
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();
  fvec3 lightDir = cmtcs->_vmatrix.inverse().column(2).xyz().normalized() * -1.0f;

  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, VP);
  _material->bindParamVec4(_param_modcolor, fvec4(1, 1, 1, 1));
  _material->bindParamVec3(_param_lightdir, lightDir);
  _material->bindParamFloat(_param_planesize, size);  // Pass size for edge alpha
  _material->bindParamFloat(_param_unlit, 1.0f);  // Planes are unlit
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

  float axisLen = scale * controller->_axis_length_scale;
  float thickness = scale * controller->_axis_thickness_scale;
  float coneH = axisLen * 0.15f;
  float coneR = coneH * 0.577f;  // tan(30 degrees) for sharper cone
  float planeOffset = 0.0f;  // At origin
  float planeSize = scale * controller->_plane_handle_scale;

  // Get dimming factors for axes (perpendicular to camera = bright)
  auto getAxisDimFactor = [&](const fvec3& axisDir, editor::ManipAxis axis) -> float {
    if (dragging) {
      switch (axis) {
        case editor::ManipAxis::X: return controller->_drag_start_dim_x;
        case editor::ManipAxis::Y: return controller->_drag_start_dim_y;
        case editor::ManipAxis::Z: return controller->_drag_start_dim_z;
        default: return 1.0f;
      }
    }
    return controller->_computeAxisDimFactor(axisDir);
  };

  // Get dimming factors for planes (facing camera = bright)
  auto getPlaneDimFactor = [&](const fvec3& planeNormal, editor::ManipAxis axis) -> float {
    if (dragging) {
      switch (axis) {
        case editor::ManipAxis::XY: return controller->_drag_start_dim_xy;
        case editor::ManipAxis::XZ: return controller->_drag_start_dim_xz;
        case editor::ManipAxis::YZ: return controller->_drag_start_dim_yz;
        default: return 1.0f;
      }
    }
    return controller->_computePlaneDimFactor(planeNormal);
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
    // Apply dimming
    color.x *= dimFactor;
    color.y *= dimFactor;
    color.z *= dimFactor;
    return color;
  };

  // Get camera position for view-dependent plane placement
  fvec3 camPos = controller->_getCameraEye();
  fvec3 camToGizmo = camPos - pos;

  // For LOCAL mode, transform camera direction to object's local space
  if (controller->space() == editor::ManipSpace::LOCAL) {
    fquat targetRot = controller->target()->getWorldRotation();
    fquat invRot = targetRot.inverse();
    camToGizmo = invRot.transform(camToGizmo);
  }

  // Compute view-dependent signs for each plane (extends away from camera)
  // Based on local-space (or world-space) camera direction
  float signXY_X = (camToGizmo.x > 0) ? +1.0f : -1.0f;
  float signXY_Y = (camToGizmo.y > 0) ? +1.0f : -1.0f;

  float signXZ_X = (camToGizmo.x > 0) ? +1.0f : -1.0f;
  float signXZ_Z = (camToGizmo.z > 0) ? +1.0f : -1.0f;

  float signYZ_Y = (camToGizmo.y > 0) ? +1.0f : -1.0f;
  float signYZ_Z = (camToGizmo.z > 0) ? +1.0f : -1.0f;

  // Plane handles (XY, XZ, YZ) - cornered at origin, extending away from camera
  // Draw planes FIRST so they sort underneath axes
  float dimXY = getPlaneDimFactor(axisZ, editor::ManipAxis::XY);  // XY plane has Z normal
  float dimXZ = getPlaneDimFactor(axisY, editor::ManipAxis::XZ);  // XZ plane has Y normal
  float dimYZ = getPlaneDimFactor(axisX, editor::ManipAxis::YZ);  // YZ plane has X normal

  fvec4 colorXY = getColor(editor::ManipAxis::XY, fvec4(0.6f, 0.6f, 0.1f, 1.0f), dimXY);
  fvec4 colorXZ = getColor(editor::ManipAxis::XZ, fvec4(0.6f, 0.1f, 0.6f, 1.0f), dimXZ);
  fvec4 colorYZ = getColor(editor::ManipAxis::YZ, fvec4(0.1f, 0.6f, 0.6f, 1.0f), dimYZ);

  _drawPlaneHandle(ctx, RCFD, VP, pos, axisX, axisY, colorXY, signXY_X, signXY_Y, planeSize);
  _drawPlaneHandle(ctx, RCFD, VP, pos, axisX, axisZ, colorXZ, signXZ_X, signXZ_Z, planeSize);
  _drawPlaneHandle(ctx, RCFD, VP, pos, axisY, axisZ, colorYZ, signYZ_Y, signYZ_Z, planeSize);

  // Offset cylinders from origin to avoid overlap at center
  float cylinderOffset = thickness * 2.0f;
  float cylinderLen = (axisLen - coneH) - cylinderOffset;

  // X axis
  float dimX = getAxisDimFactor(axisX, editor::ManipAxis::X);
  fvec4 colorX = getColor(editor::ManipAxis::X, _data->_colorX, dimX);
  _drawAxis(ctx, RCFD, VP, pos + axisX * cylinderOffset, axisX, colorX, cylinderLen, thickness);
  _drawCone(ctx, RCFD, VP, pos + axisX * (axisLen - coneH), axisX, colorX, coneR, coneH);

  // Y axis
  float dimY = getAxisDimFactor(axisY, editor::ManipAxis::Y);
  fvec4 colorY = getColor(editor::ManipAxis::Y, _data->_colorY, dimY);
  _drawAxis(ctx, RCFD, VP, pos + axisY * cylinderOffset, axisY, colorY, cylinderLen, thickness);
  _drawCone(ctx, RCFD, VP, pos + axisY * (axisLen - coneH), axisY, colorY, coneR, coneH);

  // Z axis
  float dimZ = getAxisDimFactor(axisZ, editor::ManipAxis::Z);
  fvec4 colorZ = getColor(editor::ManipAxis::Z, _data->_colorZ, dimZ);
  _drawAxis(ctx, RCFD, VP, pos + axisZ * cylinderOffset, axisZ, colorZ, cylinderLen, thickness);
  _drawCone(ctx, RCFD, VP, pos + axisZ * (axisLen - coneH), axisZ, colorZ, coneR, coneH);
}

void ManipGizmoDrawableImpl::_drawRotateGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
                                                const fvec3& pos, float scale) {
  auto controller = _data->_controller;
  if (!controller || !controller->target()) return;

  auto hovered = controller->hoveredAxis();
  auto active = controller->activeAxis();
  bool dragging = controller->isDragging();

  float radius = scale * controller->_ring_radius_scale;
  float thick = scale * controller->_ring_tube_radius_scale;

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
  // When dragging, use cached values from drag start to keep brightness constant
  auto getDimFactor = [&](const fvec3& ringNormal, editor::ManipAxis axis) -> float {
    // Use cached dimming state during drag
    if (dragging) {
      switch (axis) {
        case editor::ManipAxis::X: return controller->_drag_start_dim_x;
        case editor::ManipAxis::Y: return controller->_drag_start_dim_y;
        case editor::ManipAxis::Z: return controller->_drag_start_dim_z;
        default: return 1.0f;
      }
    }

    float dotProduct = fabs(ringNormal.dotWith(camDir));
    float angleDegrees = 90.0f - (acos(dotProduct) * 180.0f / PI);

    float threshold = controller->_min_ring_elevation_degrees;
    float transitionBand = 1.0f;  // 1 degree transition band

    if (angleDegrees >= threshold) {
      return 1.0f;  // Full brightness - active
    } else if (angleDegrees >= threshold - transitionBand) {
      // Sharp linear transition over 1 degree
      float t = (angleDegrees - (threshold - transitionBand)) / transitionBand;
      return 0.5f + 0.5f * t;
    } else {
      return 0.5f;  // Dimmed - inactive
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

  // X ring (rotates around local X axis, pattern starts at localY)
  float dimX = getDimFactor(localX, editor::ManipAxis::X);
  fvec4 colorX = getColor(editor::ManipAxis::X, _data->_colorX, dimX);
  _drawRing(ctx, RCFD, VP, pos, localX, localY, localZ, colorX, radius, thick);

  // Y ring (rotates around local Y axis, pattern starts at localZ)
  float dimY = getDimFactor(localY, editor::ManipAxis::Y);
  fvec4 colorY = getColor(editor::ManipAxis::Y, _data->_colorY, dimY);
  _drawRing(ctx, RCFD, VP, pos, localY, localZ, localX, colorY, radius, thick);

  // Z ring (rotates around local Z axis, pattern starts at localX)
  float dimZ = getDimFactor(localZ, editor::ManipAxis::Z);
  fvec4 colorZ = getColor(editor::ManipAxis::Z, _data->_colorZ, dimZ);
  _drawRing(ctx, RCFD, VP, pos, localZ, localX, localY, colorZ, radius, thick);
}

void ManipGizmoDrawableImpl::_drawScaleGizmo(Context* ctx, rcfd_ptr_t RCFD, const fmtx4& VP,
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

  float axisLen = scale * controller->_axis_length_scale;
  float thickness = scale * controller->_axis_thickness_scale;
  float cubeSize = axisLen * 0.1f;

  // Offset cylinders from origin to avoid overlap at center
  float cylinderOffset = thickness * 2.0f;
  float cylinderLen = (axisLen - cubeSize) - cylinderOffset;

  auto getColor = [&](editor::ManipAxis axis, const fvec4& baseColor) -> fvec4 {
    if (dragging && active == axis) return _data->_colorActive;
    if (hovered == axis) return _data->_colorHighlight;
    return baseColor;
  };

  bool supportsNonUniform = controller->target()->supportsNonUniformScaling();

  if (supportsNonUniform) {
    // X axis with cube (non-uniform scale)
    fvec4 colorX = getColor(editor::ManipAxis::X, _data->_colorX);
    _drawAxis(ctx, RCFD, VP, pos + axisX * cylinderOffset, axisX, colorX, cylinderLen, thickness);
    _drawCube(ctx, RCFD, VP, pos + axisX * axisLen, colorX, cubeSize);

    // Y axis with cube (non-uniform scale)
    fvec4 colorY = getColor(editor::ManipAxis::Y, _data->_colorY);
    _drawAxis(ctx, RCFD, VP, pos + axisY * cylinderOffset, axisY, colorY, cylinderLen, thickness);
    _drawCube(ctx, RCFD, VP, pos + axisY * axisLen, colorY, cubeSize);

    // Z axis with cube (non-uniform scale)
    fvec4 colorZ = getColor(editor::ManipAxis::Z, _data->_colorZ);
    _drawAxis(ctx, RCFD, VP, pos + axisZ * cylinderOffset, axisZ, colorZ, cylinderLen, thickness);
    _drawCube(ctx, RCFD, VP, pos + axisZ * axisLen, colorZ, cubeSize);
  }

  // Center cube for uniform scale
  fvec4 colorFree = getColor(editor::ManipAxis::FREE, fvec4(0.8f, 0.8f, 0.8f, 1.0f));
  _drawCube(ctx, RCFD, VP, pos, colorFree, cubeSize * 1.5f);
}

///////////////////////////////////////////////////////////////////////////////

void ManipGizmoDrawableImpl::_render(const RenderContextInstData& RCID) {
  auto context = RCID.context();


  auto controller = _data->_controller;
  if (!controller) {
    //printf("ManipGizmo: no controller\n");
    return;
  }
  if (!controller->target()) {
    //printf("ManipGizmo: no target\n");
    return;
  }
  auto RCFD = RCID.rcfd();
  const auto& CPD = RCFD->topCPD();

  if (CPD.isPicking()) {
    return;  // Don't render gizmo during picking passes (yet)
  }

  if (!_initted) {
    gpuInit(context);
  }

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
  drw->_sortkey = 1<<30;  // Render late (on top of scene)
  drw->SetRenderCallback(ManipGizmoDrawableImpl::renderGizmo);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

ManipGizmoDrawableData::ManipGizmoDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

ManipGizmoDrawableData::~ManipGizmoDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
