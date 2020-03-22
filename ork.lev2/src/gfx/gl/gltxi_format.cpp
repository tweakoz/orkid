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

texcfg GetInternalFormat(GLuint fmt, GLuint typ) {
  texcfg rval;
  rval.mInternalFormat = 0;
  rval.mFormat         = fmt;
  rval.mBPP            = 4;
  rval.mNumC           = 4;
  // printf( "fmt<%04x> typ<%04x>\n", fmt, typ );
  switch (fmt) {
    case GL_RGB:
      rval.mInternalFormat = GL_RGB;
      rval.mFormat         = GL_RGB;
      rval.mNumC           = 3;
      rval.mBPP            = 3;
      break;
    case GL_BGR:
      rval.mInternalFormat = GL_RGB;
      rval.mFormat         = GL_BGR;
      rval.mNumC           = 3;
      rval.mBPP            = 3;
      break;
    case GL_BGRA:
    case GL_RGBA:
      rval.mInternalFormat = GL_RGBA;
      rval.mNumC           = 4;
      rval.mBPP            = 4;
      break;
    default:
      break;
  }
  switch (typ) {
    case GL_UNSIGNED_BYTE:
      break;
    case GL_UNSIGNED_SHORT_5_5_5_1:
      rval.mBPP = 2;
      break;
  }
  OrkAssert(rval.mInternalFormat != 0);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Set2D(
    GlTextureInterface* txi,
    Texture* tex,
    GLuint numC,
    GLuint fmt,
    GLuint typ,
    GLuint tgt,
    int BPP,
    int inummips,
    int& iw,
    int& ih,
    DataBlockInputStream inpstream) {

  DataBlockInputStream copy_stream = inpstream;

  // size_t ifilelen       = 0;
  // EFileErrCode eFileErr = file.GetLength(ifilelen);

  int isize = iw * ih * BPP;
  auto glto = (GLTextureObject*)tex->_internalHandle;

  glto->_maxmip = 0;

  int mipbias = 0;
#if defined(__APPLE__) // todo move to gfx user settings
  if (iw >= 4096) {
    // mipbias = 2;
  }
#endif

  for (int imip = 0; imip < inummips; imip++) {
    if (iw < 4)
      continue;
    if (ih < 4)
      continue;

    GLuint nfmt = fmt;

    /////////////////////////////////////////////////
    // allocate space for image
    // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
    // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
    // basically you can call glTexImage2D once with the s3tc format as the internalformat
    //  and a null data ptr to let the driver 'allocate' space for the texture
    //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
    //  this decouples allocation from writing, allowing you to overwrite more efficiently
    /////////////////////////////////////////////////

    GLint intfmt = 0;
    // printf( "fmt<%04x>\n", fmt );
    int isiz2 = isize;
    // printf( "numC<%d> typ<%04x>\n", numC, typ );
    switch (nfmt) {
      case GL_BGR:
        intfmt = GL_RGB;
        nfmt   = GL_BGR;
        break;
      case GL_BGRA:
        if ((numC == 4) && (typ == GL_UNSIGNED_BYTE))
          intfmt = GL_RGBA;
        break;
      case GL_RGBA:
        if (numC == 4)
          switch (typ) {
            case GL_UNSIGNED_SHORT_5_5_5_1:
            case GL_UNSIGNED_BYTE:
              intfmt = GL_RGBA;
              break;
            default:
              assert(false);
          }
        break;
      case GL_RGB:
        if ((numC == 3) && (typ == GL_UNSIGNED_BYTE))
          intfmt = GL_RGB;
        break;
      default:
        break;
    }
    OrkAssert(intfmt != 0);

    // printf( "tgt<%04x> imip<%d> intfmt<%04x> w<%d> h<%d> isiz2<%d> fmt<%04x> typ<%04x>\n", tgt, imip, intfmt,
    // iw,ih,isiz2,nfmt,typ);
    GL_ERRORCHECK();

    bool bUSEPBO = false;

    if (bUSEPBO) {
      glTexImage2D(tgt, imip, intfmt, iw, ih, 0, nfmt, typ, 0);
      GL_ERRORCHECK();

      /////////////////////////////
      // imgdata->PBO
      /////////////////////////////

      const GLuint PBOOBJ = txi->_getPBO(isiz2);

      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBOOBJ);
      GL_ERRORCHECK();

      u32 map_flags = GL_MAP_WRITE_BIT;
      map_flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
      map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
      void* pgfxmem = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, isiz2, map_flags);
      // printf( "UPDATE IMAGE UNC imip<%d> iw<%d> ih<%d> isiz<%d> pbo<%d> mem<%p>\n", imip, iw, ih, isiz2, PBOOBJ, pgfxmem );
      auto copy_src = inpstream.current();
      memcpy(pgfxmem, copy_src, isiz2);
      inpstream.advance(isiz2);
      glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
      GL_ERRORCHECK();

      ////////////////////////
      // PBO->texture
      ////////////////////////

      glTexSubImage2D(tgt, imip, 0, 0, iw, ih, nfmt, typ, 0);

      GL_ERRORCHECK();

      ////////////////////////
      // unbind the PBO
      ////////////////////////

      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

      txi->_returnPBO(isiz2, PBOOBJ);
      GL_ERRORCHECK();
    } else // not PBO
    {
      auto pgfxmem = inpstream.current();
      inpstream.advance(isiz2);
      if (imip >= mipbias) {
        glTexImage2D(tgt, imip - mipbias, intfmt, iw, ih, 0, nfmt, typ, pgfxmem);
        glto->_maxmip = imip - mipbias;
      }
      GL_ERRORCHECK();
    }

    ////////////////////////

    // irdptr+=isize;
    // pimgdata = & dataBASE[irdptr];

    iw >>= 1;
    ih >>= 1;
    isize = iw * ih * BPP;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Set3D(
    GlTextureInterface* txi,
    /*GLuint numC,*/ GLuint fmt,
    GLuint typ,
    GLuint tgt,
    /*int BPP,*/ int inummips,
    int& iw,
    int& ih,
    int& id,
    DataBlockInputStream inpstream) //, int& irdptr, const u8* dataBASE )
{
  for (int imip = 0; imip < inummips; imip++) {
    /////////////////////////////////////////////////
    // allocate space for image
    // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
    // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
    // basically you can call glTexImage2D once with the s3tc format as the internalformat
    //  and a null data ptr to let the driver 'allocate' space for the texture
    //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
    //  this decouples allocation from writing, allowing you to overwrite more efficiently
    /////////////////////////////////////////////////

    texcfg tc = GetInternalFormat(fmt, typ);

    /////////////////////////////
    // imgdata->PBO
    /////////////////////////////

    int isize = id * iw * ih * tc.mBPP;

    // printf( "UPDATE IMAGE 3dUNC imip<%d> iw<%d> ih<%d> id<%d> isiz<%d>\n", imip, iw, ih, id, isize );
    GL_ERRORCHECK();
    auto pgfxmem = inpstream.current();
    glTexImage3D(tgt, imip, tc.mInternalFormat, iw, ih, id, 0, tc.mFormat, typ, pgfxmem);
    inpstream.advance(isize);
    GL_ERRORCHECK();
    /////////////////////////////

    iw >>= 1;
    ih >>= 1;
    id >>= 1;
    // irdptr+=isize;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Set2DC(
    GlTextureInterface* txi,
    GLuint fmt,
    GLuint tgt,
    int BPP,
    int inummips,
    int& iw,
    int& ih,
    DataBlockInputStream inpstream) //, int& irdptr, const u8* dataBASE )
{
  for (int imip = 0; imip < inummips; imip++) {
    int iBwidth  = (iw + 3) / 4;
    int iBheight = (ih + 3) / 4;
    int isize    = (iBwidth * iBheight) * BPP;
    // const u8* pimgdata = & dataBASE[irdptr];
    // irdptr+=isize;

    /////////////////////////////////////////////////
    // allocate space for image
    // see http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Board=3&Number=159972
    // and http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=240547
    // basically you can call glTexImage2D once with the s3tc format as the internalformat
    //  and a null data ptr to let the driver 'allocate' space for the texture
    //  then use the glCompressedTexSubImage2D to overwrite the data in the pre allocated space
    //  this decouples allocation from writing, allowing you to overwrite more efficiently
    /////////////////////////////////////////////////

    bool hasalpha = (fmt == kRGBA_DXT5) || (fmt == kRGBA_DXT3);
    GLenum extfmt = hasalpha ? GL_RGBA : GL_RGB;

    // printf("alloc texdata tgt<%d> imip<%d> fmt<%d> iw<%d> ih<%d> extfmt<%d>\n",
    //	tgt,imip,fmt,iw,ih,extfmt );

    // static const int kloadbufsize = 16 << 20;
    // static void* gloadbuf         = malloc(kloadbufsize);
    // OrkAssert(isize < kloadbufsize);
    auto copy_src = inpstream.current();
    // memcpy(gloadbuf, copy_src, isize);
    inpstream.advance(isize);
    GL_ERRORCHECK();
    glCompressedTexImage2D(tgt, imip, fmt, iw, ih, 0, isize, copy_src);

    GL_ERRORCHECK();
    ////////////////////////

    iw >>= 1;
    ih >>= 1;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Set3DC(
    GlTextureInterface* txi,
    GLuint fmt,
    GLuint tgt,
    int BPP,
    int inummips,
    int& iw,
    int& ih,
    int& id,
    DataBlockInputStream inpstream) //, int& irdptr, const u8* dataBASE )
{
  for (int imip = 0; imip < inummips; imip++) {
    int iBwidth  = (iw + 3) / 4;
    int iBheight = (ih + 3) / 4;
    int isize    = id * (iBwidth * iBheight) * BPP;
    // const u8* pimgdata = & dataBASE[irdptr];
    // printf( "READ3DT iw<%d> ih<%d> id<%d> isize<%d>\n", iw, ih, id, isize );
    // irdptr+=isize;

    if (isize) {
      // glCompressedTexImage3D(	tgt,
      //						imip,
      //						fmt,
      //						iw, ih, id,
      //						0,
      //						isize,
      //						pimgdata );
      // GL_ERRORCHECK();
    }
    iw >>= 1;
    ih >>= 1;
    id >>= 1;
  }
}

} // namespace ork::lev2
