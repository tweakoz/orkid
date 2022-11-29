////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/util/logger.h>

GLuint gLastBoundNonZeroTex = 0;

namespace ork::lev2 {

GLTextureObject::GLTextureObject()
    : mObject(0)
    , mFbo(0)
    , mDbo(0)
    , mTarget(GL_NONE) {
}

///////////////////////////////////////////////////////////////////////////////

static ork::Timer _proftimer;

GlTextureInterface::GlTextureInterface(ContextGL& tgt)
    : mTargetGL(tgt) {
      _proftimer.Start();
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::bindTextureToUnit(const Texture* tex, GLenum tex_target, int tex_unit) {

  auto tex_obj = (const GLTextureObject*)tex->_internalHandle;
  if (nullptr == tex_obj) {

    auto new_tex_obj     = new GLTextureObject;
    tex->_internalHandle = new_tex_obj;
    tex_obj              = new_tex_obj;

    /////////////////////////////////////////////////////////////
    // assign default texture object (0) to start out with
    /////////////////////////////////////////////////////////////

    new_tex_obj->mObject = 0;

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

      /*printf("MEMOBJECT<%d> fd<%d> w<%d> h<%d> size<%zu>\n",
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

      glCreateTextures(tex_target, 1, &new_tex_obj->mObject);
      glBindTexture(tex_target, new_tex_obj->mObject);

      GL_ERRORCHECK();

      glTexParameteri(tex_target, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT);
      glTexParameterf(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameterf(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameterf(tex_target, GL_TEXTURE_MAX_LEVEL, 0);
      glTexParameterf(tex_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameterf(tex_target, GL_TEXTURE_WRAP_T, GL_REPEAT);

      GL_ERRORCHECK();

      glTexStorageMem2DEXT(
          tex_target,          // texture target
          1,                      // mip count
          GL_RGB10_A2,            // internal format
          ipcdata->_image_width,  // width
          ipcdata->_image_height, // height
          mem_object,             // mem object
          0);                     // offset into memobject's data

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

  GLuint texID = tex_obj->mObject;

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

bool GlTextureInterface::LoadTexture(const AssetPath& infname, texture_ptr_t ptex) {
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

  // printf("infname<%s>\n", infname.c_str());
  // printf("final_fname<%s>\n", final_fname.c_str());

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

bool GlTextureInterface::destroyTexture(texture_ptr_t tex) {
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

    for (int i = 0; i < 4; i++) {

      auto new_pbo = std::make_shared<PboItem>();

      new_pbo->_length = _size;

      GL_ERRORCHECK();
      glGenBuffers(1, &new_pbo->_handle);
      GL_ERRORCHECK();
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, new_pbo->_handle);
      GL_ERRORCHECK();

      // ??? persistent mapped objects

#if defined(OPENGL_46)
      u32 create_flags = GL_MAP_WRITE_BIT;
      create_flags |= GL_MAP_PERSISTENT_BIT;
      create_flags |= GL_MAP_COHERENT_BIT;
      glBufferStorage(GL_PIXEL_UNPACK_BUFFER, _size, nullptr, create_flags);
      GL_ERRORCHECK();
      u32 map_flags = GL_MAP_WRITE_BIT;
      map_flags |= GL_MAP_PERSISTENT_BIT;
      map_flags |= GL_MAP_INVALIDATE_RANGE_BIT;
      map_flags |= GL_MAP_COHERENT_BIT;
      //map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
      new_pbo->_mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, _size, map_flags);
#else
      glBufferData(GL_PIXEL_UNPACK_BUFFER, _size, NULL, GL_STREAM_DRAW);
#endif
      GL_ERRORCHECK();
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      // GL_ERRORCHECK();
      _pbos.push(new_pbo);
      _pbos_perm.insert(new_pbo);
      //printf("AllocPBO objid<%d> size<%zu> mapped<%p>\n", int(new_pbo->_handle), _size, (void*) new_pbo->_mapped);
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
  auto it         = _pbosets.find(isize);
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

extern std::atomic<int> __FIND_IT;

void GlTextureInterface::ApplySamplingMode(Texture* ptex) {
  int numsamples = msaaEnumToInt(ptex->_msaa_samples);
  if(numsamples>1)
    return;

  GLTextureObject* pTEXOBJ = (GLTextureObject*)ptex->GetTexIH();
  if (pTEXOBJ) {
    GLenum tgt = (pTEXOBJ->mTarget != GL_NONE) ? pTEXOBJ->mTarget : GL_TEXTURE_2D;


    mTargetGL.makeCurrentContext();
    __FIND_IT.store(1);
    mTargetGL.debugPushGroup("ApplySamplingMode");
    GL_ERRORCHECK();

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


    }

    GL_ERRORCHECK();
    // sampler state..
    glBindTexture(tgt, pTEXOBJ->mObject);
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

  bool is_3d = (tid._d > 1);

  ///////////////////////////////////

  auto texture_target = is_3d ? GL_TEXTURE_3D : GL_TEXTURE_2D;

  ///////////////////////////////////

  size_t dst_length = tid.computeDstSize();
  auto pboitem  = this->_getPBO(dst_length);

  ///////////////////////////////////////////////////////////////////

  auto src_buffer = (const uint8_t*) tid._data;

  ///////////////////////////////////////////////////////////////////

  bool need_convert = (tid._src_format != tid._dst_format);

  if(need_convert){
    static auto rgb_buffer = (uint8_t*) malloc(16<<20);
    int srcw = tid._w;
    int srch = tid._h;
    OrkAssert(not tid._autogenmips);
    int numpixels = srcw*srch;


    switch(tid._src_format){
      case EBufferFormat::YUV420P:{
        //float t1 = _proftimer.SecsSinceStart();
        auto src_channel_y = src_buffer;
        auto src_channel_u = src_channel_y + numpixels;
        auto src_channel_v = src_channel_u + (numpixels>>2);
        OrkAssert((srch&0xf)==0);
        std::atomic<int> opcounter = 0;
        for (int rowm=0; rowm<srch; rowm+=16) {
          opcounter++;
          opq::concurrentQueue()->enqueue([=,&opcounter](){
            for( int rown=0; rown<16; rown++){
              int row = rowm+rown;
              int row_base = row*srcw;
              auto row_ybase = src_channel_y + row_base;
              int uv_base_index = (row>>1) * (srcw>>1);
              auto ptr = rgb_buffer + (srch-1-row)*srcw*3;
              for (int col = 0; col<srcw; col++) {
                float yy = float(row_ybase[col]);
                int uvindex = uv_base_index + (col>>1);
                int uuu = src_channel_u[uvindex];
                int vvv = src_channel_v[uvindex];
                float uu = float(uuu);
                float vv = float(vvv);
                float r = 1.164f * (yy - 16.0f) + 1.596f * (vv - 128.0f);
                float g = 1.164f * (yy - 16.0f) - 0.813f * (vv - 128.0f) - 0.391f * (uu - 128.0f);
                float b = 1.164f * (yy - 16.0f) + 2.018f * (uu - 128.0f);
                *ptr++ = uint8_t(ork::clamp<float>(r,0,255));
                *ptr++ = uint8_t(ork::clamp<float>(g,0,255));
                *ptr++ = uint8_t(ork::clamp<float>(b,0,255));
              }
            }
            opcounter--;
          });
        }
        while(opcounter.load()>0){
          ork::usleep(10);
        }
        // 11msec -> 2msec
        //float t2 = _proftimer.SecsSinceStart();
        //printf( "yuv msecs<%g>\n", (t2-t1)*1000.0f );
        src_buffer = rgb_buffer;
        break;
      }
      case EBufferFormat::RGB8:{
        
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }

  if(tid._truncation_length!=0){

    OrkAssert(src_buffer==tid._data);

    /////////////////////////////////////////////////////////
    // pad truncation length to next hightest multiple of 128
    /////////////////////////////////////////////////////////

    tid._truncation_length += 127;
    tid._truncation_length = (tid._truncation_length&0xfffffff80);
    dst_length = tid._truncation_length;
  }

  ////////////////////////////////////////
  // OpenGL4.6 - persistent mapped objects
  ////////////////////////////////////////

#if defined(OPENGL_46)

  auto mapped = pboitem->_mapped;
  size_t pbolen = pboitem->_length;
  GL_ERRORCHECK();
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboitem->_handle);
  GL_ERRORCHECK();
  if(tid._truncation_length!=0){
    if(tid._truncation_length>pbolen){
      logerrchannel()->log("ERROR: PBO overflow trunclen<%zu> pbolen<%zu>", //
             tid._truncation_length, //
             pbolen );
      OrkAssert(false);
    }
  }
  memcpy_fast(mapped, src_buffer, dst_length);

#else

  ///////////////////////////////////////////////////////////////////

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboitem->_handle);
  GL_ERRORCHECK();
  u32 map_flags = GL_MAP_WRITE_BIT;
  map_flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
  map_flags |= GL_MAP_INVALIDATE_RANGE_BIT;
  map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;
  void* mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, dst_length, map_flags);
  memcpy_fast(mapped, src_buffer, dst_length);
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  GL_ERRORCHECK();
#endif

  //printf("UPDATE IMAGE UNC iw<%d> ih<%d> id<%d> length<%zu> pbo<%d> mem<%p>\n", tid._w, tid._h, tid._d, length, pboitem->_handle, mapped);

  ///////////////////////////////////

  GLTextureObject* pTEXOBJ = nullptr;
  if (nullptr == ptex->_internalHandle) {
    pTEXOBJ               = new GLTextureObject;
    ptex->_internalHandle = (void*)pTEXOBJ;
    glGenTextures(1, &pTEXOBJ->mObject);
    glBindTexture(texture_target, pTEXOBJ->mObject);
    if (ptex->_debugName.length()) {
      mTargetGL.debugLabel(GL_TEXTURE, pTEXOBJ->mObject, ptex->_debugName);
    }

    ptex->_varmap.makeValueForKey<GLuint>("gltexobj") = pTEXOBJ->mObject;

  } else {
    pTEXOBJ = (GLTextureObject*)ptex->_internalHandle;
    glBindTexture(texture_target, pTEXOBJ->mObject);
  }

  ///////////////////////////////////
  GLenum internalformat, format, type;
  switch (tid._dst_format) {
    case EBufferFormat::RGB8: {
      internalformat = GL_RGB8;
      format         = GL_RGB;
      type           = GL_UNSIGNED_BYTE;
      break;
    }
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
    case EBufferFormat::R32F: {
      internalformat = GL_R32F;
      format         = GL_RED;
      type           = GL_FLOAT;
      break;
    }
    case EBufferFormat::R16: {
      internalformat = GL_R16UI;
      format         = GL_RED_INTEGER;
      type           = GL_UNSIGNED_SHORT;
      break;
    }
    case EBufferFormat::R8: {
      internalformat = GL_R8;
      format         = GL_RED;
      type           = GL_UNSIGNED_BYTE;
      break;
    }

    default:
      OrkAssert(false);
      break;
  }
  GL_ERRORCHECK();
  ///////////////////////////////////
  // update texels
  ///////////////////////////////////
  bool size_or_fmt_dirty = (ptex->_width != tid._w) or  //
                           (ptex->_height != tid._h) or //
                           (ptex->_depth != tid._d) or  //
                           (ptex->_texFormat != tid._dst_format);

  if (is_3d) {
    if (size_or_fmt_dirty)
      glTexImage3D(texture_target, 0, internalformat, tid._w, tid._h, tid._d, 0, format, type, nullptr);
    else
      glTexSubImage3D(texture_target, 0, 0, 0, 0, tid._w, tid._h, tid._d, format, type, nullptr);
  } else {
    if (size_or_fmt_dirty)
      glTexImage2D(texture_target, 0, internalformat, tid._w, tid._h, 0, format, type, nullptr);
    else
      glTexSubImage2D(texture_target, 0, 0, 0, tid._w, tid._h, format, type, nullptr);
  }

  ///////////////////////////////////
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind pbo
  this->_returnPBO(pboitem);
  ///////////////////////////////////

  ptex->_width     = tid._w;
  ptex->_height    = tid._h;
  ptex->_depth    = tid._d;
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
    if(is_3d){
      glTexParameterf(texture_target, GL_TEXTURE_WRAP_R, GL_REPEAT);
    }

  }
  if(is_3d){
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(texture_target, GL_TEXTURE_WRAP_R, GL_REPEAT);
  }

  if(is_3d){
    //ptex->TexSamplingMode().PresetTrilinearWrap();
    //this->ApplySamplingMode(ptex);
  }

  pTEXOBJ->mTarget = texture_target;

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
