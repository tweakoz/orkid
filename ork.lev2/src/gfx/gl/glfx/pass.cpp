////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>

namespace ork::lev2::glslfx {

  ///////////////////////////////////////////////////////////////////////////////

  bool Pass::hasUniformInstance(UniformInstance* puni) const {
    Uniform* pun                                               = puni->mpUniform;
    std::map<std::string, UniformInstance*>::const_iterator it = _uniformInstances.find(pun->_name);
    return it != _uniformInstances.end();
  }

  ///////////////////////////////////////////////////////////////////////////////

  const UniformInstance* Pass::uniformInstance(Uniform* puni) const {
    std::map<std::string, UniformInstance*>::const_iterator it = _uniformInstances.find(puni->_name);
    return (it != _uniformInstances.end()) ? it->second : nullptr;
  }

  ///////////////////////////////////////////////////////////////////////////////

  UniformBlockBinding Pass::uniformBlockBinding(std::string blockname){
    UniformBlockBinding rval;
    rval._blockIndex = glGetUniformBlockIndex( _programObjectId, blockname.c_str() );
    rval._pass = this;
    rval._name = blockname;

    if( rval._blockIndex == GL_INVALID_INDEX ){
      printf( "block<%s> blockindex<0x%08x>\n", blockname.c_str(), rval._blockIndex );
      printf( "perhaps the UBO is not referenced...\n");
      return rval;
    }

    glGetActiveUniformBlockiv(_programObjectId,rval._blockIndex,GL_UNIFORM_BLOCK_DATA_SIZE,&rval._blockSize);
    printf( "block<%s> blocksize<%d>\n", blockname.c_str(), rval._blockSize );

    GLint numunis = 0;
    glGetActiveUniformBlockiv(_programObjectId,rval._blockIndex,GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,&numunis);
    printf( "block<%s> numunis<%d>\n", blockname.c_str(), numunis );

    auto uniindices = new GLuint[numunis];
    glGetActiveUniformBlockiv(_programObjectId,rval._blockIndex,GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,(GLint*)uniindices);

    auto uniblkidcs = new GLint[numunis];
    glGetActiveUniformBlockiv(_programObjectId,rval._blockIndex,GL_UNIFORM_BLOCK_INDEX,uniblkidcs);

    auto unioffsets = new GLint[numunis];
    glGetActiveUniformsiv( _programObjectId, numunis, uniindices, GL_UNIFORM_OFFSET, unioffsets );

    auto unitypes = new GLint[numunis];
    glGetActiveUniformsiv( _programObjectId, numunis, uniindices, GL_UNIFORM_TYPE, unitypes );

    auto unisizes = new GLint[numunis];
    glGetActiveUniformsiv( _programObjectId, numunis, uniindices, GL_UNIFORM_SIZE, unisizes );

    auto uniarystrides = new GLint[numunis];
    glGetActiveUniformsiv( _programObjectId, numunis, uniindices, GL_UNIFORM_ARRAY_STRIDE, uniarystrides );

    auto unimtxstrides = new GLint[numunis];
    glGetActiveUniformsiv( _programObjectId, numunis, uniindices, GL_UNIFORM_MATRIX_STRIDE, unimtxstrides );

    //////////////////////////////////////////////

    for( int i=0; i<numunis; i++ ){
      UniformBlockBinding::Item item;
      item._actuniindex = uniindices[i];
      item._blockindex = uniblkidcs[i];
      item._offset = unioffsets[i];
      item._type = unitypes[i];
      item._size = unisizes[i];
      item._arraystride = uniarystrides[i];
      item._matrixstride = unimtxstrides[i];
      rval._ubbitems.push_back(item);
      printf( "block<%s> uni<%d> actidx<%d>\n", blockname.c_str(), i, uniindices[i] );
      printf( "block<%s> uni<%d> blkidx<%d>\n", blockname.c_str(), i, uniblkidcs[i] );
      printf( "block<%s> uni<%d> offset<%d>\n", blockname.c_str(), i, unioffsets[i] );
      printf( "block<%s> uni<%d> type<%d>\n", blockname.c_str(), i, unitypes[i] );
      printf( "block<%s> uni<%d> size<%d>\n", blockname.c_str(), i, unisizes[i] );
      printf( "block<%s> uni<%d> arystride<%d>\n", blockname.c_str(), i, uniarystrides[i] );
      printf( "block<%s> uni<%d> mtxstride<%d>\n", blockname.c_str(), i, unimtxstrides[i] );
    }


    //////////////////////////////////////////////

    delete[] unimtxstrides;
    delete[] uniarystrides;
    delete[] unisizes;
    delete[] unitypes;
    delete[] unioffsets;
    delete[] uniblkidcs;
    delete[] uniindices;

    _uboBindingMap[blockname] = rval;

    return rval;
  }
} // namespace ork::lev2::glslfx {
