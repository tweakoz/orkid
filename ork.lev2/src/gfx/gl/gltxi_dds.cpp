////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadDDSTexture(Texture* ptex, datablockptr_t datablock) {
  GlTexLoadReq load_req;
  load_req.ptex                  = ptex;
  load_req._inpstream._datablock = datablock;
  load_req._inpstream.advance(sizeof(dds::DDS_HEADER));
  ////////////////////////////////////////////////////////////////////
  auto ddsh = (const dds::DDS_HEADER*)load_req._inpstream.data(0);
  ////////////////////////////////////////////////////////////////////
  ptex->_width  = ddsh->dwWidth;
  ptex->_height = ddsh->dwHeight;
  ptex->_depth  = (ddsh->dwDepth > 1) ? ddsh->dwDepth : 1;
  ////////////////////////////////////////////////////////////////////
  int NumMips = (ddsh->dwFlags & dds::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
  int iwidth  = ddsh->dwWidth;
  int iheight = ddsh->dwHeight;
  int idepth  = ddsh->dwDepth;
  ///////////////////////////////////////////////
  GLTextureObject* pTEXOBJ = new GLTextureObject;
  ptex->_internalHandle    = (void*)pTEXOBJ;

  ///////////////////////////////////////////////
  load_req._ddsheader = ddsh;
  load_req.pTEXOBJ    = pTEXOBJ;
  void_lambda_t lamb  = [=]() {
    /////////////////////////////////////////////
    // texture preprocssing, if any..
    //  on main thread.
    /////////////////////////////////////////////
    if (ptex->_varmap.hasKey("preproc")) {
      auto preproc        = ptex->_varmap.typedValueForKey<Texture::proc_t>("preproc").value();
      auto orig_datablock = datablock;
      auto postblock      = preproc(ptex, &mTargetGL, orig_datablock);
    }
    this->LoadDDSTextureMainThreadPart(load_req);
  };
  opq::mainSerialQueue().enqueue(lamb);
  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadDDSTexture(const AssetPath& infname, Texture* ptex) {
  AssetPath Filename = infname;
  ptex->_debugName   = infname.c_str();
  ///////////////////////////////////////////////
  File TextureFile(Filename, EFM_READ);
  if (false == TextureFile.IsOpen()) {
    return false;
  }
  ///////////////////////////////////////////////
  size_t ifilelen       = 0;
  EFileErrCode eFileErr = TextureFile.GetLength(ifilelen);
  auto pdata            = (U8*)malloc(ifilelen);
  assert(pdata != nullptr);
  eFileErr = TextureFile.Read(pdata, ifilelen);
  ////////////////////////////////////////////////////////////////////

  /*if (0) {
    printf("  tex<%s> ptex<%p>\n", infname.c_str(), ptex);
    printf("  tex<%s> width<%d>\n", infname.c_str(), iwidth);
    printf("  tex<%s> height<%d>\n", infname.c_str(), iheight);
    printf("  tex<%s> depth<%d>\n", infname.c_str(), idepth);
    printf("  tex<%s> nummips<%d>\n", infname.c_str(), NumMips);
    printf("  tex<%s> flgs<%x>\n", infname.c_str(), int(ddsh->dwFlags));
    printf("  tex<%s> 4cc<%x>\n", infname.c_str(), int(ddsh->ddspf.dwFourCC));
    printf("  tex<%s> bitcnt<%d>\n", infname.c_str(), int(ddsh->ddspf.dwRGBBitCount));
    printf("  tex<%s> rmask<0x%x>\n", infname.c_str(), int(ddsh->ddspf.dwRBitMask));
    printf("  tex<%s> gmask<0x%x>\n", infname.c_str(), int(ddsh->ddspf.dwGBitMask));
    printf("  tex<%s> bmask<0x%x>\n", infname.c_str(), int(ddsh->ddspf.dwBBitMask));
  }*/
  auto inpdata   = std::make_shared<DataBlock>(pdata, ifilelen);
  inpdata->_name = infname.toStdString();
  return LoadDDSTexture(ptex, inpdata);
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::LoadDDSTextureMainThreadPart(GlTexLoadReq req) {
  mTargetGL.makeCurrentContext();
  mTargetGL.debugPushGroup("LoadDDSTextureMainThreadPart");
  const dds::DDS_HEADER* ddsh = req._ddsheader;
  Texture* ptex               = req.ptex;
  GLTextureObject* pTEXOBJ    = req.pTEXOBJ;
  // File& TextureFile = *req.pTEXFILE;

  int NumMips  = (ddsh->dwFlags & dds::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
  int iwidth   = ddsh->dwWidth;
  int iheight  = ddsh->dwHeight;
  int idepth   = ddsh->dwDepth;
  int iBwidth  = (iwidth + 3) / 4;
  int iBheight = (iheight + 3) / 4;

  bool bVOLUMETEX = (idepth > 1);

  GLuint TARGET = GL_TEXTURE_2D;
  if (bVOLUMETEX) {
    TARGET = GL_TEXTURE_3D;
  }
  pTEXOBJ->mTarget = TARGET;

  //////////////////

  GL_ERRORCHECK();

  // GLuint sampler_obj = 0;
  // glGenSamplers(1,&sampler_obj);
  // assert(sampler_obj!=0);
  // printf( "sampler_obj<%d>\n", int(sampler_obj));

  glGenTextures(1, &pTEXOBJ->mObject);
  glBindTexture(TARGET, pTEXOBJ->mObject);
  GL_ERRORCHECK();
  if (ptex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, pTEXOBJ->mObject, ptex->_debugName);
  }

  auto infname = req._texname;

  // printf("  tex<%p:%s> ORKTEXOBJECT<%p> GLTEXOBJECT<%d>\n", ptex, ptex->_debugName.c_str(), pTEXOBJ, int(pTEXOBJ->mObject));

  ////////////////////////////////////////////////////////////////////
  //
  ////////////////////////////////////////////////////////////////////

  glTexParameteri(TARGET, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(TARGET, GL_TEXTURE_MAX_LEVEL, NumMips - 1);

  if (dds::IsLUM(ddsh->ddspf)) {
    // printf( "  tex<%s> LUM\n", infname.c_str() );
    if (bVOLUMETEX)
      Set3D(this, ptex, GL_RED, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2D(this, ptex, 1, GL_RED, GL_UNSIGNED_BYTE, TARGET, 1, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
  } else if (dds::IsBGR5A1(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGR5A1;
    // printf( "  tex<%s> BGR5A1\n", infname.c_str() );
    // printf( "  tex<%s> size<%d>\n", infname.c_str(), 2 );
    if (bVOLUMETEX) {
      Set3D(this, ptex, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr,
    } else
      Set2D(this, ptex, 4, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 2, NumMips, iwidth, iheight, req._inpstream); // ireadptr,
                                                                                                                     // pdata
                                                                                                                     // );
  } else if (dds::IsBGRA8(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGRA8;
    int size                   = idepth * iwidth * iheight * 4;
    // printf("  tex<%s> BGRA8\n", infname.c_str());
    // printf( "  tex<%s> size<%d>\n", infname.c_str(), size );
    if (bVOLUMETEX) {
      Set3D(this, ptex, GL_RGBA, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    } else {
      Set2D(this, ptex, 4, GL_BGRA, GL_UNSIGNED_BYTE, TARGET, 4, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );

      if (NumMips > 3) {
        ptex->TexSamplingMode().PresetTrilinearWrap();
        // assert(false);
      }
    }

    GL_ERRORCHECK();
  } else if (dds::IsBGR8(ddsh->ddspf)) {
    int size = idepth * iwidth * iheight * 3;
    // printf( "  tex<%s> BGR8\n", TextureFile.msFileName.c_str() );
    // printf( "  tex<%s> size<%d>\n", TextureFile.msFileName.c_str(), size );
    // printf( "  tex<%s> BGR8\n", infname.c_str() );
    if (bVOLUMETEX) {
      Set3D(this, ptex, GL_BGR, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    } else
      Set2D(this, ptex, 3, GL_BGR, GL_UNSIGNED_BYTE, TARGET, 3, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
    GL_ERRORCHECK();
    if (NumMips > 3) {
      ptex->TexSamplingMode().PresetTrilinearWrap();
    }
  }
  //////////////////////////////////////////////////////////
  // DXT5: texturing fast path (8 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT5(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT5;
    int size                   = (iBwidth * iBheight) * li.blockBytes;
    // printf("  tex<%s> DXT5\n", infname.c_str());
    // printf("  tex<%s> size<%d>\n", infname.c_str(), size);
    if (bVOLUMETEX) {
      Set3DC(this, ptex, kRGBA_DXT5, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    } else
      Set2DC(this, ptex, kRGBA_DXT5, TARGET, li.blockBytes, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
    GL_ERRORCHECK();
    //////////////////////////////////////
  }
  //////////////////////////////////////////////////////////
  // DXT3: texturing fast path (8 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT3(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT3;
    int size                   = (iBwidth * iBheight) * li.blockBytes;
    // printf("  tex<%s> DXT3\n", infname.c_str());
    // printf("  tex<%s> size<%d>\n", infname.c_str(), size);

    if (bVOLUMETEX) {
      Set3DC(this, ptex, kRGBA_DXT3, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    } else
      Set2DC(this, ptex, kRGBA_DXT3, TARGET, li.blockBytes, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
  }
  //////////////////////////////////////////////////////////
  // DXT1: texturing fast path (4 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT1(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT1;
    int size                   = (iBwidth * iBheight) * li.blockBytes;
    // printf("  tex<%s> DXT1\n", infname.c_str());
    // printf("  tex<%s> size<%d>\n", infname.c_str(), size);
    if (bVOLUMETEX) {
      Set3DC(this, ptex, kRGBA_DXT1, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    } else
      Set2DC(this, ptex, kRGBA_DXT1, TARGET, li.blockBytes, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
  }
  //////////////////////////////////////////////////////////
  // ???
  //////////////////////////////////////////////////////////
  else {
    OrkAssert(false);
  }

  if (bVOLUMETEX) {
    ptex->TexSamplingMode().PresetTrilinearWrap();
  }

  this->ApplySamplingMode(ptex);

  ptex->_dirty = false;
  glBindTexture(TARGET, 0);
  GL_ERRORCHECK();

  ////////////////////////////////////////////////
  // done loading texture,
  //  perform postprocessing, if any..
  ////////////////////////////////////////////////

  if (ptex->_varmap.hasKey("postproc")) {
    auto dblock    = req._inpstream._datablock;
    auto postproc  = ptex->_varmap.typedValueForKey<Texture::proc_t>("postproc").value();
    auto postblock = postproc(ptex, &mTargetGL, dblock);
    OrkAssert(postblock);
  } else {
    // printf("ptex<%p> no postproc\n", ptex);
  }

  mTargetGL.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
