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
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/vr/vr.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/imgui_ged.inl>
#include <ork/lev2/imgui/imgui_internal.h>
///////////////////////////////////////////////////////////////////////////////

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets

struct Instance {
  fvec3 _curpos;
  fvec3 _curaxis;
  float _curangle = 0.0f;
  fvec3 _target;
  fvec3 _targetaxis;
  float _targetangle = 0.0f;
  float _timeout     = 0.0f;

  void update(float dt, float abstime, float base_h, float maxh, float powr) {
    fvec3 delta = _target - _curpos;
    _curpos += delta.normalized() * dt * 1.0;

    delta    = _targetaxis - _curaxis;
    _curaxis = (_curaxis + delta.normalized() * dt * 0.1).normalized();
    _curangle += (_targetangle - _curangle) * dt * 0.1;

    if (_timeout < abstime) {
      _timeout  = abstime + float(rand() % 255) / 32.0;
      _target.x = ((float(rand() % 255) / 2.55) - 50) * 3;

      float fi = float(rand() % 255) / 255.0;

      _target.y = base_h + powf(fi, powr) * maxh;

      fi = float(rand() % 255) / 255.0;

      _target.z = -10.0 + fi * 20.0f;
      //_target *= 10.0f;

      fvec3 axis;
      axis.x       = (float(rand() % 255) / 255.0f) - 0.5f;
      axis.y       = (float(rand() % 255) / 255.0f) - 0.5f;
      axis.z       = (float(rand() % 255) / 255.0f) - 0.5f;
      _targetaxis  = axis.normalized();
      _targetangle = PI2 * (float(rand() % 255) / 255.0f) - 0.5f;
    }
  }
};

struct PointLightInstance : public Instance {
  scenegraph::lightnode_ptr_t _lightnode;
  pointlightdata_ptr_t _data;
  pointlight_ptr_t _instance;

  fvec3 _curcolor;
  fvec3 _tgtcolor;
};

using instances_t           = std::vector<Instance>;
using pointlightinstances_t = std::vector<PointLightInstance>;

