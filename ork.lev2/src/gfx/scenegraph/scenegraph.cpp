#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/application/application.h>
using namespace std::string_literals;
using namespace ork;

namespace ork::lev2::scenegraph {
///////////////////////////////////////////////////////////////////////////////

Node::Node() {
}

///////////////////////////////////////////////////////////////////////////////

Node::~Node() {
}

///////////////////////////////////////////////////////////////////////////////

Layer::Layer(std::string named)
    : _name(named) {
}

///////////////////////////////////////////////////////////////////////////////

Layer::~Layer() {
}

///////////////////////////////////////////////////////////////////////////////

void Layer::addNode(scenenode_ptr_t node) {
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeNode(scenenode_ptr_t node) {
}

///////////////////////////////////////////////////////////////////////////////

SceneGraph::SceneGraph() {

  // todo - allow user parameterization

  _lmd            = std::make_shared<LightManagerData>();
  _lightManager   = std::make_shared<LightManager>(*_lmd.get());
  _compositorData = std::make_shared<CompositingData>();
  _compositorData->presetPBR();
  _compositorData->mbEnable = true;
  _compostorTechnique       = _compositorData->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
  _outputNode               = _compostorTechnique->tryOutputNodeAs<ScreenOutputCompositingNode>();
  // outpnode->setSuperSample(4);
  _compositorImpl = std::make_shared<CompositingImpl>(*_compositorData.get());
  _compositorImpl->bindLighting(_lightManager.get());
  _topCPD.addStandardLayers();
  _cameras = std::make_shared<CameraDataLut>();
  _camera  = std::make_shared<CameraData>();
  _cameras->AddSorted("spawncam"_pool, _camera.get());
}

///////////////////////////////////////////////////////////////////////////////

SceneGraph::~SceneGraph() {
}

///////////////////////////////////////////////////////////////////////////////

scenelayer_ptr_t SceneGraph::createLayer(std::string named) {

  auto it = _layers.find(named);
  OrkAssert(it == _layers.end());

  auto l         = std::make_shared<Layer>(named);
  _layers[named] = l;
  return l;
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraph::enqueueToRenderer() {
  auto DB = DrawableBuffer::LockWriteBuffer(0);
  DB->Reset();
  // todo - enumerate SceneGraph Layers into DrawableBuffer Layers
  // DB->copyCameras(cameras);
  // auto layer = DB->MergeLayer("Default"_pool);
  DrawableBuffer::UnLockWriteBuffer(DB);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraph::renderOnContext(Context* context) {
  auto DB = DrawableBuffer::acquireReadDB(7);
  if (nullptr == DB)
    return;

  RenderContextFrameData RCFD(context); // renderer per/frame data
  RCFD._cimpl = _compositorImpl.get();
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
  //_compositorImpl->assemble(drawdata);
  //_compositorImpl->composite(drawdata);
  _compositorImpl->popCPD();
  context->popRenderContextFrameData();
  context->endFrame();

  DrawableBuffer::releaseReadDB(DB);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
