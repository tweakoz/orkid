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

GLuint gLastBoundNonZeroTex = 0;

namespace ork::lev2 {

GLTextureObject::GLTextureObject()
    : mObject(0)
    , mFbo(0)
    , mDbo(0)
    , mTarget(GL_NONE) {
}

///////////////////////////////////////////////////////////////////////////////

GlTextureInterface::GlTextureInterface(ContextGL& tgt)
    : mTargetGL(tgt) {
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::DestroyTexture(Texture* tex) {
  auto glto            = (GLTextureObject*)tex->_internalHandle;
  tex->_internalHandle = nullptr;

  void_lambda_t lamb = [=]() {
    if (glto) {
      if (glto->mObject != 0)
        glDeleteTextures(1, &glto->mObject);
      delete glto;
    }
  };
  // opq::mainSerialQueue().push(lamb,get_backtrace());
  opq::mainSerialQueue().enqueue(lamb);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::TexManInit(void) {
}

///////////////////////////////////////////////////////////////////////////////

PboSet::PboSet(size_t size)
    : _size(size) {
}

///////////////////////////////////////////////////////////////////////////////

PboSet::~PboSet() {
  for (GLuint item : _pbos_perm) {
    glDeleteBuffers(1, &item);
  }
}

///////////////////////////////////////////////////////////////////////////////

GLuint PboSet::alloc() {
  GLuint rval = 0xffffffff;
  auto it     = _pbos.begin();
  if (it == _pbos.end()) {
    GL_ERRORCHECK();
    glGenBuffers(1, &rval);
    GL_ERRORCHECK();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, rval);
    GL_ERRORCHECK();
    glBufferData(GL_PIXEL_UNPACK_BUFFER, _size, NULL, GL_DYNAMIC_DRAW);
    GL_ERRORCHECK();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    GL_ERRORCHECK();
    _pbos_perm.insert(rval);
  } else {
    rval = *it;
    _pbos.erase(it);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void PboSet::free(GLuint item) {
  _pbos.insert(item);
}

///////////////////////////////////////////////////////////////////////////////

GLuint GlTextureInterface::_getPBO(size_t isize) {
  PboSet* pbs = 0;
  auto it     = mPBOSets.find(isize);
  if (it == mPBOSets.end()) {
    pbs             = new PboSet(isize);
    mPBOSets[isize] = pbs;
  } else {
    pbs = it->second;
  }
  return pbs->alloc();
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::_returnPBO(size_t isize, GLuint pbo) {
  auto it = mPBOSets.find(isize);
  OrkAssert(it != mPBOSets.end());
  PboSet* pbs = it->second;
  pbs->free(pbo);
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadTexture(const AssetPath& infname, Texture* ptex) {
  ///////////////////////////////////////////////
  AssetPath DdsFilename = infname;
  AssetPath VdsFilename = infname;
  DdsFilename.SetExtension("dds");
  VdsFilename.SetExtension("vds");
  bool bDDSPRESENT = FileEnv::GetRef().DoesFileExist(DdsFilename);
  bool bVDSPRESENT = FileEnv::GetRef().DoesFileExist(VdsFilename);

  if (bVDSPRESENT)
    return LoadVDSTexture(VdsFilename, ptex);
  else if (bDDSPRESENT)
    return LoadDDSTexture(DdsFilename, ptex);
  else
    return false;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) {
  // printf( "GlTextureInterface::UpdateAnimatedTexture( ptex<%p> tai<%p> )\n", ptex, tai );
  GLTextureObject* pTEXOBJ = (GLTextureObject*)ptex->GetTexIH();
  if (pTEXOBJ && ptex->GetTexAnim()) {
    ptex->GetTexAnim()->UpdateTexture(this, ptex, tai);
  }
}

///////////////////////////////////////////////////////////////////////////////

VdsTextureAnimation::VdsTextureAnimation(const AssetPath& pth) {
  mPath = pth.c_str();
  printf("Loading VDS<%s>\n", pth.c_str());
  AssetPath Filename = pth;
  ///////////////////////////////////////////////
  mpFile                    = new File(Filename, EFM_READ);
  mpFile->mbEnableBuffering = false;
  if (false == mpFile->IsOpen()) {
    return;
  }
  ///////////////////////////////////////////////
  size_t ifilelen       = 0;
  EFileErrCode eFileErr = mpFile->GetLength(ifilelen);
  U8* pdata             = (U8*)malloc(ifilelen);
  miFileLength          = int(ifilelen);
  OrkAssertI(pdata != 0, "out of memory ?");
  eFileErr          = mpFile->Read(pdata, sizeof(dds::DDS_HEADER));
  mpDDSHEADER       = (dds::DDS_HEADER*)pdata;
  miFrameBaseOffset = sizeof(dds::DDS_HEADER);
  ////////////////////////////////////////////////////////////////////
  miW         = mpDDSHEADER->dwWidth;
  miH         = mpDDSHEADER->dwHeight;
  miNumFrames = (mpDDSHEADER->dwDepth > 1) ? mpDDSHEADER->dwDepth : 1;
  ////////////////////////////////////////////////////////////////////
  int NumMips = (mpDDSHEADER->dwFlags & dds::DDSD_MIPMAPCOUNT) ? mpDDSHEADER->dwMipMapCount : 1;
  int iwidth  = mpDDSHEADER->dwWidth;
  int iheight = mpDDSHEADER->dwHeight;
  int idepth  = mpDDSHEADER->dwDepth;
  ////////////////////////////////////////////////////////////////////
  // printf( "  tex<%s> width<%d>\n", pth.c_str(), iwidth );
  // printf( "  tex<%s> height<%d>\n", pth.c_str(), iheight );
  // printf( "  tex<%s> depth<%d>\n", pth.c_str(), idepth );
  // printf( "  tex<%s> NumMips<%d>\n", pth.c_str(), NumMips );

  bool bVOLUMETEX = (idepth > 1);

  int iBwidth     = (iwidth + 3) / 4;
  int iBheight    = (iheight + 3) / 4;
  miFrameBaseSize = 0;

  if (dds::IsBGRA8(mpDDSHEADER->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGRA8;
    miFrameBaseSize            = iwidth * iheight * 4;
  } else if (dds::IsBGR8(mpDDSHEADER->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGR8;
    miFrameBaseSize            = iwidth * iheight * 3;
  } else if (dds::IsDXT1(mpDDSHEADER->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT1;
    miFrameBaseSize            = (iBwidth * iBheight) * li.blockBytes;
  } else if (dds::IsDXT3(mpDDSHEADER->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT3;
    miFrameBaseSize            = (iBwidth * iBheight) * li.blockBytes;
  } else if (dds::IsDXT5(mpDDSHEADER->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT5;
    miFrameBaseSize            = (iBwidth * iBheight) * li.blockBytes;
  }
  ////////////////////////////////////////////////////////////////////
  for (int i = 0; i < kframecachesize; i++) {
    void* pbuffer    = malloc(miFrameBaseSize);
    mFrameBuffers[i] = pbuffer;
  }
  ////////////////////////////////////////////////////////////////////
}
VdsTextureAnimation::~VdsTextureAnimation() {
  delete mpFile;
  for (int i = 0; i < kframecachesize; i++) {
    free(mFrameBuffers[i]);
    mFrameBuffers[i] = 0;
  }
}
void* VdsTextureAnimation::ReadFromFrameCache(int iframe, int isize) {
  int icacheentry                 = 0;
  std::map<int, int>::iterator it = mFrameCache.find(iframe);
  if (it != mFrameCache.end()) // cache hit
  {
    icacheentry = it->second;
    // printf( "cachehit iframe<%d> entry<%d>\n", iframe, icacheentry );
  } else // cache miss
  {
    int inumincache = int(mFrameCache.size());
    icacheentry     = inumincache;
    if (inumincache >= kframecachesize) // cache full
    {
      std::map<int, int>::iterator item = mFrameCache.begin();
      std::advance(item, rand() % inumincache);
      int iframeevicted = item->first;
      int ientryevicted = item->second;
      mFrameCache.erase(item);
      icacheentry = ientryevicted;
    }
    void* pcachedest = mFrameBuffers[icacheentry];
    mpFile->Read(pcachedest, miFrameBaseSize);
    mFrameCache[iframe] = icacheentry;
    // printf( "cachemiss vds<%s> iframe<%d> entry<%d>\n", mPath.c_str(), iframe, icacheentry );
  }
  void* pcachedest = mFrameBuffers[icacheentry];
  return pcachedest;
}

void VdsTextureAnimation::UpdateTexture(TextureInterface* txi, lev2::Texture* ptex, TextureAnimationInst* pinst) {
  GLTextureObject* pTEXOBJ   = (GLTextureObject*)ptex->GetTexIH();
  GlTextureInterface* pgltxi = (GlTextureInterface*)txi;
  float ftime                = pinst->GetCurrentTime();
  float fps                  = 30.0f;

  int iframe   = int(ftime * fps) % miNumFrames;
  int iseekpos = miFrameBaseOffset + (iframe * miFrameBaseSize);
  mpFile->SeekFromStart(iseekpos);

  // printf( "VdsTextureAnimation::UpdateTexture(ptex<%p> time<%f> seekpos<%d> readsiz<%d> fillen<%d> iframe<%d>)\n", ptex,
  // pinst->GetCurrentTime(),iseekpos,miFrameBaseSize, miFileLength, iframe );

  void* pdata = ReadFromFrameCache(iframe, miFrameBaseSize);

  if (dds::IsBGRA8(mpDDSHEADER->ddspf)) {
    /////////////////////////////////////////////////
    // allocate space for image
    // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
    // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
    // basically you can call glTexImage2D once with the s3tc format as the internalformat
    //  and a null data ptr to let the driver 'allocate' space for the texture
    //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
    //  this decouples allocation from writing, allowing you to overwrite more efficiently
    /////////////////////////////////////////////////

    glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);

    /////////////////////////////
    // imgdata->PBO
    /////////////////////////////

    // printf( "UPDATE IMAGE UNC iw<%d> ih<%d> to<%d>\n", miW, miH, int(pTEXOBJ->mObject) );

    const GLuint PBOOBJ = pgltxi->_getPBO(miFrameBaseSize);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBOOBJ);
    void* pgfxmem = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    // mpFile->Read( pgfxmem, miFrameBaseSize );
    memcpy(pgfxmem, pdata, miFrameBaseSize);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    ////////////////////////
    // PBO->texture
    ////////////////////////

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, miW, miH, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    ////////////////////////
    // unbind the PBO
    ////////////////////////

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    pgltxi->_returnPBO(miFrameBaseSize, PBOOBJ);
  }
  if (dds::IsDXT5(mpDDSHEADER->ddspf)) {
    /////////////////////////////////////////////////
    // allocate space for image
    // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
    // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
    // basically you can call glTexImage2D once with the s3tc format as the internalformat
    //  and a null data ptr to let the driver 'allocate' space for the texture
    //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
    //  this decouples allocation from writing, allowing you to overwrite more efficiently
    /////////////////////////////////////////////////

    glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);

    /////////////////////////////
    // imgdata->PBO
    /////////////////////////////

    // printf( "UPDATE IMAGE UNC iw<%d> ih<%d> to<%d>\n", miW, miH, int(pTEXOBJ->mObject) );

    const GLuint PBOOBJ = pgltxi->_getPBO(miFrameBaseSize);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBOOBJ);
    void* pgfxmem = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    //        mpFile->Read( pgfxmem, miFrameBaseSize );
    memcpy(pgfxmem, pdata, miFrameBaseSize);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    ////////////////////////
    // PBO->texture
    ////////////////////////

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, miW, miH, kRGBA_DXT5, miFrameBaseSize, 0);

    ////////////////////////
    // unbind the PBO
    ////////////////////////

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    pgltxi->_returnPBO(miFrameBaseSize, PBOOBJ);
  }
}
float VdsTextureAnimation::GetLengthOfTime() const {
  return 10.0f;
}
bool GlTextureInterface::LoadVDSTexture(const AssetPath& infname, Texture* ptex) {
  GLTextureObject* pTEXOBJ = new GLTextureObject;
  ptex->_internalHandle    = (void*)pTEXOBJ;
  glGenTextures(1, &pTEXOBJ->mObject);

  VdsTextureAnimation* vta = new VdsTextureAnimation(infname);
  ptex->SetTexAnim(vta);

  ptex->_width  = vta->miW;
  ptex->_height = vta->miH;
  ptex->_depth  = 1;

  glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  if (dds::IsBGRA8(vta->mpDDSHEADER->ddspf)) { // allocate uncompressed
    glTexImage2D(GL_TEXTURE_2D, 0, 4, vta->miW, vta->miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  } else if (dds::IsDXT5(vta->mpDDSHEADER->ddspf)) { // allocate compressed
    glTexImage2D(GL_TEXTURE_2D, 0, kRGBA_DXT5, vta->miW, vta->miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }

  return true;
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
      Set3D(this, GL_RED, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2D(this, ptex, 1, GL_RED, GL_UNSIGNED_BYTE, TARGET, 1, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
  } else if (dds::IsBGR5A1(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGR5A1;
    // printf( "  tex<%s> BGR5A1\n", infname.c_str() );
    // printf( "  tex<%s> size<%d>\n", infname.c_str(), 2 );
    if (bVOLUMETEX)
      Set3D(
          this, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2D(this, ptex, 4, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TARGET, 2, NumMips, iwidth, iheight, req._inpstream); // ireadptr,
                                                                                                                     // pdata
                                                                                                                     // );
  } else if (dds::IsBGRA8(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoBGRA8;
    int size                   = idepth * iwidth * iheight * 4;
    // printf("  tex<%s> BGRA8\n", infname.c_str());
    // printf( "  tex<%s> size<%d>\n", infname.c_str(), size );
    if (bVOLUMETEX)
      Set3D(this, GL_RGBA, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else {
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
    if (bVOLUMETEX)
      Set3D(this, GL_BGR, GL_UNSIGNED_BYTE, TARGET, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2D(this, ptex, 3, GL_BGR, GL_UNSIGNED_BYTE, TARGET, 3, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
    GL_ERRORCHECK();
    if (NumMips > 3) {
      ptex->TexSamplingMode().PresetTrilinearWrap();
      ApplySamplingMode(ptex);
      // assert(false);
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
    if (bVOLUMETEX)
      Set3DC(this, kRGBA_DXT5, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2DC(this, kRGBA_DXT5, TARGET, li.blockBytes, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
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

    if (bVOLUMETEX)
      Set3DC(this, kRGBA_DXT3, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2DC(this, kRGBA_DXT3, TARGET, li.blockBytes, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
  }
  //////////////////////////////////////////////////////////
  // DXT1: texturing fast path (4 bits per pixel true color)
  //////////////////////////////////////////////////////////
  else if (dds::IsDXT1(ddsh->ddspf)) {
    const dds::DdsLoadInfo& li = dds::loadInfoDXT1;
    int size                   = (iBwidth * iBheight) * li.blockBytes;
    // printf("  tex<%s> DXT1\n", infname.c_str());
    // printf("  tex<%s> size<%d>\n", infname.c_str(), size);
    if (bVOLUMETEX)
      Set3DC(this, kRGBA_DXT1, TARGET, li.blockBytes, NumMips, iwidth, iheight, idepth, req._inpstream); // ireadptr, pdata );
    else
      Set2DC(this, kRGBA_DXT1, TARGET, li.blockBytes, NumMips, iwidth, iheight, req._inpstream); // ireadptr, pdata );
  }
  //////////////////////////////////////////////////////////
  // ???
  //////////////////////////////////////////////////////////
  else {
    OrkAssert(false);
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

bool GlTextureInterface::LoadTexture(Texture* ptex, datablockptr_t datablock) {
  // todo: filetype deduction
  bool ok = LoadDDSTexture(ptex, datablock);
  return ok;
}

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

void GlTextureInterface::SaveTexture(const ork::AssetPath& fname, Texture* ptex) {
}

///////////////////////////////////////////////////////////////////////////////

static auto addrlamb = [](ETextureAddressMode inp) -> GLenum {
  switch (inp) {
    case ETEXADDR_CLAMP:
      return GL_CLAMP_TO_EDGE;
      break;
    case ETEXADDR_WRAP:
      return GL_REPEAT;
      break;
    default:
      return GL_NONE;
      break;
  }
};
//////////////////////////////////////////
static auto magfiltlamb = [](const TextureSamplingModeData& inp) -> GLenum {
  GLenum rval = GL_NEAREST;
  switch (inp.GetFiltModeMag()) {
    case ETEXFILT_POINT:
      rval = GL_NEAREST;
      break;
    case ETEXFILT_LINEAR:
      rval = GL_LINEAR;
      break;
    default:
      break;
  }
  return rval;
};
//////////////////////////////////////////
static auto minfiltlamb = [](const TextureSamplingModeData& inp) -> GLenum {
  GLenum rval = GL_NEAREST;
  switch (inp.GetFiltModeMip()) {
    case ETEXFILT_POINT:
      switch (inp.GetFiltModeMin()) {
        case ETEXFILT_POINT:
          rval = GL_NEAREST;
          break;
        case ETEXFILT_LINEAR:
          rval = GL_LINEAR;
          break;
        default:
          break;
      }
      break;
    case ETEXFILT_LINEAR:
      rval = GL_LINEAR_MIPMAP_LINEAR;
      break;
    default:
      break;
  }
  return rval;
};

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::ApplySamplingMode(Texture* ptex) {
  GLTextureObject* pTEXOBJ = (GLTextureObject*)ptex->GetTexIH();
  if (pTEXOBJ) {
    GLenum tgt = (pTEXOBJ->mTarget != GL_NONE) ? pTEXOBJ->mTarget : GL_TEXTURE_2D;
    mTargetGL.makeCurrentContext();
    mTargetGL.debugPushGroup("ApplySamplingMode");
    GL_ERRORCHECK();
    glBindTexture(tgt, pTEXOBJ->mObject);

    const auto& texmode = ptex->TexSamplingMode();

    // printf( "pTEXOBJ<%p> tgt<%p>\n", pTEXOBJ, (void*)pTEXOBJ->mTarget );

    // assert(pTEXOBJ->mTarget == GL_TEXTURE_2D );

    auto minfilt = minfiltlamb(texmode);

    int inummips = 0;
    if (minfilt == GL_LINEAR_MIPMAP_LINEAR) {
      inummips = pTEXOBJ->_maxmip;
      if (inummips < 3) {
        inummips = 0;
        minfilt  = GL_LINEAR;
      }

      // printf( "linmiplin inummips<%d>\n", inummips );

#if defined(__APPLE__)
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);
#else
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 16.0f);
#endif
    }

    GL_ERRORCHECK();
    glTexParameterf(tgt, GL_TEXTURE_MAG_FILTER, magfiltlamb(texmode));
    GL_ERRORCHECK();
    glTexParameterf(tgt, GL_TEXTURE_MIN_FILTER, minfilt);
    GL_ERRORCHECK();
    glTexParameterf(tgt, GL_TEXTURE_MAX_LEVEL, inummips);
    GL_ERRORCHECK();
    glTexParameterf(tgt, GL_TEXTURE_WRAP_S, addrlamb(texmode.GetAddrModeU()));
    GL_ERRORCHECK();
    glTexParameterf(tgt, GL_TEXTURE_WRAP_T, addrlamb(texmode.GetAddrModeV()));
    GL_ERRORCHECK();
  }
  mTargetGL.debugPopGroup();
}

void GlTextureInterface::generateMipMaps(Texture* ptex) {
  auto plattex = (GLTextureObject*)ptex->_internalHandle;
  glBindTexture(GL_TEXTURE_2D, plattex->mObject);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  int w = ptex->_width;
  int l = highestPowerOfTwo(w);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, l);
  glBindTexture(GL_TEXTURE_2D, 0);
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {
  if (nullptr == ptex->_internalHandle) {
    auto texobj           = new GLTextureObject;
    ptex->_internalHandle = (void*)texobj;
    glGenTextures(1, &texobj->mObject);
  }
  auto pTEXOBJ = (GLTextureObject*)ptex->_internalHandle;

  glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);

  bool size_or_fmt_dirty = (ptex->_width != tid._w) or (ptex->_height != tid._h) or (ptex->_texFormat != tid._format);

  ///////////////////////////////////

  size_t length = tid.computeSize();
  // printf( "UPDATE IMAGE UNC imip<%d> iw<%d> ih<%d> isiz<%d> pbo<%d> mem<%p>\n", imip, iw, ih, isiz2, PBOOBJ, pgfxmem );
  GLuint PBOOBJ = this->_getPBO(length);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBOOBJ);
  GL_ERRORCHECK();
  u32 map_flags = GL_MAP_WRITE_BIT;
  map_flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
  map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
  void* pgfxmem = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, length, map_flags);
  memcpy(pgfxmem, tid._data, length);
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  GL_ERRORCHECK();

  ///////////////////////////////////
  GLenum internalformat, format, type;
  switch (tid._format) {
    case EBUFFMT_RGBA8: {
      internalformat = GL_RGBA8;
      format         = GL_RGBA;
      type           = GL_UNSIGNED_BYTE;
      break;
    }
    case EBUFFMT_RGBA16F: {
      internalformat = GL_RGBA16F;
      format         = GL_RGBA;
      type           = GL_HALF_FLOAT;
      break;
    }
    case EBUFFMT_RGBA32F: {
      internalformat = GL_RGBA32F;
      format         = GL_RGBA;
      type           = GL_FLOAT;
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
  ///////////////////////////////////
  // update texels
  ///////////////////////////////////
  if (size_or_fmt_dirty)
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, tid._w, tid._h, 0, format, type, nullptr);
  else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tid._w, tid._h, format, type, nullptr);
  ///////////////////////////////////
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind pbo
  this->_returnPBO(length, PBOOBJ);
  ///////////////////////////////////

  ptex->_width     = tid._w;
  ptex->_height    = tid._h;
  ptex->_texFormat = tid._format;

  ///////////////////////////////////
  // update texture parameters
  ///////////////////////////////////

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (tid._autogenmips)
    glGenerateMipmap(GL_TEXTURE_2D);

  if (size_or_fmt_dirty) {
    if (tid._autogenmips) {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    } else {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  ///////////////////////////////////

  glBindTexture(GL_TEXTURE_2D, 0);
}

///////////////////////////////////////////////////////////////////////////////

Texture* GlTextureInterface::createFromMipChain(MipChain* from_chain) {
  auto tex             = new Texture;
  tex->_creatingTarget = &mTargetGL;
  tex->_chain          = from_chain;
  tex->_width          = from_chain->_width;
  tex->_height         = from_chain->_height;
  tex->_texFormat      = from_chain->_format;
  tex->_texType        = from_chain->_type;

  assert(tex->_texType == ETEXTYPE_2D);

  GLTextureObject* texobj = new GLTextureObject;
  tex->_internalHandle    = (void*)texobj;
  glGenTextures(1, &texobj->mObject);
  glBindTexture(GL_TEXTURE_2D, texobj->mObject);

  if (from_chain->_debugName.length()) {
    tex->_debugName = from_chain->_debugName;
    mTargetGL.debugLabel(GL_TEXTURE, texobj->mObject, tex->_debugName);
  }

  size_t nummips = from_chain->_levels.size();

  for (size_t l = 0; l < nummips; l++) {
    auto pchl = from_chain->_levels[l];
    switch (from_chain->_format) {
      case EBUFFMT_RGBA32F:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height * sizeof(fvec4));
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA32F, pchl->_width, pchl->_height, 0, GL_RGBA, GL_FLOAT, pchl->_data);
        break;
#if !defined(__APPLE__)
      case EBUFFMT_RGBA_BPTC_UNORM:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_RGBA_BPTC_UNORM, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
      case EBUFFMT_SRGB_ALPHA_BPTC_UNORM:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
      case EBUFFMT_RGBA_ASTC_4X4:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
      case EBUFFMT_SRGB_ASTC_4X4:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
#endif
      default:
        OrkAssert(false);
        break;
    }
  }

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nummips - 1);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glBindTexture(GL_TEXTURE_2D, 0);

  return tex;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
