#include "gl.h"
#include <ork/math/misc_math.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

glrtgroupimpl_ptr_t GlFrameBufferInterface::_buildRtgImplFromScratch(RtGroup* rtgroup) {

  int iw = rtgroup->width();
  int ih = rtgroup->height();

  if (iw < 1)
    iw = 1;
  if (ih < 1)
    ih = 1;

  rtgroup->miW = iw;
  rtgroup->miH = ih;

  auto rtg_impl        = std::make_shared<GlRtGroupImpl>();
  rtg_impl->_standard  = std::make_shared<GlFboObject>();
  rtg_impl->_depthonly = std::make_shared<GlFboObject>();
  rtgroup->_impl.set<glrtgroupimpl_ptr_t>(rtg_impl);

  int inumtargets     = rtgroup->numImageBuffers();
  int numsamples      = msaaEnumToInt(rtgroup->_msaa_samples);
  auto texture_target = (numsamples == 1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
  if (rtgroup->_cubeMap) {
    texture_target = GL_TEXTURE_CUBE_MAP;
  }
  rtg_impl->_target     = texture_target;
  rtg_impl->_numsamples = numsamples;

  glGenFramebuffers(1, &rtg_impl->_standard->_fbo);
  GL_ERRORCHECK();

  //////////////////////////////////////////
  // depth only FBO
  //////////////////////////////////////////

  glGenFramebuffers(1, &rtg_impl->_depthonly->_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);

  GL_ERRORCHECK();

  _logchan_rtgroup->log(
      "create new FBO iw<%d> ih<%d> stdFBOID<%d> donlyFBOID<%d>",
      iw,
      ih,
      int(rtg_impl->_standard->_fbo),
      int(rtg_impl->_depthonly->_fbo));

  //////////////////////////////////////////
  // depth texture
  //////////////////////////////////////////

  rtgroup->_depthBuffer = std::make_shared<RtBuffer>(rtgroup, -1, EBufferFormat::Z32F, iw, ih);
  GL_ERRORCHECK();

  auto dtex           = rtgroup->_depthBuffer->_texture;
  dtex->_width        = iw;
  dtex->_height       = ih;
  dtex->_msaa_samples = rtgroup->_msaa_samples;
  dtex->_texFormat    = EBufferFormat::Z32F;
  dtex->_debugName    = rtgroup->_name+":Depth";
  dtex->_texType      = ETEXTYPE_2D;
  auto depth_glto     = dtex->_impl.makeShared<GLTextureObject>(&mTargetGL.mTxI);

  mTargetGL.mTxI._registerTexture(dtex.get());
  GL_ERRORCHECK();

  // printf("RtGroup<%p> GenFBO<%d>\n", rtgroup, int(impl->_standard->_fbo));

  //////////////////////////////////////////
  // color buffers
  //////////////////////////////////////////

  glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
  //_logchan_rtgroup->log("bind std FBOID<%d>", int(rtg_impl->_standard->_fbo));

  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t pB = rtgroup->buffer(it);
    if (pB->_impl.isA<GlRtBufferImpl*>() == false) {
      auto bufferimpl = new GlRtBufferImpl;
      // printf("RtGroup<%p> RtBuffer<%p> initcolor1\n", rtgroup, pB);
      pB->SetSizeDirty(true);
      //////////////////////////////////////////
      Texture* ptex       = pB->texture();
      ptex->_msaa_samples = rtgroup->_msaa_samples;

      if(pB->_debugName.length()) {
        ptex->_debugName = rtgroup->_name+":"+pB->_debugName;
      } else {
        ptex->_debugName = rtgroup->_name+":Color"+std::to_string(it);
      }

      auto color_glto = bufferimpl->_teximpl.makeShared<GLTextureObject>(&mTargetGL.mTxI);

      GL_ERRORCHECK();
      glGenTextures(1, (GLuint*)&color_glto->_textureObject);
      glBindTexture(texture_target, color_glto->_textureObject);

      if (pB->_debugName.length()) {
        mTargetGL.debugLabel(GL_TEXTURE, color_glto->_textureObject, pB->_debugName);
      }
      glBindTexture(texture_target, 0);
      GL_ERRORCHECK();
      //////////////////////////////////////////
      ptex->_width  = iw;
      ptex->_height = ih;
      ptex->_impl   = bufferimpl->_teximpl;

      ptex->_vars->makeValueForKey<GLuint>("gltexobj") = color_glto->_textureObject;

      if (not rtgroup->_cubeMap) {
        mTargetGL.TXI()->ApplySamplingMode(ptex);
      }
      //////////////////////////////////////////
      pB->_impl.set<GlRtBufferImpl*>(bufferimpl);
        
      mTargetGL.mTxI._registerTexture(ptex);

    }
  }

  rtgroup->SetSizeDirty(true);

  if (rtgroup->_depthOnly) {
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
    glBindTexture(texture_target, rtg_impl->_depthonly->_depthTexObject);
    glDrawBuffers(0, nullptr);
  } else if (rtgroup->_cubeMap) {

    auto bufferimpl     = rtgroup->mMrt[0]->_impl.get<GlRtBufferImpl*>();
    auto color_glto     = bufferimpl->_teximpl.getShared<GLTextureObject>();
    color_glto->mTarget = GL_TEXTURE_CUBE_MAP;

    /////////////////////////////////////////////////////////////
    // bind rtgroup->_cubeRenderFace to
    // framebuffer color and depth attachment 0
    /////////////////////////////////////////////////////////////

    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,                                      // attachment point
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + rtgroup->_cubeRenderFace, // face
        color_glto->_textureObject,
        0); // mip

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + rtgroup->_cubeRenderFace,
        rtg_impl->_standard->_depthTexObject,
        0);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    // glBindTexture(texture_target, rtg_impl->_standard->_depthTexObject);
  }
  GL_ERRORCHECK();
  GLenum buffers[] = {
      GL_COLOR_ATTACHMENT0,
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2,
      GL_COLOR_ATTACHMENT3,
      GL_COLOR_ATTACHMENT4,
      GL_COLOR_ATTACHMENT5,
      GL_COLOR_ATTACHMENT6,
      GL_COLOR_ATTACHMENT7};

  glDrawBuffers(inumtargets, buffers);
  GL_ERRORCHECK();
  //////////////////////////////////////////////////
  // Bind Operation
  //////////////////////////////////////////////////

  rtg_impl->_bindop = [this, rtgroup, rtg_impl]() {
    if (rtgroup->IsSizeDirty()) {
      _regenRtgImplFromScratch(rtgroup);
    }

    GLenum texture_target = rtg_impl->_target;
    int inumtargets       = rtgroup->numImageBuffers();
    if (rtgroup->_depthOnly) {
      glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
    } else if (rtgroup->_cubeMap) {
      glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
      auto bufferimpl     = rtgroup->mMrt[0]->_impl.get<GlRtBufferImpl*>();
      auto color_glto     = bufferimpl->_teximpl.getShared<GLTextureObject>();
      //printf("set cube map face<%d>\n", rtgroup->_cubeRenderFace);
        // set cube map face
      glFramebufferTexture2D(
          GL_FRAMEBUFFER,
          GL_COLOR_ATTACHMENT0,
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + rtgroup->_cubeRenderFace,
          color_glto->_textureObject,
          0);
      glFramebufferTexture2D(
          GL_FRAMEBUFFER,
          GL_DEPTH_ATTACHMENT,
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + rtgroup->_cubeRenderFace,
          rtg_impl->_standard->_depthTexObject,
          0);
    } else {
      glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
      // glBindTexture(texture_target, rtg_impl->_standard->_depthTexObject);
    }
  };

  return rtg_impl;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_regenRtgImplFromScratch(RtGroup* rtgroup) {

  auto rtg_impl = rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>().value();
  int iw        = rtgroup->width();
  int ih        = rtgroup->height();
  //////////////////////////////////////////
  // regen impl
  //////////////////////////////////////////
  int inumtargets = rtgroup->numImageBuffers();

  OrkAssert(rtg_impl);

  GLenum texture_target = rtg_impl->_target;
  int numsamples        = rtg_impl->_numsamples;

  glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
  //_logchan_rtgroup->log("resize FBOID<%d> iw<%d> ih<%d> ", int(rtg_impl->_standard->_fbo), iw, ih);

  //////////////////////////////////////////
  // resize depth texture
  //////////////////////////////////////////

  GL_ERRORCHECK();
  if (rtg_impl->_standard->_depthTexObject != 0) {
    glDeleteTextures(1, &rtg_impl->_standard->_depthTexObject);
    rtg_impl->_standard->_depthTexObject = 0;
  }

  GL_ERRORCHECK();
  glGenTextures(1, &rtg_impl->_standard->_depthTexObject);
  glBindTexture(texture_target, rtg_impl->_standard->_depthTexObject);

  rtg_impl->_depthonly->_depthTexObject = rtg_impl->_standard->_depthTexObject;

  auto dtex2           = rtgroup->_depthBuffer->_texture;
  dtex2->_width        = iw;
  dtex2->_height       = ih;
  dtex2->_msaa_samples = rtgroup->_msaa_samples;
  dtex2->_texFormat    = EBufferFormat::Z32F;
  dtex2->_debugName    = rtgroup->_name+":Depth";
  auto depth_glto      = dtex2->_impl.getShared<GLTextureObject>();

  depth_glto->_textureObject = rtg_impl->_standard->_depthTexObject;
  depth_glto->mTarget        = texture_target;

  mTargetGL.mTxI._registerTexture(rtgroup->_depthBuffer->_texture.get());

  GL_ERRORCHECK();
  std::string DepthTexName("RtgDepth");
  mTargetGL.debugLabel(GL_TEXTURE, rtg_impl->_standard->_depthTexObject, DepthTexName);
  if (numsamples == 1) {
    if (rtgroup->_cubeMap) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    } else {
      glTexImage2D(texture_target, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, 0);
    }
  } else {
    glTexImage2DMultisample(
        texture_target,       // target
        numsamples,           // numsamples
        GL_DEPTH_COMPONENT32, // internal format
        iw,                   // w
        ih,                   // h
        GL_TRUE);             // fixed sample locations
  }
  GL_ERRORCHECK();

  glBindTexture(texture_target, 0);
  GL_ERRORCHECK();
  glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
  GL_ERRORCHECK();

  // printf("RtGroup<%p> initdepth3\n", rtgroup);
  if (rtgroup->_cubeMap) {
    GLenum faces[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

    for (int i = 0; i < 6; ++i) {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, faces[i], rtg_impl->_standard->_depthTexObject, 0);
    }
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, rtg_impl->_standard->_depthTexObject, 0);
  }

  GL_ERRORCHECK();
  auto depth_texture                                        = rtgroup->_depthBuffer->_texture;
  depth_texture->_vars->makeValueForKey<GLuint>("gltexobj") = rtg_impl->_standard->_depthTexObject;
  mTargetGL.TXI()->ApplySamplingMode(depth_texture.get());
  depth_texture->_isDepthTexture = true;
  auto depthtexobj               = depth_texture->_impl.get<gltexobj_ptr_t>();
  depthtexobj->_textureObject    = rtg_impl->_standard->_depthTexObject;

  //////////////////////////////////////////
  // attach depthtexture to depth only FBO
  //////////////////////////////////////////

  glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
  if (rtgroup->_cubeMap) {
    GLenum faces[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

    for (int i = 0; i < 6; ++i) {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, faces[i], rtg_impl->_standard->_depthTexObject, 0);
    }
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, rtg_impl->_standard->_depthTexObject, 0);
  }

  //////////////////////////////////////////
  // resize color buffers
  //////////////////////////////////////////

  glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);

  //////
  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t rtbuffer = rtgroup->buffer(it);
    auto bufferimpl         = rtbuffer->_impl.get<GlRtBufferImpl*>();

    auto tex = rtbuffer->texture();
    if (bufferimpl->_init or rtbuffer->mSizeDirty) {
      // printf("RtGroup<%p> RtBuffer<%p> initcolor2\n", rtgroup, rtbuffer);
      // D3DFORMAT efmt = D3DFMT_A8R8G8B8;
      GLuint glinternalformat = 0;
      GLuint glformat         = GL_RGBA;
      GLenum gltype           = 0;
      switch (rtbuffer->format()) {
        case EBufferFormat::R32F:
          glformat         = GL_RED;
          glinternalformat = GL_R32F;
          gltype           = GL_FLOAT;
          break;
        case EBufferFormat::R32UI:
          glformat                      = GL_RED_INTEGER;
          glinternalformat              = GL_R32UI;
          gltype                        = GL_UNSIGNED_INT;
          tex->_formatSupportsFiltering = false;
          break;
        case EBufferFormat::RG16F:
          glformat         = GL_RG;
          glinternalformat = GL_RG16F;
          gltype           = GL_HALF_FLOAT;
          break;
        case EBufferFormat::RG32F:
          glformat         = GL_RG;
          glinternalformat = GL_RG32F;
          gltype           = GL_FLOAT;
          break;
        case EBufferFormat::RGB8:
          glinternalformat = GL_RGB8;
          gltype           = GL_UNSIGNED_BYTE;
          break;
        case EBufferFormat::RGBA8:
          glinternalformat = GL_RGBA8;
          gltype           = GL_UNSIGNED_BYTE;
          break;
        case EBufferFormat::RGBA16F:
          glinternalformat = GL_RGBA16F;
          gltype           = GL_HALF_FLOAT;
          break;
        case EBufferFormat::RGBA16UI:
          glformat                      = GL_RGBA_INTEGER;
          glinternalformat              = GL_RGBA16UI;
          gltype                        = GL_UNSIGNED_SHORT;
          tex->_formatSupportsFiltering = false;
          break;
        case EBufferFormat::RGBA32F:
          glinternalformat = GL_RGBA32F;
          gltype           = GL_FLOAT;
          break;
        case EBufferFormat::RGB10A2:
          glinternalformat = GL_RGB10_A2;
          gltype           = GL_UNSIGNED_INT_10_10_10_2;
          break;
        case EBufferFormat::RGB32UI:
          glformat                      = GL_RGB_INTEGER;
          glinternalformat              = GL_RGB32UI;
          gltype                        = GL_UNSIGNED_INT;
          tex->_formatSupportsFiltering = false;
          break;
        case EBufferFormat::RGBA32UI:
          glformat                      = GL_RGBA_INTEGER;
          glinternalformat              = GL_RGBA32UI;
          gltype                        = GL_UNSIGNED_INT;
          tex->_formatSupportsFiltering = false;
          break;
        default:
          OrkAssert(false);
          break;
          // case EBufferFormat::RGBA32F: glinternalformat = GL_RGBA32; break;
      }

      //////////////////////////////////////////
      // initialize texture
      //////////////////////////////////////////

      auto glto     = tex->_impl.get<gltexobj_ptr_t>();
      /*
      if(glto->_textureObject == 9) {
        raise(SIGTRAP);
      }*/
      
      GLuint texobj = glto->_textureObject;
      tex->_width   = iw;
      tex->_height  = ih;
      if (tex->_formatSupportsFiltering) {
        tex->mTexSampleMode.presetTrilinearWrap();
      } else {
        tex->mTexSampleMode.presetPointAndClamp();
      }

      mTargetGL.debugPushGroup("init-rt-tex", fvec4::Magenta());

      glBindTexture(texture_target, texobj);
      GL_ERRORCHECK();
      void* initialdata = calloc(1, iw * ih * 16);
      if (numsamples == 1) {
        if (rtgroup->_cubeMap) {
          for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glinternalformat, iw, ih, 0, glformat, gltype, initialdata);
          }
        } else {
          glTexImage2D(texture_target, 0, glinternalformat, iw, ih, 0, glformat, gltype, initialdata);
        }
      } else {
        glTexImage2DMultisample(texture_target, numsamples, glinternalformat, iw, ih, GL_TRUE);
      }
      free(initialdata);
      GL_ERRORCHECK();

      switch (rtbuffer->_mipgen) {
        case RtBuffer::EMG_AUTOCOMPUTE:
        case RtBuffer::EMG_USER: {
          glGenerateMipmap(texture_target);
          int nummips   = std::ceil(log_base(2, std::max(iw, ih))) + 1;
          glto->_maxmip = nummips - 2;
          // printf("SetRtg::gentex<%d> w<%d> h<%d> nummips<%d>\n", int(glto->_textureObject), iw, ih, nummips);
          break;
        }
        default:
          break;
      }
      // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LEVEL, 0);
      // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nummips - 1);
      GL_ERRORCHECK();
      if (not rtgroup->_cubeMap) {
        mTargetGL.TXI()->ApplySamplingMode(tex);
      }
      GL_ERRORCHECK();
    } // if (bufferimpl->_init or rtbuffer->mSizeDirty) {

    //////////////////////////////////////////
    // attach texture to framebuffercolor buffer
    //////////////////////////////////////////

    auto color_glto     = bufferimpl->_teximpl.get<gltexobj_ptr_t>();
    color_glto->mTarget = texture_target;
    // printf("RtGroup<%p> RtBuffer<%p> attachcoloridx<%d> fbo<%d>\n", rtgroup, rtbuffer, it, int(glto->_textureObject));

    if (rtgroup->_cubeMap) {
      GLenum faces[6] = {
          GL_TEXTURE_CUBE_MAP_POSITIVE_X,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

      for (int i = 0; i < 6; ++i) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, faces[i], color_glto->_textureObject, 0);
      }
    } else {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, texture_target, color_glto->_textureObject, 0);
    }

    GL_ERRORCHECK();

    if (bufferimpl->_init or rtbuffer->mSizeDirty) {
      bufferimpl->_init = false;
      mTargetGL.debugPopGroup();
    }
    rtbuffer->SetSizeDirty(false);

  } //     for (int it = 0; it < inumtargets; it++) {

  //////////////////////////////////////////

  rtgroup->SetSizeDirty(false);

  validateRtGroup(rtgroup);

  //_dumpFBOstructure(rtg_impl->_standard->_fbo, "UPDIMPL::" + rtgroup->_name + ".STD",rtgroup);
  //_dumpFBOstructure(rtg_impl->_depthonly->_fbo, "UPDIMPL::" + rtgroup->_name + ".DONLY",rtgroup);
}

} // namespace ork::lev2
