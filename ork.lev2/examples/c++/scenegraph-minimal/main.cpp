////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>

#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::deferrednode;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto qtapp  = OrkEzApp::create(init_data);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;

  terraindrawabledata_ptr_t terrainData;
  drawable_ptr_t terrainDraw;
  terraindrawableinst_ptr_t terrainInst;
  scenegraph::scene_ptr_t sg_scene;
  auto cameralut = std::make_shared<CameraDataLut>();
  auto camera    = std::make_shared<CameraData>();
  cameralut->AddSorted("spawncam", camera.get());
  DrawableCache dcache;

  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    //////////////////////////////////////////////////////////
    // create scenegraph
    //////////////////////////////////////////////////////////

    sg_scene = std::make_shared<scenegraph::Scene>();
    auto sg_layer = sg_scene->createLayer("Default");

    //////////////////////////////////////////////////////////
    // create terrain drawable
    //////////////////////////////////////////////////////////

    terrainData    = std::make_shared<TerrainDrawableData>();
    terrainData->_rock1 = fvec3(1, 1, 1);
    terrainData->_writeHmapPath("src://terrain/testhmap2_2048.png");
    
    terrainDraw          = dcache.fetch(terrainData);
    terrainInst = terrainDraw->GetUserDataB().getShared<TerrainDrawableInst>();
    terrainInst->_worldHeight = 5000.0f;
    terrainInst->_worldSizeXZ = 8192.0f;

    auto sg_node = sg_layer->createDrawableNode("terrain-node", terrainDraw);


    sg_scene->gpuInit(ctx);
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  qtapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.1f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    camera->Lookat(eye, tgt, up);
    camera->Persp(0.1, 100.0, 45.0);
    ///////////////////////////////////////
    sg_scene->enqueueToRenderer(cameralut);
    ////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    sg_scene->renderOnContext(drwev->GetTarget());
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    //
    sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->mainThreadLoop();
}
