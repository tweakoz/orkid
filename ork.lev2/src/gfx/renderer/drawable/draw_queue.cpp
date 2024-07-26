////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/semaphore.h>

#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/register.h>

#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/math/collision_test.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/triple_buffer.h>
#include <ork/profiling.inl>

namespace ork::lev2 {

static ork::atomic<bool> _gInsideClearAndSync = 0;
ork::atomic<int> DrawQueue::_gate = 1;

////////////////////////////////////////////////////////////////

LayerData::LayerData() {
}

///////////////////////////////////////////////////////////////////////////////

void DrawQueue::setPreRenderCallback(int key, prerendercallback_t cb) {
  _preRenderCallbacks.AddSorted(key, cb);
}

///////////////////////////////////////////////////////////////////////////////

void DrawQueue::invokePreRenderCallbacks(lev2::rcfd_ptr_t RCFD) const {
  EASY_BLOCK("prerender");
  for (auto item : _preRenderCallbacks)
    item.second(*RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void DrawQueue::enqueueLayerToRenderQueue(const std::string& LayerName, lev2::IRenderer* renderer) const {
  lev2::Context* target                         = renderer->GetTarget();
  auto RCFD = target->topRenderContextFrameData();
  const auto& topCPD                            = RCFD->topCPD();

  if (false == topCPD.isValid()) {
    OrkAssert(false);
    return;
  }

  int numdrawables = 0;

  bool do_all = (LayerName == "All");
  target->debugMarker(FormatString("DrawQueue::enqueueLayerToRenderQueue do_all<%d>", int(do_all)));
  target->debugMarker(FormatString("DrawQueue::enqueueLayerToRenderQueue numlayers<%zu>", mLayerLut.size()));

  //printf( "rendering <%s> do_all<%d>\n", LayerName.c_str(), int(do_all) );
  //////////////////////////////////////////////////////////////////////////////////////////////
  auto do_layer = [target,renderer,&numdrawables,LayerName](const lev2::DrawQueueLayer* player){
      player->_items.atomicOp([player,target,renderer,&numdrawables,LayerName](const DrawQueueLayer::itemvect_t& unlocked){
        int max_index = player->_itemIndex;
        for (int id = 0; id < max_index; id++) {
          auto item = unlocked[id];
          const lev2::Drawable* pdrw        = item->_drawable;
          target->debugMarker(FormatString("DrawQueue::enqueueLayerToRenderQueue layer item <%d> drw<%p>", id, pdrw));
          if (pdrw) {
            numdrawables++;
            pdrw->enqueueToRenderQueue(item, renderer);
          }
        }
        if( true ) { //} renderer->_debugLog ){
          auto str = FormatString("DrawQueue::enqueueLayerToRenderQueue layer<%s> itemcount<%d>", LayerName.c_str(), max_index + 1);
          printf( "%s\n", str.c_str() );
        }
      }); // player->_items.atomicOp
  };
  //////////////////////////////////////////////////////////////////////////////////////////////
  for (const auto& layer_item : mLayerLut) {

    const std::string& TestLayerName     = layer_item.first;
    const lev2::DrawQueueLayer* player = layer_item.second;

    bool Match = (LayerName == TestLayerName);
    bool has = topCPD.HasLayer(TestLayerName);
    printf( "against layer<%s> match<%d> has<%d>\n", TestLayerName.c_str(), int(Match), int(has) );

    target->debugMarker(FormatString(
        "DrawQueue::enqueueLayerToRenderQueue TestLayerName<%s> player<%p> Match<%d>",
        TestLayerName.c_str(),
        player,
        int(Match)));

    if (do_all || (Match && topCPD.HasLayer(TestLayerName))) {
      //printf( "layer<%s> count<%d>\n", TestLayerName.c_str(), player->_itemIndex );
      target->debugMarker(FormatString("DrawQueue::enqueueLayerToRenderQueue layer itemcount<%d>", player->_itemIndex + 1));
      do_layer(player);
    }
  }

  //printf( "DB<%p> enqueued %d drawables\n", this, numdrawables);
}

///////////////////////////////////////////////////////////////////////////////

void DrawQueue::Reset() {
  ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  _state.store(999);
  miNumLayersUsed = 0;
  mLayers.clear();
  for (int il = 0; il < kmaxlayers; il++) {
    mRawLayers[il].Reset(*this);
  }
  _preRenderCallbacks.clear();
  _cameraDataLUT.atomicOp([](cameradatalut_ptr_t& unlocked){unlocked->clear();});
  _state.store(1000);
}
///////////////////////////////////////////////////////////////////////////////
void DrawQueue::terminate() {
  Reset();
  //for (int il = 0; il < kmaxlayers; il++) {
    //mRawLayers[il].terminate();
  //}
}
///////////////////////////////////////////////////////////////////////////////

void DrawQueueLayer::Reset(const DrawQueue& dB) {
  // ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  miBufferIndex = dB.miBufferIndex;
  _items.atomicOp([this](DrawQueueLayer::itemvect_t& unlocked){
    unlocked.clear();
  _itemIndex   = -1;
  });
}

///////////////////////////////////////////////////////////////////////////////

DrawQueueLayer* DrawQueue::MergeLayer(const std::string& layername) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  DrawQueueLayer* player     = 0;
  LayerLut::const_iterator itL = mLayerLut.find(layername);
  if (itL != mLayerLut.end()) {
    player = itL->second;
  } else {
    OrkAssert(miNumLayersUsed < kmaxlayers);
    player = &mRawLayers[miNumLayersUsed++];
    player->_name = layername;
    mLayers.insert(layername);
    mLayerLut.AddSorted(layername, player);
  }
  return player;
}

///////////////////////////////////////////////////////////////////////////////

drawqueueitem_ptr_t DrawQueueLayer::enqueueDrawable(const DrawQueueTransferData& xfdata, const Drawable* d) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // _items.push_back(DrawQueueItem()); // replace std::vector with an array so we can amortize construction costs
  auto item = std::make_shared<DrawQueueItem>(); // todo USE POOL
  item->_drawable = d;
  item->_dqxferdata       = xfdata;
  item->_bufferIndex = miBufferIndex;
  static std::atomic<int> counter = 0;
  item->_serialno = counter++;
  item->_sortkey = _sortkey;
  _items.atomicOp([this,item](DrawQueueLayer::itemvect_t& unlocked){
    unlocked.push_back(item);
    _itemIndex = unlocked.size();
  });
  return item;
}

///////////////////////////////////////////////////////////////////////////////

DrawQueue::DrawQueue(int ibidx)
    : miNumLayersUsed(0) 
    , miBufferIndex(ibidx) {
    _state.store(1000);

    _cameraDataLUT.atomicOp([](cameradatalut_ptr_t& unlocked){
      unlocked = std::make_shared<CameraDataLut>();
    });
}
DrawQueue::~DrawQueue() {
  _state.store(0);
}
DrawQueueItem::DrawQueueItem()
      : _drawable(0)
      , _bufferIndex(0) {
  _state.store(1000);
  }

DrawQueueItem::~DrawQueueItem() {
    _state.store(0);
  }

///////////////////////////////////////////////////////////////////////////////

DrawQueueLayer::DrawQueueLayer()
    : _itemIndex(-1)
    , miBufferIndex(-1) {
    _state.store(107);
}
DrawQueueLayer::~DrawQueueLayer(){
    _state.store(0);
}

//void DrawQueueLayer::terminate() {
//}

void DrawQueueItem::terminate() {
  _drawable = nullptr;
}
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

void DrawQueue::copyCameras(const CameraDataLut& cameras) {
  _state.store(999);
  ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  _cameraDataLUT.atomicOp([&cameras](cameradatalut_ptr_t& unlocked){
    unlocked->clear();
    for (auto itCAM = cameras.begin(); itCAM != cameras.end(); itCAM++) {
      const std::string& CameraName       = itCAM->first;
      cameradata_constptr_t pcameradata = itCAM->second;
      if (pcameradata) {
        (*unlocked)[CameraName]=pcameradata;
      }
    }
  });
  _state.store(1000);
}

///////////////////////////////////////////////////////////////////////////////

cameradata_constptr_t DrawQueue::cameraData(int index) const {
  ork::opq::assertOnQueue2( opq::mainSerialQueue() );

  int state = _state.load();

  OrkAssert(state == 1000);

  cameradata_constptr_t rval = nullptr;
  _cameraDataLUT.atomicOp([index,&rval](const cameradatalut_ptr_t& unlocked){
    int inumscenecameras = unlocked->size();
    if( index<inumscenecameras){
      auto itCAM             = unlocked->begin();
      for( int i=0; i<index; i++ ){
        itCAM++;
      }
      cameradata_constptr_t pdata = itCAM->second;
      rval = pdata;
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

cameradata_constptr_t DrawQueue::cameraData(const std::string& named) const {
  ork::opq::assertOnQueue2( opq::mainSerialQueue() );
  cameradata_constptr_t rval = nullptr;
  _cameraDataLUT.atomicOp([this,named,&rval](const cameradatalut_ptr_t& unlocked){
    int inumscenecameras = unlocked->size();
    //printf( "DB<%p> NumSceneCameras<%d>\n", this, inumscenecameras );
    rval = unlocked->find(named);
  });
  return rval;
}

/////////////////////////////////////////////////////////////////////
DrawQueueContext::DrawQueueContext()
  : _lockedBufferMutex("lbuf")
  ,_rendersync_sema("lsema")
  ,_rendersync_sema2("lsema2") {
    _lockeddrawablebuffer = std::make_shared<DrawQueue>(99);
    _rendersync_sema2.notify();
  _triple = std::make_shared<tbuf_t>();
}

DrawQueueContext::~DrawQueueContext(){
}

#if 1 // TRIPLE BUFFER

DrawQueue* DrawQueueContext::acquireForWriteLocked(){
   auto rval = _triple->begin_push();
   //printf( "dbufctx<%p:%s> acquireForWriteLocked<%p>\n", this, _name.c_str(), (void*) rval );
   //_triple->dump();
   return rval;
}
void DrawQueueContext::releaseFromWriteLocked(DrawQueue* db){
   //printf( "dbufctx<%p:%s> releaseFromWriteLocked<%p>\n", this, _name.c_str(), (void*) db );
   _triple->end_push(db);;
}
const DrawQueue* DrawQueueContext::acquireForReadLocked(){
   auto rval = _triple->begin_pull();
   //printf( "dbufctx<%p:%s> acquireForReadLocked<%p>\n", this, _name.c_str(), (void*) rval );
   return rval;
}
void DrawQueueContext::releaseFromReadLocked(const DrawQueue* db){
   //printf( "dbufctx<%p:%s> releaseFromReadLocked<%p>\n", this, _name.c_str(), (void*) db );
   _triple->end_pull(db);;
}
#else // MUTEX/GATE


DrawQueue* DrawQueueContext::acquireForWriteLocked(){
  int gate = DrawQueue::_gate.load();
   return _lockeddrawablebuffer.get();;
}
void DrawQueueContext::releaseFromWriteLocked(DrawQueue* db){
  if(db){
    _rendersync_sema.notify();
  }
}
const DrawQueue* DrawQueueContext::acquireForReadLocked(){

  int gate = DrawQueue::_gate.load();
  return _lockeddrawablebuffer.get();
}
void DrawQueueContext::releaseFromReadLocked(const DrawQueue* db){
  if(db){
    _rendersync_sema2.notify();
  }
}
#endif

/////////////////////////////////////////////////////////////////////
// flush all renderer side data
//  sync until flushed
/////////////////////////////////////////////////////////////////////
void DrawQueue::BeginClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  bool b = _gInsideClearAndSync.exchange(true);
  OrkAssert(b == false);
  _gate.store(0);
}
/////////////////////////////////////////////////////////////////////
void DrawQueue::EndClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  bool b = _gInsideClearAndSync.exchange(false);
  OrkAssert(b == true);
  _gate.store(1);
}
/////////////////////////////////////////////////////////////////////
void DrawQueue::BeginClearAndSyncWriters() {
  _gate.store(0);
}
/////////////////////////////////////////////////////////////////////
void DrawQueue::EndClearAndSyncWriters() {
  _gate.store(1);
}
/////////////////////////////////////////////////////////////////////
void DrawQueue::ClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  BeginClearAndSyncReaders();
  EndClearAndSyncReaders();
}
/////////////////////////////////////////////////////////////////////
void DrawQueue::ClearAndSyncWriters() {
  BeginClearAndSyncWriters();
  EndClearAndSyncWriters();
}
////////////////////////////////////////////////////////////////
// call at app end
////////////////////////////////////////////////////////////////
void DrawQueue::terminateAll() {
  BeginClearAndSyncWriters();
  //GetGBUFFER().rawAccess(0)->terminate();
  //GetGBUFFER().rawAccess(1)->terminate();
  //GetGBUFFER().rawAccess(2)->terminate();
}
////////////////////////////////////////////////////////////////
void DrawQueue::setUserProperty(CrcString key, rendervar_t val) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.AddSorted(key, val);
  else
    it->second = val;
}
////////////////////////////////////////////////////////////////
void DrawQueue::unSetUserProperty(CrcString key) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.erase(it);
}
////////////////////////////////////////////////////////////////
rendervar_t DrawQueue::getUserProperty(CrcString key) const {
  auto it = _userProperties.find(key);
  if (it != _userProperties.end()) {
    return it->second;
  }
  rendervar_t rval(nullptr);
  return rval;
}
////////////////////////////////////////////////////////////////
AcquiredDrawQueueForRendering::AcquiredDrawQueueForRendering(rcfd_ptr_t rcfd) {
  _RCFD = rcfd;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
