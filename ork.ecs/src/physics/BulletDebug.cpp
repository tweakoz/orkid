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
#include <ork/lev2/gfx/material_pbr.inl>

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
    } else {
      // _currentwritelq transferred to _curreadlq
    }
    _currentwritelq = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::beginRenderFrame() {
  _checkedoutreadlq = _curreadlq.exchange(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void bulletDebugRender(const RenderContextInstData& RCID) {

  auto context    = RCID.context();
  auto renderable = dynamic_cast<const lev2::CallbackRenderable*>(RCID._irenderable);
  if (context->FBI()->isPickState())
    return;
  //////////////////////////////////////////
  auto drawdata = renderable->_drawDataA.get<BulletDebugDrawDBData*>();
  auto debugger = drawdata->_debugger;
  if (false == debugger->_enabled)
    return;
  //////////////////////////////////////////
    //auto dbger                   = pdata->_debugger;
  //if (dbger->_enabled) {
    //static int i = 0;
    //auto prec = pdata->_DBRecs.data() + i;
    //i = ((i+1)%PhysicsDebugger::kmaxbuffers);
    //drawable->mDataB.set<BulletDebugDrawDBRec*>(prec);
  //}

  if (drawdata) {
    debugger->render(RCID, debugger->_checkedoutreadlq);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::endRenderFrame() {

  auto tmp = _curreadlq.exchange(_checkedoutreadlq); // return _checkedoutreadlq

  if (tmp == nullptr) { // no new one posted yet, just put it back
    // and since it was a fetch and fetch_and_store
    //  it is alread back...
  } else {                                // tmp is new rlq that was posted
    auto tmp2 = _curreadlq.exchange(tmp); // put tmp back into curread
    assert(tmp2 == _checkedoutreadlq);
    // put _checkedoutreadlq back into the pool
    _lineqpool.push(tmp2);
  }

  _checkedoutreadlq = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::_onGpuInit(Simulation* psi, lev2::Context* ctx) {
  _pbrmaterial = std::make_shared<PBRMaterial>(ctx);
  _pbrmaterial->_rasterstate.SetZWriteMask(true);
  _pbrmaterial->_variant = "vertexcolor"_crcu;
  _fxcache = _pbrmaterial->pipelineCache();
}
void PhysicsDebugger::_onGpuExit(Simulation* psi, lev2::Context* ctx) {

}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::render(const RenderContextInstData& _RCID, lineqptr_t lines) {

  //printf( "rendering...\n");

  RenderContextInstData RCIDCOPY = _RCID;

  auto context = RCIDCOPY.context();
  typedef SVtxV12C4T16 vtx_t;

  if (nullptr == lines)
    return;

  context->debugPushGroup("PhysicsDebugger");

  int inumlines = lines->size();

  //printf( "draw numlines<%d>\n", inumlines );
  auto prenderer = RCIDCOPY.GetRenderer();

  RCIDCOPY._pipeline_cache = _fxcache;


  auto pcamdata = context->topRenderContextFrameData()->topCPD().cameraMatrices();

  fvec3 szn = fvec3(0);

  DynamicVertexBuffer<vtx_t>& vb = GfxEnv::GetSharedDynamicVB();

  int icount = inumlines * 2;

  if (icount) {
    VtxWriter<vtx_t> vwriter;
    vwriter.Lock(context, &vb, icount);
    for (const auto& line : (*lines)) {

      fvec3 vf = line.mFrom + szn;
      fvec3 vt = line.mTo + szn;
      U32 ucolor = line.mColor.ABGRU32();

      vwriter.AddVertex(vtx_t(vf.x, vf.y, vf.z, 0.0f, 0.0f, ucolor));
      vwriter.AddVertex(vtx_t(vt.x, vt.y, vt.z, 0.0f, 0.0f, ucolor));
    }
    vwriter.UnLock(context);

    auto cam_z = pcamdata->_camdat.zNormal();

    context->PushModColor(fvec4::White());
    fmtx4 mtx_dbg;
    mtx_dbg.setTranslation(cam_z * -.013f);

    context->MTXI()->PushMMatrix(mtx_dbg);

    auto pmat = _pbrmaterial.get();

    ///////////////////////////////////////////////
    // draw with modified RCID (containing _fxcache)
    ///////////////////////////////////////////////

    OrkAssert(_RCID.rcfd()!=nullptr);
    OrkAssert(_RCID.rcfd()->_pbrcommon!=nullptr);
    OrkAssert(RCIDCOPY.rcfd()!=nullptr);
    OrkAssert(RCIDCOPY.rcfd()->_pbrcommon!=nullptr);

    auto pipeline = _fxcache->findPipeline(RCIDCOPY);
    OrkAssert(pipeline!=nullptr);
    pipeline->wrappedDrawCall(RCIDCOPY,[&](){
      context->GBI()->DrawPrimitiveEML(vwriter, ork::lev2::PrimitiveType::LINES);
    });

    ///////////////////////////////////////////////
    context->MTXI()->PopMMatrix();
    context->PopModColor();
  }

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
