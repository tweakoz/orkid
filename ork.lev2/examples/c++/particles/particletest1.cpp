////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <iostream>

#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/scenegraph/sgnode_particles.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
///////////////////////////////////////////////////////////////////////////////

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;

///////////////////////////////////////////////////////////////////

particles_drawable_data_ptr_t createParticleData(){

  using namespace ork::dataflow;
  using namespace particle;

  ////////////////////////////////////////////////////
  // create particlesystem dataflow graph
  ////////////////////////////////////////////////////

  auto graphdata = std::make_shared<GraphData>();

  auto ptcl_pool    = ParticlePoolData::createShared();
  auto ptcl_globals = GlobalModuleData::createShared();
  auto ptcl_emitter = NozzleEmitterData::createShared();
  auto ptcl_gravity = GravityModuleData::createShared();
  auto ptcl_sprites = SpriteRendererData::createShared();
  GraphData::addModule(graphdata, "G", ptcl_globals);
  GraphData::addModule(graphdata, "P", ptcl_pool);
  GraphData::addModule(graphdata, "E", ptcl_emitter);
  GraphData::addModule(graphdata, "GRAV", ptcl_gravity);
  GraphData::addModule(graphdata, "SPRITE", ptcl_sprites);

  ////////////////////////////////////////////////////

  ptcl_pool->_poolSize = 1024; // set num particles in pool

  ////////////////////////////////////////////////////
  // connect and init plugs
  ////////////////////////////////////////////////////

  auto E_rate     = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("EmissionRate");
  auto GR_G       = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("G");
  auto GR_Mass    = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("Mass");
  auto GR_OthMass = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("OthMass");
  auto GR_MinDist = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("MinDistance");
  auto GR_Center  = ptcl_gravity->typedInputNamed<Vec3XfPlugTraits>("Center");

  auto P_out   = ptcl_pool->outputNamed("ParticleBuffer");
  auto E_inp   = ptcl_emitter->inputNamed("ParticleBuffer");
  auto E_out   = ptcl_emitter->outputNamed("ParticleBuffer");
  auto GR_inp  = ptcl_gravity->inputNamed("ParticleBuffer");
  auto GR_out  = ptcl_gravity->outputNamed("ParticleBuffer");
  auto SPR_inp = ptcl_sprites->inputNamed("ParticleBuffer");

  graphdata->safeConnect(E_inp, P_out);
  graphdata->safeConnect(GR_inp, E_out);
  graphdata->safeConnect(SPR_inp, GR_out);

  E_rate->setValue(100.0f);
  GR_G->setValue(1);
  GR_Mass->setValue(1.0f);
  GR_OthMass->setValue(1.0f);
  GR_MinDist->setValue(1);
  GR_Center->setValue(fvec3(1, 2, 3));


  auto pdd = std::make_shared<ParticlesDrawableData>();
  pdd->_graphdata = graphdata;

  return pdd;

}

///////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(appinitdata_ptr_t init_data, //
               Context* ctx) { //

    _camlut                = std::make_shared<CameraDataLut>();
    _camdata               = std::make_shared<CameraData>();
    (*_camlut)["spawncam"] = _camdata;

    //////////////////////////////////////////////
    // create scenegraph
    //////////////////////////////////////////////

    _sg_params                                         = std::make_shared<varmap::VarMap>();
    _sg_params->makeValueForKey<std::string>("preset") = "ForwardPBR";
    _sg_scene        = std::make_shared<scenegraph::Scene>(_sg_params);
    auto sg_layer    = _sg_scene->createLayer("default");
    auto sg_compdata = _sg_scene->_compositorData;

    //////////////////////////////////////////////
    // create particle graph
    //////////////////////////////////////////////

    _particles_drawdata = createParticleData();
    _particlesDrawable = _particles_drawdata->createDrawable();

    //////////////////////////////////////////////
    // scenegraph nodes
    //////////////////////////////////////////////

    _particle_node  = sg_layer->createDrawableNode("particle-node", _particlesDrawable);

  }


  particles_drawable_data_ptr_t _particles_drawdata;
  drawable_ptr_t _particlesDrawable;
  scenegraph::node_ptr_t _particle_node;

  varmap::varmap_ptr_t _sg_params;
  scenegraph::scene_ptr_t _sg_scene;

  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;

};

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);

  auto vars = *init_data->parse();

  //init_data->_fullscreen = vars["fullscreen"].as<bool>();;
  //init_data->_top = vars["top"].as<int>();
  //init_data->_left = vars["left"].as<int>();
  //init_data->_width = vars["width"].as<int>();
  //init_data->_height = vars["height"].as<int>();
  //init_data->_msaa_samples = vars["msaa"].as<int>();
  //init_data->_ssaa_samples = vars["ssaa"].as<int>();

  //printf( "_msaa_samples<%d>\n", init_data->_msaa_samples );
  //bool use_forward = vars["forward"].as<bool>();
  //bool use_vr = vars["usevr"].as<bool>();
  //////////////////////////////////////////////////////////
  init_data->_imgui = false;
  init_data->_application_name = "ork.particletest1";
  //////////////////////////////////////////////////////////
  auto ezapp  = OrkEzApp::create(init_data);
  std::shared_ptr<GpuResources> gpurec;
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { gpurec = std::make_shared<GpuResources>(init_data, ctx); });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();
  auto dbufcontext = std::make_shared<DrawBufContext>();
  auto sframe = std::make_shared<StandardCompositorFrame>();
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime+dt+.016;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.01f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    gpurec->_camdata->Lookat(eye, tgt, up);
    gpurec->_camdata->Persp(1, 50.0, 45.0);

    ////////////////////////////////////////
    // enqueue scenegraph to renderer
    ////////////////////////////////////////

    gpurec->_sg_scene->enqueueToRenderer(gpurec->_camlut);

    ////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context = drwev->GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD.setUserProperty("vrcam"_crc, (const CameraData*) gpurec->_camdata.get() );
    gpurec->_sg_scene->renderOnContext(context, RCFD);
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([&](int w, int h) {
    gpurec->_sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  ezapp->onGpuExit([&](Context* ctx) { gpurec = nullptr; });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
