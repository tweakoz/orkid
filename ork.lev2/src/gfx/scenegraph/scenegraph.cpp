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
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

using namespace std::string_literals;
using namespace ork;

static constexpr bool DEBUG_LOG = false;

ImplementReflectionX(ork::lev2::scenegraph::DrawableDataKvPair, "SgDrawableDataKvPair");

namespace ork::lev2::scenegraph {
static logchannel_ptr_t logchan_sg = logger()->createChannel("scenegraph", fvec3(0.9, 0.2, 0.9));

void DrawableDataKvPair::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("Layer", &DrawableDataKvPair::_layername);
  clazz->directObjectProperty("DrawableData", &DrawableDataKvPair::_drawabledata);
}

///////////////////////////////////////////////////////////////////////////////

Scene::Scene() {

  ork::opq::assertOnQueue(opq::mainSerialQueue());

  _userdata = std::make_shared<varmap::VarMap>();

  auto params //
      = std::make_shared<varmap::VarMap>();
  params->makeValueForKey<std::string>("preset") = "DeferredPBR";
  this->initWithParams(params);
}
///////////////////////////////////////////////////////////////////////////////

Scene::Scene(varmap::varmap_ptr_t params) {
  _profile_timer.Start();
  _userdata = std::make_shared<varmap::VarMap>();
  initWithParams(params);
}

///////////////////////////////////////////////////////////////////////////////

