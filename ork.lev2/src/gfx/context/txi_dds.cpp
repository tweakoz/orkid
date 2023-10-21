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

  auto cmc = std::make_shared<CompressedImageMipChain>();
  cmc->_width = iwidth;
  cmc->_height = iheight;
  cmc->_depth = idepth;
    cmc->_levels.resize(NumMips);

  OrkAssert(not bVOLUMETEX);

  auto proc_mips = [&](size_t block_bytes, size_t dim_shift){
    for (int imip = 0; imip < NumMips; imip++) {
      int iBwidth   = iwidth;
      int iBheight  = iheight;
      if(dim_shift!=0){
        int add = (1<<dim_shift)-1;
        iBwidth   = (iwidth + add) >> dim_shift;
        iBheight  = (iheight + add) >> dim_shift;
      }
      int isize     = (iBwidth * iBheight) * block_bytes;
      auto datablock = std::make_shared<DataBlock>(req->_inpstream.current(),isize);
      req->_inpstream.advance(isize);
      auto& level = cmc->_levels[imip];
      level._width = iwidth;
      level._height = iheight;
      level._depth = idepth;
      level._data = datablock;
      iwidth >>= 1;
      iheight >>= 1;
      if(idepth>1){
        idepth >>= 1;
      }
    }
    req->_cmipchain = cmc;
    _createFromCompressedLoadReq(req);
  };

  if (dds::IsLUM(ddsh->ddspf)) {
    ptex->_texFormat = EBufferFormat::R8;
  } else if (dds::IsBGR5A1(ddsh->ddspf)) {
    /////////////////////////////////////////////////////////////
    ptex->_texFormat = EBufferFormat::BGR5A1;
    const dds::DdsLoadInfo& li = dds::loadInfoBGR5A1;
    ptex->_texFormat = EBufferFormat::BGRA8;
    cmc->_format = EBufferFormat::BGR5A1;
    cmc->_numcomponents = 4;
    // proc_mips(li.blockBytes, 2);
    /////////////////////////////////////////////////////////////
  } else if (dds::IsBGRA8(ddsh->ddspf)) {
    /////////////////////////////////////////////////////////////
    const dds::DdsLoadInfo& li = dds::loadInfoBGRA8;
    ptex->_texFormat = EBufferFormat::BGRA8;
    cmc->_format = EBufferFormat::BGRA8;
    cmc->_numcomponents = 4;
    proc_mips(4, 0);
    //_createFromCompressedLoadReq(req);
    if (NumMips > 3) {
      ptex->TexSamplingMode().PresetTrilinearWrap();
      // assert(false);
    }
    /////////////////////////////////////////////////////////////
  } else if (dds::IsBGR8(ddsh->ddspf)) {
    /////////////////////////////////////////////////////////////
    ptex->_texFormat = EBufferFormat::BGR8;
    cmc->_format = EBufferFormat::BGR8;
    cmc->_numcomponents = 3;
    OrkAssert(false);
    if (NumMips > 3) {
      ptex->TexSamplingMode().PresetTrilinearWrap();
    }
    /////////////////////////////////////////////////////////////
  }
  //////////////////////////////////////////////////////////
  // DXT5: texturing fast path (8 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT5(ddsh->ddspf)) {
    /////////////////////////////////////////////////////////////
    const dds::DdsLoadInfo& li = dds::loadInfoDXT5;
    ptex->_texFormat = EBufferFormat::S3TC_DXT5;
    cmc->_format = EBufferFormat::S3TC_DXT5;
    cmc->_numcomponents = 4;
    /////////////////////////////////////////////////////////////
  }
  //////////////////////////////////////////////////////////
  // DXT3: texturing fast path (8 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT3(ddsh->ddspf)) {
    ptex->_texFormat = EBufferFormat::S3TC_DXT3;
    const dds::DdsLoadInfo& li = dds::loadInfoDXT3;
    ptex->_texFormat = EBufferFormat::S3TC_DXT3;
    cmc->_format = EBufferFormat::S3TC_DXT3;
    cmc->_numcomponents = 4;
    proc_mips(li.blockBytes, 2);
    OrkAssert(false);

  }
  //////////////////////////////////////////////////////////
  // DXT1: texturing fast path (4 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT1(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT1;
    ptex->_texFormat = EBufferFormat::S3TC_DXT1;
    cmc->_numcomponents = 3;
    cmc->_format = EBufferFormat::S3TC_DXT1;
    proc_mips(li.blockBytes, 2);
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
