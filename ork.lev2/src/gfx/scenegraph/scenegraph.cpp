////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/ui/event.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

using namespace std::string_literals;
using namespace ork;

static constexpr bool DEBUG_LOG = false;

ImplementReflectionX(ork::lev2::scenegraph::DrawableDataKvPair, "SgDrawableDataKvPair");

namespace ork::lev2::scenegraph {
static logchannel_ptr_t logchan_sg = logger()->configureChannel("scenegraph", fvec3(0.9, 0.2, 0.9));

void DrawableDataKvPair::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("Layer", &DrawableDataKvPair::_layername);
  clazz->directObjectProperty("DrawableData", &DrawableDataKvPair::_drawabledata);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::__common_init() {
  _userdata                        = std::make_shared<varmap::VarMap>();
  for (int i = 0; i < K_NUMRENDERERS; i++){
    _renderers[i] = std::make_shared<IRenderer>();
  }
  _lightManagerData                = std::make_shared<LightManagerData>();
  _lightManager                    = std::make_shared<LightManager>(_lightManagerData);
  _compositorData                  = std::make_shared<CompositingData>();
  _topCPD                          = std::make_shared<CompositingPassData>();
  _dbufcontext_SG                  = std::make_shared<DrawQueueContext>();
  _dbufcontext_SG->_name           = "DBC.SceneGraph";
  _renderPresetData                = std::make_shared<RenderPresetData>();
  _loadSynchro                     = std::make_shared<asset::LoadSynchronizer>();
  _renderPresetData->_assetSynchro = _loadSynchro;
  _pbr_common                      = std::make_shared<pbr::CommonStuff>();
  _renderPresetData->_pbr_common   = _pbr_common;

  _topCPD->addStandardLayers();

}

Scene::Scene(varmap::varmap_ptr_t params) {
  _params = params;
  __common_init();
  _profile_timer.Start();

  auto opqcurrent = opq::TrackCurrent::context();
  if (opqcurrent->_queue == opq::mainSerialQueue().get()) {
    initWithParams(_params);
  } else {
    //_loadSynchro->increment();
    auto op = [=]() {
      initWithParams(_params);
      //_loadSynchro->decrement();
    };
    opq::mainSerialQueue()->enqueue(op);
  }
}

///////////////////////////////////////////////////////////////////////////////

Scene::Scene() {
  opq::assertOnQueue2(opq::mainSerialQueue());
  _params                                         = std::make_shared<varmap::VarMap>();
  _params->makeValueForKey<std::string>("preset") = "DeferredPBR";
  __common_init();
  //_loadSynchro->increment();
  initWithParams(_params);
  //_loadSynchro->decrement();
}
///////////////////////////////////////////////////////////////////////////////

Scene::~Scene() {
  if (_synchro) {
    _synchro->terminate();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::_registerUISurface(drawable_ptr_t drawable) {
  _uiSurfaces.push_back(drawable);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::_unregisterUISurface(drawable_ptr_t drawable) {
  auto it = std::find(_uiSurfaces.begin(), _uiSurfaces.end(), drawable);
  if (it != _uiSurfaces.end()) {
    _uiSurfaces.erase(it);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuInit(Context* ctx) {
  //printf("Scene::gpuInit BEGIN\n");
  if (_enable_pick_hud) {
    _sgpickbuffer = std::make_shared<SgPickBuffer>(ctx, *this);
  }
  if(0){
    printf("Scene::gpuInit: pick buffer textures:\n");
    printf("  ID: %p w=%d h=%d\n",
           _sgpickbuffer->_pickIDtexture.get(),
           _sgpickbuffer->_pickIDtexture ? _sgpickbuffer->_pickIDtexture->_width : -1,
           _sgpickbuffer->_pickIDtexture ? _sgpickbuffer->_pickIDtexture->_height : -1);
    printf("  POS: %p w=%d h=%d\n",
           _sgpickbuffer->_pickPOStexture.get(),
           _sgpickbuffer->_pickPOStexture ? _sgpickbuffer->_pickPOStexture->_width : -1,
           _sgpickbuffer->_pickPOStexture ? _sgpickbuffer->_pickPOStexture->_height : -1);
    printf("  NRM: %p w=%d h=%d\n",
           _sgpickbuffer->_pickNRMtexture.get(),
           _sgpickbuffer->_pickNRMtexture ? _sgpickbuffer->_pickNRMtexture->_width : -1,
           _sgpickbuffer->_pickNRMtexture ? _sgpickbuffer->_pickNRMtexture->_height : -1);
    }
  _dogpuinit    = false;
  _boundContext = ctx;

  // If the Scene was constructed off the main thread, initWithParams
  // was deferred.  Now we are on the main/GPU thread, so complete it.
  if (!_compositorImpl && _params) {
    initWithParams(_params);
  }

  if (_compositorImpl) {
    _compositorImpl->gpuInit(ctx);
  }
  //ctx->_beginFrameBlockers.push_back(op);
  //printf("Scene::gpuInit END\n");
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuUpdate(Context* ctx) {
  if (_lightManager && _lightManager->_needs_gpu_init) {
    _lightManager->gpuInit(ctx);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuExit(Context* ctx) {
  _sgpickbuffer     = nullptr;
  _compositorImpl   = nullptr;
  _compositorData   = nullptr;
  for (int i = 0; i < K_NUMRENDERERS; i++) {
    _renderers[i] = nullptr;
  }
  _lightManager     = nullptr;
  _lightManagerData = nullptr;
  _topCPD           = nullptr;
  _staticDrawables.clear();
  _layers.atomicOp([](layer_map_t& unlocked) { unlocked.clear(); });
  _userdata = nullptr;
  _nodes2draw.clear();
}

///////////////////////////////////////////////////////////////////////////////

void Scene::pickWithRay(fray3_constptr_t ray, SgPickBuffer::callback_t callback) {
  if (_sgpickbuffer)
    _sgpickbuffer->pickWithRay(ray, callback);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, const ViewportRect& vprect, SgPickBuffer::callback_t callback) {
  if (_sgpickbuffer)
    _sgpickbuffer->pickWithScreenCoord(cam, screencoord, vprect, callback);
}
///////////////////////////////////////////////////////////////////////////////

void Scene::applyRuntimeParams(varmap::varmap_ptr_t params) {
  if (!_pbr_common)
    return;

  if (auto try_enable_skybox = params->typedValueForKey<bool>("enable_skybox")) {
    _pbr_common->_enable_skybox = try_enable_skybox.value();
  }
  if (auto try_clearcolor = params->typedValueForKey<fvec3>("clearcolor")) {
    fvec4 clearcolor = try_clearcolor.value();
    _pbr_common->_clearcolor = clearcolor;
  } else if (auto try_clearcolor2 = params->typedValueForKey<fvec4>("clearcolor")) {
    fvec4 clearcolor = try_clearcolor2.value();
    _pbr_common->_clearcolor = clearcolor;
  }

  if (auto try_bgtex = params->typedValueForKey<std::string>("SkyboxTexPathStr")) {
    auto texture_path = try_bgtex.value();
    // aliases
    if (texture_path == "nebula") {
      texture_path = "ork_envmaps|tozenv_nebula";
    } else if (texture_path == "hellscape") {
      texture_path = "ork_envmaps|tozenv_hellscape";
    } else if (texture_path == "caustics") {
      texture_path = "ork_envmaps|tozenv_caustic1";
    } else if (texture_path == "forest") {
      texture_path = "ork_envmaps|blender_forest";
    } else if (texture_path == "city") {
      texture_path = "ork_envmaps|blender_city";
    } else if (texture_path == "courtyard") {
      texture_path = "ork_envmaps|blender_courtyard";
    } else if (texture_path == "studio") {
      texture_path = "ork_envmaps|blender_studio";
    } else if (texture_path == "interior") {
      texture_path = "ork_envmaps|blender_interior";
    } else if (texture_path == "night") {
      texture_path = "ork_envmaps|blender_night";
    } else if (texture_path == "sunrise") {
      texture_path = "ork_envmaps|blender_sunrise";
    } else if (texture_path == "sunset") {
      texture_path = "ork_envmaps|blender_sunset";
    } else if (texture_path == "arena") {
      texture_path = "ork_envmaps|arena4k";
    } else if (texture_path == "arena8k") {
      texture_path = "ork_envmaps|arena8k";
    } else if (texture_path == "club") {
      texture_path = "ork_envmaps|club4k";
    } else if (texture_path == "club8k") {
      texture_path = "ork_envmaps|club8k";
    } else if (texture_path == "cold") {
      texture_path = "ork_envmaps|cold4k";
    } else if (texture_path == "cold8k") {
      texture_path = "ork_envmaps|cold8k";
    } else if (texture_path == "pillars") {
      texture_path = "ork_envmaps|pillars4k";
    } else if (texture_path == "pillars8k") {
      texture_path = "ork_envmaps|pillars8k";
    } else if (texture_path == "desert") {
      texture_path = "ork_envmaps|desert4k";
    } else if (texture_path == "desert8k") {
      texture_path = "ork_envmaps|desert8k";
    } else if (texture_path == "ocean") {
      texture_path = "ork_envmaps|ocean4k";
    } else if (texture_path == "ocean8k") {
      texture_path = "ork_envmaps|ocean8k";
    } else if (texture_path == "crossroads") {
      texture_path = "ork_envmaps|crossroads4k";
    } else if (texture_path == "ethereal") {
      texture_path = "ork_envmaps|ethereal4k";
    } else if (texture_path == "futcity") {
      texture_path = "ork_envmaps|futcity4k";
    } else if (texture_path == "futcity8k") {
      texture_path = "ork_envmaps|futcity8k";
    }
    else {
      if( texture_path.find("<") != texture_path.npos ) {
        texture_path = file::Path::expandPathString(texture_path);
      }
    }

    _compositorData->_defaultBG = false;
    auto load_req               = std::make_shared<asset::LoadRequest>(texture_path);
    if (0)
      printf("SCENE<%p> pbrc<%p> REQ SKYBOX TEX ASSET<%s>\n", (void*)this, (void*)_pbr_common.get(), texture_path.c_str());
    _pbr_common->requestAndRefSkyboxTexture(load_req);
  }

  if (auto try_envintensity = params->tryKeyAsNumber("EnvironmentIntensity")) {
    _pbr_common->_environmentIntensity = try_envintensity.value();
  }
  if (auto try_diffuseLevel = params->tryKeyAsNumber("DiffuseIntensity")) {
    _pbr_common->_diffuseLevel = try_diffuseLevel.value();
  }
  if (auto try_ambientLevel = params->typedValueForKey<fvec3>("AmbientLight")) {
    _pbr_common->_ambientLevel = try_ambientLevel.value();
  }
  if (auto try_skyboxLevel = params->tryKeyAsNumber("SkyboxIntensity")) {
    _pbr_common->_skyboxLevel = try_skyboxLevel.value();
  }
  if (auto try_specularLevel = params->tryKeyAsNumber("SpecularIntensity")) {
    _pbr_common->_specularLevel = try_specularLevel.value();
  }
  if (auto try_DepthFogDistance = params->tryKeyAsNumber("DepthFogDistance")) {
    _pbr_common->_depthFogDistance = try_DepthFogDistance.value();
  }
  if (auto try_DepthFogPower = params->tryKeysAsNumber("DepthFogPower", "depthFogPower")) {
    _pbr_common->_depthFogPower = try_DepthFogPower.value();
  }
  if (auto try_dfdist = params->tryKeysAsNumber("DepthFogDistance", "depthFogDistance")) {
    _pbr_common->_depthFogDistance = try_dfdist.value();
  }
  if (auto try_ssao = params->tryKeyAsInteger("SSAONumSamples")) {
    _pbr_common->_ssaoNumSamples = int(try_ssao.value());
    _pbr_common->_useDepthPrepass = true;
    printf("PBRC<%p> ssao num samples<%d>\n", (void*)_pbr_common.get(), _pbr_common->_ssaoNumSamples);
  }
  if (auto try_dpp = params->typedValueForKey<bool>("DepthPrepass")) {
    _pbr_common->_useDepthPrepass = try_dpp.value();
  }
  if (auto try_ssao = params->tryKeyAsNumber("dppZbias")) {
    _pbr_common->_dppZbias = try_ssao.value();
  }
  if (auto try_ssao = params->tryKeyAsInteger("SSAONumSteps")) {
    _pbr_common->_ssaoNumSteps = int(try_ssao.value());
  }
  if (auto try_ssao = params->tryKeyAsNumber("SSAOFeedback")) {
    _pbr_common->_ssaoFeedback = try_ssao.value();
  }
  if (auto try_ssao = params->tryKeyAsNumber("SSAOBias")) {
    _pbr_common->_ssaoBias = try_ssao.value();
  }
  if (auto try_ssao = params->tryKeyAsNumber("SSAORadius")) {
    _pbr_common->_ssaoRadius = try_ssao.value();
  }
  if (auto try_ssao = params->tryKeyAsNumber("SSAOWeight")) {
    _pbr_common->_ssaoWeight = try_ssao.value();
  }
  if (auto try_ssao = params->tryKeyAsNumber("SSAOPower")) {
    _pbr_common->_ssaoPower = try_ssao.value();
  }
  if (auto try_usef32 = params->typedValueForKey<bool>("use_float_color_buffer")) {
    _pbr_common->_useFloatColorBuffer = try_usef32.value();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::initWithParams(varmap::varmap_ptr_t params) {

  _params = params;

  if (auto try_dbufcontext = params->typedValueForKey<dbufcontext_ptr_t>("dbufcontext")) {
    _dbufcontext_SG = try_dbufcontext.value();
  }

  for (auto p : params->_themap) {
    auto k = p.first;
    auto v = p.second;
    // printf( "INITSCENE P<%s:%s>\n", k.c_str(), v.typeName());
  }

  std::string preset = "DeferredPBR";

  if (auto try_preset = params->typedValueForKey<std::string>("preset")){
    preset = try_preset.value();
  }

  if (auto try_rtgroup = params->typedValueForKey<rtgroup_ptr_t>("outputRTG")) {
    _renderPresetData->_outputGroup = try_rtgroup.value();
  }

  if (auto try_orcl = params->typedValueForKey<gfxcontext_lambda_t>("onRenderComplete")) {
    this->_on_render_complete = try_orcl.value();
  }

  if (auto try_bgtex = params->typedValueForKey<std::string>("SkyboxTexPathStr")) {
    _compositorData->_defaultBG = false;
  }
  auto preset_upper = ork::toUpper(preset);

  if (preset_upper == "UNLIT") {
    _compositorPreset = _compositorData->presetUnlit(_renderPresetData);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<compositor::UnlitNode>();
    _pbr_common       = nullptr;
  }
  else if (preset_upper == "FORWARDPBR" or preset_upper == "FWDPBR") {
    _compositorPreset = _compositorData->presetForwardPBR(_renderPresetData);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    _pbr_common     = outrnode->_pbrcommon;
  } else if (preset_upper == "FWDPBRVR") {
    _compositorPreset = _compositorData->presetForwardPBRVR(_renderPresetData);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    _pbr_common     = outrnode->_pbrcommon;
  } else if (preset_upper == "FWDPBRVRDM") {
    _compositorPreset = _compositorData->presetForwardPBRVRDM(_renderPresetData);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    _pbr_common     = outrnode->_pbrcommon;
    OrkAssert(_pbr_common);
  } else if (preset_upper == "PICKTEST") {
    auto cdata = std::make_shared<CompositingData>();
    cdata->presetPickingDebug();
    _compositorData = cdata;
    _pbr_common     = nullptr;
  } else if (preset_upper == "USER") {
    _compositorData = params->typedValueForKey<compositordata_ptr_t>("compositordata").value();
    _pbr_common     = nullptr;
  } else {
    throw std::runtime_error("unknown compositor preset type");
  }

  // Install a non-owning back-pointer from the PBR common object to
  // this scene so the forward compositor can reach layersForRole()
  // via _node->_pbrcommon->_scene. Scene owns the pbr_common shared_ptr
  // so this raw pointer stays valid for the scene's lifetime.
  if (_pbr_common) {
    _pbr_common->_scene = this;
  }

  //////////////////////////////////////////////

  applyRuntimeParams(params);

  //////////////////////////////////////////////

  _compositorData->mbEnable = true;
  _compositorTechnique      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");

  _outputNode = _compositorTechnique->tryOutputNodeAs<OutputCompositingNode>();
  _renderNode = _compositorTechnique->tryRenderNodeAs<RenderCompositingNode>();

  if (params->hasKey("PostFxChain")) {
    auto& pfxchain = params->valueForKey("PostFxChain");
    if (auto as_chain = pfxchain.tryAs<postfx_node_chain_t>()) {
      _compositorTechnique->_postEffectNodes = as_chain.value();
    }
    // OrkAssert(false);
  }
  if (params->hasKey("ssaa")) {
    auto& ssaa = params->valueForKey("ssaa");
    if (auto as_ssaa = ssaa.tryAs<int>()) {
      if (auto as_scrnode = dynamic_cast<ScreenOutputCompositingNode*>(_outputNode.get())) {
        as_scrnode->setSuperSample(as_ssaa.value());
      } else if (auto as_vrnode = dynamic_cast<VrOutputNode*>(_outputNode.get())) {
        as_vrnode->setSuperSample(as_ssaa.value());
      } else if (auto as_dmvrnode = dynamic_cast<DualMonoVrOutputNode*>(_outputNode.get())) {
        as_dmvrnode->setSuperSample(as_ssaa.value());
      }
    }
  }
  _compositorImpl = _compositorData->createImpl();
  _compositorImpl->bindLighting(_lightManager);
}

///////////////////////////////////////////////////////////////////////////////

const std::vector<std::string>& Scene::layersForRole(const std::string& role) const {
  auto it = _layerRoleOverrides.find(role);
  if (it != _layerRoleOverrides.end()) {
    return it->second;
  }
  // Identity default: we need a stable reference with a single element
  // equal to the role name. Cache per-role in a thread_local map so we
  // don't allocate on every frame and can return a const ref safely.
  static thread_local std::unordered_map<std::string, std::vector<std::string>> s_identity_cache;
  auto cit = s_identity_cache.find(role);
  if (cit == s_identity_cache.end()) {
    cit = s_identity_cache.emplace(role, std::vector<std::string>{role}).first;
  }
  return cit->second;
}

void Scene::setLayerRole(const std::string& role, std::vector<std::string> layers) {
  _layerRoleOverrides[role] = std::move(layers);
}

void Scene::clearLayerRole(const std::string& role) {
  _layerRoleOverrides.erase(role);
}

///////////////////////////////////////////////////////////////////////////////

layer_ptr_t Scene::createLayer(std::string named) {

  auto l = std::make_shared<Layer>(this, named);

  _layers.atomicOp([&](layer_map_t& unlocked) {
    auto it = unlocked.find(named);
    if(it!=unlocked.end()){
      l = it->second;
      if (DEBUG_LOG) {
        logchan_sg->log("Scene<%p> preexisting layer<%p:%s>", this, l.get(), (void*)named.c_str());
      }
    }
    else{
      if (DEBUG_LOG) {
        logchan_sg->log("Scene<%p> created layer<%p:%s>", this, l.get(), (void*)named.c_str());
      }
      unlocked[named] = l;
    }
  });


  return l;
}

///////////////////////////////////////////////////////////////////////////////

layer_ptr_t Scene::findLayer(std::string named) {

  layer_ptr_t rval;

  _layers.atomicOp([&](layer_map_t& unlocked) {
    auto it = unlocked.find(named);
    if (it == unlocked.end()) {
      printf("Layer<%s> not found\n", named.c_str());
      OrkAssert(false);
    }
    rval = it->second;
  });

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

compositorpostnode_ptr_t Scene::getPostNode(size_t index) const {
  return _compositorTechnique->_postEffectNodes[index];
}

///////////////////////////////////////////////////////////////////////////////

size_t Scene::getPostNodeCount() const {
  return _compositorTechnique->_postEffectNodes.size();
}

///////////////////////////////////////////////////////////////////////////////

std::vector<drawable_node_ptr_t> Scene::drawableNodesWithType(uint64_t drawable_type) const {
  std::vector<drawable_node_ptr_t> result;
  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (const auto& [name, layer] : unlocked) {
      layer->_drawable_nodes.atomicOp([&](const Layer::drawablenodevect_t& nodes) {
        for (const auto& node : nodes) {
          if (node->_drawable && node->_drawable->_drawable_type == drawable_type) {
            result.push_back(node);
          }
        }
      });
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<drawable_node_ptr_t> Scene::drawableNodesWithTag(uint64_t tag) const {
  std::vector<drawable_node_ptr_t> result;
  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (const auto& [name, layer] : unlocked) {
      layer->_drawable_nodes.atomicOp([&](const Layer::drawablenodevect_t& nodes) {
        for (const auto& node : nodes) {
          if (node->_drawable && node->_drawable->_tag == tag) {
            result.push_back(node);
          }
        }
      });
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<lightnode_ptr_t> Scene::lightNodes() const {
  std::vector<lightnode_ptr_t> result;
  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (const auto& [name, layer] : unlocked) {
      layer->_lightnodes.atomicOp([&](const Layer::lightnodevect_t& nodes) {
        for (const auto& node : nodes) {
          result.push_back(node);
        }
      });
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<lightnode_ptr_t> Scene::lightNodesWithType(uint64_t light_type) const {
  std::vector<lightnode_ptr_t> result;
  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (const auto& [name, layer] : unlocked) {
      layer->_lightnodes.atomicOp([&](const Layer::lightnodevect_t& nodes) {
        for (const auto& node : nodes) {
          if (node->_light && node->_light->_drawable_type == light_type) {
            result.push_back(node);
          }
        }
      });
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<lightnode_ptr_t> Scene::lightNodesWithTag(uint64_t tag) const {
  std::vector<lightnode_ptr_t> result;
  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (const auto& [name, layer] : unlocked) {
      layer->_lightnodes.atomicOp([&](const Layer::lightnodevect_t& nodes) {
        for (const auto& node : nodes) {
          if (node->_light && node->_light->_tag == tag) {
            result.push_back(node);
          }
        }
      });
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<probenode_ptr_t> Scene::probeNodes() const {
  std::vector<probenode_ptr_t> result;
  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (const auto& [name, layer] : unlocked) {
      layer->_probenodes.atomicOp([&](const Layer::probenodevect_t& nodes) {
        for (const auto& node : nodes) {
          result.push_back(node);
        }
      });
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph

ImplementReflectionX(ork::lev2::scenegraph::Node, "scenegraph::Node");
