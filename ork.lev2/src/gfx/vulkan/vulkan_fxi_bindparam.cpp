////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamBool(const FxShaderParam* hpar, const bool bval) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamInt(const FxShaderParam* hpar, const int ival) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
    auto& param_set      = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param  = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fvec4>(Vec);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamFloat(const FxShaderParam* hpar, float fA) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ) {
    auto& param_set      = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param  = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<float>(fA);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
  if(nullptr==hpar) return;
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ) {
    auto& param_set      = _currentVKPASS->_vk_program->_pending_params.emplace_back();
    param_set._vk_param  = as_uniset_item.value();
    param_set._ork_param = param_set._vk_param->_orkparam.get();
    param_set._value.set<fmtx4>(Mat);
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamU32(const FxShaderParam* hpar, uint32_t uval) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamU64(const FxShaderParam* hpar, uint64_t uval) {
  if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindUniformBuffer(const FxUniformBlock* block, FxUniformBuffer* buffer) {
  /*if( auto as_uniset_item = hpar->_impl.tryAs<VkFxShaderUniformSetItem*>() ){
  }*/
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) {
  auto vk_shprog = _currentVKPASS->_vk_program;
  //printf( "vk_shprog<%p> bindparam ptex<%p> to param<%p>\n", (void*) vk_shprog.get(), (void*) pTex, (void*) hpar );
  if(pTex){
    vk_shprog->bindDescriptorTexture(hpar, pTex);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
