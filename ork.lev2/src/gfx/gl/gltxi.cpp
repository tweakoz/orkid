////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/gfx/dds.h>
#include <ork/kernel/debug.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/math/misc_math.h>
#include <ork/pch.h>
#include <ork/util/logger.h>

#include <ork/kernel/memcpy.inl>
#include <ork/profiling.inl>

#include "gl.h"

GLuint gLastBoundNonZeroTex = 0;

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
  GLFormatTriplet::GLFormatTriplet(EBufferFormat inp){
    switch (inp) {
      case EBufferFormat::RGB8: {
        _internalFormat = GL_RGB8;
        _format         = GL_RGB;
        _type           = GL_UNSIGNED_BYTE;
        break;
      }
      case EBufferFormat::BGR8: {
        _internalFormat = GL_RGB8;
        _format         = GL_BGR;
        _type           = GL_UNSIGNED_BYTE;
        break;
      }
        case EBufferFormat::BGRA8: {
          _internalFormat = GL_RGBA8;
          _format         = GL_BGRA;
          _type           = GL_UNSIGNED_BYTE;
          break;
        }
      case EBufferFormat::RGBA8: {
        _internalFormat = GL_RGBA8;
        _format         = GL_RGBA;
        _type           = GL_UNSIGNED_BYTE;
        break;
      }
      case EBufferFormat::RGBA16F: {
        _internalFormat = GL_RGBA16F;
        _format         = GL_RGBA;
        _type           = GL_HALF_FLOAT;
        break;
      }
      case EBufferFormat::RGBA16UI: {
        _internalFormat = GL_RGBA16UI;
        _format         = GL_RGBA_INTEGER;
        _type           = GL_UNSIGNED_SHORT;
        break;
      }
      case EBufferFormat::RGBA32F: {
        _internalFormat = GL_RGBA32F;
        _format         = GL_RGBA;
        _type           = GL_FLOAT;
        break;
      }
      case EBufferFormat::RGB32F: {
        _internalFormat = GL_RGB32F;
        _format         = GL_RGB;
        _type           = GL_FLOAT;
        break;
      }
      case EBufferFormat::R32F: {
        _internalFormat = GL_R32F;
        _format         = GL_RED;
        _type           = GL_FLOAT;
        break;
      }
      case EBufferFormat::R16: {
        _internalFormat = GL_R16UI;
        _format         = GL_RED_INTEGER;
        _type           = GL_UNSIGNED_SHORT;
        break;
      }
      case EBufferFormat::R8: {
        _internalFormat = GL_R8;
        _format         = GL_RED;
        _type           = GL_UNSIGNED_BYTE;
        break;
      }
#if ! defined(__APPLE__)
      case EBufferFormat::RGBA_BPTC_UNORM: {
        _internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
        _format         = GL_RGBA;
        _type           = GL_UNSIGNED_BYTE;
        break;
      }
#endif
      default:
        OrkAssert(false);
        break;
    }
  }
///////////////////////////////////////////////////////////////////////////////


std::atomic<size_t> GLTextureObject::_glto_count = 0;

GLTextureObject::GLTextureObject(GlTextureInterface* txi)
    : _txi(txi)
    , _textureObject(0)
    , mFbo(0)
    , mDbo(0)
    , mTarget(GL_NONE) {
  _glto_count.fetch_add(1);
  // printf( "create glto_count: %zu\n", _glto_count.load() );
}

///////////////////////////////////////////////////////////////////////////////

GLTextureObject::~GLTextureObject() {
  _glto_count.fetch_add(-1);
  // printf( "destroy glto_count: %zu\n", _glto_count.load() );
  if (_textureObject != 0) {
    glDeleteTextures(1, &_textureObject);
  }
}

///////////////////////////////////////////////////////////////////////////////

static ork::Timer _proftimer;

