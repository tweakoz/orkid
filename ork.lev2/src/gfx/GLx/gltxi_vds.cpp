////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
///////////////////////////////////////////////////////////////////////////////
VdsTextureAnimation::~VdsTextureAnimation() {
  delete mpFile;
  for (int i = 0; i < kframecachesize; i++) {
    free(mFrameBuffers[i]);
    mFrameBuffers[i] = 0;
  }
}
///////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////
void VdsTextureAnimation::UpdateTexture(TextureInterface* txi, lev2::Texture* ptex, TextureAnimationInst* pinst) {

  auto glto = ptex->_impl.get<gltexobj_ptr_t>();

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

    glBindTexture(GL_TEXTURE_2D, glto->mObject);

    /////////////////////////////
    // imgdata->PBO
    /////////////////////////////

    // printf( "UPDATE IMAGE UNC iw<%d> ih<%d> to<%d>\n", miW, miH, int(glto->mObject) );

    auto pbo = pgltxi->_getPBO(miFrameBaseSize);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->_handle);
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

    pgltxi->_returnPBO(pbo);
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

    glBindTexture(GL_TEXTURE_2D, glto->mObject);

    /////////////////////////////
    // imgdata->PBO
    /////////////////////////////

    // printf( "UPDATE IMAGE UNC iw<%d> ih<%d> to<%d>\n", miW, miH, int(glto->mObject) );

    auto pbo = pgltxi->_getPBO(miFrameBaseSize);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->_handle);
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
    pgltxi->_returnPBO(pbo);
  }
}
///////////////////////////////////////////////////////////////////////////////
float VdsTextureAnimation::GetLengthOfTime() const {
  return 10.0f;
}
///////////////////////////////////////////////////////////////////////////////
bool GlTextureInterface::_loadVDSTexture(const AssetPath& infname, texture_ptr_t ptex) {

  auto glto = ptex->_impl.makeShared<GLTextureObject>(this);

  glGenTextures(1, &glto->mObject);

  VdsTextureAnimation* vta = new VdsTextureAnimation(infname);
  ptex->SetTexAnim(vta);

  ptex->_width  = vta->miW;
  ptex->_height = vta->miH;
  ptex->_depth  = 1;

  glBindTexture(GL_TEXTURE_2D, glto->mObject);

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
} // namespace ork::lev2
