////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/sgnode_uisurface.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/context.h>
#include <ork/math/plane.hpp>

static const char* UISURFACE_SHADER = R"(
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_set ublock {
  mat4 mvp;
  vec2 texDim;      // texture dimensions (width, height)
  float maxSamples; // max samples per axis (e.g., 4 = 4x4 grid, 8 = 8x8 grid)
}
sampler_set sset (descriptor_set 0) {
  sampler2D ColorMap;
}
vertex_interface iface_vtx : ublock {
  inputs {
    vec4 position : POSITION;
    vec2 uv : TEXCOORD0;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    vec2 frg_uv;
    vec4 frg_clr;
  }
}
fragment_interface iface_frg : ublock : sset {
  inputs {
    vec2 frg_uv;
    vec4 frg_clr;
  }
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
vertex_shader vs_uisurface : iface_vtx {
  gl_Position = mvp * position;
  frg_uv = uv;
  frg_clr = vtxcolor;
}
fragment_shader fs_uisurface : iface_frg {
  // Compute screen-space derivatives of UV (in texel units)
  vec2 uvTexels = frg_uv * texDim;
  vec2 duvdx = dFdx(uvTexels);
  vec2 duvdy = dFdy(uvTexels);

  // Compute the texel footprint size (how many texels per pixel)
  float footprintX = length(vec2(duvdx.x, duvdy.x));
  float footprintY = length(vec2(duvdx.y, duvdy.y));
  float footprint = max(footprintX, footprintY);

  // Determine sample count based on footprint, clamped to maxSamples
  // For footprint <= 1, use 1 sample; for footprint >= maxSamples, use maxSamples
  int samplesPerAxis = clamp(int(ceil(footprint)), 1, int(maxSamples));

  // Early out for 1:1 or magnification - single sample
  if (samplesPerAxis <= 1) {
    out_color = texture(ColorMap, frg_uv) * frg_clr;
    return;
  }

  // Compute sample step in UV space
  vec2 texelSize = 1.0 / texDim;
  float fSamples = float(samplesPerAxis);

  // Sample grid centered on the pixel
  vec4 accumColor = vec4(0.0);
  float halfSpan = (fSamples - 1.0) * 0.5;

  for (int y = 0; y < samplesPerAxis; y++) {
    for (int x = 0; x < samplesPerAxis; x++) {
      vec2 offset = vec2(float(x) - halfSpan, float(y) - halfSpan) / fSamples;
      vec2 sampleUV = frg_uv + offset * texelSize * footprint;
      accumColor += texture(ColorMap, sampleUV);
    }
  }

  out_color = (accumColor / (fSamples * fSamples)) * frg_clr;
}
technique tek_uisurface {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader = vs_uisurface;
    fragment_shader = fs_uisurface;
    state_block = default;
  }
}
)";

///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::UISurfacePrimitiveData, "UISurfacePrimitiveData");

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// UISurfaceRenderImpl implementation
///////////////////////////////////////////////////////////////////////////////

UISurfaceRenderImpl::UISurfaceRenderImpl(const UISurfacePrimitiveData* data)
    : _data(data) {
  _uiContext = std::make_shared<ui::Context>();
}

UISurfaceRenderImpl::~UISurfaceRenderImpl() {
}

//////////////////////////////////////////////////////////////

void UISurfaceRenderImpl::gpuInit(Context* ctx) {
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInitFromShaderText(ctx, "uisurface_shader", UISURFACE_SHADER);
  _technique = _material->technique("tek_uisurface");
  _param_mvp = _material->param("mvp");
  _param_colormap = _material->param("ColorMap");
  _param_texdim = _material->param("texDim");
  _param_maxsamples = _material->param("maxSamples");

  _material->_rasterstate->setBlendingMacro(_data->_blendMode);
  _material->_rasterstate->setCullTest(
      _data->_doubleSided ? ECullTest::OFF : ECullTest::PASS_BACK);
  _material->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
  _material->_rasterstate->setWriteMaskZ(true);

  // Set up UIContext with the LayoutSurface as top
  if (_data->_layoutSurface) {
    _uiContext->_top = _data->_layoutSurface->layoutGroup();
    _data->_layoutSurface->_uicontext = _uiContext.get();
  }

  _initted = true;
}

//////////////////////////////////////////////////////////////

void UISurfaceRenderImpl::computeBillboardAxes(
    const CameraMatrices& camMtx,
    const fvec3& center,
    fvec3& right_out,
    fvec3& up_out,
    fvec3& normal_out) {

  const CameraData& cdata = camMtx._camdat;
  fvec3 camPos = cdata.mEye;
  fvec3 camUp = cdata.mUp;

  // Billboard faces camera
  normal_out = (camPos - center).normalized();
  right_out = camUp.crossWith(normal_out).normalized();
  up_out = normal_out.crossWith(right_out).normalized();
}

//////////////////////////////////////////////////////////////

