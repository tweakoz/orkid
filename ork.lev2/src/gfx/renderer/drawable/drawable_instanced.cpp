////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/fx_pipeline.h>

#include <ork/reflect/properties/registerX.inl>

#include "drawable_instanced_impl.inl"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

InstancedDrawableInstanceData::InstancedDrawableInstanceData(int index)
    : _index(index) {
}

///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::resize(size_t count) {

  if(count==_count)
    return;

  // Size the CPU mirror arrays to the ACTUAL count. This used to be a FIXED k_max_instances
  // allocation (4 arrays * k_max regardless of count) which both wasted memory for small instance
  // sets AND hard-capped count-sized matrices-only instancing. The GPU buffer is sized separately
  // by the drawable (combined fixed for the stock path; count*64 for matrices-only).
  _worldmatrices.resize(count);
  if (not _matrices_only) { // colors/pickids/miscdata are unused in matrices-only -> skip allocating them
    _miscdata.resize(count);
    _pickids.resize(count);
    _modcolors.resize(count);
  }
  _count = count;
  _instancePool.clear();
  for (size_t i = 0; i < count; i++) {
    _worldmatrices[i].setColumn(0,fvec4(0,0,0,0));
    _worldmatrices[i].setColumn(1,fvec4(0,0,0,0));
    _worldmatrices[i].setColumn(2,fvec4(0,0,0,0));
    _worldmatrices[i].setColumn(3,fvec4(0,0,0,1));
    _instancePool.insert(i);
    if (not _matrices_only) {
      _pickids[i]   = i;
      _modcolors[i] = fvec4(1, 1, 1, 1);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::copyFrom(const InstancedDrawableInstanceData& oth){

  _matrices_only = oth._matrices_only;  // propagate first so resize() + the per-array copies below match
  if(oth._count!=_count)
    resize(oth._count);

  std::atomic<int> ctra = 0;
  std::atomic<int> ctrb = 0;
  std::atomic<int> ctrc = 0;
  std::atomic<int> ctrd = 0;

  memcpy_async(_worldmatrices.data(), oth._worldmatrices.data(), _count*sizeof(fmtx4), ctra);
  if(not _matrices_only)
    memcpy_async(_modcolors.data(), oth._modcolors.data(), _count*sizeof(fvec4), ctrb );
  if(_uses_picking and not _matrices_only){
    memcpy_async(_pickids.data(), oth._pickids.data(), _count*sizeof(uint64_t),ctrc);

  }
  if(_uses_miscdata and not _matrices_only){
    // cant memcpy since its not a POD
    for(size_t i=0; i<_count; i++){
      _miscdata[i] = oth._miscdata[i];
    }
  }

  if(_uses_alloc_free){
    _instancePool = oth._instancePool;
  }
  while (ctra.load()) {
    ork::usleep(0);
  }
  while (ctrb.load()) {
    ork::usleep(0);
  }
  while (ctrc.load()) {
    ork::usleep(0);
  }
  while (ctrd.load()) {
    ork::usleep(0);
  }

}

///////////////////////////////////////////////////////////////////////////////

int InstancedDrawableInstanceData::allocInstance() {
  // FAIL-SOFT on pool exhaustion: gameplay-rate spawns must never abort the
  // host. -1 = no slot; callers skip the visual wiring (the entity still
  // exists; lifetime reaping recycles slots).
  if (_instancePool.empty()) {
    printf("InstancedDrawableInstanceData<%p>: instance pool EXHAUSTED (capacity %zu)\n", (void*)this, _count);
    return -1;
  }
  auto it = _instancePool.begin();
  int ID = *it;
  _instancePool.erase(ID);
  _uses_alloc_free = true;
  return ID;
}

///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::freeInstance(int instance_id) {
  // restore the IDLE-SLOT contract: zero basis = degenerate = invisible.
  // count==capacity slots are ALWAYS drawn, so a freed slot left with its
  // last pose would keep rendering a ghost at that position forever.
  if (instance_id >= 0 and instance_id < int(_count)) {
    auto& m = _worldmatrices[instance_id];
    m.setColumn(0, fvec4(0, 0, 0, 0));
    m.setColumn(1, fvec4(0, 0, 0, 0));
    m.setColumn(2, fvec4(0, 0, 0, 0));
    m.setColumn(3, fvec4(0, 0, 0, 1));
  }
  _instancePool.insert(instance_id);
  _uses_alloc_free = true;
}

///////////////////////////////////////////////////////////////////////////////
InstancedDrawable::InstancedDrawable()
    : Drawable() {
  _instancedata = std::make_shared<InstancedDrawableInstanceData>();
  _drawcount    = 0;
}
///////////////////////////////////////////////////////////////////////////////
void InstancedDrawable::resize(size_t count) {
  // matrices-only drawables use a COUNT-SIZED matrices SSBO, so k_max_instances doesn't bound them.
  OrkAssert(_matrices_only or count <= k_max_instances);
  _instancedata->_matrices_only = _matrices_only; // gate the instance-data arrays to matrices-only
  _instancedata->resize(count);
  _count = count;
}
///////////////////////////////////////////////////////////////////////////////
drawqueueitem_ptr_t InstancedDrawable::enqueueOnLayer(
    const DrawQueueTransferData& xfdata, //
    DrawQueueLayer& buffer) const {
  auto instances_copy = _idbuf_pool.begin_push();
  instances_copy->copyFrom(*_instancedata);
  drawqueueitem_ptr_t dbufitem = Drawable::enqueueOnLayer(xfdata, buffer);
  _idbuf_pool.end_push(instances_copy);
  return dbufitem;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
