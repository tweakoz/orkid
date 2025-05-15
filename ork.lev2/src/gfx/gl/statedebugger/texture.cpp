////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "statedebug.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::_validateTextures() {

  using namespace ftxui;
  node_vect_t NODES;
  GL_ERRORCHECK();

  size_t num_textures = _glctx->mTxI._texture_set.size();

  /////////////////////////////////////////////////////////////////////

  auto hdrstr1 = FormatString("TEXOBJ");
  auto hdrstr2 = FormatString("DIM");
  auto hdrstr3 = FormatString("FMT");
  auto hdrstr4 = FormatString("IFMT");
  auto hdrstr5 = FormatString("TGT");
  auto hdrstr6 = FormatString("LEVS");
  auto hdrstr7 = FormatString("NAME");

  _colortext(
      NODES, //
      YEL,
      BLK,                                          //
      " %-7s %-14s %-10s %-22s %-22s %-7s %-40s\n", //
      hdrstr1.c_str(),                              //
      hdrstr2.c_str(),                              //
      hdrstr3.c_str(),                              //
      hdrstr4.c_str(),                              //
      hdrstr5.c_str(),                              //
      hdrstr6.c_str(),                              //
      hdrstr7.c_str());

  /////////////////////////////////////////////////////////////////////
  size_t index = 0;
  for (auto item : _glctx->mTxI._texture_set) {
    // std::string EBufferFormatToName(EBufferFormat fmt){

    GLuint texid               = item.first;
    const Texture* the_tex     = item.second;
    auto format                = EBufferFormatToName(the_tex->_texFormat);
    auto glto                  = the_tex->_impl.get<gltexobj_ptr_t>();
    GLenum texture_target      = glto->mTarget;
    GLenum tex_target_specific = texture_target;
    if (texture_target == GL_TEXTURE_CUBE_MAP) {
      tex_target_specific = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    //////////////////
    // count mip maps (using opengl 4.1 query)
    //////////////////

    GLuint save_current;
    // glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&save_current);
    // int nummips = 0;
    // glBindTexture(GL_TEXTURE_2D, texid);
    // int max_level;
    // glGetTexParameter( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &max_level );

    //////////////////

    auto texobjstr     = FormatString("%d", texid);
    auto dimstr        = FormatString("%d %d %d", the_tex->_width, the_tex->_height, the_tex->_depth);
    auto namestr       = FormatString("%s", the_tex->_debugName.c_str());
    std::string levstr = "";

    ///////////////////////
    // if texture is RGBA8, save it to disk as a png
    ///////////////////////

    GL_ERRORCHECK();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(texture_target, texid);
    GL_ERRORCHECK();

    GLint itw = 0;
    GLint ith = 0;
    switch (the_tex->_texFormat) {
      case EBufferFormat::RGB8:
      case EBufferFormat::RGBA8: {
        // bind texture
        // get texture data (mip0)

        GL_ERRORCHECK();
        glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_WIDTH, &itw);
        glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_HEIGHT, &ith);
        GL_ERRORCHECK();

        if (texture_target == GL_TEXTURE_2D) {

          int bpp     = (the_tex->_texFormat == EBufferFormat::RGB8) ? 3 : 4;
          int mipsize = itw * ith * bpp;

          // dimstr += FormatString(" [ mip0<%d %d> ]", itw, ith);
          //  std::vector<uint8_t> mipdata;
          //  mipdata.resize(mipsize);
          //   save to disk
          std::string filename = FormatString("tex_%d.png", texid);
          Image img;
          img.init(itw, ith, bpp, 1);
          img._format = the_tex->_texFormat;
          if (the_tex->_texFormat == EBufferFormat::RGB8) {
            glGetTexImage(tex_target_specific, 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)img._data->data());
          } else if (the_tex->_texFormat == EBufferFormat::RGBA8) {
            glGetTexImage(tex_target_specific, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)img._data->data());
          }
          GL_ERRORCHECK();
          //
          img.writeToFile(filename);
        }
        break;
      }
      case EBufferFormat::RGB32F: {
        // bind texture
        // get texture data (mip0)
        GL_ERRORCHECK();
        glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_WIDTH, &itw);
        glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_HEIGHT, &ith);
        GL_ERRORCHECK();
        if (texture_target == GL_TEXTURE_2D) {
          int bpc              = 4;
          int numc             = 3;
          int mipsize          = itw * ith * bpc * numc;
          std::string filename = FormatString("tex_%d.exr", texid);
          Image img;
          img.init(itw, ith, numc, bpc);
          img._format = the_tex->_texFormat;
          glGetTexImage(tex_target_specific, 0, GL_RGB, GL_FLOAT, (void*)img._data->data());
          img.writeToFile(filename);
        }
        break;
      }
      case EBufferFormat::RGBA32F: {
        // bind texture
        // get texture data (mip0)
        GL_ERRORCHECK();
        glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_WIDTH, &itw);
        glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_HEIGHT, &ith);
        GL_ERRORCHECK();
        // OrkAssert(itw==width);
        // OrkAssert(ith==height);
        if (texture_target == GL_TEXTURE_2D) {
          int bpc     = 4;
          int numc    = 4;
          int mipsize = itw * ith * bpc * numc;
          // dimstr += FormatString(" [ mip0<%d %d> ]", itw, ith);
          //  std::vector<uint8_t> mipdata;
          //  mipdata.resize(mipsize);
          //   save to disk
          std::string filename = FormatString("tex_%d.exr", texid);
          Image img;
          img.init(itw, ith, numc, bpc);
          img._format = the_tex->_texFormat;
          glGetTexImage(tex_target_specific, 0, GL_RGBA, GL_FLOAT, (void*)img._data->data());
          //
          img.writeToFile(filename);
          GL_ERRORCHECK();
        }
        break;
      }
      default:
        break;
    }
    ///////////////
    // get number of levels (or faces(cubemap)) that are filled in
    ///////////////

    switch (texture_target) {
      case GL_TEXTURE_1D:
      case GL_TEXTURE_2D:
      case GL_TEXTURE_3D: {
        levstr = FormatString("1");
        break;
      }
      case GL_TEXTURE_1D_ARRAY:
      case GL_TEXTURE_2D_ARRAY: {
        levstr = FormatString("%d", the_tex->_depth);
        break;
      }
      case GL_TEXTURE_CUBE_MAP: {
        // Check if all 6 faces are filled in with proper mips
        bool all_faces_ok = true;
        int mip_count     = 0;
        GLint max_level   = 0;

        // Get max mip level
        glGetTexParameteriv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, &max_level);

        // Check each face to ensure it has valid data
        for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++) {

          // Check mip level 0 dimensions (should all be equal for cube maps)
          GLint width = 0, height = 0;
          glGetTexLevelParameteriv(face, 0, GL_TEXTURE_WIDTH, &width);
          glGetTexLevelParameteriv(face, 0, GL_TEXTURE_HEIGHT, &height);

          // Cube maps must be square
          if (width != height || width <= 0) {
            all_faces_ok = false;
            break;
          }

          // Count how many mips are actually filled
          int face_mips = 0;
          for (int level = 0; level <= max_level; level++) {
            GLint width_at_level = 0;
            glGetTexLevelParameteriv(face, level, GL_TEXTURE_WIDTH, &width_at_level);

            // If we get a width of 0, this mip level isn't defined
            if (width_at_level <= 0)
              break;

            face_mips++;
          }

          // Update minimum mip count across all faces
          if (face == GL_TEXTURE_CUBE_MAP_POSITIVE_X) {
            mip_count = face_mips;
          } else if (face_mips != mip_count) {
            // All faces should have the same number of mips
            all_faces_ok = false;
            break;
          }
        }

        // Format the level string based on our checks
        if (all_faces_ok) {
          // Perfect cube map: all 6 faces with same number of mips
          levstr = FormatString("F6M%d", mip_count);
        } else {
          // Incomplete cube map
          levstr = FormatString("F?M?");
        }
        break;
      }

      default: {
        levstr = FormatString("?");
        break;
      }
    }
    ///////////////
    // get internal format via gl query
    GLint internalFormat = 0;
    auto tgtstr          = GLenumToString(GLenum(texture_target));
    //printf("TEX<%s> TGT<%s> FMT<%s>\n", texobjstr.c_str(), tgtstr.c_str(), format.c_str());
    GL_ERRORCHECK();
    glGetTexLevelParameteriv(tex_target_specific, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
    GL_ERRORCHECK();
    auto ifmtstr   = GLenumToString(GLenum(internalFormat));
    auto targetstr = GLenumToString(GLenum(texture_target));
    auto texstr    = FormatString(
        " %-7s %-14s %-10s %-22s %-22s %-7s %-40s\n", //
        texobjstr.c_str(),                            //
        dimstr.c_str(),                               //
        format.c_str(),                               //
        ifmtstr.c_str(),                              //
        targetstr.c_str(),                            //
        levstr.c_str(),                               //
        namestr.c_str());

    bool odd = (index % 2) == 0;
    auto BG  = odd ? GR1 : BLU1;
    _colortext(NODES, WHI, BG, "%s\n", texstr.c_str());
    index++;
  }

  /////////////////////////////////////////////////////////////////////

  _node_textures = vbox({
      text("Textures"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::_validateTextureBindingState() {
  // display all texture state
  using namespace ftxui;
  node_vect_t NODES;
  GLint numTexUnits = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);
  _colortext(NODES, WHI, BLK, "numTexUnits<%d>\n", numTexUnits);
  for (int i = 0; i < numTexUnits; i++) {
    GLint currentTexture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &currentTexture);
    glActiveTexture(GL_TEXTURE0 + i);
    GLint currentTexObj = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexObj);
    GLint currentTexObj3D = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &currentTexObj3D);
    GLint currentTexObj2DArray = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &currentTexObj2DArray);
    GLint currentTexObjCube = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &currentTexObjCube);
    GLint currentTexObjRect = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &currentTexObjRect);
    GLint currentTexObjBuffer = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &currentTexObjBuffer);
    GLint currentTexObjMS = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &currentTexObjMS);
    GLint currentTexObjMSArray = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, &currentTexObjMSArray);
    GLint currentTexObj1D = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_1D, &currentTexObj1D);
    GLint currentTexObj1DArray = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY, &currentTexObj1DArray);
    GLint currentTexObjCubeArray = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &currentTexObjCubeArray);
    GLint currentTexObjShadowRect = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &currentTexObjShadowRect);
    GLint currentTexObjShadowCube = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &currentTexObjShadowCube);
    GLint currentTexObjShadow2DArray = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &currentTexObjShadow2DArray);
    GLint currentTexObjShadowCubeArray = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &currentTexObjShadowCubeArray);
    GLint currentTexObjBufferShadow = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &currentTexObjBufferShadow);
    GLint currentTexObjRectShadow = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &currentTexObjRectShadow);
    GLint currentTexObjMSArrayShadow = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, &currentTexObjMSArrayShadow);
    GLint currentTexObjMSShadow = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &currentTexObjMSShadow);

    std::string texunit_str = FormatString("TexUnit<%d>", i);
    bool show               = false;
    if (i == currentTexture) {
      texunit_str += " (active)";
      show = true;
    }
    if (currentTexObj != 0) {
      texunit_str += FormatString(" 2D<%d>", currentTexObj);
      show = true;
    }
    if (currentTexObj3D != 0) {
      texunit_str += FormatString(" 3D<%d>", currentTexObj3D);
      show = true;
    }
    if (currentTexObj2DArray != 0) {
      texunit_str += FormatString(" 2DA<%d>", currentTexObj2DArray);
      show = true;
    }
    if (currentTexObjCube != 0) {
      texunit_str += FormatString(" Cube<%d>", currentTexObjCube);
      show = true;
    }
    if (currentTexObjRect != 0) {
      texunit_str += FormatString(" Rect<%d>", currentTexObjRect);
      show = true;
    }
    if (currentTexObjBuffer != 0) {
      texunit_str += FormatString(" Buff<%d>", currentTexObjBuffer);
      show = true;
    }
    if (currentTexObjMS != 0) {
      texunit_str += FormatString(" MS<%d>", currentTexObjMS);
      show = true;
    }
    if (currentTexObjMSArray != 0) {
      texunit_str += FormatString(" MSA<%d>", currentTexObjMSArray);
      show = true;
    }
    if (currentTexObj1D != 0) {
      texunit_str += FormatString(" 1D<%d>", currentTexObj1D);
      show = true;
    }
    if (currentTexObj1DArray != 0) {
      texunit_str += FormatString(" 1DA<%d>", currentTexObj1DArray);
      show = true;
    }
    if (currentTexObjCubeArray != 0) {
      texunit_str += FormatString(" CubeA<%d>", currentTexObjCubeArray);
      show = true;
    }
    if (currentTexObjShadowRect != 0) {
      texunit_str += FormatString(" RectS<%d>", currentTexObjShadowRect);
      show = true;
    }
    if (currentTexObjShadowCube != 0) {
      texunit_str += FormatString(" CubeS<%d>", currentTexObjShadowCube);
      show = true;
    }
    if (currentTexObjShadow2DArray != 0) {
      texunit_str += FormatString(" 2DSA<%d>", currentTexObjShadow2DArray);
      show = true;
    }
    if (currentTexObjShadowCubeArray != 0) {
      texunit_str += FormatString(" CubeSA<%d>", currentTexObjShadowCubeArray);
      show = true;
    }
    if (currentTexObjBufferShadow != 0) {
      texunit_str += FormatString(" BuffS<%d>", currentTexObjBufferShadow);
      show = true;
    }
    if (currentTexObjRectShadow != 0) {
      texunit_str += FormatString(" RectS<%d>", currentTexObjRectShadow);
      show = true;
    }
    if (currentTexObjMSArrayShadow != 0) {
      texunit_str += FormatString(" MSAS<%d>", currentTexObjMSArrayShadow);
      show = true;
    }
    if (currentTexObjMSShadow != 0) {
      texunit_str += FormatString(" MSS<%d>", currentTexObjMSShadow);
      show = true;
    }
    if (show) {
      _colortext(NODES, WHI, BLK, "%s\n", texunit_str.c_str());
    }
  }
  _node_texturebindingstate = vbox({
      text("Texture State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