void UISurfaceRenderImpl::computeQuadCorners(
    const CameraMatrices& camMtx,
    fvec3& corner00_out,
    fvec3& corner10_out,
    fvec3& corner11_out,
    fvec3& corner01_out) const {

  fvec3 right, up, normal;
  computeBillboardAxes(camMtx, _data->_center, right, up, normal);

  auto surface = _data->_layoutSurface;
  float aspectRatio = float(surface->width()) / float(surface->height());
  float halfH = _data->_size * 0.5f;
  float halfW = halfH * aspectRatio;

  corner00_out = _data->_center - right * halfW - up * halfH;  // bottom-left
  corner10_out = _data->_center + right * halfW - up * halfH;  // bottom-right
  corner11_out = _data->_center + right * halfW + up * halfH;  // top-right
  corner01_out = _data->_center - right * halfW + up * halfH;  // top-left
}

//////////////////////////////////////////////////////////////

fmtx4 UISurfaceRenderImpl::computeWorldToSurface(const CameraMatrices& camMtx) const {
  fvec3 right, up, normal;
  computeBillboardAxes(camMtx, _data->_center, right, up, normal);

  auto surface = _data->_layoutSurface;
  float aspectRatio = float(surface->width()) / float(surface->height());
  float halfH = _data->_size * 0.5f;
  float halfW = halfH * aspectRatio;

  // Build surface-to-world matrix
  // Surface local space: origin at center, X = right, Y = up
  // Coordinates in [-0.5, 0.5] range
  fmtx4 surfaceToWorld;
  surfaceToWorld.setColumn(0, fvec4(right * halfW * 2.0f, 0));
  surfaceToWorld.setColumn(1, fvec4(up * halfH * 2.0f, 0));
  surfaceToWorld.setColumn(2, fvec4(normal, 0));
  surfaceToWorld.setColumn(3, fvec4(_data->_center, 1));

  return surfaceToWorld.inverse();
}

//////////////////////////////////////////////////////////////

bool UISurfaceRenderImpl::rayIntersect(
    const fray3& worldRay,
    const CameraMatrices& camMtx,
    fvec2& uv_out,
    fvec3& worldHitPos_out) const {

  fvec3 right, up, normal;
  computeBillboardAxes(camMtx, _data->_center, right, up, normal);

  // Create plane from billboard
  fplane3 billboardPlane(normal, _data->_center);

  // Ray-plane intersection
  float t;
  if (!billboardPlane.Intersect(worldRay, t, worldHitPos_out)) {
    return false;
  }

  // Check if intersection is in front of ray origin
  if (t < 0) return false;

  // Compute world-to-surface transform
  fmtx4 worldToSurface = computeWorldToSurface(camMtx);

  // Transform world hit position to surface local space
  fvec4 localHit = worldToSurface * fvec4(worldHitPos_out, 1.0f);

  // Local coordinates are in [-0.5, 0.5] range
  // Convert to UV [0, 1] range
  float u = localHit.x + 0.5f;
  float v = localHit.y + 0.5f;

  // Check if within quad bounds
  if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
    return false;
  }

  uv_out = fvec2(u, v);
  return true;
}

//////////////////////////////////////////////////////////////

ui::HandlerResult UISurfaceRenderImpl::routeUiEvent(
    int viewportWidth,
    int viewportHeight,
    const CameraMatrices& camMtx,
    ui::event_constptr_t ev) {

  ui::HandlerResult result;
  auto surface = _data->_layoutSurface;
  if (!surface) {
    return result;
  }

  // Generate ray from mouse position
  float nx = (2.0f * ev->miX / viewportWidth) - 1.0f;
  float ny = 1.0f - (2.0f * ev->miY / viewportHeight);

  // Unproject near and far points
  fmtx4 invVP = (camMtx._pmatrix * camMtx._vmatrix).inverse();
  fvec4 nearPt = invVP * fvec4(nx, ny, -1.0f, 1.0f);
  fvec4 farPt = invVP * fvec4(nx, ny, 1.0f, 1.0f);
  nearPt /= nearPt.w;
  farPt /= farPt.w;

  fvec3 rayOrigin = nearPt.xyz();
  fvec3 rayDir = (farPt.xyz() - rayOrigin).normalized();
  fray3 worldRay(rayOrigin, rayDir);

  // Hit test
  fvec2 uv;
  fvec3 worldHitPos;
  bool hit = rayIntersect(worldRay, camMtx, uv, worldHitPos);

  // Handle enter/leave
  if (hit && !_mouseInside) {
    _mouseInside = true;
    // Generate MOUSE_ENTER event
    auto enterEv = std::make_shared<ui::Event>(*ev);
    enterEv->_eventcode = ui::EventCode::MOUSE_ENTER;
    int pixelX = int(uv.x * surface->width());
    int pixelY = int((1.0f - uv.y) * surface->height());  // Flip Y
    enterEv->miX = pixelX;
    enterEv->miY = pixelY;
    surface->handleUiEvent(enterEv);
  } else if (!hit && _mouseInside) {
    _mouseInside = false;
    // Generate MOUSE_LEAVE event
    auto leaveEv = std::make_shared<ui::Event>(*ev);
    leaveEv->_eventcode = ui::EventCode::MOUSE_LEAVE;
    surface->handleUiEvent(leaveEv);
    return result;
  }

  if (!hit) {
    return result;
  }

  // Transform event coordinates to surface pixel space
  int pixelX = int(uv.x * surface->width());
  int pixelY = int((1.0f - uv.y) * surface->height());  // Flip Y for UI coords

  // Create transformed event
  auto surfaceEv = std::make_shared<ui::Event>(*ev);
  surfaceEv->miX = pixelX;
  surfaceEv->miY = pixelY;

  // Route to the layout surface
  result = surface->handleUiEvent(surfaceEv);
  result.mHandler = surface.get();

  return result;
}

