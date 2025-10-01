////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

TextureInterface::TextureInterface(context_rawptr_t ctx)
  : _ctx(ctx){
    
  }

///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::LoadTexture(texture_ptr_t ptex, datablock_ptr_t datablock) {
  DataBlockInputStream checkstream(datablock);
  uint32_t magic = checkstream.getItem<uint32_t>();
  bool ok        = false;
  if (Char4("chkf") == Char4(magic)){
    if(0)printf("TXI::LoadTexture loading as xtx\n");
    ptex->_source = ETextureSource::FROM_XTX;
    ok = _loadXTXTexture(ptex, datablock);
  }
  else if (Char4("DDS ") == Char4(magic)){
    if(0)printf("TXI::LoadTexture loading as dds\n");
    ptex->_source = ETextureSource::FROM_DDS;
    ok = _loadDDSTexture(ptex, datablock);
  }
  else {
    if(0)printf("TXI::LoadTexture loading as generic\n");
    ptex->_source = ETextureSource::FROM_IMAGE;
    ok = _loadImageTexture(ptex, datablock);
  }

  ptex->_contentHash = datablock->hash();

  return ok;
}

///////////////////////////////////////////////////////////////////////////////

bool TextureInterface::LoadTexture(const AssetPath& fname, texture_ptr_t ptex) {
  AssetPath DdsFilename = fname;
  AssetPath PngFilename = fname;
  AssetPath XtxFilename = fname;
  DdsFilename.setExtension("dds");
  PngFilename.setExtension("png");
  XtxFilename.setExtension("xtx");
  ptex->_debugName = fname.toStdString();
  ptex->_source = ETextureSource::FROM_ASSET;
  AssetPath final_fname;
  if (FileEnv::GetRef().DoesFileExist(PngFilename))
    final_fname = PngFilename;
  if (FileEnv::GetRef().DoesFileExist(DdsFilename))
    final_fname = DdsFilename;
  if (FileEnv::GetRef().DoesFileExist(XtxFilename))
    final_fname = XtxFilename;

  //printf("TXI::LoadTexture fname<%s>\n", fname.c_str());
  //printf("TXI::LoadTexture final_fname<%s>\n", final_fname.c_str());
  if (auto dblock = datablockFromFileAtPath(final_fname)){
    //printf("TXI::LoadTexture dblock<%p>\n", (void*) dblock.get());
    return LoadTexture(ptex, dblock);
  }
  else
    return false;
}

///////////////////////////////////////////////////////////////////////////////

void TextureInterface::SaveTexture(const ork::AssetPath& fname, Texture* ptex) {

}

///////////////////////////////////////////////////////////////////////////////

texture_ptr_t TextureInterface::createColorTexture(fvec4 color, int w, int h){
  auto rval = std::make_shared<Texture>();
  rval->_source = ETextureSource::FROM_DEFAULT;

  int numpixels = (w*h);
  auto data = new uint32_t[numpixels];
  auto swizzled = color.ABGRU32();
  for( int i=0; i<numpixels; i++ ){
    data[i] = swizzled;
  }

  TextureInitData tid;
  tid._w = w;
  tid._h = h;
  tid._src_format = EBufferFormat::RGBA8;
  tid._dst_format = EBufferFormat::RGBA8;
  tid._autogenmips = false;
  //tid._allow_async = false;
  tid._data = (const void*) data;

  initTextureFromData(rval.get(),tid);

  delete[] data;

  return rval;
}

texture_ptr_t TextureInterface::createColorTextureV3(fvec3 color, int w, int h){
  auto rval = std::make_shared<Texture>();
  rval->_source = ETextureSource::FROM_DEFAULT;

  int numpixels = (w*h);
  auto data = new uint8_t[numpixels*3];
  uint8_t r = uint8_t(color.x*255.0f);
  uint8_t g = uint8_t(color.y*255.0f);
  uint8_t b = uint8_t(color.z*255.0f);
  for( int i=0; i<numpixels; i++ ){
    data[i*3+0] = r;
    data[i*3+1] = g;
    data[i*3+2] = b;
  }

  TextureInitData tid;
  tid._w = w;
  tid._h = h;
  tid._src_format = EBufferFormat::BGR8;
  tid._dst_format = EBufferFormat::BGR8;
  tid._autogenmips = false;
  //tid._allow_async = false;
  tid._data = (const void*) data;

  initTextureFromData(rval.get(),tid);

  delete[] data;

  return rval;
}

texturearray_ptr_t TextureInterface::createColorTextureV3Array(fvec3 color, int w, int h, int d){

  auto image = std::make_shared<Image>();
  image->initRGB8WithColor(w,h,color);

  TextureArrayInitData TID;


  TID._slices.resize(d);
  for(uint32_t i=0; i<d; i++){
    TID._slices[i] = TextureArrayInitSubItem{i, image};
  }
  auto array = std::make_shared<TextureArray>();
  auto tex = array->_tex;
  tex->_debugName = "tidtexarray";
  initTextureArray2DFromData(array.get(), TID);

  return array;
}

