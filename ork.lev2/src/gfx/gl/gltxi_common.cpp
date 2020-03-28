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

bool GlTextureInterface::LoadTexture(Texture* ptex, datablockptr_t datablock) {
  DataBlockInputStream checkstream(datablock);
  uint32_t magic = checkstream.getItem<uint32_t>();
  bool ok        = false;
  if (Char4("chkf") == Char4(magic)) {
    ok = LoadXTXTexture(ptex, datablock);
  } else {
    ok = LoadDDSTexture(ptex, datablock);
  }

  return ok;
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
    case EBufferFormat::RGBA8: {
      internalformat = GL_RGBA8;
      format         = GL_RGBA;
      type           = GL_UNSIGNED_BYTE;
      break;
    }
    case EBufferFormat::RGBA16F: {
      internalformat = GL_RGBA16F;
      format         = GL_RGBA;
      type           = GL_HALF_FLOAT;
      break;
    }
    case EBufferFormat::RGBA32F: {
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
      case EBufferFormat::RGBA32F:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height * sizeof(fvec4));
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA32F, pchl->_width, pchl->_height, 0, GL_RGBA, GL_FLOAT, pchl->_data);
        break;
#if !defined(__APPLE__)
      case EBufferFormat::RGBA_BPTC_UNORM:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_RGBA_BPTC_UNORM, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
      case EBufferFormat::SRGB_ALPHA_BPTC_UNORM:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
      case EBufferFormat::RGBA_ASTC_4X4:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, l, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, pchl->_width, pchl->_height, 0, pchl->_length, pchl->_data);
        break;
      case EBufferFormat::SRGB_ASTC_4X4:
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
