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

bool TextureInterface::LoadTexture(const AssetPath& fname, texture_ptr_t ptex) {
  AssetPath DdsFilename = fname;
  AssetPath PngFilename = fname;
  AssetPath XtxFilename = fname;
  DdsFilename.setExtension("dds");
  PngFilename.setExtension("png");
  XtxFilename.setExtension("xtx");
  ptex->_debugName = fname.toStdString();
  AssetPath final_fname;
  if (FileEnv::GetRef().DoesFileExist(PngFilename))
    final_fname = PngFilename;
  if (FileEnv::GetRef().DoesFileExist(DdsFilename))
    final_fname = DdsFilename;
  if (FileEnv::GetRef().DoesFileExist(XtxFilename))
    final_fname = XtxFilename;

  printf("fname<%s>\n", fname.c_str());
  printf("final_fname<%s>\n", final_fname.c_str());
  if (auto dblock = datablockFromFileAtPath(final_fname))
    return LoadTexture(ptex, dblock);
  else
    return false;
}

void TextureInterface::SaveTexture(const ork::AssetPath& fname, Texture* ptex) {

}

///////////////////////////////////////////////////////////////////////////////

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
      length *= 3;
      break;

    case EBufferFormat::R16:
      length *= 2;
      break;

    case EBufferFormat::R32F:
    case EBufferFormat::RG16F:
    case EBufferFormat::RGB10A2:
    case EBufferFormat::RGBA8:
    case EBufferFormat::Z32:
    case EBufferFormat::Z24S8:
      length *= 4;
      break;
    case EBufferFormat::RG32F:
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
      length *= 8;
      break;
    case EBufferFormat::RGBA32F:
    case EBufferFormat::RGB32UI:
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
      length *= 3;
      break;

    case EBufferFormat::R16:
      length *= 2;
      break;

    case EBufferFormat::R32F:
    case EBufferFormat::RG16F:
    case EBufferFormat::RGB10A2:
    case EBufferFormat::RGBA8:
    case EBufferFormat::Z32:
    case EBufferFormat::Z24S8:
      length *= 4;
      break;
    case EBufferFormat::RG32F:
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
      length *= 8;
      break;
    case EBufferFormat::RGBA32F:
    case EBufferFormat::RGB32UI:
      length *= 16;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return length;
}

texture_ptr_t TextureInterface::createColorTexture(fvec4 color, int w, int h){
  auto rval = std::make_shared<Texture>();

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

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
