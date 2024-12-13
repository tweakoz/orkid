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

  size_t GPU_SIZE = InstancedModelDrawable::k_max_instances;
  OrkAssert(count<=GPU_SIZE);

  _worldmatrices.resize(GPU_SIZE);
  _miscdata.resize(GPU_SIZE);
  _pickids.resize(GPU_SIZE);
  _modcolors.resize(GPU_SIZE);
  _count = count;
  _instancePool.clear();
  for (size_t i = 0; i < GPU_SIZE; i++) {
    _pickids[i]   = i;
    _modcolors[i] = fvec4(1, 1, 1, 1);
    _worldmatrices[i].setColumn(0,fvec4(0,0,0,0));
    _worldmatrices[i].setColumn(1,fvec4(0,0,0,0));
    _worldmatrices[i].setColumn(2,fvec4(0,0,0,0));
    _worldmatrices[i].setColumn(3,fvec4(0,0,0,1));
  }
  for (size_t i = 0; i < count; i++) {
    _instancePool.insert(i);
  }
}

///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::copyFrom(const InstancedDrawableInstanceData& oth){

  if(oth._count!=_count)
    resize(oth._count);

  std::atomic<int> ctra = 0;
  std::atomic<int> ctrb = 0;
  std::atomic<int> ctrc = 0;
  std::atomic<int> ctrd = 0;

  memcpy_async(_worldmatrices.data(), oth._worldmatrices.data(), _count*sizeof(fmtx4), ctra);
  memcpy_async(_modcolors.data(), oth._modcolors.data(), _count*sizeof(fvec4), ctrb );
  if(_uses_picking){
    memcpy_async(_pickids.data(), oth._pickids.data(), _count*sizeof(uint64_t),ctrc);

  }
  if(_uses_miscdata){
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
  OrkAssert(_instancePool.size()>0);
  auto it = _instancePool.begin();
  int ID = *it;
  _instancePool.erase(ID);
  _uses_alloc_free = true;
  return ID;
}

///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::freeInstance(int instance_id) {
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
  OrkAssert(count <= k_max_instances);
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
