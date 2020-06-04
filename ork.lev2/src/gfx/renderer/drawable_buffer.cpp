////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/math/collision_test.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/triple_buffer.h>
#include <ork/profiling.inl>

namespace ork::lev2 {

ork::MpMcBoundedQueue<RenderSyncToken> DrawableBuffer::mOfflineRenderSynchro;
ork::MpMcBoundedQueue<RenderSyncToken> DrawableBuffer::mOfflineUpdateSynchro;

ork::atomic<bool> DrawableBuffer::gbInsideClearAndSync;

////////////////////////////////////////////////////////////////

LayerData::LayerData() {
}

///////////////////////////////////////////////////////////////////////////////

RenderSyncToken DrawableBuffer::acquireRenderToken() {
  lev2::RenderSyncToken syntok;
  bool have_token = false;
  Timer totim;
  totim.Start();
  while (false == have_token && (totim.SecsSinceStart() < 2.0f)) {
    have_token = lev2::DrawableBuffer::mOfflineRenderSynchro.try_pop(syntok);
    usleep(100);
  }
  return syntok;
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

  bool DoAll = LayerName == "All";
  target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue doall<%d>", int(DoAll)));
  target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue numlayers<%zu>", mLayerLut.size()));

  for (const auto& layer_item : mLayerLut) {
    const std::string& TestLayerName     = layer_item.first;
    const lev2::DrawableBufLayer* player = layer_item.second;

    bool Match = (LayerName == TestLayerName);

    target->debugMarker(FormatString(
        "DrawableBuffer::enqueueLayerToRenderQueue TestLayerName<%s> player<%p> Match<%d>",
        TestLayerName.c_str(),
        player,
        int(Match)));

    if (DoAll || (Match && topCPD.HasLayer(TestLayerName))) {
      target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue layer itemcount<%d>", player->miItemIndex + 1));
      for (int id = 0; id <= player->miItemIndex; id++) {
        const lev2::DrawableBufItem& item = player->mDrawBufItems[id];
        const lev2::Drawable* pdrw        = item.GetDrawable();
        target->debugMarker(FormatString("DrawableBuffer::enqueueLayerToRenderQueue layer item <%d> drw<%p>", id, pdrw));
        if (pdrw) {
          pdrw->enqueueToRenderQueue(item, renderer);
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::Reset() {
  // ork::opq::assertOnQueue2( opq::updateSerialQueue() );

  miNumLayersUsed = 0;
  mLayers.clear();
  for (int il = 0; il < kmaxlayers; il++) {
    mRawLayers[il].Reset(*this);
  }
  _preRenderCallbacks.clear();
  _cameraDataLUT.clear();
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBufLayer::Reset(const DrawableBuffer& dB) {
  // ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  miBufferIndex = dB.miBufferIndex;
  miItemIndex   = -1;
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
    mLayers.insert(layername);
    mLayerLut.AddSorted(layername, player);
  }
  return player;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBufItem& DrawableBufLayer::Queue(const DrawQueueXfData& xfdata, const Drawable* d) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // mDrawBufItems.push_back(DrawableBufItem()); // replace std::vector with an array so we can amortize construction costs
  miItemIndex++;
  OrkAssert(miItemIndex < kmaxitems);
  DrawableBufItem& item = mDrawBufItems[miItemIndex];
  item.SetDrawable(d);
  item.mUserData0    = 0;
  item.mUserData1    = 0;
  item.mXfData       = xfdata;
  item.miBufferIndex = miBufferIndex;
  return item;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::DrawableBuffer(int ibidx)
    : miBufferIndex(ibidx)
    , miNumLayersUsed(0) {
}

DrawableBufLayer::DrawableBufLayer()
    : miItemIndex(-1)
    , miBufferIndex(-1) {
}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::~DrawableBuffer() {
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::copyCameras(const CameraDataLut& cameras) {
  _cameraDataLUT.clear();
  for (auto itCAM = cameras.begin(); itCAM != cameras.end(); itCAM++) {
    const std::string& CameraName       = itCAM->first;
    const lev2::CameraData* pcameradata = itCAM->second;
    if (pcameradata) {
      _cameraDataLUT.AddSorted(CameraName, pcameradata);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

const CameraData* DrawableBuffer::cameraData(int icam) const {
  int inumscenecameras = _cameraDataLUT.size();
  // printf( "NumSceneCameras<%d>\n", inumscenecameras );
  if (icam >= 0 && inumscenecameras) {
    icam                    = icam % inumscenecameras;
    auto& itCAM             = _cameraDataLUT.GetItemAtIndex(icam);
    const CameraData* pdata = itCAM.second;
    auto pcam               = pdata->getUiCamera();
    // printf( "icam<%d> pdata<%p> pcam<%p>\n", icam, pdata, pcam );
    return pdata;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

const CameraData* DrawableBuffer::cameraData(const std::string& named) const {
  int inumscenecameras = _cameraDataLUT.size();
  auto itCAM           = _cameraDataLUT.find(named);
  if (itCAM != _cameraDataLUT.end()) {
    const CameraData* pdata = itCAM->second;
    return pdata;
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////
static concurrent_triple_buffer<DrawableBuffer> gBuffers;
/////////////////////////////////////////////////////////////////////
const DrawableBuffer* DrawableBuffer::acquireForRead(int lid) {
  return gBuffers.begin_pull();
}
/////////////////////
void DrawableBuffer::releaseFromRead(const DrawableBuffer* db) {
  gBuffers.end_pull(db);
}
/////////////////////////////////////////////////////////////////////
DrawableBuffer* DrawableBuffer::acquireForWrite(int lid) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  DrawableBuffer* wbuf = gBuffers.begin_push();
  return wbuf;
}
void DrawableBuffer::releaseFromWrite(DrawableBuffer* db) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  gBuffers.end_push(db);
}
/////////////////////////////////////////////////////////////////////
// flush all renderer side data
//  sync until flushed
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  bool b = gbInsideClearAndSync.exchange(true);
  OrkAssert(b == false);
  // printf( "DrawableBuffer::BeginClearAndSyncReaders()\n");
  gBuffers.disable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSyncReaders() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  bool b = gbInsideClearAndSync.exchange(false);
  OrkAssert(b == true);
  ////////////////////
  // printf( "DrawableBuffer::EndClearAndSyncReaders()\n");
  gBuffers.enable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSyncWriters() {
  // ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  // printf( "DrawableBuffer::BeginClearAndSyncWriters()\n");
  gBuffers.disable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSyncWriters() {
  // ork::opq::assertOnQueue2( opq::updateSerialQueue() );
  ////////////////////
  // printf( "DrawableBuffer::EndClearAndSyncWriters()\n");
  gBuffers.enable();
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
} // namespace ork::lev2