Scene::~Scene() {
  if (_synchro) {
    _synchro->terminate();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuInit(Context* ctx) {
  _sgpickbuffer = std::make_shared<SgPickBuffer>(ctx, *this);
  _dogpuinit    = false;
  _boundContext = ctx;

  _compositorImpl->gpuInit(ctx);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuExit(Context* ctx) {
  _sgpickbuffer     = nullptr;
  _compositorImpl   = nullptr;
  _compositorData   = nullptr;
  _renderer         = nullptr;
  _lightManager     = nullptr;
  _lightManagerData = nullptr;
  _topCPD           = nullptr;
  _staticDrawables.clear();
  _layers.atomicOp([](layer_map_t& unlocked) { unlocked.clear(); });
  _userdata = nullptr;
  _nodes2draw.clear();
  //_instancednodes2draw.clear();
}

///////////////////////////////////////////////////////////////////////////////

void Scene::pickWithRay(fray3_constptr_t ray, SgPickBuffer::callback_t callback) {
  if (_sgpickbuffer)
    _sgpickbuffer->pickWithRay(ray, callback);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, SgPickBuffer::callback_t callback) {
  if (_sgpickbuffer)
    _sgpickbuffer->pickWithScreenCoord(cam, screencoord, callback);
}
///////////////////////////////////////////////////////////////////////////////

void Scene::initWithParams(varmap::varmap_ptr_t params) {

  _params = params;

  _dbufcontext_SG        = std::make_shared<DrawBufContext>();
  _dbufcontext_SG->_name = "DBC.SceneGraph";

  if (auto try_dbufcontext = params->typedValueForKey<dbufcontext_ptr_t>("dbufcontext")) {
    _dbufcontext_SG = try_dbufcontext.value();
  }

  _renderer         = std::make_shared<IRenderer>();
  _lightManagerData = std::make_shared<LightManagerData>();
  _lightManager     = std::make_shared<LightManager>(_lightManagerData);
  _compositorData   = std::make_shared<CompositingData>();
  _topCPD           = std::make_shared<CompositingPassData>();

  for (auto p : params->_themap) {
    auto k = p.first;
    auto v = p.second;
    // printf( "INITSCENE P<%s:%s>\n", k.c_str(), v.typeName());
  }

  std::string preset = "DeferredPBR";
  // std::string output = "SCREEN";

  if (auto try_preset = params->typedValueForKey<std::string>("preset"))
    preset = try_preset.value();
  // if (auto try_output = params->typedValueForKey<std::string>("output"))
  // output = try_output.value();

  rtgroup_ptr_t outRTG;

  if (auto try_rtgroup = params->typedValueForKey<rtgroup_ptr_t>("outputRTG")) {
    outRTG = try_rtgroup.value();
  }

  if (auto try_orcl = params->typedValueForKey<gfxcontext_lambda_t>("onRenderComplete")) {
    this->_on_render_complete = try_orcl.value();
  }

  pbr::commonstuff_ptr_t pbrcommon;

  if (preset == "Unlit") {
    _compositorPreset = _compositorData->presetUnlit(outRTG);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<compositor::UnlitNode>();
  }
  if (preset == "ForwardPBR") {
    _compositorPreset = _compositorData->presetForwardPBR(outRTG);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    pbrcommon         = outrnode->_pbrcommon;
  } else if (preset == "DeferredPBR") {
    _compositorPreset = _compositorData->presetDeferredPBR(outRTG);
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outpnode     = nodetek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::deferrednode::DeferredCompositingNodePbr>();

    if (auto try_supersample = params->typedValueForKey<int>("supersample")) {
      if (outpnode) {
        outpnode->setSuperSample(try_supersample.value());
      }
    }
    OrkAssert(outrnode);
    pbrcommon = outrnode->_pbrcommon;
  } else if (preset == "PBRVR") {
    _compositorPreset = _compositorData->presetPBRVR();
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::deferrednode::DeferredCompositingNodePbr>();
    pbrcommon         = outrnode->_pbrcommon;
  } else if (preset == "FWDPBRVR") {
    _compositorPreset = _compositorData->presetForwardPBRVR();
    auto nodetek      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode     = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    pbrcommon         = outrnode->_pbrcommon;
  } else if (preset == "PICKTEST") {
    auto cdata = std::make_shared<CompositingData>();
    cdata->presetPickingDebug();
    _compositorData = cdata;
  } else if (preset == "USER") {
    _compositorData = params->typedValueForKey<compositordata_ptr_t>("compositordata").value();
  } else {
    throw std::runtime_error("unknown compositor preset type");
  }
  //////////////////////////////////////////////

  if (pbrcommon) {

    if (auto try_bgtex = params->typedValueForKey<std::string>("SkyboxTexPathStr")) {
      auto texture_path = try_bgtex.value();
      pbrcommon->requestAndRefSkyboxTexture(texture_path);
    }

    if (auto try_envintensity = params->tryKeyAsNumber("EnvironmentIntensity")) {
      pbrcommon->_environmentIntensity = try_envintensity.value();
    }
    if (auto try_diffuseLevel = params->tryKeyAsNumber("DiffuseIntensity")) {
      pbrcommon->_diffuseLevel = try_diffuseLevel.value();
    }
    if (auto try_ambientLevel = params->typedValueForKey<fvec3>("AmbientLight")) {
      pbrcommon->_ambientLevel = try_ambientLevel.value();
    }
    if (auto try_skyboxLevel = params->tryKeyAsNumber("SkyboxIntensity")) {
      pbrcommon->_skyboxLevel = try_skyboxLevel.value();
    }
    if (auto try_specularLevel = params->tryKeyAsNumber("SpecularIntensity")) {
      pbrcommon->_specularLevel = try_specularLevel.value();
    }
    if (auto try_DepthFogDistance = params->tryKeyAsNumber("DepthFogDistance")) {
      pbrcommon->_depthFogDistance = try_DepthFogDistance.value();
    }
    if (auto try_DepthFogPower = params->tryKeysAsNumber("DepthFogPower", "depthFogPower")) {
      pbrcommon->_depthFogPower = try_DepthFogPower.value();
    }
    if (auto try_dfdist = params->tryKeysAsNumber("DepthFogDistance", "depthFogDistance")) {
      pbrcommon->_depthFogDistance = try_dfdist.value();
    }
  }
  //////////////////////////////////////////////

  _compositorData->mbEnable = true;
  _compositorTechnique      = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");

  _outputNode = _compositorTechnique->tryOutputNodeAs<OutputCompositingNode>();
  _renderNode = _compositorTechnique->tryRenderNodeAs<RenderCompositingNode>();

  if (params->hasKey("PostFxChain")) {
    auto& pfxchain = params->valueForKey("PostFxChain");
    if( auto as_chain = pfxchain.tryAs<postfx_node_chain_t>() ){
      _compositorTechnique->_postEffectNodes = as_chain.value();
    }
    // OrkAssert(false);
  }

  _compositorImpl = _compositorData->createImpl();
  _compositorImpl->bindLighting(_lightManager.get());
  _topCPD->addStandardLayers();
}

///////////////////////////////////////////////////////////////////////////////

layer_ptr_t Scene::createLayer(std::string named) {

  auto l = std::make_shared<Layer>(this, named);

  _layers.atomicOp([&](layer_map_t& unlocked) {
    auto it = unlocked.find(named);
    OrkAssert(it == unlocked.end());
    unlocked[named] = l;
  });

  if (DEBUG_LOG) {
    logchan_sg->log("Scene<%p> create layer<%p:%s>", this, l.get(), (void*)named.c_str());
  }

  return l;
}

///////////////////////////////////////////////////////////////////////////////

layer_ptr_t Scene::findLayer(std::string named) {

  layer_ptr_t rval;

  _layers.atomicOp([&](layer_map_t& unlocked) {
    auto it = unlocked.find(named);
    if(it==unlocked.end()){
      printf( "Layer<%s> not found\n", named.c_str());
      OrkAssert(false);
    }
    rval = it->second;
  });

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

compositorpostnode_ptr_t Scene::getPostNode( size_t index ) const {
  return _compositorTechnique->_postEffectNodes[index];
}

///////////////////////////////////////////////////////////////////////////////

size_t Scene::getPostNodeCount() const {
  return _compositorTechnique->_postEffectNodes.size();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph

ImplementReflectionX(ork::lev2::scenegraph::Node, "scenegraph::Node");
