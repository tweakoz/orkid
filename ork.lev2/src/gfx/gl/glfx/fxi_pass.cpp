////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>

namespace ork::lev2::orksl {

///////////////////////////////////////////////////////////////////////////////

bool Pass::hasUniformInstance(UniformInstance* puni) const {
  Uniform* pun = puni->mpUniform;
  auto it      = _uniformInstances.find(pun->_name);
  return it != _uniformInstances.end();
}

///////////////////////////////////////////////////////////////////////////////

const UniformInstance* Pass::uniformInstance(Uniform* puni) const {
  auto it = _uniformInstances.find(puni->_name);
  return (it != _uniformInstances.end()) ? it->second : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

UniformBlockBinding* Pass::uniformBlockBinding(UniformBlock* block) {

  auto it = _uboBindingMap.find(block);
  if (it != _uboBindingMap.end()) {
    return it->second;
  }

  auto rval = new UniformBlockBinding;

  rval->_blockIndex = glGetUniformBlockIndex(_programObjectId, block->_name.c_str());
  rval->_pass       = this;
  rval->_block      = block;

  if (rval->_blockIndex == GL_INVALID_INDEX) {
    printf("block<%s> blockindex<0x%08x>\n", block->_name.c_str(), rval->_blockIndex);
    printf("perhaps the UBO is not referenced...\n");
    return rval;
  }

  glUniformBlockBinding(_programObjectId, rval->_blockIndex, 0);

  glGetActiveUniformBlockiv(_programObjectId, rval->_blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &rval->_blockSize);
  // printf("block<%s> blocksize<%d>\n", block->_name.c_str(), rval->_blockSize);

  GLint numunis = 0;
  glGetActiveUniformBlockiv(_programObjectId, rval->_blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numunis);
  // printf("block<%s> numunis<%d>\n", block->_name.c_str(), numunis);

  auto uniindices = new GLuint[numunis];
  glGetActiveUniformBlockiv(_programObjectId, rval->_blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, (GLint*)uniindices);

  // auto uniblkidcs = new GLint[numunis];
  // glGetActiveUniformBlockiv(_programObjectId,rval._blockIndex,GL_UNIFORM_BLOCK_INDEX,uniblkidcs);

  auto unioffsets = new GLint[numunis];
  glGetActiveUniformsiv(_programObjectId, numunis, uniindices, GL_UNIFORM_OFFSET, unioffsets);

  auto unitypes = new GLint[numunis];
  glGetActiveUniformsiv(_programObjectId, numunis, uniindices, GL_UNIFORM_TYPE, unitypes);

  auto unisizes = new GLint[numunis];
  glGetActiveUniformsiv(_programObjectId, numunis, uniindices, GL_UNIFORM_SIZE, unisizes);

  auto uniarystrides = new GLint[numunis];
  glGetActiveUniformsiv(_programObjectId, numunis, uniindices, GL_UNIFORM_ARRAY_STRIDE, uniarystrides);

  auto unimtxstrides = new GLint[numunis];
  glGetActiveUniformsiv(_programObjectId, numunis, uniindices, GL_UNIFORM_MATRIX_STRIDE, unimtxstrides);

  //////////////////////////////////////////////

  for (int i = 0; i < numunis; i++) {
    UniformBlockBinding::Item item;
    item._actuniindex = uniindices[i];
    // item._blockindex = uniblkidcs[i];
    item._offset       = unioffsets[i];
    item._type         = unitypes[i];
    item._size         = unisizes[i];
    item._arraystride  = uniarystrides[i];
    item._matrixstride = unimtxstrides[i];
    rval->_ubbitems.push_back(item);
    // printf("block<%s> uni<%d> actidx<%d>\n", block->_name.c_str(), i, uniindices[i]);
    // printf( "block<%s> uni<%d> blkidx<%d>\n", block->_name.c_str(), i, uniblkidcs[i] );
    // printf("block<%s> uni<%d> offset<%d>\n", block->_name.c_str(), i, unioffsets[i]);
    // printf("block<%s> uni<%d> type<%d>\n", block->_name.c_str(), i, unitypes[i]);
    // printf("block<%s> uni<%d> size<%d>\n", block->_name.c_str(), i, unisizes[i]);
    // printf("block<%s> uni<%d> arystride<%d>\n", block->_name.c_str(), i, uniarystrides[i]);
    // printf("block<%s> uni<%d> mtxstride<%d>\n", block->_name.c_str(), i, unimtxstrides[i]);
  }

  //////////////////////////////////////////////

  delete[] unimtxstrides;
  delete[] uniarystrides;
  delete[] unisizes;
  delete[] unitypes;
  delete[] unioffsets;
  // delete[] uniblkidcs;
  delete[] uniindices;

  _uboBindingMap[block] = rval;

  return rval;
}

void Pass::bindUniformBlockBuffer(UniformBlock* block, UniformBuffer* buffer) {

  auto binding = uniformBlockBinding(block);
  assert(binding != nullptr);
  assert(binding->_blockIndex != GL_INVALID_INDEX);
  GLuint ubo_bindingindex = binding->_blockIndex;

  if (_ubobindings.size() < (ubo_bindingindex + 1)) {
    _ubobindings.resize(ubo_bindingindex + 1);
    // printf("RESIZEUBOB<%d>\n", ubo_bindingindex + 1);
  }

  if (_ubobindings[ubo_bindingindex] != buffer) {
    GLintptr ubo_offset = 0;
    GLintptr ubo_size   = buffer->_length;
    GL_ERRORCHECK();
    // glBindBufferRange(GL_UNIFORM_BUFFER,   // target
    //                ubo_bindingindex,    // index
    //              buffer->_bufid, // buffer objid
    //            ubo_offset,          // offset
    //          ubo_size);           // length
    glBindBufferBase(
        GL_UNIFORM_BUFFER, // target
        ubo_bindingindex,  // index
        buffer->_bufid); // buffer objid
    printf("glBindBufferRange bidx<%d> bufid<%d>\n", int(ubo_bindingindex), int(buffer->_bufid));
    GL_ERRORCHECK();
    _ubobindings[ubo_bindingindex] = buffer;
  }
}

////////////////////////////////////////////////////////////////////////////////

void Pass::postProc(rootcontainer_ptr_t container) {
  auto flatunimap = container->flatUniMap();
  //////////////////////////
  // query unis
  //////////////////////////

  GLint numunis = 0;
  GL_ERRORCHECK();
  glGetProgramiv(_programObjectId, GL_ACTIVE_UNIFORMS, &numunis);
  GL_ERRORCHECK();

  _samplerCount = 0;

  for (int i = 0; i < numunis; i++) {
    GLsizei namlen = 0;
    GLint unisiz   = 0;
    GLenum unityp  = GL_ZERO;
    std::string str_name;

    {
      GLchar nambuf[256];
      glGetActiveUniform(_programObjectId, i, sizeof(nambuf), &namlen, &unisiz, &unityp, nambuf);
      OrkAssert(namlen < sizeof(nambuf));
      // printf("find uni<%s>\n", nambuf);
      GL_ERRORCHECK();

      str_name = nambuf;
      auto its = str_name.find('[');
      if (its != str_name.npos) {
        str_name = str_name.substr(0, its);
        // printf( "nnam<%s>\n", str_name.c_str() );
      }
    }
    auto it = container->_uniforms.find(str_name);
    if (it != container->_uniforms.end()) {

      Uniform* puni = it->second;

      puni->_apitype.Set<GLenum>(unityp);

      switch(unityp){
        case GL_FLOAT:
          puni->_type = orksl::EUniformType::Float;
          break;
        case GL_FLOAT_VEC2:
          puni->_type = orksl::EUniformType::Vec2f;
          break;
        case GL_FLOAT_VEC3:
          puni->_type = orksl::EUniformType::Vec3f;
          break;
        case GL_FLOAT_VEC4:
          puni->_type = orksl::EUniformType::Vec4f;
          break;
        case GL_FLOAT_MAT3:
          puni->_type = orksl::EUniformType::Mat33f;
          break;
        case GL_FLOAT_MAT4:
          puni->_type = orksl::EUniformType::Mat44f;
          break;
        case GL_INT:
          puni->_type = orksl::EUniformType::Int;
          break;
        case GL_INT_VEC2:
          puni->_type = orksl::EUniformType::Vec2i;
          break;
        case GL_INT_VEC3:
          puni->_type = orksl::EUniformType::Vec3i;
          break;
        case GL_INT_VEC4:
          puni->_type = orksl::EUniformType::Vec4i;
          break;
        case GL_UNSIGNED_INT:
          puni->_type = orksl::EUniformType::UnsignedInt;
          break;
        case GL_UNSIGNED_INT_VEC2:
          puni->_type = orksl::EUniformType::Vec2u;
          break;
        case GL_UNSIGNED_INT_VEC3:
          puni->_type = orksl::EUniformType::Vec3u;
          break;
        case GL_UNSIGNED_INT_VEC4:
          puni->_type = orksl::EUniformType::Vec4u;
          break;
        case GL_SAMPLER_1D:
          puni->_type = orksl::EUniformType::Sampler1D;
          break;          
        case GL_SAMPLER_2D:
          puni->_type = orksl::EUniformType::Sampler2D;
          break;          
        case GL_SAMPLER_3D:
          puni->_type = orksl::EUniformType::Sampler3D;
          break;          
        case GL_SAMPLER_CUBE:
          puni->_type = orksl::EUniformType::SamplerCube;
          break;          
        case GL_SAMPLER_1D_SHADOW:
          puni->_type = orksl::EUniformType::SamplerShadow1D;
          break;          
        case GL_SAMPLER_2D_SHADOW:
          puni->_type = orksl::EUniformType::SamplerShadow2D;
          break;          
        case GL_UNSIGNED_INT_SAMPLER_1D:
          puni->_type = orksl::EUniformType::Sampler1Du;
          break;          
        case GL_UNSIGNED_INT_SAMPLER_2D:
          puni->_type = orksl::EUniformType::Sampler2Du;
          break;          
        case GL_UNSIGNED_INT_SAMPLER_3D:
          puni->_type = orksl::EUniformType::Sampler3Du;
          break;          
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
          puni->_type = orksl::EUniformType::SamplerCubeu;
          break;          
        default:
          printf( "Unknown uniform datatype<%08x>\n", unityp);
          OrkAssert(false);
      }

      UniformInstance* pinst = new UniformInstance;
      pinst->mpUniform       = puni;

      GLint uniloc     = glGetUniformLocation(_programObjectId, str_name.c_str());
      pinst->_location = uniloc;

      if (puni->_typeName == "sampler2D") {
        pinst->mSubItemIndex = this->_samplerCount++;
        pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
      } else if (puni->_typeName == "usampler2D") {
        pinst->mSubItemIndex = this->_samplerCount++;
        pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
      } else if (puni->_typeName == "sampler3D") {
        pinst->mSubItemIndex = this->_samplerCount++;
        pinst->mPrivData.Set<GLenum>(GL_TEXTURE_3D);
      } else if (puni->_typeName == "usampler3D") {
        pinst->mSubItemIndex = this->_samplerCount++;
        pinst->mPrivData.Set<GLenum>(GL_TEXTURE_3D);
      } else if (puni->_typeName == "sampler2DShadow") {
        pinst->mSubItemIndex = this->_samplerCount++;
        pinst->mPrivData.Set<GLenum>(GL_TEXTURE_2D);
      }

      this->_uniformInstances[puni->_name] = pinst;
    } else {
      it = flatunimap.find(str_name);
      assert(it != flatunimap.end());
      printf("uni<%s> not found!\n", str_name.c_str());
      // prob a UBO uni
    }
  }
}

} // namespace ork::lev2::glslfx
