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
  // create instanced model drawable
  //////////////////////////////////////////////////////////
  auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://tests/pbr1/pbr1");
  auto drw        = std::make_shared<InstancedModelDrawable>(nullptr);
  drw->_model     = modl_asset->_model;
  auto sg_node    = sg_layer->createNode("model-node", drw);
  //////////////////////////////////////////////////////////
  constexpr size_t NUMINSTANCES = 4096;
  //////////////////////////////////////////////////////////
  drw->resize(NUMINSTANCES);
  auto instdata = drw->_instancedata;
  for (int i = 0; i < NUMINSTANCES; i++) {
    int ix   = (rand() & 0xff) - 0x80;
    int iy   = (rand() & 0xff) - 0x80;
    int iz   = (rand() & 0xff) - 0x80;
    float fx = float(ix) / 10.0f;
    float fy = float(iy) / 10.0f;
    float fz = float(iz) / 10.0f;
    instdata->_worldmatrices[i].compose(fvec3(fx, fy, fz), fquat(), 1.0f);
  }
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {

  });
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
    int instance_index = rand() % NUMINSTANCES;
    int ix             = (rand() & 0xff) - 0x80;
    int iy             = (rand() & 0xff) - 0x80;
    int iz             = (rand() & 0xff) - 0x80;
    float fx           = float(ix) / 10.0f;
    float fy           = float(iy) / 10.0f;
    float fz           = float(iz) / 10.0f;
    instdata->_worldmatrices[instance_index]. //
        compose(
            fvec3(fx, fy, fz), //
            fquat(),
            1.0f);
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
  qtapp->onDraw([&](const ui::DrawEvent& drwev) { //
    sg_scene->renderOnContext(drwev.GetTarget());
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    //
    sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->exec();
}
