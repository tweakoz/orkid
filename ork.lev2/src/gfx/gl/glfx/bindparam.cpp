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

namespace ork::lev2::glslfx {
///////////////////////////////////////////////////////////////////////////////
void Interface::BindParamBool(const FxShaderParam* hpar, const bool bv) {
}
///////////////////////////////////////////////////////////////////////////////
void Interface::_stdbindparam(const FxShaderParam* hpar, const stdparambinder_t& binder) {
  Container* container = static_cast<Container*>(_activeShader->GetInternalHandle());
  Uniform* puni        = static_cast<Uniform*>(hpar->GetPlatformHandle());
  assert(container->_activePass != nullptr);
  const UniformInstance* pinst = container->_activePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->_semantic.c_str();
      const char* pnam = puni->_name.c_str();
      GLenum etyp      = puni->_type;
      binder(iloc, etyp);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void Interface::BindParamInt(const FxShaderParam* hpar, const int iv) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_INT);
    GL_ERRORCHECK();
    glUniform1i(iloc, iv);
    GL_ERRORCHECK();
  });
}
void Interface::BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC2);
    GL_ERRORCHECK();
    glUniform2fv(iloc, 1, Vec.GetArray());
    GL_ERRORCHECK();
  });
}
void Interface::BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC3);
    GL_ERRORCHECK();
    glUniform3fv(iloc, 1, Vec.GetArray());
    GL_ERRORCHECK();
  });
}
void Interface::BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC4);
    GL_ERRORCHECK();
    glUniform4fv(iloc, 1, Vec.GetArray());
    GL_ERRORCHECK();
  });
}
void Interface::BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_VEC4);
    GL_ERRORCHECK();
    glUniform4fv(iloc, icount, (float*)Vec);
    GL_ERRORCHECK();
  });
}
void Interface::BindParamFloat(const FxShaderParam* hpar, float fA) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT);
    GL_ERRORCHECK();
    glUniform1f(iloc, fA);
    GL_ERRORCHECK();
  });
}
void Interface::BindParamFloatArray(const FxShaderParam* hpar, const float* pfa, const int icount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT);
    GL_ERRORCHECK();
    glUniform1fv(iloc, icount, pfa);
    GL_ERRORCHECK();
  });
}
void Interface::BindParamU32(const FxShaderParam* hpar, uint32_t uval) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_UNSIGNED_INT);
    GL_ERRORCHECK();
    glUniform1ui(iloc, uval);
    GL_ERRORCHECK();
  });
}
#if !defined(__APPLE__)
void Interface::BindParamU64(const FxShaderParam* hpar, uint64_t uval) {
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
#endif
void Interface::BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_MAT4);
    GL_ERRORCHECK();
    glUniformMatrix4fv(iloc, 1, GL_FALSE, Mat.GetArray());
    GL_ERRORCHECK();
  });
}
void Interface::BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_MAT3);
    GL_ERRORCHECK();
    glUniformMatrix3fv(iloc, 1, GL_FALSE, Mat.GetArray());
    GL_ERRORCHECK();
  });
}
void Interface::BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* Mat, int iCount) {
  _stdbindparam(hpar, [&](int iloc, GLenum checktype) {
    OrkAssert(checktype == GL_FLOAT_MAT4);
    GL_ERRORCHECK();
    glUniformMatrix4fv(iloc, iCount, GL_FALSE, (const float*)Mat);
    GL_ERRORCHECK();
  });
}
///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) {
  auto container               = static_cast<Container*>(_activeShader->GetInternalHandle());
  auto puni                    = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->_activePass->uniformInstance(puni);
  // printf("Bind1 Tex<%p> puni<%p> par<%s> pinst<%p>\n", pTex, puni, hpar->_name.c_str(), pinst);
  if (pinst) {
    int iloc = pinst->mLocation;

    const char* teknam = container->mActiveTechnique->_name.c_str();

    // printf("Bind2 Tex<%p> par<%s> iloc<%d> teknam<%s>\n", pTex, hpar->_name.c_str(), iloc, teknam);
    if (iloc >= 0) {
      const char* psem = puni->_semantic.c_str();
      const char* pnam = puni->_name.c_str();
      GLenum etyp      = puni->_type;
      // OrkAssert( etyp == GL_FLOAT_MAT4 );

      if (pTex != 0) {
        const GLTextureObject* pTEXOBJ = (GLTextureObject*)pTex->GetTexIH();
        GLuint texID                   = pTEXOBJ ? pTEXOBJ->mObject : 0;
        int itexunit                   = pinst->mSubItemIndex;

        GLenum textgt = pinst->mPrivData.Get<GLenum>();

        /*printf(
            "Bind3 ISDEPTH<%d> tex<%p> texobj<%d> itexunit<%d> textgt<% d>\n ",
            int(pTex->_isDepthTexture),
            pTex,
            texID,
            itexunit,
            int(textgt));*/

        GL_ERRORCHECK();
        glActiveTexture(GL_TEXTURE0 + itexunit);
        GL_ERRORCHECK();
        glBindTexture(textgt, texID);
        GL_ERRORCHECK();
        // glEnable( GL_TEXTURE_2D );
        // GL_ERRORCHECK();
        glUniform1i(iloc, itexunit);
        GL_ERRORCHECK();
      }
    }
  }
  /*
          if( 0 == hpar ) return;
          CgFxContainer* container = static_cast<CgFxContainer*>(
     _activeShader->GetInternalHandle() ); CGeffect cgeffect = container->mCgEffect;
          CGparameter cgparam =
     reinterpret_cast<CGparameter>(hpar->GetPlatformHandle()); if( (pTex!=0) &&
     (cgparam!=0) )
          {
                  const GLTextureObject* pTEXOBJ = (GLTextureObject*)
     pTex->GetTexIH();
                  //orkprintf( "BINDTEX param<%p:%s> pTEX<%p> pTEXOBJ<%p>
     obj<%d>\n", hpar, hpar->mParameterName.c_str(), pTex, pTEXOBJ,
     pTEXOBJ->mObject ); cgGLSetTextureParameter( cgparam, pTEXOBJ ?
     pTEXOBJ->mObject : 0 );
          }
          else
          {
                  cgGLSetTextureParameter( cgparam, 0 );
          }
          GL_ERRORCHECK();
  */
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