///////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(appinitdata_ptr_t init_data, //
               Context* ctx, //
               bool use_forward, //
               bool use_vr) { //

    if(use_vr){
      auto vrdev = orkidvr::novr::novr_device();
      orkidvr::setDevice(vrdev);
      vrdev->overrideSize(init_data->_width,init_data->_height);

      vrdev->_posemap["hmd"].setTranslation(0,0,-5);
    }

    _camlut                = std::make_shared<CameraDataLut>();
    _camdata               = std::make_shared<CameraData>();
    (*_camlut)["spawncam"] = _camdata;

    _spikee_instanced_drawable = std::make_shared<InstancedModelDrawable>();

    //////////////////////////////////////////////
    // create scenegraph
    //////////////////////////////////////////////

    _sg_params                                         = std::make_shared<varmap::VarMap>();

    _sg_params->makeValueForKey<float>("SkyboxIntensity") = 2.0f;
    if(use_vr){
      _sg_params->makeValueForKey<std::string>("preset") = use_forward ? "FWDPBRVR" : "PBRVR" ;
    }
    else{
      _sg_params->makeValueForKey<std::string>("preset") = use_forward ? "ForwardPBR" : "DeferredPBR";
    }
    _sg_params->makeValueForKey<bool>("DepthPrepass") = true;

    _sg_scene        = std::make_shared<scenegraph::Scene>(_sg_params);
    std::string std_layer = use_forward ? "std_forward" : "std_deferred";
    auto sg_layer    = _sg_scene->createLayer(std_layer);
    auto sg_compdata = _sg_scene->_compositorData;

    //////////////////////////////////////////////////////////

    _spikee_instances = std::make_shared<instances_t>();

    ctx->debugPushGroup("main.onGpuInit");

    _spikee_modelasset = asset::AssetManager<XgmModelAsset>::load("data://tests/pbr1/pbr1.glb");

    _spikee_instanced_drawable->bindModel(_spikee_modelasset->getSharedModel());
    _spikee_instanced_drawable->_name = "spikee";

    constexpr size_t KNUMINSTANCES = 30;

    _spikee_instanced_drawable->resize(KNUMINSTANCES);
    _spikee_instanced_drawable->gpuInit(ctx);

    for (int i = 0; i < KNUMINSTANCES; i++) {
      Instance inst;
      _spikee_instances->push_back(inst);
    }

    _spikee_mesh_instance_data = _spikee_instanced_drawable->_instancedata;

    //////////////////////////////////////////////////////////
    // create lighting
    //////////////////////////////////////////////////////////

    _lights          = std::make_shared<pointlightinstances_t>();
    _lights_drawable = std::make_shared<InstancedModelDrawable>();
    _light_modelasset = asset::AssetManager<XgmModelAsset>::load("data://tests/pbr_emissive");
    _lights_drawable->bindModel(_light_modelasset->getSharedModel());
    _lights_drawable->_name = "lights";

    constexpr size_t NUMLIGHTS = 32;
    _lights_drawable->resize(NUMLIGHTS);
    _lights_drawable->gpuInit(ctx);

    // create light instances

    for (int i = 0; i < NUMLIGHTS; i++) {
      auto create_light = [&](fvec3 pos, float intensity) {
        auto lightdata      = std::make_shared<ork::lev2::PointLightData>();
        lightdata->_radius  = 100.0f;
        lightdata->_falloff = 0.5f;
        lightdata->SetColor(fvec3::White() * intensity);
        auto light         = std::make_shared<ork::lev2::PointLight>(nullptr, lightdata.get());
        auto sg_light_node = sg_layer->createLightNode("lightnode", light);

        light->_xformgenerator = [sg_light_node]() -> fmtx4 { //
          return sg_light_node->_dqxfdata._worldTransform->composed(); //
        };

        PointLightInstance lr;
        lr._instance  = light;
        lr._lightnode = sg_light_node;
        lr._data      = lightdata;
        lr._curpos    = fvec3(0, 0, 2);

        _lights->push_back(lr);
      };

      create_light(fvec3(0, 5, 10), 1.0f);
    }

    _light_mesh_instance_data = _lights_drawable->_instancedata;

    //////////////////////////////////////////////
    // scenegraph nodes
    //////////////////////////////////////////////

    _lightsnode  = sg_layer->createDrawableNode("lightmeshes-node", _lights_drawable);
    _spikee_node = sg_layer->createDrawableNode("meshes-node", _spikee_instanced_drawable);

    ctx->debugPopGroup();
  }

  std::shared_ptr<instances_t> _spikee_instances;
  lev2::xgmmodelassetptr_t _spikee_modelasset; // retain model
  instanced_modeldrawable_ptr_t _spikee_instanced_drawable;
  instanceddrawinstancedata_ptr_t _spikee_mesh_instance_data;
  scenegraph::node_ptr_t _spikee_node;

  std::shared_ptr<pointlightinstances_t> _lights;
  lev2::xgmmodelassetptr_t _light_modelasset; // retain model
  instanced_modeldrawable_ptr_t _lights_drawable;
  instanceddrawinstancedata_ptr_t _light_mesh_instance_data;
  scenegraph::node_ptr_t _lightsnode;

  varmap::varmap_ptr_t _sg_params;
  scenegraph::scene_ptr_t _sg_scene;

  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;
};

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);

  auto desc = init_data->commandLineOptions("model3dpbr example Options");
  desc->add_options()                  //
      ("help", "produce help message") //
      ("msaa", po::value<int>()->default_value(1), "msaa samples(*1,4,9,16,25)")
      ("ssaa", po::value<int>()->default_value(1), "ssaa samples(*1,4,9,16,25)")
      ("forward", po::bool_switch()->default_value(false), "forward renderer")
      ("fullscreen", po::bool_switch()->default_value(false), "fullscreen mode")                              
      ("left",  po::value<int>()->default_value(100), "left window offset")                              
      ("top",  po::value<int>()->default_value(100), "top window offset")                              
      ("width",  po::value<int>()->default_value(1280), "window width")                              
      ("height",  po::value<int>()->default_value(720), "window height")
      ("usevr",  po::bool_switch()->default_value(false), "use vr output");                             

  auto vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }
  init_data->_fullscreen = vars["fullscreen"].as<bool>();;
  init_data->_top = vars["top"].as<int>();
  init_data->_left = vars["left"].as<int>();
  init_data->_width = vars["width"].as<int>();
  init_data->_height = vars["height"].as<int>();

  init_data->_msaa_samples = vars["msaa"].as<int>();
  init_data->_ssaa_samples = vars["ssaa"].as<int>();

  printf( "_msaa_samples<%d>\n", init_data->_msaa_samples );
  bool use_forward = vars["forward"].as<bool>();
  bool use_vr = vars["usevr"].as<bool>();
  //////////////////////////////////////////////////////////
  init_data->_imgui = true;
  init_data->_application_name = "ork.model3dpbr";
  //////////////////////////////////////////////////////////
  auto ezapp  = OrkEzApp::create(init_data);
  std::shared_ptr<GpuResources> gpurec;
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) { gpurec = std::make_shared<GpuResources>(init_data, ctx, use_forward,use_vr); });
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
    float phase    = abstime * PI2 * 0.01f;
    float distance = 3.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    gpurec->_camdata->Lookat(eye, tgt, up);
    gpurec->_camdata->Persp(1, 50.0, 45.0);

    ////////////////////////////////////////
    // animate all instances
    ////////////////////////////////////////

    int spikee_index = 0;
    for (auto& inst : *gpurec->_spikee_instances) {
      inst.update(dt, abstime, 0, 20, 2);
      fquat q;
      q.fromAxisAngle(fvec4(inst._curaxis, inst._curangle));
      gpurec->_spikee_mesh_instance_data->_worldmatrices[spikee_index++].compose(inst._curpos, q, 0.3f);
    }

    ////////////////////////////////////////
    // animate lights
    ////////////////////////////////////////

    int light_index = 0;
    for (auto& light : *gpurec->_lights) {

      /////////////////////
      // update light fsm
      /////////////////////

      fvec3 delta = light._tgtcolor - light._curcolor;
      light._curcolor += delta.normalized() * dt * 1.0;
      if (light._timeout < abstime) {
        light._tgtcolor.x = (float(rand() % 255) / 255.0f);
        light._tgtcolor.y = (float(rand() % 255) / 255.0f);
        light._tgtcolor.z = (float(rand() % 255) / 255.0f);
      }
      light.update(dt, abstime, 0, 22, 3);

      /////////////////////
      // set visual mesh data
      /////////////////////

      fquat q;
      q.fromAxisAngle(fvec4(light._curaxis, light._curangle));
      fmtx4 mtxM;
      mtxM.compose(light._curpos, q, 0.25f);
      gpurec->_light_mesh_instance_data->_worldmatrices[light_index] = mtxM;
      gpurec->_light_mesh_instance_data->_modcolors[light_index]     = light._curcolor * 2.0;

      /////////////////////
      // set lighting data
      /////////////////////

      light._data->SetColor(light._curcolor * 8.0);
      light._lightnode->_dqxfdata._worldTransform->set(light._curpos, fquat(), 1.0f);

      light_index++;
    }

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
  ezapp->onResize([&](int w, int h) {
    gpurec->_sg_scene->_compositorImpl->compositingContext().Resize(w, h);
  });
  ezapp->onGpuExit([&](Context* ctx) { gpurec = nullptr; });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
