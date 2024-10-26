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
#include <ork/lev2/lev2_asset.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

bool GlTextureInterface::_loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  auto asset_load_req = ptex->loadRequest();
  GlTexLoadReq load_req;
  ptex->_final_datablock = datablock;
  load_req.ptex                  = ptex;
  load_req._inpstream._datablock = datablock;
  load_req._cmipchain            = std::make_shared<CompressedImageMipChain>();
  load_req._cmipchain->readXTX(datablock);
  ///////////////////////////////////////////////
  auto glto = ptex->_impl.makeShared<GLTextureObject>(this);
  ////////////////////////////////////////////////////////////////////
  ptex->_width     = load_req._cmipchain->_width;
  ptex->_height    = load_req._cmipchain->_height;
  ptex->_depth     = 1;
  ptex->_texFormat = load_req._cmipchain->_format;
  ///////////////////////////////////////////////
   auto keys = load_req._cmipchain->_varmap.dumpkeys();
   //printf("\nxtx w<%lu>\n", ptex->_width);
   //printf("xtx h<%lu>\n", ptex->_height);
   //printf("xtx d<%lu>\n", load_req._cmipchain->_depth);
   //printf("xtx fmt<%zx>\n", (uint64_t)load_req._cmipchain->_format);
   //for (auto k : keys) {
    //printf("xtx mipchain varmap-key<%s>\n", k.c_str());
  //}
   //for (auto k : ptex->_vars->dumpkeys()) {
   //printf("xtx ptex varmap-key<%s>\n", k.c_str());
  //}
  void_lambda_t lamb = [=]() {
    //printf( "XTX MAINTHREAD<%p>\n",ptex);
    /////////////////////////////////////////////
    // texture preprocssing, if any..
    //  on main thread.
    /////////////////////////////////////////////
    if (ptex->_vars->hasKey("preproc")) {
      auto preproc        = ptex->_vars->typedValueForKey<Texture::proc_t>("preproc").value();
      auto orig_datablock = datablock;
      if(asset_load_req and asset_load_req->_on_event){
        asset_load_req->_on_event("beginPreProc"_crcu, nullptr);
      }
      auto postblock      = preproc(ptex, &mTargetGL, orig_datablock);
      if(asset_load_req and asset_load_req->_on_event){
        asset_load_req->_on_event("beginPostProc"_crcu, nullptr);
      }
    }

    if(asset_load_req and asset_load_req->_on_event){
      asset_load_req->_on_event("beginLoadMainThread"_crcu, nullptr);
    }
    this->_loadXTXTextureMainThreadPart(load_req);
    if(asset_load_req and asset_load_req->_on_event){
      asset_load_req->_on_event("endLoadMainThread"_crcu,nullptr);
    }
    if(asset_load_req and asset_load_req->_on_event){
      auto data = std::make_shared<varmap::VarMap>();
      data->makeValueForKey<std::string>("loader") = "_loadXTXTexture";
      asset_load_req->_on_event("loadComplete"_crcu,data);
    }
  };
  opq::mainSerialQueue()->enqueue(lamb);
  ///////////////////////////////////////////////
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlTextureInterface::_loadXTXTextureMainThreadPart(GlTexLoadReq req) {

  auto asset_load_req = req.ptex->loadRequest();

  mTargetGL.makeCurrentContext();
  mTargetGL.debugPushGroup("_loadXTXTextureMainThreadPart");
  OrkAssert(req._cmipchain.get() != nullptr);
  auto glto = req.ptex->_impl.get<gltexobj_ptr_t>();
  glGenTextures(1, &glto->_textureObject);
  glBindTexture(GL_TEXTURE_2D, glto->_textureObject);
  GL_ERRORCHECK();
  req.ptex->_vars->makeValueForKey<GLuint>("gltexobj") = glto->_textureObject;
  if (req.ptex->_debugName.length()) {
    mTargetGL.debugLabel(GL_TEXTURE, glto->_textureObject, req.ptex->_debugName);
  }
  int inummips = req._cmipchain->_levels.size();
  OrkAssert(inummips > 0);
  GL_ERRORCHECK();
   //printf("inummips<%d>\n", inummips);
  for (int imip = 0; imip < inummips; imip++) {
    auto& level = req._cmipchain->_levels[imip];
     //printf("tex<%s> mip<%d> w<%ld> h<%ld> len<%zu>\n", req.ptex->_debugName.c_str(), imip, level._width, level._height, level._data->length());
    switch (req.ptex->_texFormat) {
      case EBufferFormat::R16:
        if(asset_load_req and asset_load_req->_on_event){
          auto data = std::make_shared<varmap::VarMap>();
          data->makeValueForKey<int>("level") = imip;
          data->makeValueForKey<int>("width") = level._width;
          data->makeValueForKey<int>("height") = level._height;
          data->makeValueForKey<datablock_ptr_t>("data") = level._data;
          data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::R16);
          data->makeValueForKey<std::string>("format_string") = "R16";
          asset_load_req->_on_event("onMipLoad"_crcu,data);
        }
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_R16,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_RED,          // format
            GL_UNSIGNED_BYTE, // datatype
            level._data->data());
        break;
      case EBufferFormat::RGBA16:
        if(asset_load_req and asset_load_req->_on_event){
          auto data = std::make_shared<varmap::VarMap>();
          data->makeValueForKey<int>("level") = imip;
          data->makeValueForKey<int>("width") = level._width;
          data->makeValueForKey<int>("height") = level._height;
          data->makeValueForKey<datablock_ptr_t>("data") = level._data;
          data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::RGBA16);
          data->makeValueForKey<std::string>("format_string") = "RGBA16";
          asset_load_req->_on_event("onMipLoad"_crcu,data);
        }
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_RGBA16,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_RGBA,          // format
            GL_UNSIGNED_BYTE, // datatype
            level._data->data());
        break;
      case EBufferFormat::BGR8:
        if(asset_load_req and asset_load_req->_on_event){
          auto data = std::make_shared<varmap::VarMap>();
          data->makeValueForKey<int>("level") = imip;
          data->makeValueForKey<int>("width") = level._width;
          data->makeValueForKey<int>("height") = level._height;
          data->makeValueForKey<datablock_ptr_t>("data") = level._data;
          data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::RGB8);
          data->makeValueForKey<std::string>("format_string") = "BGR8";
          asset_load_req->_on_event("onMipLoad"_crcu,data);
        }
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_RGB8,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_BGR,          // format
            GL_UNSIGNED_BYTE, // datatype
            level._data->data());
        break;
        case EBufferFormat::RGB8:
          if(asset_load_req and asset_load_req->_on_event){
            auto data = std::make_shared<varmap::VarMap>();
            data->makeValueForKey<int>("level") = imip;
            data->makeValueForKey<int>("width") = level._width;
            data->makeValueForKey<int>("height") = level._height;
            data->makeValueForKey<datablock_ptr_t>("data") = level._data;
            data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::RGB8);
            data->makeValueForKey<std::string>("format_string") = "RGB8";
            asset_load_req->_on_event("onMipLoad"_crcu,data);
          }
          glTexImage2D(         //
              GL_TEXTURE_2D,    // target
              imip,             // miplevel
              GL_RGB8,         // internalformat
              level._width,     // width
              level._height,    // height
              0,                // border
              GL_RGB,          // format
              GL_UNSIGNED_BYTE, // datatype
              level._data->data());
          break;
      case EBufferFormat::RGBA8:
        if(asset_load_req and asset_load_req->_on_event){
          auto data = std::make_shared<varmap::VarMap>();
          data->makeValueForKey<int>("level") = imip;
          data->makeValueForKey<int>("width") = level._width;
          data->makeValueForKey<int>("height") = level._height;
          data->makeValueForKey<datablock_ptr_t>("data") = level._data;
          data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::RGBA8);
          data->makeValueForKey<std::string>("format_string") = "RGBA8";
          asset_load_req->_on_event("onMipLoad"_crcu,data);
        }
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_RGBA8,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_RGBA,          // format
            GL_UNSIGNED_BYTE, // datatype
            level._data->data());
        break;
      case EBufferFormat::RGB16:{
        if(asset_load_req and asset_load_req->_on_event){
          auto data = std::make_shared<varmap::VarMap>();
          data->makeValueForKey<int>("level") = imip;
          data->makeValueForKey<int>("width") = level._width;
          data->makeValueForKey<int>("height") = level._height;
          data->makeValueForKey<datablock_ptr_t>("data") = level._data;
          data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::RGB16);
          data->makeValueForKey<std::string>("format_string") = "RGB16";
          asset_load_req->_on_event("onMipLoad"_crcu,data);
        }
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_RGB16,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_RGB,          // format
            GL_UNSIGNED_SHORT, // datatype
            level._data->data());
        break;
      }
