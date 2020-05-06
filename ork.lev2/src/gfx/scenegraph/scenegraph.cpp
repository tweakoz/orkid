#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/application/application.h>
using namespace std::string_literals;
using namespace ork;

namespace ork::lev2::scenegraph {
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
}

///////////////////////////////////////////////////////////////////////////////

DrawableNode::~DrawableNode() {
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

drawablenode_ptr_t Layer::createDrawableNode(std::string named, drawable_ptr_t drawable) {
  drawablenode_ptr_t rval  = std::make_shared<DrawableNode>(named, drawable);
  _drawablenode_map[named] = rval;
  _drawablenodes.push_back(rval);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeDrawableNode(drawablenode_ptr_t node) {
}

///////////////////////////////////////////////////////////////////////////////

lightnode_ptr_t Layer::createLightNode(std::string named, light_ptr_t light) {
  lightnode_ptr_t rval  = std::make_shared<LightNode>(named, light);
  _lightnode_map[named] = rval;
  _lightnodes.push_back(rval);

  auto lmgr = _scene->_lightManager;
  lmgr->mGlobalMovingLights.AddLight(light.get());

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeLightNode(lightnode_ptr_t node) {
}

///////////////////////////////////////////////////////////////////////////////

Scene::Scene() {
  _userdata = std::make_shared<varmap::VarMap>();

  auto params //
      = std::make_shared<varmap::VarMap>();
  params->makeValueForKey<std::string>("preset") = "PBR";
  this->initWithParams(params);
}
///////////////////////////////////////////////////////////////////////////////

Scene::Scene(varmap::varmap_ptr_t params) {
  _userdata = std::make_shared<varmap::VarMap>();
  initWithParams(params);
}

///////////////////////////////////////////////////////////////////////////////

Scene::~Scene() {
}

///////////////////////////////////////////////////////////////////////////////

void Scene::gpuInit(Context* ctx) {
  _pickbuffer   = std::make_shared<PickBuffer>(ctx, *this);
  _dogpuinit    = false;
  _boundContext = ctx;
}

///////////////////////////////////////////////////////////////////////////////

uint64_t Scene::pickWithRay(fray3_constptr_t ray) {
  OrkAssert(_pickbuffer);
  return _pickbuffer->pickWithRay(ray);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::initWithParams(varmap::varmap_ptr_t params) {
  _lightManagerData = std::make_shared<LightManagerData>();
  _lightManager     = std::make_shared<LightManager>(*_lightManagerData.get());
  _compositorData   = std::make_shared<CompositingData>();

  std::string preset = "PBR";
  // std::string output = "SCREEN";

  if (auto try_preset = params->typedValueForKey<std::string>("preset"))
    preset = try_preset.value();
  // if (auto try_output = params->typedValueForKey<std::string>("output"))
  // output = try_output.value();

  if (preset == "PBR")
    _compositorData->presetPBR();
  else if (preset == "PBRVR")
    _compositorData->presetPBRVR();
  else
    throw std::runtime_error("unknown compositor preset type");

  _compositorData->mbEnable = true;
  _compostorTechnique       = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);

  _outputNode = _compostorTechnique->tryOutputNodeAs<OutputCompositingNode>();
  //_outputNode = _compostorTechnique->tryOutputNodeAs<VrCompositingNode>();
  // throw std::runtime_error("unknown compositor outputnode type");

  // outpnode->setSuperSample(4);
  _compositorImpl = _compositorData->createImpl();
  _compositorImpl->bindLighting(_lightManager.get());
  _topCPD.addStandardLayers();
}

///////////////////////////////////////////////////////////////////////////////

layer_ptr_t Scene::createLayer(std::string named) {

  auto it = _layers.find(named);
  OrkAssert(it == _layers.end());
  auto l         = std::make_shared<Layer>(this, named);
  _layers[named] = l;
  return l;
}

///////////////////////////////////////////////////////////////////////////////

void Scene::enqueueToRenderer(cameradatalut_ptr_t cameras) {
  auto DB = DrawableBuffer::LockWriteBuffer(0);
  DB->Reset();
  DB->copyCameras(*cameras.get());
  for (auto layer_item : _layers) {
    auto scenegraph_layer = layer_item.second;
    auto drawable_layer   = DB->MergeLayer("Default"_pool);
    for (auto n : scenegraph_layer->_drawablenodes) {
      if (n->_drawable) {
        n->_drawable->enqueueOnLayer(n->_transform, *drawable_layer);
      }
    }
  }
  DrawableBuffer::UnLockWriteBuffer(DB);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderOnContext(Context* context) {

  if (_dogpuinit) {
    gpuInit(context);
  }

  auto DB = DrawableBuffer::acquireReadDB(7);
  if (nullptr == DB) {
    printf("Dont have a DB!\n");
    return;
  }

  _renderer.setContext(context);

  RenderContextFrameData RCFD(context); // renderer per/frame data
  RCFD._cimpl = _compositorImpl;
  RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
  context->pushRenderContextFrameData(&RCFD);
  auto fbi  = context->FBI();  // FrameBufferInterface
  auto fxi  = context->FXI();  // FX Interface
  auto mtxi = context->MTXI(); // matrix Interface
  auto gbi  = context->GBI();  // GeometryBuffer Interface
  ///////////////////////////////////////
  // compositor setup
  ///////////////////////////////////////
  float TARGW = context->mainSurfaceWidth();
  float TARGH = context->mainSurfaceHeight();
  lev2::UiViewportRenderTarget rt(nullptr);
  auto tgtrect           = ViewportRect(0, 0, TARGW, TARGH);
  _topCPD._irendertarget = &rt;
  _topCPD.SetDstRect(tgtrect);
  _compositorImpl->pushCPD(_topCPD);
  ///////////////////////////////////////
  // Draw!
  ///////////////////////////////////////
  fbi->SetClearColor(fvec4(0, 0, 0, 1));
  fbi->setViewport(tgtrect);
  fbi->setScissor(tgtrect);
  context->beginFrame();
  FrameRenderer framerenderer(RCFD, [&]() {});
  CompositorDrawData drawdata(framerenderer);
  drawdata._properties["primarycamindex"_crcu].Set<int>(0);
  drawdata._properties["cullcamindex"_crcu].Set<int>(0);
  drawdata._properties["irenderer"_crcu].Set<lev2::IRenderer*>(&_renderer);
  drawdata._properties["simrunning"_crcu].Set<bool>(true);
  drawdata._properties["DB"_crcu].Set<const DrawableBuffer*>(DB);
  drawdata._cimpl = _compositorImpl;
  _compositorImpl->assemble(drawdata);
  _compositorImpl->composite(drawdata);
  _compositorImpl->popCPD();
  context->popRenderContextFrameData();
  context->endFrame();

  DrawableBuffer::releaseReadDB(DB);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
