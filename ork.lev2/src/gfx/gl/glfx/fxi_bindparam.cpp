////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>

namespace ork::lev2::glslfx {

///////////////////////////////////////////////////////////////////////////////

void Interface::_stdbindparam(const FxShaderParam* hpar, const stdparambinder_t& binder) {
  auto container = _activeShader->_internalHandle.get<rootcontainer_ptr_t>();
  OrkAssert(hpar != nullptr);
  auto puni  = hpar->_impl.get<Uniform*>();
  assert(container->_activePass != nullptr);
  const UniformInstance* pinst = container->_activePass->uniformInstance(puni);
  if (pinst) {
    puni = pinst->mpUniform;
    int iloc = pinst->_locations[0];
    if (iloc >= 0) {
      const char* psem = puni->_semantic.c_str();
      const char* pnam = puni->_name.c_str();
      GLenum etyp      = puni->_type;
      if(etyp==0){
        auto sh = _activeShader;
        auto shname = sh->mName.c_str();
        auto pass = container->_activePass;
        auto tek = container->mActiveTechnique;
        auto tekname = tek->_name.c_str();
        printf("shader<%s> tek<%s> uni<%p> unistate<%d> bindParam<%s> loc<%d> sem<%s> type<%d>\n", shname, tekname, puni, puni->_state, pnam, iloc, psem, etyp);
        OrkAssert(false);
      }
      binder(iloc, etyp);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamBool(const FxShaderParam* hpar, const bool bv) {
    _stdbindparam(hpar, [&](int iloc, GLenum checktype)    {
        OrkAssert(checktype == GL_BOOL);
        GL_ERRORCHECK();
        glUniform1i(iloc, static_cast<GLint>(bv));
        GL_ERRORCHECK();
    });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamInt(const FxShaderParam* hpar, const int iv) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_INT);
    GL_ERRORCHECK();
    glUniform1i(iloc, iv);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC2);
    GL_ERRORCHECK();
    glUniform2fv(iloc, 1, Vec.asArray());
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC3);
    GL_ERRORCHECK();
    glUniform3fv(iloc, 1, Vec.asArray());
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC4);
    GL_ERRORCHECK();
    glUniform4fv(iloc, 1, Vec.asArray());
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC2);
    GL_ERRORCHECK();
    glUniform2fv(iloc, icount, (float*)Vec);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC3);
    GL_ERRORCHECK();
    glUniform3fv(iloc, icount, (float*)Vec);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC4);
    GL_ERRORCHECK();
    glUniform4fv(iloc, icount, (float*)Vec);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamFloat(const FxShaderParam* hpar, float fA) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT);
    GL_ERRORCHECK();
    glUniform1f(iloc, fA);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamFloatArray(const FxShaderParam* hpar, const float* pfa, const int icount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT);
    GL_ERRORCHECK();
    glUniform1fv(iloc, icount, pfa);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamU32(const FxShaderParam* hpar, uint32_t uval) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_UNSIGNED_INT);
    GL_ERRORCHECK();
    glUniform1ui(iloc, uval);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamU64(const FxShaderParam* hpar, uint64_t uval) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_UNSIGNED_INT_VEC4);
    GL_ERRORCHECK();
    uint32_t split[4];
    split[0] = uval & 0xffff;
    split[1] = (uval >> 16) & 0xffff;
    split[2] = (uval >> 32) & 0xffff;
    split[3] = (uval >> 48) & 0xffff;
    glUniform4uiv(iloc, 1, split);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_MAT4);
    GL_ERRORCHECK();
    glUniformMatrix4fv(iloc, 1, GL_FALSE, Mat.asArray());
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_MAT3);
    GL_ERRORCHECK();
    glUniformMatrix3fv(iloc, 1, GL_FALSE, Mat.asArray());
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* Mat, int iCount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_MAT4);
    GL_ERRORCHECK();
    glUniformMatrix4fv(iloc, iCount, GL_FALSE, (const float*)Mat);
    GL_ERRORCHECK();
  });
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamTextureArray(const FxShaderParam* hpar, const TextureArray* tex_array) {
  OrkAssert(tex_array);

  //////////////////////////////
  // update dirty slices
  //////////////////////////////

  if(tex_array->_dirty_slices.size() > 0) {
    auto GLTXI    = (GlTextureInterface*)mTarget.TXI();
    auto mutable_texa = const_cast<TextureArray*>(tex_array);
    GLTXI->initTextureArray2D(mutable_texa);
    for (int islice : tex_array->_dirty_slices) {
      auto slice_ref = tex_array->slice(islice);
      auto it = tex_array->_images.find(islice);
      if(it == tex_array->_images.end()) {
        printf("ERROR TEXARRAY: image not found for dirty slice<%d>\n", islice);
      }
      else{
        auto img       = it->second;
        OrkAssert(img != nullptr);
        printf("TEXARRAY: update slice<%d>\n", islice);
        GLTXI->updateTextureArraySlice(slice_ref.get(), img);  
      }
    }

    tex_array->_dirty_slices.clear();
  }

  //////////////////////////////

  bindParamTexture(hpar, tex_array->_tex.get());
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamTexture(const FxShaderParam* hpar, const Texture* pTex) {
  auto container               = _activeShader->_internalHandle.get<rootcontainer_ptr_t>();
  auto puni  = hpar->_impl.get<Uniform*>();
  const UniformInstance* pinst = container->_activePass->uniformInstance(puni);
  //printf("Bind1 Tex<%p> puni<%p> par<%s> pinst<%p>\n", pTex, puni, hpar->_name.c_str(), pinst);
  if (pinst) {

    auto pass = container->_activePass;

    int uniloc = pinst->_locations[0];

    if(false and pTex){
      const char* texnam = pTex->_debugName.c_str();
      const char* teknam = container->mActiveTechnique->_name.c_str();
      const char* parname = hpar->_name.c_str();
      printf("Bind2 Tex<%p:%s> par<%s> uniloc<%d> teknam<%s>\n", pTex, texnam, parname, uniloc, teknam);
    }
    // if (uniloc >= 0) {
    // const char* psem = puni->_semantic.c_str();
    // const char* pnam = puni->_name.c_str();
    // GLenum etyp      = puni->_type;
    // OrkAssert( etyp == GL_FLOAT_MAT4 );

    if (pTex != 0) {

      auto GLTXI    = (GlTextureInterface*)mTarget.TXI();
      GLenum textgt = pinst->mPrivData.get<GLenum>();

      int itexunit  = pass->assignSampler(uniloc);
      GLTXI->bindTextureToUnit(pTex, uniloc, textgt, itexunit);

      glUniform1i(uniloc, itexunit);
      //printf( "ASSSAMP LOC<%d> UNIT<%d>\n", uniloc, itexunit );
      GL_ERRORCHECK();
    }
    //}
  }
}

///////////////////////////////////////////////////////////////////////////////

void Interface::bindParamTextureList(const FxShaderParam* hpar, texture_rawlist_t texlist) {
  auto container               = _activeShader->_internalHandle.get<rootcontainer_ptr_t>();
  auto puni  = hpar->_impl.get<Uniform*>();
  const UniformInstance* pinst = container->_activePass->uniformInstance(puni);

  if (pinst and not texlist.empty()) {
    OrkAssert(pinst->_is_array); // only array uniforms are supported (for now
    auto GLTXI    = (GlTextureInterface*)mTarget.TXI();
    auto pass     = container->_activePass;
    GLenum textgt = pinst->mPrivData.get<GLenum>();
    int uniloc0   = pinst->_locations[0];
    std::vector<GLint> texunits(texlist.size());
    for (int i = 0; i < texlist.size(); ++i) {
      const Texture* pTex = texlist[i];
      if (pTex) {
        int uniloc   = pinst->_locations[i];
        int itexunit = pass->assignSampler(uniloc);
        texunits[i]  = itexunit;
        printf("aryidx<%d> loc<%d> unit<%d> ", i, uniloc, itexunit);
        GLTXI->bindTextureToUnit(pTex, uniloc, textgt, itexunit);
        glUniform1i(uniloc, itexunit);
        GL_ERRORCHECK();
      }
      // printf( "\n" );
    }
    // GLfloat values[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    // glUniform1iv(uniloc0, texunits.size(), texunits.data());
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
