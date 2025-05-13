#include "gl.h"
#include <ork/math/misc_math.h>

namespace ork::lev2 {

void _validateRtGroup(RtGroup* rtgroup);

void GlFrameBufferInterface::_buildRtgImplFromTextureArraySlice(RtGroup* rtgroup) {
  auto slice = rtgroup->_slice;
  int slice_index = slice->_slice;
  auto texarray = slice->_array;
  auto tex = texarray->_tex;
  int iw = texarray->_width;
  int ih = texarray->_height;
  OrkAssert(iw>=8);
  OrkAssert(ih>=8);
  int max_slices = texarray->_maxslices;
  GLenum texture_target = GL_TEXTURE_2D_ARRAY;
  int numsamples = 1;
  /////////////////////////////////////////////
  // get texture object
  /////////////////////////////////////////////
  gltexobj_ptr_t glto;
  if( auto as_glto = tex->_impl.tryAs<gltexobj_ptr_t>() ){
    glto = as_glto.value();
  } else {
    auto txi = (GlTextureInterface*) & mTargetGL.mTxI;
    txi->initTextureArray2D(texarray);
    GL_ERRORCHECK();
    glto = tex->_impl.getShared<GLTextureObject>();
  }
  /////////////////////////////////////////////
  // create FBO for this slice
  //  the FBO should use the texture array slice
  //  as the color or depth attachment
  //   (depending on the format of the texarray)
  // the texarray will already have a texture object
  /////////////////////////////////////////////

  GLuint texobj = glto->_textureObject;

  auto rtg_impl = std::make_shared<GlRtGroupImpl>();
  rtg_impl->_target    = texture_target;
  rtg_impl->_numsamples = numsamples;
  rtgroup->_impl.set<glrtgroupimpl_ptr_t>(rtg_impl);
  bool is_depth = (texarray->_format == EBufferFormat::Z32F);


  if(is_depth) {
      rtg_impl->_depthonly = std::make_shared<GlFboObject>();

      glGenFramebuffers(1, &rtg_impl->_depthonly->_fbo);
      GL_ERRORCHECK();
      glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
      GL_ERRORCHECK();

      glFramebufferTextureLayer( //
        GL_FRAMEBUFFER,      // fb target
        GL_DEPTH_ATTACHMENT, // attachment
        texobj,              // array texture object
        0,                   // mip level
        slice_index);        // layer of array

      rtg_impl->_bindop = [this, rtgroup, rtg_impl]() {
        glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
      };
      
  }
  else { // color
    rtg_impl->_standard  = std::make_shared<GlFboObject>();

    glGenFramebuffers(1, &rtg_impl->_standard->_fbo);
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);

    glFramebufferTextureLayer( //
      GL_FRAMEBUFFER,       // fb target
      GL_COLOR_ATTACHMENT0, // attachment
      texobj,               // array texture object
      0,                    // mip level
      slice_index);         // layer of array


      rtg_impl->_bindop = [this, rtgroup, rtg_impl]() {
        glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);    
      };
    
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);


}

} // namespace ork::lev2 {