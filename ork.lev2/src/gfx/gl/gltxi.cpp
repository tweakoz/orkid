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

void GlTextureInterface::bindTextureToUnit(const Texture* tex,
                                           GLenum tex_target,
                                           int tex_unit){

    auto tex_obj = (const GLTextureObject*) tex->_internalHandle;
    if(nullptr == tex_obj){

      auto new_tex_obj = new GLTextureObject;
      tex->_internalHandle = new_tex_obj;
      tex_obj = new_tex_obj;

      /////////////////////////////////////////////////////////////
      // assign default texture object (0) to start out with
      /////////////////////////////////////////////////////////////

      new_tex_obj->mObject = 0;

      /////////////////////////////////////////////////////////////
      // check to see if we are referencing external memory objects
      //  if we are and tex_obj is null - we need to create the 
      //  GL texture referencing the external memory...
      /////////////////////////////////////////////////////////////
#if defined(LINUX)
      auto ipcdata = tex->_external_memory;
      if( ipcdata ){
        
        GLuint mem_object = 0;
        glCreateMemoryObjectsEXT(1,&mem_object);

        printf("MEMOBJECT<%d> fd<%d> w<%d> h<%d> size<%zu>\n", 
               int(mem_object),
               ipcdata->_image_fd,
               ipcdata->_image_width,
               ipcdata->_image_height,
               ipcdata->_image_size );

        GL_ERRORCHECK();

        GLint is_dedicated = GL_FALSE;
        glMemoryObjectParameterivEXT(mem_object,
                                     GL_DEDICATED_MEMORY_OBJECT_EXT,
                                     &is_dedicated);

        GL_ERRORCHECK();

        glImportMemoryFdEXT(mem_object, // mem object
                            ipcdata->_image_size, // image size
                            GL_HANDLE_TYPE_OPAQUE_FD_EXT, // handle type
                            ipcdata->_image_fd); // file descriptor

        GL_ERRORCHECK();

        glCreateTextures(GL_TEXTURE_2D,1,&new_tex_obj->mObject);
        glBindTexture(GL_TEXTURE_2D, new_tex_obj->mObject);

        GL_ERRORCHECK();

        glTexStorageMem2DEXT(GL_TEXTURE_2D, // texture target
                             1, // mip count
                             GL_RGB10_A2, // internal format
                             ipcdata->_image_width, // width
                             ipcdata->_image_height, // height
                             mem_object, // mem object
                             0); // offset into memobject's data

        GL_ERRORCHECK();
        glBindTexture(GL_TEXTURE_2D, 0);
        GL_ERRORCHECK();

      	//int _sema_complete_fd = 0;
	      //int _sema_ready_fd = 0;

        //OrkAssert(false);
      }
#endif
      /////////////////////////////////////////////////////////////
    }

    GLuint texID                   = tex_obj->mObject;


    /*printf(
        "Bind3 ISDEPTH<%d> tex<%p> texobj<%d> tex_unit<%d> textgt<% d>\n ",
        int(pTex->_isDepthTexture),
        pTex,
        texID,
        tex_unit,
        int(textgt));*/

    GL_ERRORCHECK();
    glActiveTexture(GL_TEXTURE0 + tex_unit);
    GL_ERRORCHECK();
    glBindTexture(tex_target, texID);
    GL_ERRORCHECK();

}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadTexture(const AssetPath& infname, Texture* ptex) {
  AssetPath DdsFilename = infname;
  AssetPath PngFilename = infname;
  AssetPath XtxFilename = infname;
  DdsFilename.SetExtension("dds");
  PngFilename.SetExtension("png");
  XtxFilename.SetExtension("xtx");
  ptex->_debugName = infname.toStdString();
  AssetPath final_fname;
  if (FileEnv::GetRef().DoesFileExist(PngFilename))
    final_fname = PngFilename;
  if (FileEnv::GetRef().DoesFileExist(DdsFilename))
    final_fname = DdsFilename;
  if (FileEnv::GetRef().DoesFileExist(XtxFilename))
    final_fname = XtxFilename;

   printf("infname<%s>\n", infname.c_str());
   printf("final_fname<%s>\n", final_fname.c_str());

  if (auto dblock = datablockFromFileAtPath(final_fname))
    return LoadTexture(ptex, dblock);
  else
    return false;
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadTexture(Texture* ptex, datablock_ptr_t datablock) {
  DataBlockInputStream checkstream(datablock);
  uint32_t magic = checkstream.getItem<uint32_t>();
  bool ok        = false;
  if (Char4("chkf") == Char4(magic))
    ok = _loadXTXTexture(ptex, datablock);
  else if (Char4("DDS ") == Char4(magic))
    ok = _loadDDSTexture(ptex, datablock);
  else
    ok = _loadImageTexture(ptex, datablock);

  ptex->_contentHash = datablock->hash();

  return ok;
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
  // opq::mainSerialQueue()->push(lamb,get_backtrace());
  opq::mainSerialQueue()->enqueue(lamb);
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
  for (pboptr_t item : _pbos_perm) {

    glDeleteBuffers(1, &item->_handle);
  }
}

///////////////////////////////////////////////////////////////////////////////

pboptr_t PboSet::alloc() {

  if (_pbos.empty()) {

    for( int i=0; i<4; i++ ) {

      auto new_pbo = std::make_shared<PboItem>();

      new_pbo->_length = _size;

      GL_ERRORCHECK();
      glGenBuffers(1, &new_pbo->_handle);
      GL_ERRORCHECK();
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, new_pbo->_handle);
      GL_ERRORCHECK();
      #if defined(ORK_OSX)
       glBufferData(GL_PIXEL_UNPACK_BUFFER, _size, NULL, GL_STREAM_DRAW);
      #else
       u32 create_flags = GL_MAP_WRITE_BIT;
      
      create_flags |= GL_MAP_PERSISTENT_BIT;
      create_flags |= GL_MAP_COHERENT_BIT;
      glBufferStorage( GL_PIXEL_UNPACK_BUFFER, _size,  nullptr, create_flags );
      GL_ERRORCHECK();
      u32 map_flags = GL_MAP_WRITE_BIT;
      map_flags |= GL_MAP_PERSISTENT_BIT;
      map_flags |= GL_MAP_INVALIDATE_RANGE_BIT;
      map_flags |= GL_MAP_COHERENT_BIT;
      //map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
      new_pbo->_mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _size, map_flags);
      #endif
      //new_pbo->_mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
      GL_ERRORCHECK();
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      //GL_ERRORCHECK();
      _pbos.push(new_pbo);
      _pbos_perm.insert(new_pbo);
    }
    // printf("AllocPBO objid<%d> size<%zu>\n", int(rval), _size);
  }

  auto rval = _pbos.front();
  _pbos.pop();

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void PboSet::free(pboptr_t item) {
  _pbos.push(item);
}

