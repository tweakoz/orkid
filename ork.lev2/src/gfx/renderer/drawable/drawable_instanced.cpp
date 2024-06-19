////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/fx_pipeline.h>

#include <ork/reflect/properties/registerX.inl>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::resize(size_t count) {

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

int InstancedDrawableInstanceData::allocInstance() {
  OrkAssert(_instancePool.size()>0);
  auto it = _instancePool.begin();
  int ID = *it;
  _instancePool.erase(ID);
  return ID;
}

///////////////////////////////////////////////////////////////////////////////

void InstancedDrawableInstanceData::freeInstance(int instance_id) {
  _instancePool.insert(instance_id);
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
drawablebufitem_ptr_t InstancedDrawable::enqueueOnLayer(
    const DrawQueueXfData& xfdata, //
    DrawableBufLayer& buffer) const {
  auto instances_copy            = std::make_shared<InstancedDrawableInstanceData>();
  *instances_copy                = *_instancedata;
  drawablebufitem_ptr_t dbufitem = Drawable::enqueueOnLayer(xfdata, buffer);
  dbufitem->_usermap["rtthread_instance_data"_crcu].set<instanceddrawinstancedata_ptr_t>(instances_copy);
  // printf( "_instancedata.count<%zu>\n", _instancedata->_count );
  return dbufitem;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