//////////////////////////////////////////////////////////////

void UISurfaceRenderImpl::render(const RenderContextInstData& RCID) {
  auto ctx = RCID.context();
  auto surface = _data->_layoutSurface;

  if (!surface) {
    return;
  }

  // Lazy GPU init
  if (!_initted) {
    gpuInit(ctx);
  }

  // Get camera matrices
  auto RCFD = ctx->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();

  // Compute billboard axes on-demand
  fvec3 right, up, normal;
  computeBillboardAxes(*cmtcs, _data->_center, right, up, normal);

  // Compute quad corners on-demand
  fvec3 corner00, corner10, corner11, corner01;
  computeQuadCorners(*cmtcs, corner00, corner10, corner11, corner01);

  // Ensure UI texture is current
  surface->updateTextureIfNeeded(ctx);

  // Debug: log layoutGroup geometry
  auto lg = surface->layoutGroup();
  static bool dumped = false;
  if (!dumped) {
    lg->dumpLayoutHierarchy();
    dumped = true;
  }

  // Get texture
  if (!surface->_rtgroup) {
    return;
  }

  auto texture = surface->_rtgroup->buffer(0)->texture();
  if (!texture) {
    return;
  }

  // Build vertex data using SVtxV16T16C16 (position, texcoord, color)
  using vtx_t = SVtxV16T16C16;
  auto& VB = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;
  vw.Lock(ctx, &VB, 6);

  fvec4 white(1, 1, 1, 1);

  // UV coordinates (note Y-flip for texture orientation)
  // corner00 = bottom-left in world, UV (0, 1)
  // corner11 = top-right in world, UV (1, 0)

  // Triangle 1: bottom-left, top-right, bottom-right (flipped winding)
  vw.AddVertex(vtx_t(corner00, fvec4(0, 1, 0, 0), white));
  vw.AddVertex(vtx_t(corner11, fvec4(1, 0, 0, 0), white));
  vw.AddVertex(vtx_t(corner10, fvec4(1, 1, 0, 0), white));

  // Triangle 2: bottom-left, top-left, top-right (flipped winding)
  vw.AddVertex(vtx_t(corner00, fvec4(0, 1, 0, 0), white));
  vw.AddVertex(vtx_t(corner01, fvec4(0, 0, 0, 0), white));
  vw.AddVertex(vtx_t(corner11, fvec4(1, 0, 0, 0), white));

  vw.UnLock(ctx);

  // Compute MVP from camera
  const auto& V = cmtcs->_vmatrix;
  const auto& P = cmtcs->_pmatrix;
  fmtx4 MVP = P * V;  // Model is identity since corners are in world space

  // Draw with freestyle material
  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, MVP);
  _material->bindParamTexture(_param_colormap, texture);
  _material->bindParamVec2(_param_texdim, fvec2(surface->width(), surface->height()));
  _material->bindParamFloat(_param_maxsamples, _data->_maxSamplesPerAxis);
  ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

//////////////////////////////////////////////////////////////

void UISurfaceRenderImpl::renderCallback(RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  renderable->GetDrawableDataA().getShared<UISurfaceRenderImpl>()->render(RCID);
}

///////////////////////////////////////////////////////////////////////////////
// Helper to get render impl from drawable
///////////////////////////////////////////////////////////////////////////////

uisurface_renderimpl_ptr_t getUISurfaceRenderImpl(drawable_ptr_t drawable) {
  auto cbdrawable = std::dynamic_pointer_cast<CallbackDrawable>(drawable);
  if (cbdrawable) {
    return cbdrawable->GetUserDataA().getShared<UISurfaceRenderImpl>();
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void UISurfacePrimitiveData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t UISurfacePrimitiveData::createDrawable() const {
  auto impl = std::make_shared<UISurfaceRenderImpl>(this);
  auto rval = std::make_shared<CallbackDrawable>(nullptr);

  rval->SetRenderCallback(UISurfaceRenderImpl::renderCallback);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10000;  // Render after opaque geometry

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

UISurfacePrimitiveData::UISurfacePrimitiveData()
    : _center(0, 0, 0)
    , _size(1.0f)
    , _blendMode(BlendingMacro::ALPHA)
    , _doubleSided(false)
    , _maxSamplesPerAxis(4.0f) {  // Default 4x4 = 16 samples for 16:1 minification
}

///////////////////////////////////////////////////////////////////////////////

UISurfacePrimitiveData::~UISurfacePrimitiveData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
