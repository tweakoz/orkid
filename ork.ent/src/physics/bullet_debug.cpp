////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <btBulletDynamicsCommon.h>
#include <pkg/ent/bullet.h>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>

#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/scene.h>

#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/register.h>

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
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

BulletDebugDrawDBData::BulletDebugDrawDBData(BulletSystem* system)
    : _bulletSystem(system)
    , _debugger(0) {
  for (int i = 0; i < DrawableBuffer::kmaxbuffers; i++) {
    mDBRecs[i]; //._bulletSystem = system;
  }
}

///////////////////////////////////////////////////////////////////////////////

void bulletDebugEnqueueToLayer(DrawableBufItem& cdb) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  BulletDebugDrawDBData* pdata = cdb.mUserData0.Get<BulletDebugDrawDBData*>();
  auto dbger                   = pdata->_debugger;
  if (dbger->_enabled) {
    BulletDebugDrawDBRec* prec = pdata->mDBRecs + cdb.miBufferIndex;
    cdb.mUserData1.Set(prec);
  }
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
  auto renderable = dynamic_cast<const lev2::CallbackRenderable*>(RCID._dagrenderable);
  if (context->FBI()->isPickState())
    return;
  //////////////////////////////////////////
  auto drawdata = renderable->GetDrawableDataA().Get<BulletDebugDrawDBData*>();
  auto debugger = drawdata->_debugger;
  if (false == debugger->_enabled)
    return;
  //////////////////////////////////////////
  if (auto as_rec = renderable->GetUserData1().TryAs<BulletDebugDrawDBRec*>()) {
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

void PhysicsDebugger::render(const RenderContextInstData& RCID, lineqptr_t lines) {
  auto context = RCID.context();
  typedef SVtxV12C4T16 vtx_t;

  if (nullptr == lines)
    return;

  int inumlines = lines->size();

  // printf( "draw numlines<%d>\n", inumlines );
  auto prenderer = RCID.GetRenderer();

  auto pcamdata = context->topRenderContextFrameData()->topCPD().cameraMatrices();

  fvec3 szn = 0;

  DynamicVertexBuffer<vtx_t>& vb = GfxEnv::GetSharedDynamicVB();

  int icount = inumlines * 2;

  if (icount) {
    VtxWriter<vtx_t> vwriter;
    vwriter.Lock(context, &vb, icount);
    for (const auto& line : (*lines)) {

      U32 ucolor = line.mColor.GetABGRU32();

      fvec3 vf = line.mFrom + szn;
      fvec3 vt = line.mTo + szn;

      vwriter.AddVertex(vtx_t(vf.x, vf.y, vf.z, 0.0f, 0.0f, ucolor));
      vwriter.AddVertex(vtx_t(vt.x, vt.y, vt.z, 0.0f, 0.0f, ucolor));
    }
    vwriter.UnLock(context);

    auto cam_z = pcamdata->_camdat.zNormal();

    static GfxMaterial3DSolid material(context);
    material._rasterstate.SetZWriteMask(true);
    material.SetColorMode(GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
    context->PushModColor(fvec4::White());
    fmtx4 mtx_dbg;
    mtx_dbg.SetTranslation(cam_z * -.013f);

    context->MTXI()->PushMMatrix(mtx_dbg);
    context->GBI()->DrawPrimitive(&material, vwriter, ork::lev2::PrimitiveType::LINES);
    context->MTXI()->PopMMatrix();
    context->PopModColor();
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::addLine(const fvec3& from, const fvec3& to, const fvec3& color) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  if (_currentwritelq != nullptr)
    _currentwritelq->push_back(PhysicsDebuggerLine(from, to, color));
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawLine(const btVector3& from, const btVector3& to, const btVector3& color) {
  fvec3 vfrom = !from;
  fvec3 vto   = !to;
  fvec3 vclr  = !color * (1.0f / 256.0f);

  addLine(vfrom, vto, vclr);
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawContactPoint(
    const btVector3& PointOnB,
    const btVector3& normalOnB,
    btScalar distance,
    int lifeTime,
    const btVector3& color) {
  fvec3 vfrom = !PointOnB;
  fvec3 vdir  = !normalOnB;
  fvec3 vto   = vfrom + vdir * 4.0F; // distance;
  fvec3 vclr  = !normalOnB;
  vclr        = (vclr + fvec3(1, 1, 1)) * 0.5;
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
  return _enabled ? (btIDebugDraw::DBG_DrawContactPoints | btIDebugDraw::DBG_FastWireframe | btIDebugDraw::DBG_DrawAabb)
                  : (btIDebugDraw::DBG_NoDebug);
}

} // namespace ork::ent
