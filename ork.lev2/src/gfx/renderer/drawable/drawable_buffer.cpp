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
ork::atomic<int> DrawableBuffer::_gate = 1;

////////////////////////////////////////////////////////////////

LayerData::LayerData() {
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::setPreRenderCallback(int key, prerendercallback_t cb) {
  _preRenderCallbacks.AddSorted(key, cb);
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::invokePreRenderCallbacks(lev2::RenderContextFrameData& RCFD) const {
  EASY_BLOCK("prerender");
  for (auto item : _preRenderCallbacks)
    item.second(RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::enqueueLayerToRenderQueue(const std::string& LayerName, lev2::IRenderer* renderer) const {
  lev2::Context* target                         = renderer->GetTarget();
  const ork::lev2::RenderContextFrameData* RCFD = target->topRenderContextFrameData();
  const auto& topCPD                            = RCFD->topCPD();

  if (false == topCPD.isValid()) {
    OrkAssert(false);
    return;
  }

  int numdrawables = 0;

  bool do_all = (LayerName == "All");
  target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue do_all<%d>", int(do_all)));
  target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue numlayers<%zu>", mLayerLut.size()));

  //printf( "rendering <%s> do_all<%d>\n", LayerName.c_str(), int(do_all) );
  //////////////////////////////////////////////////////////////////////////////////////////////
  auto do_layer = [target,renderer,&numdrawables](const lev2::DrawableBufLayer* player){
      player->_items.atomicOp([player,target,renderer,&numdrawables](const DrawableBufLayer::itemvect_t& unlocked){
        int max_index = player->_itemIndex;
        for (int id = 0; id < max_index; id++) {
          auto item = unlocked[id];
          const lev2::Drawable* pdrw        = item->_drawable;
          target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue layer item <%d> drw<%p>", id, pdrw));
          if (pdrw) {
            numdrawables++;
            pdrw->enqueueToRenderQueue(item, renderer);
          }
        }
      }); // player->_items.atomicOp
  };
  //////////////////////////////////////////////////////////////////////////////////////////////
  for (const auto& layer_item : mLayerLut) {

    const std::string& TestLayerName     = layer_item.first;
    const lev2::DrawableBufLayer* player = layer_item.second;

    bool Match = (LayerName == TestLayerName);
    bool has = topCPD.HasLayer(TestLayerName);
    //printf( "against layer<%s> match<%d> has<%d>\n", TestLayerName.c_str(), int(Match), int(has) );

    target->debugMarker(FormatString(
        "DrawableBuffer::enqueueLayerToRenderQueue TestLayerName<%s> player<%p> Match<%d>",
        TestLayerName.c_str(),
        player,
        int(Match)));

    if (do_all || (Match && topCPD.HasLayer(TestLayerName))) {
      //printf( "layer<%s> count<%d>\n", TestLayerName.c_str(), player->_itemIndex );
      target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue layer itemcount<%d>", player->_itemIndex + 1));
      do_layer(player);
    }
  }

  //printf( "DB<%p> enqueued %d drawables\n", this, numdrawables);
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::Reset() {
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
void DrawableBuffer::terminate() {
  Reset();
  //for (int il = 0; il < kmaxlayers; il++) {
    //mRawLayers[il].terminate();
  //}
}
///////////////////////////////////////////////////////////////////////////////

void DrawableBufLayer::Reset(const DrawableBuffer& dB) {
  // ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  miBufferIndex = dB.miBufferIndex;
  _items.atomicOp([this](DrawableBufLayer::itemvect_t& unlocked){
    unlocked.clear();
  _itemIndex   = -1;
  });
}

///////////////////////////////////////////////////////////////////////////////

DrawableBufLayer* DrawableBuffer::MergeLayer(const std::string& layername) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  DrawableBufLayer* player     = 0;
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

drawablebufitem_ptr_t DrawableBufLayer::enqueueDrawable(const DrawQueueXfData& xfdata, const Drawable* d) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // _items.push_back(DrawableBufItem()); // replace std::vector with an array so we can amortize construction costs
  auto item = std::make_shared<DrawableBufItem>(); // todo USE POOL
  item->_drawable = d;
  item->mXfData       = xfdata;
  item->_bufferIndex = miBufferIndex;
  static std::atomic<int> counter = 0;
  item->_serialno = counter++;
  item->_sortkey = _sortkey;
  _items.atomicOp([this,item](DrawableBufLayer::itemvect_t& unlocked){
    unlocked.push_back(item);
    _itemIndex = unlocked.size();
  });
  return item;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::DrawableBuffer(int ibidx)
    : miNumLayersUsed(0) 
    , miBufferIndex(ibidx) {
    _state.store(1000);

    _cameraDataLUT.atomicOp([](cameradatalut_ptr_t& unlocked){
      unlocked = std::make_shared<CameraDataLut>();
    });
}
DrawableBuffer::~DrawableBuffer() {
  _state.store(0);
}
DrawableBufItem::DrawableBufItem()
      : _drawable(0)
      , _bufferIndex(0) {
  _state.store(1000);
  }

DrawableBufItem::~DrawableBufItem() {
    _state.store(0);
  }

///////////////////////////////////////////////////////////////////////////////

DrawableBufLayer::DrawableBufLayer()
    : _itemIndex(-1)
    , miBufferIndex(-1) {
    _state.store(107);
}
DrawableBufLayer::~DrawableBufLayer(){
    _state.store(0);
}

//void DrawableBufLayer::terminate() {
//}

void DrawableBufItem::terminate() {
  _drawable = nullptr;
}
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::copyCameras(const CameraDataLut& cameras) {
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

cameradata_constptr_t DrawableBuffer::cameraData(int index) const {
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

cameradata_constptr_t DrawableBuffer::cameraData(const std::string& named) const {
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
DrawBufContext::DrawBufContext()
  : _lockedBufferMutex("lbuf")
  ,_rendersync_sema("lsema")
  ,_rendersync_sema2("lsema2") {
    _lockeddrawablebuffer = std::make_shared<DrawableBuffer>(99);
    _rendersync_sema2.notify();
  _triple = std::make_shared<tbuf_t>();
}

DrawBufContext::~DrawBufContext(){
}

#if 1 // TRIPLE BUFFER

DrawableBuffer* DrawBufContext::acquireForWriteLocked(){
   auto rval = _triple->begin_push();
   //printf( "dbufctx<%p:%s> acquireForWriteLocked<%p>\n", this, _name.c_str(), (void*) rval );
   //_triple->dump();
   return rval;
}
void DrawBufContext::releaseFromWriteLocked(DrawableBuffer* db){
   //printf( "dbufctx<%p:%s> releaseFromWriteLocked<%p>\n", this, _name.c_str(), (void*) db );
   _triple->end_push(db);;
}
const DrawableBuffer* DrawBufContext::acquireForReadLocked(){
   auto rval = _triple->begin_pull();
   //printf( "dbufctx<%p:%s> acquireForReadLocked<%p>\n", this, _name.c_str(), (void*) rval );
   return rval;
}
void DrawBufContext::releaseFromReadLocked(const DrawableBuffer* db){
   //printf( "dbufctx<%p:%s> releaseFromReadLocked<%p>\n", this, _name.c_str(), (void*) db );
   _triple->end_pull(db);;
}
#else // MUTEX/GATE


DrawableBuffer* DrawBufContext::acquireForWriteLocked(){
  int gate = DrawableBuffer::_gate.load();
   return _lockeddrawablebuffer.get();;
}
void DrawBufContext::releaseFromWriteLocked(DrawableBuffer* db){
  if(db){
    _rendersync_sema.notify();
  }
}
const DrawableBuffer* DrawBufContext::acquireForReadLocked(){

  int gate = DrawableBuffer::_gate.load();
  return _lockeddrawablebuffer.get();
}
void DrawBufContext::releaseFromReadLocked(const DrawableBuffer* db){
  if(db){
    _rendersync_sema2.notify();
  }
}
#endif

/////////////////////////////////////////////////////////////////////
// flush all renderer side data
//  sync until flushed
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  bool b = _gInsideClearAndSync.exchange(true);
  OrkAssert(b == false);
  _gate.store(0);
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  bool b = _gInsideClearAndSync.exchange(false);
  OrkAssert(b == true);
  _gate.store(1);
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSyncWriters() {
  _gate.store(0);
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSyncWriters() {
  _gate.store(1);
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::ClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  BeginClearAndSyncReaders();
  EndClearAndSyncReaders();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::ClearAndSyncWriters() {
  BeginClearAndSyncWriters();
  EndClearAndSyncWriters();
}
////////////////////////////////////////////////////////////////
// call at app end
////////////////////////////////////////////////////////////////
void DrawableBuffer::terminateAll() {
  BeginClearAndSyncWriters();
  //GetGBUFFER().rawAccess(0)->terminate();
  //GetGBUFFER().rawAccess(1)->terminate();
  //GetGBUFFER().rawAccess(2)->terminate();
}
////////////////////////////////////////////////////////////////
void DrawableBuffer::setUserProperty(CrcString key, rendervar_t val) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.AddSorted(key, val);
  else
    it->second = val;
}
////////////////////////////////////////////////////////////////
void DrawableBuffer::unSetUserProperty(CrcString key) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.erase(it);
}
////////////////////////////////////////////////////////////////
rendervar_t DrawableBuffer::getUserProperty(CrcString key) const {
  auto it = _userProperties.find(key);
  if (it != _userProperties.end()) {
    return it->second;
  }
  rendervar_t rval(nullptr);
  return rval;
}
////////////////////////////////////////////////////////////////
AcquiredRenderDrawBuffer::AcquiredRenderDrawBuffer(rcfd_ptr_t rcfd) {
  _RCFD = rcfd;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
