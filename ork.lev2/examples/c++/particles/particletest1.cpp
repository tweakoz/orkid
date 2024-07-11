////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <iostream>

#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/camera/uicam.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/scenegraph/sgnode_particles.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
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
  auto ptcl_turbulence = TurbulenceModuleData::createShared();
  auto ptcl_vortex = VortexModuleData::createShared();
  auto ptcl_sprites = SpriteRendererData::createShared();
  GraphData::addModule(graphdata, "G", ptcl_globals);
  GraphData::addModule(graphdata, "P", ptcl_pool);
  GraphData::addModule(graphdata, "E", ptcl_emitter);
  GraphData::addModule(graphdata, "GRAV", ptcl_gravity);
  GraphData::addModule(graphdata, "TURB", ptcl_turbulence);
  GraphData::addModule(graphdata, "VORTEX", ptcl_vortex);
  GraphData::addModule(graphdata, "SPRITE", ptcl_sprites);

  ////////////////////////////////////////////////////

  ptcl_pool->_poolSize = 8192; // set num particles in pool

  ////////////////////////////////////////////////////
  // connect and init plugs
  ////////////////////////////////////////////////////

  auto E_life     = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("LifeSpan");
  auto E_rate     = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("EmissionRate");
  auto E_vel     = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("EmissionVelocity");
  auto E_ang     = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("DispersionAngle");
  auto E_pos     = ptcl_emitter->typedInputNamed<Vec3XfPlugTraits>("Offset");
  auto GR_G       = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("G");
  auto GR_Mass    = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("Mass");
  auto GR_OthMass = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("OthMass");
  auto GR_MinDist = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("MinDistance");
  auto GR_Center  = ptcl_gravity->typedInputNamed<Vec3XfPlugTraits>("Center");
  auto TURB_Amount  = ptcl_turbulence->typedInputNamed<Vec3XfPlugTraits>("Amount");
  auto VORTEX_Strength  = ptcl_vortex->typedInputNamed<FloatXfPlugTraits>("VortexStrength");
  auto VORTEX_Outward  = ptcl_vortex->typedInputNamed<FloatXfPlugTraits>("OutwardStrength");
  auto VORTEX_Falloff  = ptcl_vortex->typedInputNamed<FloatXfPlugTraits>("Falloff");

  E_life->setValue(10.0f);
  E_rate->setValue(800.0f);
  E_vel->setValue(1.0f);
  E_ang->setValue(45);
  E_pos->setValue(fvec3(1,2,3));
  GR_G->setValue(1);
  GR_Mass->setValue(1.0f);
  GR_OthMass->setValue(1.0f);
  GR_MinDist->setValue(1);
  GR_Center->setValue(fvec3(0,0,0));
  TURB_Amount->setValue(fvec3(1.5,1.5,1.5));
  VORTEX_Strength->setValue(1.0f);
  VORTEX_Outward->setValue(0.0f);
  VORTEX_Falloff->setValue(1.0f);
  ///////////////////////////////////////////////////////////////
  // particle buffer IO
  ///////////////////////////////////////////////////////////////

  auto P_out   = ptcl_pool->outputNamed("pool");
  auto E_inp   = ptcl_emitter->inputNamed("pool");
  auto E_out   = ptcl_emitter->outputNamed("pool");
  auto GR_inp  = ptcl_gravity->inputNamed("pool");
  auto GR_out  = ptcl_gravity->outputNamed("pool");
  auto TURB_inp  = ptcl_turbulence->inputNamed("pool");
  auto TURB_out  = ptcl_turbulence->outputNamed("pool");
  auto VORTEX_inp  = ptcl_vortex->inputNamed("pool");
  auto VORTEX_out  = ptcl_vortex->outputNamed("pool");
  auto SPR_inp = ptcl_sprites->inputNamed("pool");

  graphdata->safeConnect(E_inp, P_out);
  graphdata->safeConnect(GR_inp, E_out);
  graphdata->safeConnect(TURB_inp, GR_out);
  graphdata->safeConnect(VORTEX_inp, GR_out);
  graphdata->safeConnect(SPR_inp, VORTEX_out);

  ///////////////////////////////////////////////////////////////


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

    //////////////////////////////////////////////


  _uicamera                 = std::make_shared<EzUiCam>();
  _uicamera->_constrainZ    = true;
  _uicamera->_base_zmoveamt = 2.0f;
  _uicamera->mfLoc          = 25.0f;

  }


  particles_drawable_data_ptr_t _particles_drawdata;
  drawable_ptr_t _particlesDrawable;
  scenegraph::node_ptr_t _particle_node;

  varmap::varmap_ptr_t _sg_params;
  scenegraph::scene_ptr_t _sg_scene;

  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;
  lev2::ezuicam_ptr_t _uicamera;

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
  auto dbufcontext = std::make_shared<DrawQueueContext>();
  auto sframe = std::make_shared<StandardCompositorFrame>();
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime+dt+.016;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    gpurec->_uicamera->_fov = 45.0 * DTOR;
    gpurec->_uicamera->updateMatrices();
    (*gpurec->_camdata) = gpurec->_uicamera->_camcamdata;
    ////////////////////////////////////////
    // enqueue scenegraph to renderer
    ////////////////////////////////////////

    gpurec->_sg_scene->enqueueToRenderer(gpurec->_camlut);

    ////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  auto rcfd = std::make_shared<RenderContextFrameData>();
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context = drwev->GetTarget();
    rcfd->_target = context; // renderer per/frame data
    rcfd->setUserProperty("vrcam"_crc, (const CameraData*) gpurec->_camdata.get() );
    gpurec->_sg_scene->renderOnContext(context, rcfd);
  });
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool handled = gpurec->_uicamera->UIEventHandler(ev);
    return ui::HandlerResult();
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
