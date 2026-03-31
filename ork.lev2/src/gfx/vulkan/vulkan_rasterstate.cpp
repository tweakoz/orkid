////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_ub_layout.inl"
#include <ork/lev2/gfx/shadman.h>
#include <ork/util/hexdump.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

LockedResource<VkRasterState::rsmap_t> VkRasterState::_global_rasterstate_map;

VkRasterState::VkRasterState(rasterstate_ptr_t rstate, int attachment_count, const std::vector<VkFormat>* formats){
  _attachment_count = attachment_count;

  // Store formats for cache invalidation comparison
  if (formats) {
    _vkformats = *formats;
  }

  initializeVkStruct(_VKRSCI, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  initializeVkStruct(_VKDSSCI, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  initializeVkStruct(_VKCBSI, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);

  boost::Crc64 hasher;
  hasher.init();

  _VKRSCI.depthClampEnable = VK_FALSE;
  _VKRSCI.rasterizerDiscardEnable = VK_FALSE;
  _VKRSCI.polygonMode = VK_POLYGON_MODE_FILL;
  _VKRSCI.lineWidth = 1.0f;
  _VKRSCI.depthBiasEnable = VK_FALSE;
  _VKRSCI.depthBiasConstantFactor = 0.0f; // Optional
  _VKRSCI.depthBiasClamp = 0.0f;          // Optional
  _VKRSCI.depthBiasSlopeFactor = 0.0f;    // Optional
  hasher.accumulateItem(rstate->_depthtest);
  switch( rstate->_depthtest ){
    case EDepthTest::OFF: {
      _VKDSSCI.depthTestEnable = VK_FALSE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
    }
    case EDepthTest::LESS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_LESS;
      break;
    }
    case EDepthTest::LEQUALS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
      break;
    }
    case EDepthTest::GREATER: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_GREATER;
      break;
    }
    case EDepthTest::GEQUALS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
      break;
    }
    case EDepthTest::EQUALS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_EQUAL;
      break;
    }
    case EDepthTest::ALWAYS: {
      _VKDSSCI.depthTestEnable = VK_TRUE;
      _VKDSSCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
      break;
    }
  }
  hasher.accumulateItem(rstate->_culltest);
  switch( rstate->_culltest ){
    case ECullTest::OFF: {
      _VKRSCI.cullMode = VK_CULL_MODE_NONE;
      break;
    }
    case ECullTest::PASS_FRONT: {
      _VKRSCI.cullMode = VK_CULL_MODE_BACK_BIT;
      break;
    }
    case ECullTest::PASS_BACK: {
      _VKRSCI.cullMode = VK_CULL_MODE_FRONT_BIT;
      break;
    }
  }
  hasher.accumulateItem(rstate->_frontface);
  switch(rstate->_frontface){
    case EFrontFace::CLOCKWISE: {
      _VKRSCI.frontFace = FLIP_Y_LIKE_OPENGL ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
      break;
    }
    case EFrontFace::COUNTER_CLOCKWISE: {
      _VKRSCI.frontFace = FLIP_Y_LIKE_OPENGL ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
      break;
    }
  }

  _VKDSSCI.depthWriteEnable = rstate->_writemaskZ ? VK_TRUE : VK_FALSE;
  _VKDSSCI.depthBoundsTestEnable = VK_FALSE;
  _VKDSSCI.minDepthBounds = 0.0f; // Optional
  _VKDSSCI.maxDepthBounds = 1.0f; // Optional
  _VKDSSCI.stencilTestEnable = VK_FALSE;
  _VKDSSCI.front = {}; // Optional
  _VKDSSCI.back = {};  // Optional

  hasher.accumulateItem(rstate->_writemaskZ);


  _VKCBATT.colorWriteMask = 0;
  if(rstate->_writemaskRGB) {
    _VKCBATT.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
  }
  if(rstate->_writemaskA) {
    _VKCBATT.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
  }
  
  hasher.accumulateItem(rstate->_writemaskRGB);
  hasher.accumulateItem(rstate->_writemaskA);

  _VKCBATT.blendEnable = rstate->_blendEnable ? VK_TRUE : VK_FALSE;

  hasher.accumulateItem(rstate->_blendEnable);

  auto do_blend_factor = [&](BlendingFactor bf) -> VkBlendFactor {

    hasher.accumulateItem(bf);

    VkBlendFactor rval;
    switch(bf){
      case BlendingFactor::ZERO:
        rval = VK_BLEND_FACTOR_ZERO;
        break;
      case BlendingFactor::ONE:
        rval = VK_BLEND_FACTOR_ONE;
        break;
      case BlendingFactor::SRC_COLOR:
        rval = VK_BLEND_FACTOR_SRC_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_SRC_COLOR:
        rval = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        break;
      case BlendingFactor::DST_COLOR:
        rval = VK_BLEND_FACTOR_DST_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_DST_COLOR:
        rval = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        break;
      case BlendingFactor::SRC_ALPHA:
        rval = VK_BLEND_FACTOR_SRC_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_SRC_ALPHA:
        rval = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
      case BlendingFactor::DST_ALPHA:
        rval = VK_BLEND_FACTOR_DST_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_DST_ALPHA:
        rval = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        break;
      case BlendingFactor::CONSTANT_COLOR:
        rval = VK_BLEND_FACTOR_CONSTANT_COLOR;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_COLOR:
        rval = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        break;
      case BlendingFactor::CONSTANT_ALPHA:
        rval = VK_BLEND_FACTOR_CONSTANT_ALPHA;
        break;
      case BlendingFactor::ONE_MINUS_CONSTANT_ALPHA:
        rval = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        break;
      case BlendingFactor::SRC_ALPHA_SATURATE:
        rval = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        break;
      default:
        OrkAssert(false);
        break;
    }
    return rval;
  };
  auto do_blend_op = [&](BlendingOp bo) -> VkBlendOp {
    hasher.accumulateItem(bo);
    VkBlendOp rval;
    switch(bo){
      case BlendingOp::ADD:
        rval = VK_BLEND_OP_ADD;
        break;
      case BlendingOp::SUBTRACT:
        rval = VK_BLEND_OP_SUBTRACT;
        break;
      case BlendingOp::REVERSE_SUBTRACT:
        rval = VK_BLEND_OP_REVERSE_SUBTRACT;
        break;
      case BlendingOp::MIN:
        rval = VK_BLEND_OP_MIN;
        break;
      case BlendingOp::MAX:
        rval = VK_BLEND_OP_MAX;
        break;
      default:
        OrkAssert(false);
       break;
    }
    return rval;
  };

  _VKCBATT.srcColorBlendFactor = do_blend_factor(rstate->_blendFactorSrcRGB);
  _VKCBATT.dstColorBlendFactor = do_blend_factor(rstate->_blendFactorDstRGB);
  _VKCBATT.colorBlendOp = do_blend_op(rstate->_blendOpRGB);
  _VKCBATT.srcAlphaBlendFactor = do_blend_factor(rstate->_blendFactorSrcA);
  _VKCBATT.dstAlphaBlendFactor = do_blend_factor(rstate->_blendFactorDstA);
  _VKCBATT.alphaBlendOp = do_blend_op(rstate->_blendOpA);

  _VKCBSI.logicOpEnable = VK_FALSE;
  _VKCBSI.logicOp = VK_LOGIC_OP_COPY; // Optional
  
  // Setup blend attachments for MRT
  _VKCBATT_array.resize(_attachment_count);
  for(int i = 0; i < _attachment_count; i++) {
    _VKCBATT_array[i] = _VKCBATT; // Copy base blend state to all attachments
    
    // Check if format is integer and disable blending if so
    if (formats && i < formats->size()) {
      VkFormat fmt = (*formats)[i];
      // Check for integer formats (UI = unsigned int, SI = signed int)
      bool is_integer_format = false;
      switch(fmt) {
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
          is_integer_format = true;
          break;
        default:
          is_integer_format = false;
          break;
      }
      
      if (is_integer_format) {
        // Disable blending for integer formats
        _VKCBATT_array[i].blendEnable = VK_FALSE;
      }
      
      hasher.accumulateItem(fmt); // Include format in hash
    }
  }
  
  _VKCBSI.attachmentCount = _attachment_count;
  _VKCBSI.pAttachments = (_attachment_count>0) ? _VKCBATT_array.data() : nullptr;
  _VKCBSI.blendConstants[0] = 0.0f;
  _VKCBSI.blendConstants[1] = 0.0f;
  _VKCBSI.blendConstants[2] = 0.0f;
  _VKCBSI.blendConstants[3] = 0.0f;
  // Blend constants are now dynamic state - not included in hash
  // hasher.accumulateItem(rstate->_blendConstant);
  hasher.accumulateItem(_attachment_count); // Include attachment count in hash

  ////////////////////////////////////
  // we dont have enough bits in _pipeline_bits
  // to represent all rasterstate combos
  // so we use a global map to assign
  // unique indices to unique rasterstates
  ////////////////////////////////////

  hasher.finish();
  uint64_t hashed = hasher.result();
  _rasterstate_hash = hashed;

  auto op = [&](VkRasterState::rsmap_t& unlocked){

    auto it = unlocked.find(hashed);
    if( it == unlocked.end() ){
      _pipeline_bits = unlocked.size();
      if(0)printf( "VkRasterState<%p:%s> hashed<%016llx> NEW<%d>\n", //
              (void*) this,                                     //
              rstate->_name.c_str(),                            //
              hashed,                                           //
              _pipeline_bits );
      unlocked[hashed] = _pipeline_bits;
      OrkAssert(_pipeline_bits<256);
      OrkAssert(_pipeline_bits>=0);
    }
    else{
      _pipeline_bits = it->second;
      if(0)printf( "VkRasterState<%p:%s> hashed<%016llx> PRV<%d>\n", //
              (void*) this,                                     //
              rstate->_name.c_str(),                            //
              hashed,                                           //
              _pipeline_bits );
      OrkAssert(_pipeline_bits<256);
      OrkAssert(_pipeline_bits>=0);
    }
  };
  _global_rasterstate_map.atomicOp(op);

} // VkRasterState::VkRasterState()

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
