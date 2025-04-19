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

  size_t num_textures = _glctx->mTxI._texture_set.size();

  /////////////////////////////////////////////////////////////////////

  auto hdrstr1 = FormatString("TEXOBJ");
  auto hdrstr2 = FormatString("DIM");
  auto hdrstr3 = FormatString("FMT");
  auto hdrstr4 = FormatString("NAME");

  _colortext(NODES, YEL, BLK, " %-9s %-16s %-12s %-40s\n", hdrstr1.c_str(), hdrstr2.c_str(), hdrstr3.c_str(), hdrstr4.c_str());

  /////////////////////////////////////////////////////////////////////
  size_t index = 0;
  for (auto item : _glctx->mTxI._texture_set) {
    // std::string EBufferFormatToName(EBufferFormat fmt){

    GLuint texid           = item.first;
    const Texture* the_tex = item.second;
    auto format            = EBufferFormatToName(the_tex->_texFormat);

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

    auto texobjstr = FormatString("%d", texid);
    auto dimstr    = FormatString("%d %d %d", the_tex->_width, the_tex->_height, the_tex->_depth);
    auto namestr   = FormatString("%s", the_tex->_debugName.c_str());

    ///////////////////////
    // if texture is RGBA8, save it to disk as a png
    ///////////////////////

    GLint itw = 0;
    GLint ith = 0;
    switch (the_tex->_texFormat) {
      case EBufferFormat::RGB8:
      case EBufferFormat::RGBA8: {
        // bind texture
        // get texture data (mip0)

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texid);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &itw);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ith);
        // OrkAssert(itw==width);
        // OrkAssert(ith==height);

        int bpp = (the_tex->_texFormat==EBufferFormat::RGB8) ? 3 : 4;
        int mipsize = itw * ith * bpp;

        dimstr += FormatString(" [ mip0<%d %d> ]", itw, ith);
        // std::vector<uint8_t> mipdata;
        // mipdata.resize(mipsize);
        //  save to disk
        std::string filename = FormatString("tex_%d.png", texid);
        Image img;
        img.init(itw, ith, bpp, 1);
        img._format = the_tex->_texFormat;
        if( the_tex->_texFormat == EBufferFormat::RGB8 ){
          glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)img._data->data());
        }
        else if( the_tex->_texFormat == EBufferFormat::RGBA8 ){
          glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)img._data->data());
        }
        //
        img.writeToFile(filename);
        break;
      }
      case EBufferFormat::RGB32F:{
          // bind texture
        // get texture data (mip0)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texid);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &itw);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ith);
        // OrkAssert(itw==width);
        // OrkAssert(ith==height);
        int bpc = 4;
        int numc = 3;
        int mipsize = itw * ith * bpc*numc;
        dimstr += FormatString(" [ mip0<%d %d> ]", itw, ith);
        // std::vector<uint8_t> mipdata;
        // mipdata.resize(mipsize);
        //  save to disk
        std::string filename = FormatString("tex_%d.exr", texid);
        Image img;
        img.init(itw, ith, numc, bpc);
        img._format = the_tex->_texFormat;
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, (void*)img._data->data());
        img.writeToFile(filename);
        break;
      }
      case EBufferFormat::RGBA32F:{
        // bind texture
        // get texture data (mip0)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texid);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &itw);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ith);
        // OrkAssert(itw==width);
        // OrkAssert(ith==height);
        int bpc = 4;
        int numc = 4;
        int mipsize = itw * ith * bpc*numc;
        dimstr += FormatString(" [ mip0<%d %d> ]", itw, ith);
        // std::vector<uint8_t> mipdata;
        // mipdata.resize(mipsize);
        //  save to disk
        std::string filename = FormatString("tex_%d.exr", texid);
        Image img;
        img.init(itw, ith, numc, bpc);
        img._format = the_tex->_texFormat;
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, (void*)img._data->data());
        //
        img.writeToFile(filename);
        break;
      }  
      default:
        break;
    }

    auto texstr = FormatString(" %-9s %-16s %-12s %-40s\n", texobjstr.c_str(), dimstr.c_str(), format.c_str(), namestr.c_str());

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
