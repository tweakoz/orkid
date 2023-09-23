////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

namespace ork::lev2 {

constexpr uint16_t kRGB_DXT1  = 0x83F0;
constexpr uint16_t kRGBA_DXT1 = 0x83F1;
constexpr uint16_t kRGBA_DXT3 = 0x83F2;
constexpr uint16_t kRGBA_DXT5 = 0x83F3;
//constexpr GLuint PBOOBJBASE   = 0x12340000;

///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::_loadDDSTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  auto load_req = std::make_shared<TexLoadReq>();
  load_req->ptex                  = ptex;
  load_req->_inpstream._datablock = datablock;
  load_req->_inpstream.advance(sizeof(dds::DDS_HEADER));
  ////////////////////////////////////////////////////////////////////
  auto ddsh = (const dds::DDS_HEADER*)load_req->_inpstream.data(0);
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
  //gltexobj_ptr_t pTEXOBJ = std::make_shared<GLTextureObject>(this);
  //ptex->_impl.set<gltexobj_ptr_t>(pTEXOBJ);

  ///////////////////////////////////////////////
  load_req->_ddsheader = ddsh;
  //load_req->pTEXOBJ    = pTEXOBJ;
  void_lambda_t lamb  = [=]() {
    /////////////////////////////////////////////
    // texture preprocssing, if any..
    //  on main thread.
    /////////////////////////////////////////////
    if (ptex->_varmap.hasKey("preproc")) {
      auto preproc        = ptex->_varmap.typedValueForKey<Texture::proc_t>("preproc").value();
      auto orig_datablock = datablock;
      //auto postblock      = preproc(ptex, &mTargetGL, orig_datablock);
    }
    this->_loadDDSTextureMainThreadPart(load_req);
  };
  if (ptex->_varmap.hasKey("loadimmediate")) {
    lamb();
  } else {
    opq::mainSerialQueue()->enqueue(lamb);
  }

  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::_loadDDSTexture(const AssetPath& infname, texture_ptr_t ptex) {
  AssetPath Filename = infname;
  ptex->_debugName   = infname.toStdString();
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

  auto inpdata   = std::make_shared<DataBlock>(pdata, ifilelen);
  inpdata->_name = infname.toStdString();
  return _loadDDSTexture(ptex, inpdata);
}

///////////////////////////////////////////////////////////////////////////////

void TextureInterface::_loadDDSTextureMainThreadPart(texloadreq_ptr_t req) {
  //mTargetGL.makeCurrentContext();
  //mTargetGL.debugPushGroup("loadDDSTextureMainThreadPart");
  const dds::DDS_HEADER* ddsh = req->_ddsheader;
  texture_ptr_t ptex               = req->ptex;
  //auto pTEXOBJ    = req->pTEXOBJ;
  // File& TextureFile = *req->pTEXFILE;

  int NumMips  = (ddsh->dwFlags & dds::DDSD_MIPMAPCOUNT) ? ddsh->dwMipMapCount : 1;
  int iwidth   = ddsh->dwWidth;
  int iheight  = ddsh->dwHeight;
  int idepth   = ddsh->dwDepth;
  int iBwidth  = (iwidth + 3) / 4;
  int iBheight = (iheight + 3) / 4;

  bool bVOLUMETEX = (idepth > 1);

  //GLuint TARGET = GL_TEXTURE_2D;
  //if (bVOLUMETEX) {
    //TARGET = GL_TEXTURE_3D;
  //}
  //pTEXOBJ->mTarget = TARGET;

  if (1) {
    auto dbgname = ptex->_debugName;
    printf("  tex<%s> ptex<%p>\n", dbgname.c_str(), (void*) ptex.get());
    printf("  tex<%s> width<%d>\n", dbgname.c_str(), iwidth);
    printf("  tex<%s> height<%d>\n", dbgname.c_str(), iheight);
    printf("  tex<%s> depth<%d>\n", dbgname.c_str(), idepth);
    printf("  tex<%s> nummips<%d>\n", dbgname.c_str(), NumMips);
    printf("  tex<%s> flgs<%x>\n", dbgname.c_str(), int(ddsh->dwFlags));
    printf("  tex<%s> 4cc<%x>\n", dbgname.c_str(), int(ddsh->ddspf.dwFourCC));
    printf("  tex<%s> bitcnt<%d>\n", dbgname.c_str(), int(ddsh->ddspf.dwRGBBitCount));
    printf("  tex<%s> rmask<0x%x>\n", dbgname.c_str(), int(ddsh->ddspf.dwRBitMask));
    printf("  tex<%s> gmask<0x%x>\n", dbgname.c_str(), int(ddsh->ddspf.dwGBitMask));
    printf("  tex<%s> bmask<0x%x>\n", dbgname.c_str(), int(ddsh->ddspf.dwBBitMask));
  }

  //////////////////

  //GL_ERRORCHECK();

  // GLuint sampler_obj = 0;
  // glGenSamplers(1,&sampler_obj);
  // assert(sampler_obj!=0);
  // printf( "sampler_obj<%d>\n", int(sampler_obj));

  //glGenTextures(1, &pTEXOBJ->mObject);
  //glBindTexture(TARGET, pTEXOBJ->mObject);
  //GL_ERRORCHECK();
  if (ptex->_debugName.length()) {
    //mTargetGL.debugLabel(GL_TEXTURE, pTEXOBJ->mObject, ptex->_debugName);
  }

  //ptex->_varmap.makeValueForKey<GLuint>("gltexobj") = pTEXOBJ->mObject;


  auto infname = req->_texname;

  // printf("  tex<%p:%s> ORKTEXOBJECT<%p> GLTEXOBJECT<%d>\n", ptex, ptex->_debugName.c_str(), pTEXOBJ, int(pTEXOBJ->mObject));

  ////////////////////////////////////////////////////////////////////
  //
  ////////////////////////////////////////////////////////////////////

  //glTexParameteri(TARGET, GL_TEXTURE_BASE_LEVEL, 0);
  //glTexParameteri(TARGET, GL_TEXTURE_MAX_LEVEL, NumMips - 1);

  if (dds::IsLUM(ddsh->ddspf)) {
    // printf( "  tex<%s> LUM\n", ptex->_debugName.c_str() );
    ptex->_texFormat = EBufferFormat::R8;
    //if (bVOLUMETEX)
      //Set3D(this, ptex.get(), GL_RED, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr, pdata );
    //else
      //Set2D(this, ptex.get(), 1, GL_RED, GL_UNSIGNED_BYTE, TARGET, 1, NumMips, iwidth, iheight, req->_inpstream); // ireadptr, pdata );
  } else if (dds::IsBGR5A1(ddsh->ddspf)) {
    ptex->_texFormat = EBufferFormat::R8;
    const dds::DdsLoadInfo& li = dds::loadInfoBGR5A1;
    // printf("  tex<%s> BGR5A1\n", ptex->_debugName.c_str());
    // printf( "  tex<%s> size<%d>\n", ptex->_debugName.c_str(), 2 );
    //if (bVOLUMETEX) {
      //Set3D(this, ptex.get(), GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr,
    //} else
      //Set2D(this, ptex.get(), 4, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 2, NumMips, iwidth, iheight, req->_inpstream); // ireadptr,
                                                                                                                     // pdata
                                                                                                                     // );
  } else if (dds::IsBGRA8(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGRA8;
    int size                   = idepth * iwidth * iheight * 4;
    // printf("  tex<%s> BGRA8\n", ptex->_debugName.c_str());
    // printf( "  tex<%s> size<%d>\n", ptex->_debugName.c_str(), size );
    ptex->_texFormat = EBufferFormat::BGRA8;
    if (bVOLUMETEX) {
      //Set3D(this, ptex.get(), GL_RGBA, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr, pdata );
    } else {
      //Set2D(this, ptex.get(), 4, GL_BGRA, GL_UNSIGNED_BYTE, TARGET, 4, NumMips, iwidth, iheight, req->_inpstream); // ireadptr, pdata );

      if (NumMips > 3) {
        ptex->TexSamplingMode().PresetTrilinearWrap();
        // assert(false);
      }
    }

    //GL_ERRORCHECK();
  } else if (dds::IsBGR8(ddsh->ddspf)) {
    int size = idepth * iwidth * iheight * 3;
    // printf("  tex<%s> BGR8\n", ptex->_debugName.c_str());
    // printf( "  tex<%s> size<%d>\n", TextureFile.msFileName.c_str(), size );
    // printf( "  tex<%s> BGR8\n", ptex->_debugName.c_str() );
    ptex->_texFormat = EBufferFormat::BGR8;
    //if (bVOLUMETEX) {
      //Set3D(this, ptex.get(), GL_BGR, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr, pdata );
    //} else
      //Set2D(this, ptex.get(), 3, GL_BGR, GL_UNSIGNED_BYTE, TARGET, 3, NumMips, iwidth, iheight, req->_inpstream); // ireadptr, pdata );
    //GL_ERRORCHECK();
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
    // printf("  tex<%s> DXT5\n", ptex->_debugName.c_str());
    // printf("  tex<%s> size<%d>\n", ptex->_debugName.c_str(), size);
    ptex->_texFormat = EBufferFormat::S3TC_DXT5;
    //if (bVOLUMETEX) {
      //Set3DC(this, ptex.get(), kRGBA_DXT5, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr, pdata );
    //} else
      //Set2DC(this, ptex.get(), kRGBA_DXT5, TARGET, li.blockBytes, NumMips, iwidth, iheight, req->_inpstream); // ireadptr, pdata );
   // GL_ERRORCHECK();
    //////////////////////////////////////
  }
  //////////////////////////////////////////////////////////
  // DXT3: texturing fast path (8 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT3(ddsh->ddspf)) {
    ptex->_texFormat = EBufferFormat::S3TC_DXT3;
    const dds::DdsLoadInfo& li = dds::loadInfoDXT3;
    int size                   = (iBwidth * iBheight) * li.blockBytes;
    // printf("  tex<%s> DXT3\n", ptex->_debugName.c_str());
    // printf("  tex<%s> size<%d>\n", ptex->_debugName.c_str(), size);
  
    //if (bVOLUMETEX) {
      //Set3DC(this, ptex.get(), kRGBA_DXT3, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr, pdata );
    //} else
      //Set2DC(this, ptex.get(), kRGBA_DXT3, TARGET, li.blockBytes, NumMips, iwidth, iheight, req->_inpstream); // ireadptr, pdata );
  }
  //////////////////////////////////////////////////////////
  // DXT1: texturing fast path (4 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT1(ddsh->ddspf)) {
    ptex->_texFormat = EBufferFormat::S3TC_DXT1;
    const dds::DdsLoadInfo& li = dds::loadInfoDXT1;
    int size                   = (iBwidth * iBheight) * li.blockBytes;
    auto cmc = std::make_shared<CompressedImageMipChain>();
    req->_cmipchain            = cmc;
    cmc->_width = iwidth;
    cmc->_height = iheight;
    cmc->_depth = idepth;
    cmc->_numcomponents = 3;
    cmc->_format = EBufferFormat::S3TC_DXT1;
    cmc->_levels.resize(NumMips);
    for (int imip = 0; imip < NumMips; imip++) {
      int iBwidth   = (iwidth + 3) / 4;
      int iBheight  = (iheight + 3) / 4;
      int isize     = (iBwidth * iBheight) * li.blockBytes;
      auto datablock = std::make_shared<DataBlock>(req->_inpstream.current(),isize);
      req->_inpstream.advance(isize);
      auto& level = cmc->_levels[imip];
      level._width = iwidth;
      level._height = iheight;
      level._depth = idepth;
      level._data = datablock;
      iwidth >>= 1;
      iheight >>= 1;
      idepth >>= 1;
    }
    _createFromCompressedLoadReq(req);
    // printf("  tex<%s> DXT1\n", ptex->_debugName.c_str());
    // printf("  tex<%s> size<%d> nummips<%d> w<%d> h<%d> \n", ptex->_debugName.c_str(), size, NumMips, iwidth, iheight);
    //if (bVOLUMETEX) {
      //Set3DC(this, ptex.get(), kRGB_DXT1, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req->_inpstream); // ireadptr, pdata );
    //} else
      //Set2DC(this, ptex.get(), kRGB_DXT1, TARGET, li.blockBytes, NumMips, iwidth, iheight, req->_inpstream); // ireadptr, pdata );
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

  this->ApplySamplingMode(ptex.get());

  ptex->_dirty = false;
  //glBindTexture(TARGET, 0);
  //GL_ERRORCHECK();

  ////////////////////////////////////////////////
  // done loading texture,
  //  perform postprocessing, if any..
  ////////////////////////////////////////////////

  if (ptex->_varmap.hasKey("postproc")) {
    auto dblock    = req->_inpstream._datablock;
    auto postproc  = ptex->_varmap.typedValueForKey<Texture::proc_t>("postproc").value();
    auto postblock = postproc(ptex, _ctx, dblock);
    OrkAssert(postblock);
  } else {
    // printf("ptex<%p> no postproc\n", ptex);
  }

  //mTargetGL.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2