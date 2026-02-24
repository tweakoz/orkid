////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/opq.h>
#include <ork/kernel/orklut.hpp>

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/renderer.h>

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

#include "bullet_impl.h"
#include <ork/lev2/gfx/gfxvtxbuf.inl>

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

PhysicsDebugger::PhysicsDebugger() {
  for (int i = 0; i < 4; i++) {
    _lineqpool.push(new lineq_t);
  }
  _curreadlq = nullptr;
  DefaultColors mycolors;
  mycolors.m_activeObject = btVector3(.5, 1, .5);
  setDefaultColors(mycolors);
}

///////////////////////////////////////////////////////////////////////////////

BulletDebugDrawDBData::BulletDebugDrawDBData(PhysicsDebugger* debugger)
    : _debugger(debugger) {
  
  _DBRecs.resize(PhysicsDebugger::kmaxbuffers);
  //for (int i = 0; i < PhysicsDebugger::kmaxbuffers; i++) {
    //_DBRecs[i]._bulletSystem = system;
  //}
}

BulletDebugDrawDBData::~BulletDebugDrawDBData(){

}

///////////////////////////////////////////////////////////////////////////////

void bulletDebugEnqueueToLayer(ork::lev2::drawqueueitem_constptr_t cdb) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  auto drawable = cdb->_drawable;
  auto pdata = drawable->_implA.get<BulletDebugDrawDBData*>();
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::beginSimFrame(BulletSystem* system) {

  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  if (_enabled) {
    _currentwritelq = nullptr;
    bool got_one    = _lineqpool.try_pop(_currentwritelq);
    if (got_one) {
      assert(_currentwritelq != nullptr);
      _currentwritelq->clear();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::endSimFrame(BulletSystem* system) {

  if (_enabled and _currentwritelq) {
    system->BulletWorld()->debugDrawWorld();
    auto prevread = _curreadlq.exchange(_currentwritelq);
    if (prevread) {              // replacing old readbuffer
      _lineqpool.push(prevread); // so return old readbuffer to pool
    }
    _currentwritelq = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::beginRenderFrame() {
}

///////////////////////////////////////////////////////////////////////////////

void bulletDebugRender(const RenderContextInstData& RCID) {

  auto context    = RCID.context();
  auto renderable = dynamic_cast<const lev2::CallbackRenderable*>(RCID._irenderable);
  if (context->FBI()->isPickState())
    return;
  auto drawdata = renderable->_drawDataA.get<BulletDebugDrawDBData*>();
  if (!drawdata)
    return;
  auto debugger = drawdata->_debugger;
  if (!debugger->_enabled)
    return;

  // checkout the read buffer directly in the render callback
  auto lines = debugger->_curreadlq.exchange(nullptr);
  if (lines) {
    debugger->render(RCID, lines);
    // return the buffer to the pool
    auto prev = debugger->_curreadlq.exchange(lines);
    if (prev) {
      // sim posted a new one while we were rendering — return the old one
      debugger->_lineqpool.push(prev);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::endRenderFrame() {
}

///////////////////////////////////////////////////////////////////////////////

static const char* BULLET_DEBUG_SHADER = R"(
fxconfig fxcfg_bulletdebug {
  glsl_version = "330";
}
uniform_set ublock_bulletdebug {
  mat4 mvp;
  vec4 modcolor;
}
vertex_interface iface_vtx_bulletdebug : ublock_bulletdebug {
  inputs {
    vec4 position : POSITION;
    vec2 uv : TEXCOORD0;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    vec4 frg_clr;
  }
}
fragment_interface iface_frg_bulletdebug : ublock_bulletdebug {
  inputs {
    vec4 frg_clr;
  }
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
vertex_shader vs_bulletdebug : iface_vtx_bulletdebug {
  gl_Position = mvp * position;
  frg_clr = vec4(vtxcolor.xyz,1.0);
}
fragment_shader fs_bulletdebug : iface_frg_bulletdebug {
  out_color = frg_clr;
}
state_block sb_bulletdebug : default {
  CullTest = OFF;
  DepthTest = LEQUALS;
  DepthMask = true;
  BlendMode = ADDITIVE;
}
technique tek_bulletdebug {
  fxconfig = fxcfg_bulletdebug;
  pass p0 {
    vertex_shader = vs_bulletdebug;
    fragment_shader = fs_bulletdebug;
    state_block = sb_bulletdebug;
  }
}
)";

void PhysicsDebugger::_onGpuInit(Simulation* psi, lev2::Context* ctx) {
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInitFromShaderText(ctx, "bulletdebug_shader", BULLET_DEBUG_SHADER);
  _technique = _material->technique("tek_bulletdebug");
  _paramMVP = _material->param("mvp");
  _paramModColor = _material->param("modcolor");
}
void PhysicsDebugger::_onGpuExit(Simulation* psi, lev2::Context* ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::render(const RenderContextInstData& _RCID, lineqptr_t lines) {

  if (nullptr == lines)
    return;

  auto context = _RCID.context();
  int inumlines = lines->size();
  if (inumlines == 0)
    return;

  context->debugPushGroup("PhysicsDebugger");

  auto RCFD = _RCID.rcfd();
  const auto& CPD = RCFD->topCPD();
  auto pcamdata = CPD.cameraMatrices();
  auto V = pcamdata->_vmatrix;
  auto P = pcamdata->_pmatrix;

  auto cam_z = pcamdata->_camdat.zNormal();
  fmtx4 mtx_dbg;
  mtx_dbg.setTranslation(cam_z * -.025f);
  fmtx4 MVP = P * V * mtx_dbg;

  using vtx_t = SVtxV16T16C16;
  auto vb = GfxEnv::GetSharedDynamicV16T16C16();
  int icount = inumlines * 2;

  VtxWriter<vtx_t> vwriter;
  vwriter.Lock(context, vb.get(), icount);
  for (const auto& line : (*lines)) {
    fvec4 clr(line.mColor, 1.0f);
    vwriter.AddVertex(vtx_t(fvec4(line.mFrom, 1.0f), fvec4(0, 0, 0, 0), clr));
    vwriter.AddVertex(vtx_t(fvec4(line.mTo, 1.0f), fvec4(0, 0, 0, 0), clr));
  }
  vwriter.UnLock(context);

  auto fxi = context->FXI();
  auto gbi = context->GBI();
  _material->_rasterstate->_priority = 1<<20;
  fxi->pushRasterState(_material->_rasterstate);
  _material->begin(_technique, RCFD);
  _material->bindParamMatrix(_paramMVP, MVP);
  _material->bindParamVec4(_paramModColor, fvec4(1, 1, 1, 1));
  gbi->DrawPrimitiveEML(vwriter, PrimitiveType::LINES);
  _material->end(RCFD);
  fxi->popRasterState();

  context->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::addLine(const fvec3& from, const fvec3& to, const fvec3& color) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  if (_currentwritelq != nullptr)
    _currentwritelq->push_back(PhysicsDebuggerLine(from, to, color));
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawLine(const btVector3& from, const btVector3& to, const btVector3& color) {
  fvec3 vfrom = btv3toorkv3(from);
  fvec3 vto   = btv3toorkv3(to);
  fvec3 vclr  = btv3toorkv3(color);// * (1.0f / 256.0f);

  addLine(vfrom, vto, vclr);
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawSphere (btScalar radius, //
                                  const btTransform &transform, 
                                  const btVector3 &color) {
  btIDebugDraw::drawSphere(radius,transform,color);
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawContactPoint(
    const btVector3& PointOnB,
    const btVector3& normalOnB,
    btScalar distance,
    int lifeTime,
    const btVector3& color) {
  fvec3 vfrom = btv3toorkv3(PointOnB);
  fvec3 vdir  = btv3toorkv3(normalOnB);
  fvec3 vto   = vfrom + vdir * 4.0f; // distance;
  fvec3 vclr  = btv3toorkv3(normalOnB);
  vclr        = (vclr + fvec3(1)) * 0.5;
  addLine(vfrom, vto, vclr);
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::reportErrorWarning(const char* warningString) {
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::draw3dText(const btVector3& location, const char* textString) {
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::setDebugMode(int debugMode) {
}

///////////////////////////////////////////////////////////////////////////////

int PhysicsDebugger::getDebugMode() const {
  return _enabled ? (btIDebugDraw::DBG_DrawContactPoints 
                  | btIDebugDraw::DBG_DrawWireframe)
                  //| btIDebugDraw::DBG_DrawAabb)
                  : (btIDebugDraw::DBG_NoDebug);
}

} // namespace ork::ecs