#if !defined(__APPLE__)
      case EBufferFormat::RGBA_BPTC_UNORM:
        glCompressedTexImage2D( //
            GL_TEXTURE_2D,      //
            imip,               //
            GL_COMPRESSED_RGBA_BPTC_UNORM,
            level._width,
            level._height,
            0,
            level._data->length(),
            level._data->data());
        break;
#endif
      case EBufferFormat::RGB32F:
        printf( "unsupported format<RGB32F>\n");
        OrkAssert(false);
        break;
      case EBufferFormat::RGBA32F:
        if(asset_load_req and asset_load_req->_on_event){
          auto data = std::make_shared<varmap::VarMap>();
          data->makeValueForKey<int>("level") = imip;
          data->makeValueForKey<int>("width") = level._width;
          data->makeValueForKey<int>("height") = level._height;
          data->makeValueForKey<datablock_ptr_t>("data") = level._data;
          data->makeValueForKey<uint32_t>("format") = int(EBufferFormat::RGBA32F);
          data->makeValueForKey<std::string>("format_string") = "RGBA32F";
          asset_load_req->_on_event("onMipLoad"_crcu,data);
        }
        glTexImage2D(         //
            GL_TEXTURE_2D,    // target
            imip,             // miplevel
            GL_RGBA32F  ,         // internalformat
            level._width,     // width
            level._height,    // height
            0,                // border
            GL_RGBA,          // format
            GL_FLOAT, // datatype
            level._data->data());
        break;       
      case EBufferFormat::NONE:
        printf( "unsupported format<NONE>\n");
        OrkAssert(false);
        break;
      default:
        printf( "unsupported format<%llx>\n", (uint64_t)req.ptex->_texFormat);
        OrkAssert(false);
        break;
    }
    GL_ERRORCHECK();
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  GL_ERRORCHECK();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, inummips - 1);
  glto->_maxmip = inummips - 1;
  req.ptex->_num_mips = inummips;
  GL_ERRORCHECK();
  req.ptex->TexSamplingMode().PresetTrilinearWrap();
  this->ApplySamplingMode(req.ptex.get());
  req.ptex->_dirty = false;
  glBindTexture(GL_TEXTURE_2D, 0);
  ////////////////////////////////////////////////
  // done loading texture,
  //  perform postprocessing, if any..
  ////////////////////////////////////////////////

  if(req.ptex->_debugName== "filtenvmap-processed-specular"){
    //OrkAssert(false);
  }

  if (req.ptex->_vars->hasKey("postproc")) {
    auto dblock    = req._inpstream._datablock;
    auto postproc  = req.ptex->_vars->typedValueForKey<Texture::proc_t>("postproc").value();
    if(asset_load_req and asset_load_req->_on_event){
      asset_load_req->_on_event("beginPostProc"_crcu,nullptr);
    }
    auto postblock = postproc(req.ptex, &mTargetGL, dblock);
    if(asset_load_req and asset_load_req->_on_event){
      asset_load_req->_on_event("endPostProc"_crcu,nullptr);
    }
    OrkAssert(postblock);
  } else {
    // printf("ptex<%p> no postproc\n", ptex);
  }
  GL_ERRORCHECK();
  mTargetGL.debugPopGroup();
  req.ptex->_residenceState.fetch_or(1);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
