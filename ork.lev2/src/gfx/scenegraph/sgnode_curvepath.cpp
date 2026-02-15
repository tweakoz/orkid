////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/sgnode_curvepath.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/reflect/properties/registerX.inl>

///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::CurvePathDrawableData, "CurvePathDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static const char* CURVEPATH_SHADER = R"(
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
vertex_shader vs_curvepath : iface_vtx {
  gl_Position = mvp * position;
  frg_clr = vtxcolor * modcolor;
}
fragment_shader fs_curvepath : iface_frg {
  out_color = frg_clr;
}
state_block sb_curvepath : default {
  CullTest = OFF;
  DepthTest = LEQUALS;
  DepthMask = true;
}
technique tek_curvepath {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader = vs_curvepath;
    fragment_shader = fs_curvepath;
    state_block = sb_curvepath;
  }
}
)";

///////////////////////////////////////////////////////////////////////////////

CurvePathDrawableImpl::CurvePathDrawableImpl(const CurvePathDrawableData* data)
    : _data(data) {
}

///////////////////////////////////////////////////////////////////////////////

CurvePathDrawableImpl::~CurvePathDrawableImpl() {
}

///////////////////////////////////////////////////////////////////////////////

void CurvePathDrawableImpl::gpuInit(lev2::Context* ctx) {
  _lineMaterial = std::make_shared<FreestyleMaterial>();
  _lineMaterial->gpuInitFromShaderText(ctx, "curvepath_shader", CURVEPATH_SHADER);
  _lineTechnique = _lineMaterial->technique("tek_curvepath");
  _paramMVP = _lineMaterial->param("mvp");
  _paramModColor = _lineMaterial->param("modcolor");
  _initted = true;
}

///////////////////////////////////////////////////////////////////////////////

void CurvePathDrawableImpl::_render(const RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto context = RCID.context();

  if (not _initted) {
    gpuInit(context);
  }

  bool isPickState = context->FBI()->isPickState();
  if (isPickState) {
    return;
  }

  auto curve = _data->_curve;
  if (!curve || curve->numPoints() < 2) {
    return;
  }

  auto RCFD = RCID.rcfd();
  const auto& CPD = RCFD->topCPD();
  auto V = CPD.cameraMatrices()->_vmatrix;
  auto P = CPD.cameraMatrices()->_pmatrix;
  fmtx4 MVP = P * V;

  int numSegs = curve->numPoints() - 1;
  int subdivs = _data->_lineSubdivisions;
  int numVerts = numSegs * subdivs + 1;

  using vtx_t = SVtxV16T16C16;
  auto vb = GfxEnv::GetSharedDynamicV16T16C16();
  VtxWriter<vtx_t> vw;
  vw.Lock(context, vb.get(), numVerts);

  auto lineColor = _data->_lineColor;

  for (int seg = 0; seg < numSegs; seg++) {
    for (int sub = 0; sub < subdivs; sub++) {
      float t_base = curve->getPoint(seg)._time;
      float t_next = curve->getPoint(seg + 1)._time;
      float frac = float(sub) / float(subdivs);
      float t = t_base + (t_next - t_base) * frac;
      fvec3 pos = curve->samplePosition(t);
      vw.AddVertex(vtx_t(fvec4(pos, 1.0f), fvec4(0, 0, 0, 0), lineColor));
    }
  }
  // last point
  {
    fvec3 pos = curve->samplePosition(curve->getPoint(numSegs)._time);
    vw.AddVertex(vtx_t(fvec4(pos, 1.0f), fvec4(0, 0, 0, 0), lineColor));
  }

  vw.UnLock(context);

  auto fxi = context->FXI();
  auto gbi = context->GBI();

  fxi->pushRasterState(_lineMaterial->_rasterstate);
  _lineMaterial->begin(_lineTechnique, RCFD);
  _lineMaterial->bindParamMatrix(_paramMVP, MVP);
  _lineMaterial->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vw, PrimitiveType::LINESTRIP);
  _lineMaterial->end(RCFD);
  fxi->popRasterState();
}

///////////////////////////////////////////////////////////////////////////////

void CurvePathDrawableImpl::renderCurvePath(RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto drawable = renderable->_drawable;
  drawable->_implA.getShared<CurvePathDrawableImpl>()->_render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void CurvePathDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

CurvePathDrawableData::CurvePathDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

CurvePathDrawableData::~CurvePathDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t CurvePathDrawableData::createDrawable() const {
  auto drw = std::make_shared<CallbackDrawable>(nullptr);
  auto impl = drw->_implA.makeShared<CurvePathDrawableImpl>(this);
  drw->_sortkey = 50;
  drw->SetRenderCallback(CurvePathDrawableImpl::renderCurvePath);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t CurvePathDrawableData::createControlPointDrawable() const {
  auto idata = std::make_shared<InstancedModelDrawableData>("data://tests/pbr_calib.glb");
  idata->resize(256);
  auto drw = std::dynamic_pointer_cast<InstancedModelDrawable>(idata->createDrawable());
  _cpDrawable = drw;
  _cpInstanceData = drw->_instancedata;
  updateControlPoints();
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

void CurvePathDrawableData::updateControlPoints() const {
  if (!_cpInstanceData || !_curve) {
    return;
  }
  int n = _curve->numPoints();
  for (int i = 0; i < n && i < 256; i++) {
    const auto& pt = _curve->getPoint(i);
    fmtx4 mtx;
    mtx.compose(pt._position, fquat(), _controlPointScale);
    _cpInstanceData->_worldmatrices[i] = mtx;
    _cpInstanceData->_modcolors[i] = (i == _selectedPointIndex) ? _cpSelectedColor : _cpColor;
  }
  // zero out remaining
  for (int i = n; i < 256; i++) {
    fmtx4 mtx;
    mtx.compose(fvec3(0, -10000, 0), fquat(), 0.0f);
    _cpInstanceData->_worldmatrices[i] = mtx;
    _cpInstanceData->_modcolors[i] = fvec4(0, 0, 0, 0);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