GlTextureInterface::GlTextureInterface(ContextGL& tgt)
    : mTargetGL(tgt) {
  _proftimer.Start();
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::bindTextureToUnit(const Texture* tex, int loc, GLenum tex_target, int tex_unit) {
  gltexobj_ptr_t tex_obj;

  if (tex->_impl.isA<gltexobj_ptr_t>()) {
    tex_obj = tex->_impl.get<gltexobj_ptr_t>();
    ;
  } else {
    tex_obj       = tex->_impl.makeShared<GLTextureObject>(this);
    tex_obj->_txi = this;

    /////////////////////////////////////////////////////////////
    // assign default texture object (0) to start out with
    /////////////////////////////////////////////////////////////

    tex_obj->_textureObject = 0;

    /////////////////////////////////////////////////////////////
    // check to see if we are referencing external memory objects
    //  if we are and tex_obj is null - we need to create the
    //  GL texture referencing the external memory...
    /////////////////////////////////////////////////////////////

#if defined(LINUX) and defined(OPENGL_46)
    auto ipcdata = tex->_external_memory;
    if (ipcdata) {
      GLuint mem_object = 0;
      glCreateMemoryObjectsEXT(1, &mem_object);

      /*printf("ME_textureObject<%d> fd<%d> w<%d> h<%d> size<%zu>\n",
             int(mem_object),
             ipcdata->_image_fd,
             ipcdata->_image_width,
             ipcdata->_image_height,
             ipcdata->_image_size );*/

      GL_ERRORCHECK();

      GLint is_dedicated = GL_FALSE;
      glMemoryObjectParameterivEXT(mem_object, GL_DEDICATED_MEMORY_OBJECT_EXT, &is_dedicated);

      GL_ERRORCHECK();

      glImportMemoryFdEXT(
          mem_object,                   // mem object
          ipcdata->_image_size,         // image size
          GL_HANDLE_TYPE_OPAQUE_FD_EXT, // handle type
          ipcdata->_image_fd);          // file descriptor

      GL_ERRORCHECK();

      glCreateTextures(tex_target, 1, &tex_obj->_textureObject);
      glBindTexture(tex_target, tex_obj->_textureObject);

      GL_ERRORCHECK();

      glTexParameteri(tex_target, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
      glTexParameterf(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameterf(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(tex_target, GL_TEXTURE_MAX_LEVEL, 0);
      glTexParameterf(tex_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameterf(tex_target, GL_TEXTURE_WRAP_T, GL_REPEAT);

      GL_ERRORCHECK();

      glTexStorageMem2DEXT(
          tex_target,             // texture target
          1,                      // mip count
          GL_RGB10_A2,            // internal format
          ipcdata->_image_width,  // width
          ipcdata->_image_height, // height
          mem_object,             // mem object
          0);                     // offset into me_textureObject's data

      GL_ERRORCHECK();
      glBindTexture(tex_target, 0);
      GL_ERRORCHECK();

      // int _sema_complete_fd = 0;
      // int _sema_ready_fd = 0;

      // OrkAssert(false);
    }
#endif // defined(LINUX)

    /////////////////////////////////////////////////////////////
  }

  GLuint texID = tex_obj->_textureObject;

  //if (texID != 0) {
    //_checkTexture(texID, "");
 // }

  if (0) {
    auto fxi       = mTargetGL.FXI();
    auto container = fxi->activeShader()->_internalHandle.get<glslfx::rootcontainer_ptr_t>();
    auto pass      = container->_activePass;

    // get texname
    std::string texname = tex->_debugName;

    printf(
        "Bind3 pass<%s> loc<%d> unit<%d> obj<%d> tgt<%d> tex<%p:%s> \n",
        pass->_name.c_str(),
        loc,
        tex_unit,
        texID,
        int(tex_target),
        tex,
        texname.c_str());
  }

  GL_ERRORCHECK();
  glActiveTexture(GL_TEXTURE0 + tex_unit);
  GL_ERRORCHECK();
  glBindTexture(tex_target, texID);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadTexture(const AssetPath& infname, texture_ptr_t ptex) {
  AssetPath DdsFilename = infname;
  AssetPath PngFilename = infname;
  AssetPath XtxFilename = infname;
  DdsFilename.setExtension("dds");
  PngFilename.setExtension("png");
  XtxFilename.setExtension("xtx");
  ptex->_debugName = infname.toStdString();
  AssetPath final_fname;
  if (FileEnv::GetRef().DoesFileExist(PngFilename))
    final_fname = PngFilename;
  if (FileEnv::GetRef().DoesFileExist(DdsFilename))
    final_fname = DdsFilename;
  if (FileEnv::GetRef().DoesFileExist(XtxFilename))
    final_fname = XtxFilename;

  //printf("infname<%s>\n", infname.c_str());
  //printf("final_fname<%s>\n", final_fname.c_str());

  if (auto dblock = datablockFromFileAtPath(final_fname))
    return LoadTexture(ptex, dblock);
  else
    return false;
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::LoadTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  DataBlockInputStream checkstream(datablock);
  uint32_t magic = checkstream.getItem<uint32_t>();
  bool ok        = false;
  ptex->_contentHash = datablock->hash();
  if (Char4("chkf") == Char4(magic))
    ok = _loadXTXTexture(ptex, datablock);
  else if (Char4("DDS ") == Char4(magic))
    ok = _loadDDSTexture(ptex, datablock);
  else
    ok = _loadImageTexture(ptex, datablock);
  return ok;
}

///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::destroyTexture(texture_ptr_t tex) {
  auto glto = tex->_impl.get<gltexobj_ptr_t>();
  tex->_impl.set<void*>(nullptr);

  void_lambda_t lamb = [=]() {
    if (glto) {
      if (glto->_textureObject != 0)
        glDeleteTextures(1, &glto->_textureObject);
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
      //printf( "New PboSet size<%zu>\n", size );
}

///////////////////////////////////////////////////////////////////////////////

PboSet::~PboSet() {
  for (pboptr_t item : _pbos_perm) {
    glDeleteBuffers(1, &item->_handle);
  }
}

///////////////////////////////////////////////////////////////////////////////

static std::atomic<int> ipbocount = 0;
pboptr_t PboSet::alloc(GlTextureInterface* txi) {
  if (_pbos.empty()) {

    #if defined(__APPLE__)
    constexpr int num_pbos_per_set = 2;
    #else
    constexpr int num_pbos_per_set = 4;
    #endif
    for (int i = 0; i < num_pbos_per_set; i++) {
      auto new_pbo = std::make_shared<PboItem>();

      new_pbo->_length = _size;

      GL_ERRORCHECK();
      glGenBuffers(1, &new_pbo->_handle);
      GL_ERRORCHECK();
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, new_pbo->_handle);
      GL_ERRORCHECK();

      // printf("GlTextureInterface:: ipbocount<%d>\n", ipbocount.fetch_add(1));

      // ??? persistent mapped objects

#if defined(OPENGL_46)
      if (txi->mTargetGL._SUPPORTS_PERSISTENT_MAP) {
        u32 create_flags = GL_MAP_WRITE_BIT;
        create_flags |= GL_MAP_PERSISTENT_BIT;
        create_flags |= GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_PIXEL_UNPACK_BUFFER, _size, nullptr, create_flags);
        GL_ERRORCHECK();
        u32 map_flags = GL_MAP_WRITE_BIT;
        map_flags |= GL_MAP_PERSISTENT_BIT;
        map_flags |= GL_MAP_INVALIDATE_RANGE_BIT;
        map_flags |= GL_MAP_COHERENT_BIT;
        // map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
        new_pbo->_mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _size, map_flags);
      } else {
        glBufferData(GL_PIXEL_UNPACK_BUFFER, _size, NULL, GL_STREAM_DRAW);
      }
#else
      glBufferData(GL_PIXEL_UNPACK_BUFFER, _size, NULL, GL_STREAM_DRAW);
#endif

      GL_ERRORCHECK();
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      // GL_ERRORCHECK();
      _pbos.push(new_pbo);
      _pbos_perm.insert(new_pbo);
      // printf("AllocPBO objid<%d> size<%zu> mapped<%p>\n",
      // int(new_pbo->_handle), _size, (void*) new_pbo->_mapped);
    }
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
  size_t npot     = nextPowerOfTwo(isize);
  auto it         = _pbosets.find(npot);
  if (it == _pbosets.end()) {
    pbs            = std::make_shared<PboSet>(npot);
    _pbosets[npot] = pbs;
  } else {
    pbs = it->second;
  }
  return pbs->alloc(this);
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
  // printf( "GlTextureInterface::UpdateAnimatedTexture( ptex<%p> tai<%p> )\n",
  // ptex, tai );
  auto glto = ptex->_impl.get<gltexobj_ptr_t>();
  if (glto && ptex->GetTexAnim()) {
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

extern std::atomic<int> __FIND_IT;

void GlTextureInterface::ApplySamplingMode(Texture* ptex) {
  int numsamples = msaaEnumToInt(ptex->_msaa_samples);
  if (numsamples > 1)
    return;

  auto glto = ptex->_impl.get<gltexobj_ptr_t>();
  if (glto) {
    GLenum tgt = (glto->mTarget != GL_NONE) ? glto->mTarget : GL_TEXTURE_2D;

    if (tgt == GL_TEXTURE_CUBE_MAP) {
      return;
    }
    if (tgt == GL_TEXTURE_2D_ARRAY) {
      return;
    }

    mTargetGL.makeCurrentContext();
    __FIND_IT.store(1);
    mTargetGL.debugPushGroup("ApplySamplingMode");
    GL_ERRORCHECK();

    const auto& texmode = ptex->TexSamplingMode();

    // printf( "glto<%p> tgt<%p>\n", glto, (void*)glto->mTarget );

    // assert(glto->mTarget == GL_TEXTURE_2D );

    auto minfilt = minfiltlamb(texmode);

    int inummips = glto->_maxmip;
    if (minfilt == GL_LINEAR_MIPMAP_LINEAR) {
      if (inummips < 3) {
        inummips = 0;
        minfilt  = GL_LINEAR;
      }

      // printf( "linmiplin inummips<%d>\n", inummips );
    }

    GL_ERRORCHECK();
    // sampler state..
    glBindTexture(tgt, glto->_textureObject);
#if defined(OPENGL_41)
    glTexParameterf(tgt, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16.0f);
#elif defined(OPENGL_46)
    glTexParameterf(tgt, GL_TEXTURE_MAX_ANISOTROPY, 16.0f);
#endif
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

    mTargetGL.debugPopGroup();
  }
}

void GlTextureInterface::generateMipMaps(Texture* ptex) {
  auto glto = ptex->_impl.get<gltexobj_ptr_t>();
  auto gltgt = glto->mTarget;
  GL_ERRORCHECK();

  switch(gltgt){
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP:
      break;
    default:
      OrkAssert(false);
  }

  glBindTexture(gltgt, glto->_textureObject);
  glGenerateMipmap(gltgt);
  GL_ERRORCHECK();
  glTexParameterf(gltgt, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(gltgt, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  GL_ERRORCHECK();
  int w = ptex->_width;
  int l = highestPowerOfTwo(w);
  glTexParameterf(gltgt, GL_TEXTURE_MAX_LEVEL, l);
  GL_ERRORCHECK();
  glBindTexture(gltgt, 0);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

#if defined(OPENGL_46)

void PboItem::copyPersistentMapped(const TextureInitData& tid, size_t length, const void* src_data) {
  auto mapped   = _mapped;
  size_t pbolen = _length;
  GL_ERRORCHECK();
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _handle);
  GL_ERRORCHECK();
  if (tid._truncation_length != 0) {
    if (tid._truncation_length > pbolen) {
      logerrchannel()->log(
          "ERROR: PBO overflow trunclen<%zu> pbolen<%zu>", //
          tid._truncation_length,                          //
          pbolen);
      OrkAssert(false);
    }
  }
  memcpy_fast(mapped, src_data, length);
}

#endif

///////////////////////////////////////////////////////////////////////////////

void PboItem::copyWithTempMapped(const TextureInitData& tid, size_t length, const void* src_data) {
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _handle);
  if(0){
    glBufferData(GL_PIXEL_UNPACK_BUFFER,_length,src_data,GL_DYNAMIC_DRAW); // orphan ?
  }
  else{
    glBufferData(GL_PIXEL_UNPACK_BUFFER,_length,nullptr,GL_STREAM_DRAW); // orphan ?
    GL_ERRORCHECK();
    u32 map_flags = GL_MAP_WRITE_BIT;
    map_flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
    map_flags |= GL_MAP_INVALIDATE_RANGE_BIT;
    map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
    void* mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, length, map_flags);
    memcpy_fast(mapped, src_data, length);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    GL_ERRORCHECK();
  }
}

bool _checkTexture(GLuint texID, const std::string& name) {
    if (texID == 0) {
        std::cout << "Texture ID is zero, which is not a valid texture object for " << name << "." << std::endl;
        OrkAssert(false);
        return false;
    }

    GLint prevBoundTexture;
    GLint width, height, depth, internalFormat;
    GLint minFilter, magFilter, wrapS, wrapT, wrapR;
    bool isTextureValid = false;

    GLenum targets[] = {GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP};
    GLenum binding_queries[] = {GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_CUBE_MAP};
    std::string typeNames[] = {"2D", "3D", "Cubemap"};

    for (int i = 0; i < 3; i++) {
        printf("Checking texture type %d (%s)\n", i, typeNames[i].c_str());
        GLenum target = targets[i];
        glGetIntegerv(binding_queries[i], &prevBoundTexture);
        glBindTexture(target, texID);

        if (glGetError() == GL_NO_ERROR) {
            GLenum faces[] = {target,target,target,target,target,target};
            int faceCount = 1;

            if (target == GL_TEXTURE_CUBE_MAP) {
                faceCount = 6;
                faces[0] = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                faces[1] = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
                faces[2] = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
                faces[3] = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
                faces[4] = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
                faces[5] = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
            }

            isTextureValid = true; // Assume valid until proven otherwise

            for (int j = 0; j < faceCount; j++) {
                GLenum face = faces[j];
                glGetTexLevelParameteriv(face, 0, GL_TEXTURE_WIDTH, &width);
                glGetTexLevelParameteriv(face, 0, GL_TEXTURE_HEIGHT, &height);
                glGetTexLevelParameteriv(face, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);

                if (target == GL_TEXTURE_3D) {
                    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, &depth);
                }

                bool dimensionCheck = width > 0 && height > 0 && (target != GL_TEXTURE_3D || depth > 0);
                bool formatCheck = internalFormat != 0;
                bool wrappingCheck = true; // Simplified for clarity

                glGetTexParameteriv(target, GL_TEXTURE_MIN_FILTER, &minFilter);
                glGetTexParameteriv(target, GL_TEXTURE_MAG_FILTER, &magFilter);
                glGetTexParameteriv(target, GL_TEXTURE_WRAP_S, &wrapS);
                glGetTexParameteriv(target, GL_TEXTURE_WRAP_T, &wrapT);
                if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_3D) {
                    glGetTexParameteriv(target, GL_TEXTURE_WRAP_R, &wrapR);
                }

                wrappingCheck = (wrapS != 0 && wrapT != 0) && ((target != GL_TEXTURE_CUBE_MAP && target != GL_TEXTURE_3D) || wrapR != 0);

                if (!(dimensionCheck && formatCheck && wrappingCheck)) {
                    std::cout << "Texture " << name << " (" << typeNames[i] << ") is invalid on face " << j << std::endl;
                    std::cout << "Width: " << width << ", Height: " << height << ", Internal Format: " << internalFormat << std::endl;
                    std::cout << "Min Filter: " << minFilter << ", Mag Filter: " << magFilter << std::endl;
                    std::cout << "Wrap S: " << wrapS << ", Wrap T: " << wrapT << (target != GL_TEXTURE_2D ? ", Wrap R: " + std::to_string(wrapR) : "") << std::endl;
                    OrkAssert(false);
                    isTextureValid = false;
                    break;
                }
            }

            if (isTextureValid) {
                std::cout << "Texture " << name << " (" << typeNames[i] << ") is valid." << std::endl;
                glBindTexture(target, prevBoundTexture);
                return true;
            }
        }
        glBindTexture(target, prevBoundTexture);
    }

    std::cout << "Texture " << name << " is not a valid 2D, 3D, or Cubemap texture." << std::endl;
    OrkAssert(false);
    return false;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid) {

  ///////////////////////////////////

  bool is_3d   = (tid._d > 1);
  bool is_cube = tid._initCubeTexture;
    EASY_BLOCK("gltxi::itfd:1", profiler::colors::Red);


  ///////////////////////////////////

  auto texture_target = is_3d ? GL_TEXTURE_3D : GL_TEXTURE_2D;
  if (is_cube) {
    texture_target = GL_TEXTURE_CUBE_MAP;
  }
  
  ///////////////////////////////////

  size_t dst_length = tid.computeDstSize();
  auto pboitem      = this->_getPBO(dst_length);

  ///////////////////////////////////////////////////////////////////

  auto src_buffer = (const uint8_t*)tid._data;

  ///////////////////////////////////////////////////////////////////

  bool need_convert = (tid._src_format != tid._dst_format);

  if (need_convert) {
    static auto rgb_buffer = (uint8_t*)malloc(16 << 20);
    int srcw               = tid._w;
    int srch               = tid._h;
    OrkAssert(not tid._autogenmips);
    int numpixels = srcw * srch;

    switch (tid._src_format) {
      case EBufferFormat::YUV420P: {
        // float t1 = _proftimer.SecsSinceStart();
        auto src_channel_y = src_buffer;
        auto src_channel_u = src_channel_y + numpixels;
        auto src_channel_v = src_channel_u + (numpixels >> 2);
        OrkAssert((srch & 0xf) == 0);
        std::atomic<int> opcounter = 0;
        for (int rowm = 0; rowm < srch; rowm += 16) {
          opcounter++;
          opq::concurrentQueue()->enqueue([=, &opcounter]() {
            for (int rown = 0; rown < 16; rown++) {
              int row           = rowm + rown;
              int row_base      = row * srcw;
              auto row_ybase    = src_channel_y + row_base;
              int uv_base_index = (row >> 1) * (srcw >> 1);
              auto ptr          = rgb_buffer + (srch - 1 - row) * srcw * 3;
              for (int col = 0; col < srcw; col++) {
                float yy    = float(row_ybase[col]);
                int uvindex = uv_base_index + (col >> 1);
                int uuu     = src_channel_u[uvindex];
                int vvv     = src_channel_v[uvindex];
                float uu    = float(uuu);
                float vv    = float(vvv);
                float r     = 1.164f * (yy - 16.0f) + 1.596f * (vv - 128.0f);
                float g     = 1.164f * (yy - 16.0f) - 0.813f * (vv - 128.0f) - 0.391f * (uu - 128.0f);
                float b     = 1.164f * (yy - 16.0f) + 2.018f * (uu - 128.0f);
                *ptr++      = uint8_t(ork::clamp<float>(r, 0, 255));
                *ptr++      = uint8_t(ork::clamp<float>(g, 0, 255));
                *ptr++      = uint8_t(ork::clamp<float>(b, 0, 255));
              }
            }
            opcounter--;
          });
        }
        while (opcounter.load() > 0) {
          ork::usleep(10);
        }
        // 11msec -> 2msec
        // float t2 = _proftimer.SecsSinceStart();
        // printf( "yuv msecs<%g>\n", (t2-t1)*1000.0f );
        src_buffer = rgb_buffer;
        break;
      }
      case EBufferFormat::RGB8: {
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
  EASY_END_BLOCK;
    EASY_BLOCK("gltxi::itfd:2", profiler::colors::Red);

  if (tid._truncation_length != 0) {
    OrkAssert(src_buffer == tid._data);

    /////////////////////////////////////////////////////////
    // pad truncation length to next hightest multiple of 128
    /////////////////////////////////////////////////////////

    tid._truncation_length += 127;
    tid._truncation_length = (tid._truncation_length & 0xfffffff80);
    dst_length             = tid._truncation_length;
  }
  EASY_END_BLOCK;
    EASY_BLOCK("gltxi::itfd:3", profiler::colors::Red);

  ///////////////////////////////////
#if defined(OPENGL_46)
  // OpenGL4.6 - persistent mapped objects
  if (mTargetGL._SUPPORTS_PERSISTENT_MAP) {
    pboitem->copyPersistentMapped(tid, dst_length, src_buffer);
  } else {
    pboitem->copyWithTempMapped(tid, dst_length, src_buffer);
  }
#else
  pboitem->copyWithTempMapped(tid, dst_length, src_buffer);
#endif
  ///////////////////////////////////
  EASY_END_BLOCK;

  /*printf(
      "UPDATE IMAGE UNC iw<%d> ih<%d> id<%d> dst_length<%zu> pbo<%d> mem<%p>\n",
      tid._w,
      tid._h,
      tid._d,
      dst_length,
      pboitem->_handle,
      mapped);*/

  ///////////////////////////////////
    EASY_BLOCK("gltxi::itfd:4", profiler::colors::Red);

  gltexobj_ptr_t glto;

  if (ptex->_impl.isA<gltexobj_ptr_t>()) {
    glto = ptex->_impl.get<gltexobj_ptr_t>();
    GL_ERRORCHECK();
    glBindTexture(texture_target, glto->_textureObject);
    GL_ERRORCHECK();
    // printf( "OLD obj<%p:%d>\n", (void*) glto.get(), int(glto->_textureObject));
  } else { // new texture
    glto = ptex->_impl.makeShared<GLTextureObject>(this);

    GL_ERRORCHECK();
    glGenTextures(1, &glto->_textureObject);
    glBindTexture(texture_target, glto->_textureObject);
    GL_ERRORCHECK();
    if (ptex->_debugName.length()) {
      mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, ptex->_debugName);
    }

    ptex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;
    // printf( "NEW obj<%p:%d>\n",  (void*) glto.get(), int(glto->_textureObject));

    // ptex->_impl._assert_on_destroy = true;
  }

  glto->mTarget = texture_target;

  GL_ERRORCHECK();
  EASY_END_BLOCK;
    EASY_BLOCK("gltxi::itfd:5", profiler::colors::Red);

  GLFormatTriplet triplet(tid._dst_format);

  GL_ERRORCHECK();
  ///////////////////////////////////
  // update texels
  ///////////////////////////////////
  bool size_or_fmt_dirty = (ptex->_width != tid._w) or  //
                           (ptex->_height != tid._h) or //
                           (ptex->_depth != tid._d) or  //
                           (ptex->_texFormat != tid._dst_format);

  if (is_cube) {
    if (size_or_fmt_dirty) { // allocating
      // init all 6 faces with PBO data
      for (int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, triplet._internalFormat, tid._w, tid._h, 0, triplet._format, triplet._type, nullptr);
      }
    } else { // non allocating
      for (int i = 0; i < 6; i++) {
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, tid._w, tid._h, triplet._format, triplet._type, nullptr);
      }
    }
  } else if (is_3d) {
    if (size_or_fmt_dirty) // allocating
      glTexImage3D(texture_target, 0, triplet._internalFormat, tid._w, tid._h, tid._d, 0, triplet._format, triplet._type, nullptr);
    else // non allocating
      glTexSubImage3D(texture_target, 0, 0, 0, 0, tid._w, tid._h, tid._d, triplet._format, triplet._type, nullptr);
    GL_ERRORCHECK();
  } else {
      if(0) printf( "pboitem<%d> size_or_fmt_dirty<%d> w<%d> h<%d>\n", int(pboitem->_handle), int(size_or_fmt_dirty), tid._w, tid._h );
    if (size_or_fmt_dirty) // allocating
      glTexImage2D(texture_target, 0, triplet._internalFormat, tid._w, tid._h, 0, triplet._format, triplet._type, nullptr);
    else // non allocating
      glTexSubImage2D(texture_target, 0, 0, 0, tid._w, tid._h, triplet._format, triplet._type, nullptr);
    GL_ERRORCHECK();
  }
  EASY_END_BLOCK;
    EASY_BLOCK("gltxi::itfd:6", profiler::colors::Red);

  ///////////////////////////////////
  GL_ERRORCHECK();
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind pbo
  this->_returnPBO(pboitem);
  ///////////////////////////////////

  ptex->_width     = tid._w;
  ptex->_height    = tid._h;
  ptex->_depth     = tid._d;
  ptex->_texFormat = tid._dst_format;

  ///////////////////////////////////
  // update texture parameters
  ///////////////////////////////////
  GL_ERRORCHECK();

  glTexParameterf(texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  if (tid._autogenmips)
    glGenerateMipmap(texture_target);

  if (size_or_fmt_dirty) {
    if ((not is_3d) and tid._autogenmips) {
      glTexParameterf(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameterf(texture_target, GL_TEXTURE_MAX_LEVEL, 3);
    } else {
      glTexParameterf(texture_target, GL_TEXTURE_MAX_LEVEL, 0);
    }
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (is_3d) {
      glTexParameterf(texture_target, GL_TEXTURE_WRAP_R, GL_REPEAT);
    }
  }
  if (is_3d) {
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_R, GL_REPEAT);
  }

  if (is_3d) {
    // ptex->TexSamplingMode().PresetTrilinearWrap();
    // this->ApplySamplingMode(ptex);
  }

  glto->mTarget = texture_target;

  //_checkTexture(glto->_textureObject, "");
  ///////////////////////////////////

  glBindTexture(texture_target, 0);
  GL_ERRORCHECK();
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

  auto glto = tex->_impl.makeShared<GLTextureObject>(this);

  glGenTextures(1, &glto->_textureObject);
  glBindTexture(GL_TEXTURE_2D, glto->_textureObject);

  if (from_chain->_debugName.length()) {
    tex->_debugName = from_chain->_debugName;
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, tex->_debugName);
  }

  size_t nummips = from_chain->_levels.size();

  for (size_t l = 0; l < nummips; l++) {
    auto pchl = from_chain->_levels[l];
    switch (from_chain->_format) {
      case EBufferFormat::RGB32F:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height * sizeof(fvec3));
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGB32F, pchl->_width, pchl->_height, 0, GL_RGB, GL_FLOAT, pchl->_data);
        break;
      case EBufferFormat::RGBA32F:
        OrkAssert(pchl->_length == pchl->_width * pchl->_height * sizeof(fvec4));
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA32F, pchl->_width, pchl->_height, 0, GL_RGBA, GL_FLOAT, pchl->_data);
        break;
#if defined(ORK_ARCHITECTURE_X86_64) and defined(LINUX)
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
