////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#if defined( ORK_CONFIG_OPENGL ) && defined(_USE_GLSLFX)
#include "glslfxi.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

namespace ork { namespace lev2 { 



namespace FakeLoadGlslFx {

////////////////////////////////////////////////////////////////
static GlslFxTechnique* AddTek( const char* name, FxShader* pshader )
{
    GlslFxContainer* pcont = (GlslFxContainer*) pshader->GetInternalHandle();
    GlslFxTechnique* pGLSLTEK = new GlslFxTechnique;
    pGLSLTEK->mName = name;
    FxShaderTechnique* pABSTEK = new FxShaderTechnique;
    pABSTEK->mTechniqueName = name;
    pABSTEK->mInternalHandle = (void*) pGLSLTEK;
    pcont->mTechniqueMap[name] = pGLSLTEK;
    pshader->AddTechnique(pABSTEK);
    return pGLSLTEK;
    
}


/*   mVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(mVS, 1, &vsrc, NULL);
    glCompileShader(mVS);

    mFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(mFS, 1, &fsrc, NULL);
    glCompileShader(mFS);

    mSP = glCreateProgram();
    glAttachShader(mSP, mVS);
    glAttachShader(mSP, mFS);
    glLinkProgram(mSP);

   glUseProgram(mSP);
    mParamWidth = glGetUniformLocation(mSP, "width");
    mParamHeight = glGetUniformLocation(mSP, "height");
    mParamFrame = glGetUniformLocation(mSP, "frame");

*/
////////////////////////////////////////////////////////////////
void FakeLoadUI( FxShader* pfxshader )
{
    GlslFxContainer* pcont = new GlslFxContainer;
    pcont->mEffectName = "miniorkshader://ui";
    pfxshader->SetInternalHandle((void*)pcont);
    
    /////////////////////////////////////////////////////////////
    GlslFxVtxShader* pdefaultVS = new GlslFxVtxShader;
    GlslFxPixShader* pdefaultFS = new GlslFxPixShader;
    GlslFxStateBlock* pdefaultSB = new GlslFxStateBlock;
    /////////////////////////////////////////////////////////////
    const char* defVScode = 
    "//uniform float waveTime;"
    "//uniform float waveWidth;"
    "//uniform float waveHeight;"
    ""
    "void main(void)"
    "{"
    "    vec4 v = vec4(gl_Vertex);"
    ""
    "    //v.z = sin(waveWidth * v.x + waveTime) * cos(waveWidth * v.y + waveTime) * waveHeight;"
    ""
    "    gl_Position = gl_ModelViewProjectionMatrix * v;"
    "}";
    const char* defPScode = 
    "void main()"
    "{"
    "    gl_FragColor=vec4(1.0f,1.0f,1.0f,1.0f);"
    "}";
    /////////////////////////////////////////////////////////////
    pdefaultVS->InitFromString(defVScode);
    pdefaultFS->InitFromString(defPScode);
    /////////////////////////////////////////////////////////////
    GlslFxTechnique*ptek_uitext = AddTek( "uitext", pfxshader );
    ptek_uitext->NewPass( "p0", pdefaultVS, pdefaultFS, pdefaultSB );
    /////////////////////////////////////////////////////////////
    AddTek( "uidev_modcolor", pfxshader );
    AddTek( "uicolor", pfxshader );
    AddTek( "uicircle", pfxshader );
    AddTek( "uitextured", pfxshader );
    AddTek( "ui_vtx", pfxshader );
    AddTek( "ui_vtxmod", pfxshader );

}
////////////////////////////////////////////////////////////////
void FakeLoadSolid( FxShader* pfxshader )
{
    GlslFxContainer* pcont = new GlslFxContainer;
    pcont->mEffectName = "miniorkshader://solid";
    pfxshader->SetInternalHandle((void*)pcont);
    
    AddTek( "vtxcolor", pfxshader );
    AddTek( "vtxmodcolor", pfxshader );
    AddTek( "mmodcolor", pfxshader );
    AddTek( "texcolor", pfxshader );
    AddTek( "texmodcolor", pfxshader );
    AddTek( "textexmodcolor", pfxshader );
    AddTek( "texvtxcolor", pfxshader );
    AddTek( "texmodcolorFB", pfxshader );
    AddTek( "distortedfeedback", pfxshader );

}
////////////////////////////////////////////////////////////////
}}}

#endif
