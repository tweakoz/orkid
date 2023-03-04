////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/ui/event.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

using namespace std::string_literals;
using namespace ork;

static constexpr bool DEBUG_LOG = false;

ImplementReflectionX(ork::lev2::scenegraph::DrawableDataKvPair, "SgDrawableDataKvPair");

namespace ork::lev2::scenegraph {
static logchannel_ptr_t logchan_sg = logger()->createChannel("scenegraph",fvec3(0.9,0.2,0.9));

void DrawableDataKvPair::describeX(object::ObjectClass* clazz){
  clazz->directProperty("Layer", &DrawableDataKvPair::_layername);
  clazz->directObjectProperty("DrawableData", &DrawableDataKvPair::_drawabledata);
}

Synchro::Synchro(){

  _updcount.store(0);
  _rencount.store(0);
}

Synchro::~Synchro(){
  terminate();
}
void Synchro::terminate(){
  printf( "Synchro<%p> terminating...\n", this );
}

///////////////////////////////////////////////////

bool Synchro::beginUpdate(){
  return (_updcount.load()==_rencount.load());
}

///////////////////////////////////////////////////

void Synchro::endUpdate(){
  _updcount++;
}

///////////////////////////////////////////////////

bool Synchro::beginRender(){
  return ((_updcount.load()-1)==_rencount.load());
}

///////////////////////////////////////////////////

void Synchro::endRender(){
  _rencount++;
}

///////////////////////////////////////////////////////////////////////////////

Node::Node(std::string named)
    : _name(named) {
  _userdata = std::make_shared<varmap::VarMap>();
}

///////////////////////////////////////////////////////////////////////////////

Node::~Node() {
}

///////////////////////////////////////////////////////////////////////////////

DrawableNode::DrawableNode(std::string named, drawable_ptr_t drawable)
    : Node(named)
    , _drawable(drawable) {
    _modcolor = fvec4(1,1,1,1);
}

///////////////////////////////////////////////////////////////////////////////

DrawableNode::~DrawableNode() {
}

///////////////////////////////////////////////////////////////////////////////

InstancedDrawableNode::InstancedDrawableNode(std::string named, instanced_drawable_ptr_t drawable)
    : Node(named)
    , _drawable(drawable) {
}

///////////////////////////////////////////////////////////////////////////////

InstancedDrawableNode::~InstancedDrawableNode() {
}

///////////////////////////////////////////////////////////////////////////////

LightNode::LightNode(std::string named, light_ptr_t light)
    : Node(named)
    , _light(light) {
}

///////////////////////////////////////////////////////////////////////////////

LightNode::~LightNode() {
}

///////////////////////////////////////////////////////////////////////////////

Layer::Layer(Scene* scene, std::string named)
    : _scene(scene)
    , _name(named) {
}

///////////////////////////////////////////////////////////////////////////////

Layer::~Layer() {
}

///////////////////////////////////////////////////////////////////////////////

drawable_node_ptr_t Layer::createDrawableNode(std::string named, drawable_ptr_t drawable) {
  drawable_node_ptr_t rval = std::make_shared<DrawableNode>(named, drawable);
  _drawable_nodes.atomicOp([rval](Layer::drawablenodevect_t& unlocked) { unlocked.push_back(rval); });
  if(DEBUG_LOG){
    logchan_sg->log( "createDrawableNode layer<%s> named<%s> drawable<%p> node<%p>", //
                     _name.c_str(), //
                     named.c_str(), // 
                     drawable.get(), // 
                     rval.get() ); //
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeDrawableNode(drawable_node_ptr_t node) {
  _drawable_nodes.atomicOp([node](Layer::drawablenodevect_t& unlocked) {
    auto it = std::remove_if(unlocked.begin(), unlocked.end(), [node](drawable_node_ptr_t d) -> bool {
      bool matched = (node == d);
      return matched;
    });
    unlocked.erase(it);
  });
}

///////////////////////////////////////////////////////////////////////////////

instanced_drawable_node_ptr_t Layer::createInstancedDrawableNode(std::string named, instanced_drawable_ptr_t drawable) {
  instanced_drawable_node_ptr_t rval = std::make_shared<InstancedDrawableNode>(named, drawable);
  size_t count = 0;
  _instanced_drawable_map.atomicOp([rval,drawable,&count](Layer::instanced_drawmap_t& unlocked) { //
    auto& vect = unlocked[drawable];
    rval->_instanced_drawable_id = vect.size();
    vect.push_back(rval);
    count = rval->_instanced_drawable_id+1;
  });
  drawable->_instancedata->resize(count);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeInstancedDrawableNode(instanced_drawable_node_ptr_t node) {

  _instanced_drawable_map.atomicOp([node](Layer::instanced_drawmap_t& unlocked) {

    auto drawable = node->_drawable;
  
    auto it_d = unlocked.find(drawable);
    OrkAssert(it_d!=unlocked.end());

    auto& vect = it_d->second;

    if(vect.size()>1){
      auto it = vect.begin() + node->_instanced_drawable_id;
      auto rit = vect.rbegin();
      *it = *rit;
      vect.erase(rit.base());
    }
    else{
      vect.clear();
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

lightnode_ptr_t Layer::createLightNode(std::string named, light_ptr_t light) {
  lightnode_ptr_t rval = std::make_shared<LightNode>(named, light);

  _lightnodes.atomicOp([rval](Layer::lightnodevect_t& unlocked) { unlocked.push_back(rval); });

  auto lmgr = _scene->_lightManager;
  lmgr->mGlobalMovingLights.AddLight(light.get());

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeLightNode(lightnode_ptr_t node) {
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
  if(_synchro){
    _synchro->terminate();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuInit(Context* ctx) {
  _pickbuffer   = std::make_shared<PickBuffer>(ctx, *this);
  _dogpuinit    = false;
  _boundContext = ctx;

  _compositorImpl->gpuInit(ctx);
}

void Scene::gpuExit(Context* ctx){
  _pickbuffer = nullptr;
  _compositorImpl = nullptr;
  _compositorData = nullptr;
  _renderer = nullptr;
  _lightManager = nullptr;
  _lightManagerData = nullptr;
  _topCPD = nullptr;
  _staticDrawables.clear();
  _layers.atomicOp([](layer_map_t& unlocked){
    unlocked.clear();
  });
  _userdata = nullptr;
  _nodes2draw.clear();
  //_instancednodes2draw.clear();
}

///////////////////////////////////////////////////////////////////////////////

uint64_t Scene::pickWithRay(fray3_constptr_t ray) {
  OrkAssert(_pickbuffer);
  return _pickbuffer->pickWithRay(ray);
}

///////////////////////////////////////////////////////////////////////////////

uint64_t Scene::pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord) {
  OrkAssert(_pickbuffer);
  return _pickbuffer->pickWithScreenCoord(cam, screencoord);
}
///////////////////////////////////////////////////////////////////////////////

void Scene::initWithParams(varmap::varmap_ptr_t params) {
  
  _params = params;

  _dbufcontext_SG = std::make_shared<DrawBufContext>();
  _dbufcontext_SG->_name = "DBC.SceneGraph";

  _renderer = std::make_shared<DefaultRenderer>();
  _lightManagerData = std::make_shared<LightManagerData>();
  _lightManager     = std::make_shared<LightManager>(*_lightManagerData.get());
  _compositorData   = std::make_shared<CompositingData>();
  _topCPD = std::make_shared<CompositingPassData>();

  for( auto p : params->_themap ){
    auto k = p.first;
    auto v = p.second;
    printf( "INITSCENE P<%s:%s>\n", k.c_str(), v.typeName());
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
    auto nodetek  = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode = nodetek->tryRenderNodeAs<compositor::UnlitNode>();
  }
  if (preset == "ForwardPBR") {
    _compositorPreset = _compositorData->presetForwardPBR(outRTG);
    auto nodetek  = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    pbrcommon = outrnode->_pbrcommon;
  }
  else if (preset == "DeferredPBR") {
    _compositorPreset = _compositorData->presetDeferredPBR(outRTG);
    auto nodetek  = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outpnode = nodetek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
    auto outrnode = nodetek->tryRenderNodeAs<pbr::deferrednode::DeferredCompositingNodePbr>();

    if (auto try_supersample = params->typedValueForKey<int>("supersample")) {
      if(outpnode){
        outpnode->setSuperSample(try_supersample.value());
      }
    }
    OrkAssert(outrnode);
    pbrcommon = outrnode->_pbrcommon;
  } else if (preset == "PBRVR") {
    _compositorPreset = _compositorData->presetPBRVR();
    auto nodetek  = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode = nodetek->tryRenderNodeAs<pbr::deferrednode::DeferredCompositingNodePbr>();
    pbrcommon = outrnode->_pbrcommon;
  } else if (preset == "FWDPBRVR") {
    _compositorPreset = _compositorData->presetForwardPBRVR();
    auto nodetek  = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto outrnode = nodetek->tryRenderNodeAs<pbr::ForwardNode>();
    pbrcommon = outrnode->_pbrcommon;
  } else if (preset == "USER"){
    _compositorData = params->typedValueForKey<compositordata_ptr_t>("compositordata").value();
  } else {
    throw std::runtime_error("unknown compositor preset type");
  }
  //////////////////////////////////////////////

  if(pbrcommon){

      if (auto try_bgtex = params->typedValueForKey<std::string>("SkyboxTexPathStr")) {
        auto texture_path        = try_bgtex.value();
        auto load_req = pbrcommon->requestSkyboxTexture(texture_path);
      }

      if (auto try_envintensity = params->typedValueForKey<float>("EnvironmentIntensity")) {
        pbrcommon->_environmentIntensity = try_envintensity.value();
      }
      if (auto try_diffuseLevel = params->typedValueForKey<float>("DiffuseIntensity")) {
        pbrcommon->_diffuseLevel = try_diffuseLevel.value();
      }
      if (auto try_ambientLevel = params->typedValueForKey<fvec3>("AmbientLight")) {
        pbrcommon->_ambientLevel = try_ambientLevel.value();
      }
      if (auto try_skyboxLevel = params->typedValueForKey<float>("SkyboxIntensity")) {
        pbrcommon->_skyboxLevel = try_skyboxLevel.value();
      }
      if (auto try_specularLevel = params->typedValueForKey<float>("SpecularIntensity")) {
        pbrcommon->_specularLevel = try_specularLevel.value();
      }
      if (auto try_DepthFogDistance = params->typedValueForKey<float>("DepthFogDistance")) {
        pbrcommon->_depthFogDistance = try_DepthFogDistance.value();
      }
      if (auto try_DepthFogPower = params->typedValueForKey<float>("DepthFogPower")) {
        pbrcommon->_depthFogPower = try_DepthFogPower.value();
      }
      if (auto try_dfdist = params->typedValueForKey<float>("depthFogDistance")) {
        pbrcommon->_depthFogDistance = try_dfdist.value();
      }
      if (auto try_dfpwr = params->typedValueForKey<float>("depthFogPower")) {
        pbrcommon->_depthFogPower = try_dfpwr.value();
      }
  }
  //////////////////////////////////////////////

  _compositorData->mbEnable = true;
  _compositorTechnique       = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");

  _outputNode = _compositorTechnique->tryOutputNodeAs<OutputCompositingNode>();
  _renderNode = _compositorTechnique->tryRenderNodeAs<RenderCompositingNode>();

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

  if(DEBUG_LOG){
    logchan_sg->log( "Scene<%p> create layer<%p:%s>", this, l.get(), (void*) named.c_str() );
  }

  return l;
}

///////////////////////////////////////////////////////////////////////////////

layer_ptr_t Scene::findLayer(std::string named) {

  layer_ptr_t rval;

  _layers.atomicOp([&](layer_map_t& unlocked) {
    auto it = unlocked.find(named);
    OrkAssert(it != unlocked.end());
    rval = it->second;
  });

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// enqueue scenegraph to renderer (update thread)
///////////////////////////////////////////////////////////////////////////////

void Scene::enqueueToRenderer(cameradatalut_ptr_t cameras,on_enqueue_fn_t on_enqueue) {

  if(_synchro){
    bool OK = _synchro->beginUpdate();
    if(not OK){
      return;
    }
  }


  auto DB = _dbufcontext_SG->acquireForWriteLocked();
  DB->Reset();
  DB->copyCameras(*cameras.get());

  on_enqueue(DB);

  if(DEBUG_LOG){
    logchan_sg->log( "Scene<%p> enqueueToRenderer DB<%p>", this, DB );
  }

  _nodes2draw.clear();
  //_instancednodes2draw.clear();

  std::vector<layer_ptr_t> layers;

  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (auto layer_item : unlocked) {
      layers.push_back(layer_item.second);
    }
  });

  ////////////////////////////////////////////////////////////////////////////

  for (auto l : layers) {

    auto drawable_layer = DB->MergeLayer(l->_name);

    l->_drawable_nodes.atomicOp([drawable_layer, this](const Layer::drawablenodevect_t& unlocked) {
      for (auto n : unlocked) {
        if (n->_drawable and n->_enabled) {
          DrawItem item;
          item._layer   = drawable_layer;
          item._drwnode = n;
          _nodes2draw.push_back(item);
        }
      }
    });

  ////////////////////////////////////////////////////////////////////////////

    l->_instanced_drawable_map.atomicOp([drawable_layer, this](const Layer::instanced_drawmap_t& unlocked) {
      for (const auto& map_item : unlocked) {
        instanced_drawable_ptr_t drawable = map_item.first;
        if(drawable){
          const Layer::instanced_drawablenodevect_t& instances_vect = map_item.second;
          size_t instances_count = instances_vect.size();
          drawable->resize(instances_count);
          if(DEBUG_LOG){
            logchan_sg->log( "enqueue instanced-drawable<%s> instances_count<%zu>", drawable->_name.c_str(), instances_count );
          }
        }
      }
    });
  
  } // for (auto l : layers) {

  ////////////////////////////////////////////////////////////////////////////

  if(1)for (auto item : _nodes2draw) {
    auto& drawable_layer = item._layer;
    auto n              = item._drwnode;
    if(DEBUG_LOG){

      logchan_sg->log( "enqueue drawable<%s> on layer<%s>", //
                       (void*) n->_drawable->_name.c_str(), //
                       drawable_layer->_name.c_str() );
    }
    n->_drawable->enqueueOnLayer(n->_dqxfdata, *drawable_layer);
  }

  ////////////////////////////////////////////////////////////////////////////

  if(1)for (auto item : _staticDrawables) {
    auto drawable_layer = DB->MergeLayer(item._layername);
    auto drawable = item._drawable;
    DrawQueueXfData xfd;
    if(DEBUG_LOG){
      logchan_sg->log( "enqueue static-drawable<%s>",  (void*) drawable->_name.c_str() );
    }
    drawable->enqueueOnLayer(xfd, *drawable_layer);
  }

  _dbufcontext_SG->releaseFromWriteLocked(DB);

  if(_synchro){
    _synchro->endUpdate();
  }

}

///////////////////////////////////////////////////////////////////////////////

void Scene::_renderIMPL(Context* context,RenderContextFrameData& RCFD){

  if (_dogpuinit) {
    gpuInit(context);
  }

  if(_synchro){
    bool OK = _synchro->beginRender();
    if( not OK ){
      return;
    }
  }

  auto DB = _dbufcontext_SG->acquireForReadLocked();

  RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));

  RCFD.setUserProperty("time"_crc,_currentTime);

  RCFD._cimpl = _compositorImpl;

  _renderer->setContext(context);

  context->pushRenderContextFrameData(&RCFD);
  auto fbi  = context->FBI();  // FrameBufferInterface
  auto fxi  = context->FXI();  // FX Interface
  auto mtxi = context->MTXI(); // matrix Interface
  auto gbi  = context->GBI();  // GeometryBuffer Interface
  ///////////////////////////////////////
  // compositor setup
  ///////////////////////////////////////
  float TARGW = fbi->GetVPW();
  float TARGH = fbi->GetVPH();
  lev2::UiViewportRenderTarget rt(nullptr);
  auto tgtrect           = ViewportRect(0, 0, TARGW, TARGH);
  _topCPD->_irendertarget = &rt;
  _topCPD->SetDstRect(tgtrect);
  _compositorImpl->pushCPD(*_topCPD);
  ///////////////////////////////////////
  // Draw!
  ///////////////////////////////////////
  fbi->SetClearColor(fvec4(0, 0, 0, 1));
  fbi->setViewport(tgtrect);
  fbi->setScissor(tgtrect);
  if (1) {
    context->beginFrame();
    FrameRenderer framerenderer(RCFD, [&]() {});
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(_renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawableBuffer*>(DB);
    drawdata._cimpl = _compositorImpl;
    _compositorImpl->assemble(drawdata);
    _compositorImpl->composite(drawdata);
    _compositorImpl->popCPD();
    context->popRenderContextFrameData();


    if(_on_render_complete){
      _on_render_complete(context);
    }

    ////////////////////////////////////////////////////////////////////////////
    // debug picking here, so it shows up in renderdoc (within frame boundaries)
    ////////////////////////////////////////////////////////////////////////////
    if (0) {
      auto r   = std::make_shared<fray3>(fvec3(0, 0, 5), fvec3(0, 0, -1));
      auto val = pickWithRay(r);
      // printf("%zx\n", val);
    }
    ////////////////////////////////////////////////////////////////////////////

    context->endFrame();
  }
  _dbufcontext_SG->releaseFromReadLocked(DB);

  if(_synchro){
    _synchro->endRender();
  }

}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderWithStandardCompositorFrame(standardcompositorframe_ptr_t sframe){
  auto context = sframe->_drawEvent->GetTarget();
  if (_dogpuinit) {
    sframe->attachDrawBufContext(_dbufcontext_SG);
    gpuInit(context);
  }
  _renderer->setContext(context);
  sframe->compositor = _compositorImpl;
  sframe->renderer = _renderer;
  sframe->passdata   = _topCPD;
  sframe->render();
}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderOnContext(Context* context, RenderContextFrameData& RCFD) {
  _renderIMPL(context,RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderOnContext(Context* context) {
  // from SceneGraphSystem::_onRender
  RenderContextFrameData RCFD(context); // renderer per/frame data
  _renderIMPL(context,RCFD);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
