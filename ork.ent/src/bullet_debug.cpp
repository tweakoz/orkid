////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <btBulletDynamicsCommon.h>
#include <pkg/ent/bullet.h>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>

#include <pkg/ent/drawable.h>
#include <pkg/ent/scene.h>

#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <ork/gfx/camera.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

PhysicsDebugger::PhysicsDebugger() {
  //: _mutex("bulletdebuggerlines") {

  for (int i = 0; i < 4; i++) {
    _lineqpool.push(new lineq_t);
  }

  _curreadlq = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

// void PhysicsDebugger::Lock() { _mutex.Lock(); }

///////////////////////////////////////////////////////////////////////////////

// void PhysicsDebugger::UnLock() { _mutex.UnLock(); }

///////////////////////////////////////////////////////////////////////////////

BulletDebugDrawDBData::BulletDebugDrawDBData(BulletSystem* system)
    : _bulletSystem(system), _debugger(0) {
  for (int i = 0; i < DrawableBuffer::kmaxbuffers; i++) {
    mDBRecs[i]._bulletSystem = system;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::flushLines() { // final
}

///////////////////////////////////////////////////////////////////////////////

void bulletDebugEnqueueToLayer(DrawableBufItem& cdb) {
  AssertOnOpQ2(UpdateSerialOpQ());

  BulletDebugDrawDBData* pdata = cdb.mUserData0.Get<BulletDebugDrawDBData*>();
  auto dbger = pdata->_debugger;

  if (dbger->_enabled) {
    BulletSystem* system = pdata->_bulletSystem;

    BulletDebugDrawDBRec* prec = pdata->mDBRecs + cdb.miBufferIndex;
    cdb.mUserData1.Set(prec);

    if (system) {

      printf("physdbg enq\n");

      prec->_lines = dbger->_curreadlq.exchange(nullptr);

      /*dbger->_currentwritelq = nullptr;

      while( false == dbger->_lineqpool.try_pop(dbger->_currentwritelq) ){
        sched_yield();
      }

      // dbger->_currentwritelq is now valid

      pdata->_debugger->Lock();
      dbger->_currentwritelq->clear();
      system->BulletWorld()->debugDrawWorld();
      pdata->_debugger->UnLock();
      dbger->_currentwritelq = nullptr;*/
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void bulletDebugRender(RenderContextInstData& rcid, GfxTarget* targ, const CallbackRenderable* pren) {
  AssertOnOpQ2(MainThreadOpQ());

  if (targ->FBI()->IsPickState())
    return;

  //////////////////////////////////////////
  auto drawdata = pren->GetDrawableDataA().Get<BulletDebugDrawDBData*>();

  auto debugger = drawdata->_debugger;

  if (false == debugger->_enabled)
    return;
  //////////////////////////////////////////

  if (auto as_rec = pren->GetUserData1().TryAs<BulletDebugDrawDBRec*>()) {
    //////////////////////////////////////////

    const BulletSystem* system = drawdata->_bulletSystem;

    printf("physdbg begin render\n");

    //////////////////////////////////////////
    //const RenderContextFrameData* framedata = targ->GetRenderContextFrameData();
    //const ork::CameraData* cdata = framedata->GetCameraData();
    //auto srec = ;

    // drawdata->_debugger->Lock();
    debugger->render(rcid, targ, as_rec.value()->_lines);

    auto tmp = debugger->_curreadlq.exchange(as_rec.value()->_lines);

    if( tmp == nullptr ){ //no new one posted yet, just put it back
      // and since it was a fetch and fetch_and_store
      //  it is alread back...
    }
    else { // a new one was posted
      // put back into the pool
      debugger->_lineqpool.push(tmp);
    }

    printf("physdbg end render\n");

    // drawdata->_debugger->UnLock();

    ////////////////////////
    // done rendering lines, return to pool
    ////////////////////////

    // drawdata->_debugger->_lineqpool.push(srec->_lines);

    ////////////////////////
  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::beginSimFrame(BulletSystem* system) {
  if (_enabled) {

    _currentwritelq = nullptr;

    bool got_one = _lineqpool.try_pop(_currentwritelq);
    assert(got_one);
    assert(_currentwritelq != nullptr);
    _currentwritelq->clear();
    printf("checked out _currentwritelq<%p>\n", _currentwritelq);
  }
}
void PhysicsDebugger::endSimFrame(BulletSystem* system) {
  if (_enabled and _currentwritelq) {
    //system->BulletWorld()->debugDrawWorld();
    printf("returning _currentwritelq<%p>\n", _currentwritelq);

    auto prevread = _curreadlq.exchange(_currentwritelq);

    if( prevread ) { //
      _lineqpool.push(prevread);
    }

    _currentwritelq = nullptr;

  }
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::render(RenderContextInstData& rcid, GfxTarget* ptarg, lineqptr_t lines) {

  typedef SVtxV12C4T16 vtx_t;

  if(nullptr == lines)
    return;

  int inumlines = lines->size();


  printf( "draw numlines<%d>\n", inumlines );
  const Renderer* prenderer = rcid.GetRenderer();

  const ork::CameraData* pcamdata = ptarg->GetRenderContextFrameData()->GetCameraData();

  fvec3 szn = 0;

  DynamicVertexBuffer<vtx_t>& vb = GfxEnv::GetSharedDynamicVB();

  int icount = inumlines * 2;

  if (icount) {
    VtxWriter<vtx_t> vwriter;
    vwriter.Lock(ptarg, &vb, icount);
    for (const auto& line : (*lines)) {

      U32 ucolor = line.mColor.GetBGRAU32();

      fvec3 vf = line.mFrom + szn;
      fvec3 vt = line.mTo + szn;

      vwriter.AddVertex(vtx_t(vf.x, vf.y, vf.z, 0.0f, 0.0f, ucolor));
      vwriter.AddVertex(vtx_t(vt.x, vt.y, vt.z, 0.0f, 0.0f, ucolor));
    }
    vwriter.UnLock(ptarg);

    auto cam_z = pcamdata->GetZNormal();

    static GfxMaterial3DSolid material(ptarg);
    material.mRasterState.SetZWriteMask(true);
    material.SetColorMode(GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
    ptarg->BindMaterial(&material);
    ptarg->PushModColor(fvec4::White());
    ptarg->FXI()->InvalidateStateBlock();
    fmtx4 mtx_dbg;
    mtx_dbg.SetTranslation(cam_z * -.013f);

    ptarg->MTXI()->PushMMatrix(mtx_dbg);
    ptarg->GBI()->DrawPrimitive(vwriter, ork::lev2::EPRIM_LINES);
    ptarg->MTXI()->PopMMatrix();
    ptarg->PopModColor();
    ptarg->BindMaterial(0);
  }

  printf( "draw done\n" );

}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::addLine(const fvec3& from, const fvec3& to, const fvec3& color) {
  if(_currentwritelq!=nullptr);
    _currentwritelq->push_back(PhysicsDebuggerLine(from, to, color));
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawLine(const btVector3& from, const btVector3& to, const btVector3& color) {
  fvec3 vfrom = !from;
  fvec3 vto = !to;
  fvec3 vclr = !color * (1.0f / 256.0f);

  addLine(vfrom, vto, vclr);
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime,
                                       const btVector3& color) {
  fvec3 vfrom = !PointOnB;
  fvec3 vdir = !normalOnB;
  fvec3 vto = vfrom + vdir * 4.0F; // distance;
  fvec3 vclr = !color;

  addLine(vfrom, vto, vclr);
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::reportErrorWarning(const char* warningString) {}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::draw3dText(const btVector3& location, const char* textString) {}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::setDebugMode(int debugMode) {}

///////////////////////////////////////////////////////////////////////////////

int PhysicsDebugger::getDebugMode() const {
  return _enabled ? (btIDebugDraw::DBG_DrawContactPoints | btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb)
                  : (btIDebugDraw::DBG_NoDebug);
}

} // namespace ork::ent
