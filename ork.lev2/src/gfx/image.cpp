////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>
#include <ispc_texcomp.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <ork/file/chunkfile.inl>

OIIO_NAMESPACE_USING

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void Image::init(size_t w, size_t h, size_t numc) {
  _numcomponents = numc;
  _width         = w;
  _height        = h;
  _data          = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents);
}

///////////////////////////////////////////////////////////////////////////////

Image Image::clone() const {
  Image rval;
  rval._numcomponents = _numcomponents;
  rval._width         = _width;
  rval._height        = _height;
  rval._data          = std::make_shared<DataBlock>(_data->data(), _data->length());
  rval._debugName     = _debugName;
  return rval;
}

static fvec3 _image_deco(0.1, 0.2, 0.3);

///////////////////////////////////////////////////////////////////////////////

void Image::initFromInMemoryFile(std::string fmtguess, const void* srcdata, size_t srclen) {
  ImageSpec config;                                          // ImageSpec describing input configuration options
  Filesystem::IOMemReader memreader((void*)srcdata, srclen); // I/O proxy object
  void* ptr = &memreader;
  config.attribute("oiio:ioproxy", TypeDesc::PTR, &ptr);

  auto name = std::string("inmem.") + fmtguess;

  auto in               = ImageInput::open(name, &config);
  const ImageSpec& spec = in->spec();
  _width                = spec.width;
  _height               = spec.height;
  _numcomponents        = spec.nchannels;
  _data                 = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents);
  auto pixels = (uint8_t*)_data->data();
  in->read_image(TypeDesc::UINT8, &pixels[0]);
  in->close();

  // deco::printf(_image_deco, "///////////////////////////////////\n");
  // deco::printf(_image_deco, "// Image::initFromInMemoryFile()\n");
  // deco::printf(_image_deco, "// _width<%zu>\n", _width);
  // deco::printf(_image_deco, "// _height<%zu>\n", _height);
  // deco::printf(_image_deco, "// _numcomponents<%zu>\n", _numcomponents);
  // adeco::printf(_image_deco, "///////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

void Image::writeToFile(ork::file::Path outpath) const {
  auto cstrpath = outpath.c_str();
  auto out      = ImageOutput::create(cstrpath);
  if (!out)
    return;
  ImageSpec spec(_width, _height, _numcomponents, TypeDesc::UINT8);
  out->open(cstrpath, spec);
  out->write_image(TypeDesc::UINT8, _data->data());
  out->close();
}

///////////////////////////////////////////////////////////////////////////////

uint8_t* Image::pixel(int x, int y) {
  int index = (y * _width + x) * _numcomponents;
  return ((uint8_t*)_data->data()) + index;
}
const uint8_t* Image::pixel(int x, int y) const {
  int index = (y * _width + x) * _numcomponents;
  return ((const uint8_t*)_data->data()) + index;
}

///////////////////////////////////////////////////////////////////////////////

