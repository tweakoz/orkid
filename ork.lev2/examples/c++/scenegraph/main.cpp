#include <QWindow>
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

int main(int argc, char** argv) {
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;

  //////////////////////////////////////////////////////////
  // create scenegraph
  //////////////////////////////////////////////////////////

  auto sg_scene = std::make_shared<scenegraph::Scene>();
  auto sg_layer = sg_scene->createLayer("default");

  //////////////////////////////////////////////////////////
  // create terrain drawable
  //////////////////////////////////////////////////////////

  auto terrainData    = std::make_shared<TerrainDrawableData>();
  terrainData->_rock1 = fvec3(1, 1, 1);
  terrainData->_writeHmapPath("data://terrain/testhmap2_2048.png");
  auto terrainInst          = terrainData->createInstance();
  terrainInst->_worldHeight = 5000.0f;
  terrainInst->_worldSizeXZ = 8192.0f;
  auto terrainDrawable      = terrainInst->createCallbackDrawable();

  auto sg_node = sg_layer->createNode("terrain-node", terrainDrawable);

  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {});
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  auto cameralut = std::make_shared<CameraDataLut>();
  auto camera    = std::make_shared<CameraData>();
  cameralut->AddSorted("spawncam"_pool, camera.get());
  qtapp->onUpdate([&](updatedata_ptr_t updata) {
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
  qtapp->onDraw([&](const ui::DrawEvent& drwev) { sg_scene->renderOnContext(drwev.GetTarget()); });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    //
    sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->exec();
}
