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

UISurfaceRenderImpl::UISurfaceRenderImpl(std::shared_ptr<const UISurfacePrimitiveData> data, ui::layoutsurface_ptr_t surface)
    : _data(data)
    , _layoutSurface(surface) {
  // Use the LayoutSurface's owned context (it creates its own in constructor)
  if (_layoutSurface) {
    _uiContext = _layoutSurface->_ownedContext;
  }
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
  _material->_rasterstate->_priority = 1<<10;

  // UIContext setup is done in LayoutSurface constructor

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

  fvec3 center = _worldTransform ? _worldTransform->_translation : fvec3(0);
  fvec3 right, up, normal;
  computeBillboardAxes(camMtx, center, right, up, normal);

  float aspectRatio = float(_layoutSurface->width()) / float(_layoutSurface->height());
  float halfH = _data->_size * 0.5f;
  float halfW = halfH * aspectRatio;

  corner00_out = center - right * halfW - up * halfH;  // bottom-left
  corner10_out = center + right * halfW - up * halfH;  // bottom-right
  corner11_out = center + right * halfW + up * halfH;  // top-right
  corner01_out = center - right * halfW + up * halfH;  // top-left
}

//////////////////////////////////////////////////////////////

fmtx4 UISurfaceRenderImpl::computeWorldToSurface(const CameraMatrices& camMtx, const fvec3& center) const {
  fvec3 right, up, normal;
  computeBillboardAxes(camMtx, center, right, up, normal);

  float aspectRatio = float(_layoutSurface->width()) / float(_layoutSurface->height());
  float halfH = _data->_size * 0.5f;
  float halfW = halfH * aspectRatio;

  // To transform world -> surface local:
  // 1. Translate so center is at origin
  // 2. Project onto billboard axes and scale to [-0.5, 0.5]
  //
  // For a point P in world space:
  //   localX = dot(P - center, right) / (halfW * 2)
  //   localY = dot(P - center, up) / (halfH * 2)
  //
  // This gives us coordinates in [-0.5, 0.5] when on the billboard

  // Build world-to-surface matrix directly
  // Row 0: right / (halfW * 2), with translation component
  // Row 1: up / (halfH * 2), with translation component
  // Row 2: normal (for completeness)
  // Row 3: 0, 0, 0, 1

  fvec3 scaledRight = right / (halfW * 2.0f);
  fvec3 scaledUp = up / (halfH * 2.0f);

  fmtx4 worldToSurface;
  worldToSurface.setRow(0, fvec4(scaledRight.x, scaledRight.y, scaledRight.z, -center.dotWith(scaledRight)));
  worldToSurface.setRow(1, fvec4(scaledUp.x, scaledUp.y, scaledUp.z, -center.dotWith(scaledUp)));
  worldToSurface.setRow(2, fvec4(normal.x, normal.y, normal.z, -center.dotWith(normal)));
  worldToSurface.setRow(3, fvec4(0, 0, 0, 1));

  return worldToSurface;
}

//////////////////////////////////////////////////////////////

bool UISurfaceRenderImpl::rayIntersect(
    const fray3& worldRay,
    const CameraMatrices& camMtx,
    fvec2& uv_out,
    fvec3& worldHitPos_out) const {

  fvec3 center =  _worldTransform->_translation;
  bool view_relative = _worldTransform->_view_relative;
  if(view_relative) {
    auto vmtx = camMtx.GetIVMatrix();
    center = center.transform(vmtx).xyz();
  }
  fray3 ray = worldRay;



  if(0)printf("rayIntersect: _worldTransform=%p center=(%f,%f,%f)\n",
         _worldTransform.get(), center.x, center.y, center.z);
  if(0)printf("  ray origin=(%f,%f,%f) dir=(%f,%f,%f)\n",
         ray.mOrigin.x, ray.mOrigin.y, ray.mOrigin.z,
         ray.mDirection.x, ray.mDirection.y, ray.mDirection.z);

  fvec3 right, up, normal;
  computeBillboardAxes(camMtx, center, right, up, normal);

  // Create plane from billboard
  fplane3 billboardPlane(normal, center);

  // Ray-plane intersection (in view space if view_relative, else world space)
  fvec3 hitPos;
  float t;
  if (!billboardPlane.Intersect(ray, t, hitPos)) {
    if(0)printf("  plane intersection failed\n");
    return false;
  }

  if(0)printf("  plane t=%f hitPos=(%f,%f,%f)\n", t, hitPos.x, hitPos.y, hitPos.z);

  // Check if intersection is in front of ray origin
  if (t < 0) {
    if(0)printf("  t < 0, behind camera\n");
    return false;
  }

  // Compute world-to-surface transform
  fmtx4 worldToSurface = computeWorldToSurface(camMtx, center);

  // Transform hit position to surface local space
  fvec4 localHit = worldToSurface * fvec4(hitPos, 1.0f);

  if(0)printf("  localHit=(%f,%f,%f,%f)\n", localHit.x, localHit.y, localHit.z, localHit.w);

  // Local coordinates are in [-0.5, 0.5] range
  // Convert to UV [0, 1] range, flip V for Vulkan convention
  float u = localHit.x + 0.5f;
  float v = 1.0f - (localHit.y + 0.5f);

  if(0)printf("  uv=(%f,%f)\n", u, v);

  // Check if within quad bounds
  if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
    if(0)printf("  uv out of bounds\n");
    return false;
  }

  uv_out = fvec2(u, v);

  worldHitPos_out = hitPos;

  return true;
}

//////////////////////////////////////////////////////////////

