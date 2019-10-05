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

void Interface::BindParamBool(FxShader* hfx, const FxShaderParam* hpar, const bool bv) {}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamInt(FxShader* hfx, const FxShaderParam* hpar, const int iv) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_INT);

      GL_ERRORCHECK();
      glUniform1i(iloc, iv);
      GL_ERRORCHECK();
    } else {
      assert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamVect2(FxShader* hfx, const FxShaderParam* hpar, const fvec2& Vec) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_VEC2);

      GL_ERRORCHECK();
      glUniform2fv(iloc, 1, Vec.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamVect3(FxShader* hfx, const FxShaderParam* hpar, const fvec3& Vec) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_VEC3);

      GL_ERRORCHECK();
      glUniform3fv(iloc, 1, Vec.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamVect4(FxShader* hfx, const FxShaderParam* hpar, const fvec4& Vec) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_VEC4);

      GL_ERRORCHECK();
      glUniform4fv(iloc, 1, Vec.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamVect4Array(FxShader* hfx, const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC4 );

          glUniform4fv( iloc, icount, (float*) Vec );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat(FxShader* hfx, const FxShaderParam* hpar, float fA) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT);

      GL_ERRORCHECK();
      glUniform1f(iloc, fA);
      GL_ERRORCHECK();
    }
  }
}
void Interface::BindParamFloatArray(FxShader* hfx, const FxShaderParam* hpar, const float* pfa, const int icount) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == GL_FLOAT );

          glUniform1fv( iloc, icount, pfa );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat2(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC2 );

          fvec2 v2( fA, fB );

          glUniform2fv( iloc, 1, v2.GetArray() );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat3(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC3 );

          fvec3 v3( fA, fB, fC );

          glUniform3fv( iloc, 1, v3.GetArray() );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamFloat4(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD) {
  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == 	GL_FLOAT_VEC4 );

          fvec4 v4( fA, fB, fC, fD );

          glUniform4fv( iloc, 1, v4.GetArray() );
          GL_ERRORCHECK();
  }*/
}

void Interface::BindParamU32(FxShader* hfx, const FxShaderParam* hpar, U32 uval) {
  /*
          CgFxContainer* container = static_cast<CgFxContainer*>(
     hfx->GetInternalHandle() ); CGeffect cgeffect = container->mCgEffect;
          CGparameter cgparam =
     reinterpret_cast<CGparameter>(hpar->GetPlatformHandle()); GL_ERRORCHECK();
  */
}

void Interface::BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx4& Mat) {
  Container* container = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni        = static_cast<Uniform*>(hpar->GetPlatformHandle());
  assert(container->mActivePass != nullptr);
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_MAT4);

      GL_ERRORCHECK();
      glUniformMatrix4fv(iloc, 1, GL_FALSE, Mat.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx3& Mat) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_MAT3);

      GL_ERRORCHECK();
      glUniformMatrix3fv(iloc, 1, GL_FALSE, Mat.GetArray());
      GL_ERRORCHECK();
    }
  }
}

void Interface::BindParamMatrixArray(FxShader* hfx, const FxShaderParam* hpar, const fmtx4* Mat, int iCount) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  if (pinst) {
    int iloc = pinst->mLocation;
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      OrkAssert(etyp == GL_FLOAT_MAT4);

      // printf( "pnam<%s>\n", pnam );
      GL_ERRORCHECK();
      glUniformMatrix4fv(iloc, iCount, GL_FALSE, (const float*)Mat);
      GL_ERRORCHECK();
    }
  }

  /*Container* container = static_cast<Container*>(
  hfx->GetInternalHandle() ); Uniform* puni = static_cast<Uniform*>(
  hpar->GetPlatformHandle() ); int iloc = puni->mLocation; if( iloc>=0 )
  {
          const char* psem = puni->mSemantic.c_str();
          const char* pnam = puni->mName.c_str();
          GLenum etyp = puni->meType;
          OrkAssert( etyp == GL_FLOAT_MAT4 );

          glUniformMatrix4fv( iloc, iCount, GL_FALSE, (float*) Mat );
          GL_ERRORCHECK();
  }*/
}

///////////////////////////////////////////////////////////////////////////////

void Interface::BindParamCTex(FxShader* hfx, const FxShaderParam* hpar, const Texture* pTex) {
  Container* container         = static_cast<Container*>(hfx->GetInternalHandle());
  Uniform* puni                = static_cast<Uniform*>(hpar->GetPlatformHandle());
  const UniformInstance* pinst = container->mActivePass->uniformInstance(puni);
  // printf( "Bind1 Tex<%p> par<%s> pinst<%p>\n",
  // pTex,hpar->mParameterName.c_str(), pinst );
  if (pinst) {
    int iloc = pinst->mLocation;

    const char* teknam = container->mActiveTechnique->mName.c_str();

    // printf( "Bind2 Tex<%p> par<%s> iloc<%d> teknam<%s>\n",
    // pTex,hpar->mParameterName.c_str(), iloc, teknam );
    if (iloc >= 0) {
      const char* psem = puni->mSemantic.c_str();
      const char* pnam = puni->mName.c_str();
      GLenum etyp      = puni->meType;
      // OrkAssert( etyp == GL_FLOAT_MAT4 );

      if (pTex != 0) {
        const GLTextureObject* pTEXOBJ = (GLTextureObject*)pTex->GetTexIH();
        GLuint texID                   = pTEXOBJ ? pTEXOBJ->mObject : 0;
        int itexunit                   = pinst->mSubItemIndex;

        GLenum textgt = pinst->mPrivData.Get<GLenum>();

        // printf( "Bind3 ISDEPTH<%d> tex<%p> texobj<%d> itexunit<%d>
        // textgt<%d>\n", int(pTex->_isDepthTexture), pTex, texID, itexunit,
        // int(textgt) );

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
     hfx->GetInternalHandle() ); CGeffect cgeffect = container->mCgEffect;
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
} //namespace ork::lev2::glslfx {