///////////////////////////////////////////////////////////////////////////////

pboptr_t GlTextureInterface::_getPBO(size_t isize) {
  pbosetptr_t pbs = nullptr;
  auto it     = _pbosets.find(isize);
  if (it == _pbosets.end()) {
    pbs             = std::make_shared<PboSet>(isize);
    _pbosets[isize] = pbs;
  } else {
    pbs = it->second;
  }
  return pbs->alloc();
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::_returnPBO(pboptr_t pbo) {
  auto it = _pbosets.find(pbo->_length);
  OrkAssert(it != _pbosets.end());
  pbosetptr_t pbs = it->second;
  pbs->free(pbo);
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

void GlTextureInterface::SaveTexture(const ork::AssetPath& fname, Texture* ptex) {
}

///////////////////////////////////////////////////////////////////////////////

static auto addrlamb = [](TextureAddressMode inp) -> GLenum {
  switch (inp) {
    case TextureAddressMode::CLAMP:
      return GL_CLAMP_TO_EDGE;
      break;
    case TextureAddressMode::WRAP:
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

  ///////////////////////////////////

  size_t length = tid.computeSize();
  auto pboitem = this->_getPBO(length);
  auto mapped = pboitem->_mapped;
  if(mapped){
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboitem->_handle);
    memcpy_fast(mapped, tid._data, length);
  }
  else{
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboitem->_handle);
    GL_ERRORCHECK();
    u32 map_flags = GL_MAP_WRITE_BIT;
    map_flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
    map_flags |= GL_MAP_INVALIDATE_RANGE_BIT;
    map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
    void* pgfxmem = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, length, map_flags);
     printf("UPDATE IMAGE UNC iw<%d> ih<%d> length<%zu> pbo<%d> mem<%p>\n", tid._w, tid._h, length, pboitem->_handle, pgfxmem);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    GL_ERRORCHECK();
  }

///////////////////////////////////

  GLTextureObject* pTEXOBJ = nullptr;
  if (nullptr == ptex->_internalHandle) {
    pTEXOBJ               = new GLTextureObject;
    ptex->_internalHandle = (void*)pTEXOBJ;
    glGenTextures(1, &pTEXOBJ->mObject);
    glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);
    if (ptex->_debugName.length()) {
      mTargetGL.debugLabel(GL_TEXTURE, pTEXOBJ->mObject, ptex->_debugName);
    }
  } else {
    pTEXOBJ = (GLTextureObject*)ptex->_internalHandle;
    glBindTexture(GL_TEXTURE_2D, pTEXOBJ->mObject);
  }

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
    case EBufferFormat::RGBA16UI: {
      internalformat = GL_RGBA16UI;
      format         = GL_RGBA_INTEGER;
      type           = GL_UNSIGNED_SHORT;
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
  bool size_or_fmt_dirty = (ptex->_width != tid._w) or
                           (ptex->_height != tid._h) or
                           (ptex->_texFormat != tid._format);

  if (size_or_fmt_dirty)
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, tid._w, tid._h, 0, format, type, nullptr);
  else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tid._w, tid._h, format, type, nullptr);
  ///////////////////////////////////
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind pbo
  this->_returnPBO(pboitem);
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