ui::HandlerResult UISurfaceRenderImpl::routeUiEvent(
    int viewportWidth,
    int viewportHeight,
    const CameraMatrices& camMtx,
    ui::event_constptr_t ev) {

  ui::HandlerResult result;
  if (!_layoutSurface) {
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

  // Hit test
  fvec2 uv;
  fvec3 worldHitPos;
  fray3 worldRay(rayOrigin, rayDir);
  bool hit = rayIntersect(worldRay, camMtx, uv, worldHitPos);

  // Handle enter/leave
  if (hit && !_mouseInside) {
    _mouseInside = true;
    // Generate MOUSE_ENTER event
    auto enterEv = std::make_shared<ui::Event>(*ev);
    enterEv->_eventcode = ui::EventCode::MOUSE_ENTER;
    int pixelX = int(uv.x * _layoutSurface->width());
    int pixelY = int(uv.y * _layoutSurface->height());
    enterEv->miX = pixelX;
    enterEv->miY = pixelY;
    _layoutSurface->handleUiEvent(enterEv);
  } else if (!hit && _mouseInside) {
    _mouseInside = false;
    // Generate MOUSE_LEAVE event
    auto leaveEv = std::make_shared<ui::Event>(*ev);
    leaveEv->_eventcode = ui::EventCode::MOUSE_LEAVE;
    _layoutSurface->handleUiEvent(leaveEv);
    return result;
  }

  if (!hit) {
    return result;
  }

  // Transform event coordinates to surface pixel space
  int pixelX = int(uv.x * _layoutSurface->width());
  int pixelY = int((1.0f - uv.y) * _layoutSurface->height());  // Flip Y for UI coords

  // Create transformed event
  auto surfaceEv = std::make_shared<ui::Event>(*ev);
  surfaceEv->miX = pixelX;
  surfaceEv->miY = pixelY;

  // Route to the layout surface
  result = _layoutSurface->handleUiEvent(surfaceEv);
  result.mHandler = _layoutSurface.get();

  return result;
}

//////////////////////////////////////////////////////////////

void UISurfaceRenderImpl::render(const RenderContextInstData& RCID) {
  auto ctx = RCID.context();
  auto fxi = ctx->FXI();

  if (!_layoutSurface) {
    return;
  }

  // Get world transform from RCID (set by the drawable node)
  fmtx4 worldMtx = RCID.worldMatrix();
  fvec3 center = _worldTransform->_translation;


  // Lazy GPU init
  if (!_initted) {
    gpuInit(ctx);
  }

  // Get camera matrices
  auto RCFD = ctx->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  auto cmtcs = CPD.cameraMatrices();

  if(_worldTransform->_view_relative) {
    // For stereo: use center camera (no IPD offset) so surface doesn't shift between eyes
    // For mono: use the current camera
    fmtx4 vmatrix;
    if (CPD._stereo_cam_matrices && CPD._stereo_cam_matrices->_mono) {
      vmatrix = CPD._stereo_cam_matrices->_mono->GetIVMatrix();
    } else {
      vmatrix = cmtcs->GetIVMatrix();
    }
    center = center.transform(vmatrix).xyz();
  }


  // Compute billboard axes on-demand
  fvec3 right, up, normal;
  computeBillboardAxes(*cmtcs, center, right, up, normal);

  // Compute quad corners - need to set _worldTransform temporarily for this call
  // Create a temporary DecompTransform with the center position
  auto tempXf = std::make_shared<DecompTransform>();
  tempXf->_translation = center;
  auto savedXf = _worldTransform;
  const_cast<UISurfaceRenderImpl*>(this)->_worldTransform = tempXf;

  fvec3 corner00, corner10, corner11, corner01;
  computeQuadCorners(*cmtcs, corner00, corner10, corner11, corner01);

  const_cast<UISurfaceRenderImpl*>(this)->_worldTransform = savedXf;

  // Ensure UI texture is current
  _layoutSurface->updateTextureIfNeeded(ctx);

  // Debug: log layoutGroup geometry
  auto lg = _layoutSurface->layoutGroup();
  static bool dumped = false;
  if (!dumped) {
    lg->dumpLayoutHierarchy();
    dumped = true;
  }

  // Get texture
  if (!_layoutSurface->_rtgroup) {
    return;
  }

  auto texture = _layoutSurface->_rtgroup->buffer(0)->texture();
  if (!texture) {
    return;
  }

  // Build vertex data using SVtxV16T16C16 (position, texcoord, color)
  using vtx_t = SVtxV16T16C16;
  auto vb = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;
  vw.Lock(ctx, vb.get(), 6);

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

  fxi->pushRasterState(_material->_rasterstate);
  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_param_mvp, MVP);
  _material->bindParamTexture(_param_colormap, texture);
  _material->bindParamVec2(_param_texdim, fvec2(_layoutSurface->width(), _layoutSurface->height()));
  _material->bindParamFloat(_param_maxsamples, _data->_maxSamplesPerAxis);
  ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  _material->end(RCFD);
  fxi->popRasterState();
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
  if (cbdrawable && cbdrawable->GetUserDataA().isShared<UISurfaceRenderImpl>()) {
    return cbdrawable->GetUserDataA().getShared<UISurfaceRenderImpl>();
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void UISurfacePrimitiveData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t UISurfacePrimitiveData::createDrawable() const {
  // Default implementation with no surface - caller must set surface via the impl
  return createDrawable(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t UISurfacePrimitiveData::createDrawable(ui::layoutsurface_ptr_t surface) const {
  auto impl = std::make_shared<UISurfaceRenderImpl>(dataShared<UISurfacePrimitiveData>(), surface);
  auto rval = std::make_shared<CallbackDrawable>(nullptr);

  rval->SetRenderCallback(UISurfaceRenderImpl::renderCallback);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10000;  // Render after opaque geometry

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

UISurfacePrimitiveData::UISurfacePrimitiveData()
    : _size(1.0f)
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