texture_ptr_t TextureInterface::createColorCubeTexture(fvec4 color, int w, int h){
  auto rval = std::make_shared<Texture>();
  rval->_source = ETextureSource::FROM_DEFAULT;
  rval->_texType = ETEXTYPE_CUBE;

  // Cube textures need data for all 6 faces
  int numpixels_per_face = (w*h);
  int total_pixels = numpixels_per_face * 6; // 6 faces for cube
  auto data = new uint32_t[total_pixels];
  auto swizzled = color.ABGRU32();
  for( int i=0; i<total_pixels; i++ ){
    data[i] = swizzled;
  }

  TextureInitData tid;
  tid._initCubeTexture = true;
  tid._w = w;
  tid._h = h;
  tid._d = 6; // Set depth to 6 for cube textures
  tid._src_format = EBufferFormat::RGBA8;
  tid._dst_format = EBufferFormat::RGBA8;
  tid._autogenmips = true;
  //tid._allow_async = false;
  tid._data = (const void*) data;

  initTextureFromData(rval.get(),tid);
  delete[] data;

  return rval;
}

size_t TextureInitData::computeSrcSize() const {
  size_t length = _w * _h * _d;
  switch (_src_format) {

    case EBufferFormat::R8:
      length *= 1;
      break;
    case EBufferFormat::YUV420P:
      length = length+(length>>1);
      break;

    case EBufferFormat::RGB8:
    case EBufferFormat::BGR8:
      length *= 3;
      break;

    case EBufferFormat::R16UI:
      length *= 2;
      break;

    case EBufferFormat::R32F:
    case EBufferFormat::RG16F:
    case EBufferFormat::RGB10A2:
    case EBufferFormat::RGBA8:
    case EBufferFormat::Z32F:
    case EBufferFormat::Z24S8:
    case EBufferFormat::R32UI:
      length *= 4;
      break;
    case EBufferFormat::RG32F:
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
      length *= 8;
      break;
    case EBufferFormat::RGB32F:
    case EBufferFormat::RGB32UI:
      length *= 12;
      break;
    case EBufferFormat::RGBA32F:
    case EBufferFormat::RGBA32UI:
      length *= 16;
      break;
default:
      OrkAssert(false);
      break;
  }
  return length;
}
size_t TextureInitData::computeDstSize() const {
  size_t length = _w * _h * _d;
  switch (_dst_format) {

    case EBufferFormat::R8:
      length *= 1;
      break;


    case EBufferFormat::RGB8:
    case EBufferFormat::BGR8:
      length *= 3;
      break;

    case EBufferFormat::R16UI:
      length *= 2;
      break;

    case EBufferFormat::R32F:
    case EBufferFormat::RG16F:
    case EBufferFormat::RGB10A2:
    case EBufferFormat::RGBA8:
    case EBufferFormat::Z32F:
    case EBufferFormat::Z24S8:
    case EBufferFormat::R32UI:
      length *= 4;
      break;
    case EBufferFormat::RG32F:
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
      length *= 8;
      break;
    case EBufferFormat::RGB32F:
    case EBufferFormat::RGB32UI:
      length *= 12;
      break;
    case EBufferFormat::RGBA32F:
    case EBufferFormat::RGBA32UI:
      length *= 16;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return length;
}

///////////////////////////////////////////////////////////////////////////////

void TextureInterface::initTextureFromImage(Texture* ptex, image_ptr_t img) {
  TextureInitData tid;
  tid._w           = img->_width;
  tid._h           = img->_height;
  tid._d           = 1;
  image_ptr_t img_to_use = img;
  switch(img->_format) {
    case EBufferFormat::R8:
      img_to_use = std::make_shared<Image>();
      img_to_use->convertFromImageToFormat(*img,EBufferFormat::RGBA8);
      tid._src_format  = EBufferFormat::RGBA8;
      tid._dst_format  = EBufferFormat::RGBA8;
      break;
    case EBufferFormat::RGB8:
      img_to_use = std::make_shared<Image>();
      img_to_use->convertFromImageToFormat(*img,EBufferFormat::RGBA8);
      tid._src_format  = EBufferFormat::RGBA8;
      tid._dst_format  = EBufferFormat::RGBA8;
      break;
    case EBufferFormat::RGBA8:
      tid._src_format  = img->_format;
      tid._dst_format  = img->_format;
      break;
    case EBufferFormat::BGR8:
      img_to_use = std::make_shared<Image>();
      img_to_use->convertFromImageToFormat(*img,EBufferFormat::RGBA8);
      tid._src_format  = EBufferFormat::RGBA8;
      tid._dst_format  = EBufferFormat::RGBA8;
      break;
      break;
    default:
      OrkAssert(false); // unsupported image format
      break;
  }
  tid._autogenmips = false;
  tid._allow_async = false;
  tid._data        = (const void*) img_to_use->_data->data();
  initTextureFromData(ptex, tid);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
