////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
using namespace ork::lev2::pbr::deferrednode;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;

  //////////////////////////////////////////////////////////
  // create scenegraph
  //////////////////////////////////////////////////////////

  scenegraph::scene_ptr_t sg_scene;

  //////////////////////////////////////////////////////////
  // create instanced model drawable
  //////////////////////////////////////////////////////////
  auto drw = std::make_shared<InstancedModelDrawable>();
  //////////////////////////////////////////////////////////
  constexpr size_t NUMINSTANCES = 65536;
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
    instdata->_worldmatrices[i].compose(fvec3(fx, fy, fz), fquat(), 0.03f);
  }
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) {
    sg_scene = std::make_shared<scenegraph::Scene>();
    drw->bindModelAsset("data://src/environ/objects/misc/ref/uvsph");
    auto sg_layer = sg_scene->createLayer("default");
    sg_layer->createDrawableNode("model-node", std::move(drw));
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  auto cameralut = std::make_shared<CameraDataLut>();
  auto camera    = cameralut->create("spawncam");
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ///////////////////////////////////////
    for (int i = 0; i < 400; i++) {
      int instance_index = rand() % NUMINSTANCES;
      int ix             = (rand() & 0xff) - 0x80;
      int iy             = (rand() & 0xff) - 0x80;
      int iz             = (rand() & 0xff) - 0x80;
      int irx            = (rand() & 0xff) - 0x80;
      int iry            = (rand() & 0xff) - 0x80;
      int irz            = (rand() & 0xff) - 0x80;
      int irangle        = (rand() & 0xff) - 0x80;
      int iscale         = (rand() & 0xff);

      float fx = float(ix) / 10.0f;
      float fy = float(iy) / 10.0f;
      float fz = float(iz) / 10.0f;

      float rx     = float(irx) / 128.0f;
      float ry     = float(iry) / 128.0f;
      float rz     = float(irz) / 128.0f;
      float rangle = float(irz) / 128.0f;
      float scale  = float(iscale) / 2048.0f;
      fvec3 axis   = fvec3(rx, ry, rz).normalized();
      fquat rot    = fquat(axis, rangle);
      instdata->_worldmatrices[instance_index]. //
          compose(
              fvec3(fx, fy, fz), //
              rot,
              scale);
    }
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.003f;
    float distance = 1.0f;
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
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) { //
    sg_scene->renderOnContext(drwev->GetTarget());
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) {
    //
    sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  ezapp->onGpuExit([&](Context* ctx) {
    sg_scene = nullptr;
  });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