void Image::downsample(Image& imgout) const {
  OrkAssert((_width & 1) == 0);
  OrkAssert((_height & 1) == 0);
  imgout.init(_width >> 1, _height >> 1, _numcomponents);
  auto inp_pixels = (const uint8_t*)_data->data();
  auto out_pixels = (uint8_t*)imgout._data->data();
  for (size_t y = 0; y < imgout._height; y++) {
    size_t ya = y * 2;
    size_t yb = ya + 1;
    for (size_t x = 0; x < imgout._width; x++) {
      size_t xa = x * 2;
      size_t xb = xa + 1;

      auto outpixel     = imgout.pixel(x, y);
      auto inppixelXAYA = pixel(xa, ya);
      auto inppixelXBYA = pixel(xb, ya);
      auto inppixelXAYB = pixel(xa, yb);
      auto inppixelXBYB = pixel(xb, yb);
      for (size_t c = 0; c < _numcomponents; c++) {
        double xaya  = double(inppixelXAYA[c]);
        double xbya  = double(inppixelXBYA[c]);
        double xayb  = double(inppixelXAYB[c]);
        double xbyb  = double(inppixelXBYB[c]);
        double avg   = (xaya + xbya + xayb + xbyb) * 0.25;
        uint8_t uavg = uint8_t(avg);
        outpixel[c]  = uavg;
      }
    }
  }
  imgout._debugName = _debugName + "_ds";
  // auto pathr        = FormatString("%s.png", imgout._debugName.c_str());
  // auto path         = file::Path::temp_dir() / pathr;
  // writeToFile(path);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
  // deco::printf(_image_deco, "// Image::downsample(%s)\n", imgout._debugName.c_str());
  // deco::printf(_image_deco, "// imgout._width<%zu>\n", imgout._width);
  // deco::printf(_image_deco, "// imgout._height<%zu>\n", imgout._height);
  // deco::printf(_image_deco, "// imgout._numcomponents<%zu>\n", imgout._numcomponents);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

void Image::convertToRGBA(Image& imgout) const {
  imgout.init(_width, _height, 4);
  auto inp_pixels = (const uint8_t*)_data->data();
  auto out_pixels = (uint8_t*)imgout._data->data();
  switch (_numcomponents) {
    case 3:
      for (size_t y = 0; y < imgout._height; y++) {
        for (size_t x = 0; x < imgout._width; x++) {
          auto inppixel = pixel(x, y);
          auto outpixel = imgout.pixel(x, y);
          outpixel[0]   = inppixel[0];
          outpixel[1]   = inppixel[1];
          outpixel[2]   = inppixel[2];
          outpixel[3]   = 0xff;
        }
      }
      break;
    case 4:
      memcpy(out_pixels, inp_pixels, _width * _height * 4);
      break;
    default:
      OrkAssert(false);
      break;
  }
  imgout._debugName = _debugName + "_2rgba";
  // auto pathr        = FormatString("%s.png", imgout._debugName.c_str());
  // auto path         = file::Path::temp_dir() / pathr;
  // imgout.writeToFile(path);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
  // deco::printf(_image_deco, "// Image::convertToRGBA(%s)\n", imgout._debugName.c_str());
  // deco::printf(_image_deco, "// imgout._width<%zu>\n", imgout._width);
  // deco::printf(_image_deco, "// imgout._height<%zu>\n", imgout._height);
  // deco::printf(_image_deco, "// imgout._numcomponents<%zu>\n", imgout._numcomponents);
  // deco::printf(_image_deco, "///////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

void Image::compressBC7(CompressedImage& imgout) const {
  deco::printf(_image_deco, "///////////////////////////////////\n");
  deco::printf(_image_deco, "// Image::compressBC7(%s)\n", _debugName.c_str());
  deco::printf(_image_deco, "// imgout._width<%zu>\n", _width);
  deco::printf(_image_deco, "// imgout._height<%zu>\n", _height);
  imgout._format               = EBufferFormat::RGBA_BPTC_UNORM;
  bool width_is_multiple_of_4  = ((_width & 3) == 0);
  bool height_is_multiple_of_4 = ((_height & 3) == 0);
  OrkAssert((_numcomponents == 3) or (_numcomponents == 4));
  imgout._width          = _width;
  imgout._height         = _height;
  imgout._blocked_width  = _width;
  imgout._blocked_height = _height;
  imgout._numcomponents  = 4;
  //////////////////////////////////////////////////////////////////
  // pad images which do not line up with block sizes
  //////////////////////////////////////////////////////////////////
  if (not(width_is_multiple_of_4 and height_is_multiple_of_4)) {
    OrkAssert(false);
  }
  //////////////////////////////////////////////////////////////////
  imgout._data = std::make_shared<DataBlock>();
  auto dest    = (uint8_t*)imgout._data->allocateBlock(_width * _height);
  bc7_enc_settings settings;
  GetProfile_alpha_basic(&settings);

  Image src_as_rgba;
  convertToRGBA(src_as_rgba);

  rgba_surface surface;
  surface.width  = _width;
  surface.height = _height;
  surface.stride = _width * 4;
  surface.ptr    = (uint8_t*)src_as_rgba._data->data();
  ork::Timer timer;
  timer.Start();
  CompressBlocksBC7(&surface, dest, &settings);
  float time = timer.SecsSinceStart();
  float MPPS = float(_width * _height) * 1e-6 / time;
  deco::printf(_image_deco, "// compression time<%g> MPPS<%g>\n", time, MPPS);
  deco::printf(_image_deco, "///////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

CompressedImageMipChain Image::compressedMipChainBC7() const {
  CompressedImageMipChain rval;
  rval._width         = _width;
  rval._height        = _height;
  rval._format        = EBufferFormat::RGBA_BPTC_UNORM;
  rval._numcomponents = 4;
  Image imga          = this->clone();
  Image imgb;
  int mipindex = 0;
  while ((imga._width >= 4) and (imga._height >= 4)) {
    CompressedImage cimg;
    imga.compressBC7(cimg);
    rval._levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
    mipindex++;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

constexpr size_t KXTXVERSION = "xtx-ver0"_crcu;

void CompressedImageMipChain::writeXTX(datablockptr_t& out_datablock) {
  //////////////////////////////////////////
  chunkfile::Writer chunkwriter("xtx");
  auto hdrstream = chunkwriter.AddStream("header");
  auto imgstream = chunkwriter.AddStream("image");
  hdrstream->AddItem<size_t>(KXTXVERSION);
  hdrstream->AddItem<size_t>(_width);
  hdrstream->AddItem<size_t>(_height);
  hdrstream->AddItem<size_t>(_depth);
  hdrstream->AddItem<size_t>(_numcomponents);
  hdrstream->AddItem<EBufferFormat>(_format);
  hdrstream->AddItem<size_t>(_levels.size());
  hdrstream->addVarMap(_varmap, chunkwriter);
  //////////////////////////////////////////
  OrkAssert(_depth == 1); // only 2D for now..
  //////////////////////////////////////////
  for (size_t levidx = 0; levidx < _levels.size(); levidx++) {
    const auto& level = _levels[levidx];
    hdrstream->AddItem<size_t>(levidx);
    hdrstream->AddItem<size_t>(level._width);
    hdrstream->AddItem<size_t>(level._height);

    size_t mipbase   = imgstream->GetSize();
    auto mipdata     = (const void*)level._data->data();
    size_t miplength = level._data->length();

    hdrstream->AddItem<size_t>(mipbase);
    hdrstream->AddItem<size_t>(miplength);
    imgstream->AddData(mipdata, miplength);
  }
  chunkwriter.writeToDataBlock(out_datablock);
}

///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::writeXTX(const file::Path& outpath) {
  //////////////////////////////////////////
  chunkfile::Writer chunkwriter("xtx");
  auto hdrstream = chunkwriter.AddStream("header");
  auto imgstream = chunkwriter.AddStream("image");
  hdrstream->AddItem<size_t>(KXTXVERSION);
  hdrstream->AddItem<size_t>(_width);
  hdrstream->AddItem<size_t>(_height);
  hdrstream->AddItem<size_t>(_depth);
  hdrstream->AddItem<size_t>(_numcomponents);
  hdrstream->AddItem<EBufferFormat>(_format);
  hdrstream->AddItem<size_t>(_levels.size());
  hdrstream->addVarMap(_varmap, chunkwriter);
  //////////////////////////////////////////
  OrkAssert(_depth == 1); // only 2D for now..
  //////////////////////////////////////////
  for (size_t levidx = 0; levidx < _levels.size(); levidx++) {
    const auto& level = _levels[levidx];
    hdrstream->AddItem<size_t>(levidx);
    hdrstream->AddItem<size_t>(level._width);
    hdrstream->AddItem<size_t>(level._height);

    size_t mipbase   = imgstream->GetSize();
    auto mipdata     = (const void*)level._data->data();
    size_t miplength = level._data->length();

    hdrstream->AddItem<size_t>(mipbase);
    hdrstream->AddItem<size_t>(miplength);
    imgstream->AddData(mipdata, miplength);
  }
  chunkwriter.WriteToFile(outpath);
}

///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::readXTX(const file::Path& inppath) {
  auto dblock = datablockFromFileAtPath(inppath);
  if (dblock)
    readXTX(dblock);
}

///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::readXTX(datablockptr_t datablock) {
  //////////////////////////////////////////
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(datablock, allocator);
  OrkAssert(chunkreader._chunkfiletype == "xtx");
  if (chunkreader.IsOk()) {
    auto hdrstream     = chunkreader.GetStream("header");
    auto imgstream     = chunkreader.GetStream("image");
    size_t xtx_version = 0;
    size_t numlevels   = 0;
    hdrstream->GetItem<size_t>(xtx_version);
    OrkAssert(xtx_version == KXTXVERSION);
    hdrstream->GetItem<size_t>(_width);
    hdrstream->GetItem<size_t>(_height);
    hdrstream->GetItem<size_t>(_depth);
    hdrstream->GetItem<size_t>(_numcomponents);
    hdrstream->GetItem<EBufferFormat>(_format);
    hdrstream->GetItem<size_t>(numlevels);
    hdrstream->getVarMap(_varmap, chunkreader);
    //////////////////////////////////////////
    OrkAssert(_depth == 1); // only 2D for now..
    //////////////////////////////////////////
    for (size_t levidx = 0; levidx < numlevels; levidx++) {
      CompressedImage level;
      size_t lidx      = 0;
      size_t mipbase   = 0;
      size_t miplength = 0;
      hdrstream->GetItem<size_t>(lidx);
      hdrstream->GetItem<size_t>(level._width);
      hdrstream->GetItem<size_t>(level._height);
      hdrstream->GetItem<size_t>(mipbase);
      hdrstream->GetItem<size_t>(miplength);
      auto mipdata = imgstream->GetDataAt(mipbase);
      level._data  = std::make_shared<DataBlock>(mipdata, miplength);
      _levels.push_back(level);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
