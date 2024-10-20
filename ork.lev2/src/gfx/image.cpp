////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/texman.h>
#include <math.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/memcpy.inl>

#if defined(ENABLE_ISPC)
#include <ispc_texcomp.h>
#endif

OIIO_NAMESPACE_USING

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

CompressedImage::CompressedImage() {
  _vars = std::make_shared<varmap::VarMap>();
}

void Image::init(size_t w, size_t h, size_t numc, int bpc) {
  _numcomponents   = numc;
  _width           = w;
  _height          = h;
  _bytesPerChannel = bpc;
  _data            = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents * bpc);
}

///////////////////////////////////////////////////////////////////////////////

Image Image::clone() const {
  Image rval;
  rval._format          = _format;
  rval._bytesPerChannel = _bytesPerChannel;
  rval._numcomponents   = _numcomponents;
  rval._width           = _width;
  rval._height          = _height;
  rval._data            = std::make_shared<DataBlock>(_data->data(), _data->length());
  rval._debugName       = _debugName;
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
  switch (spec.format.basetype) {
    case TypeDesc::UINT8:
      _bytesPerChannel = 1;
      switch (_numcomponents) {
        case 1:
          _format = EBufferFormat::R8;
          break;
        case 3:
          _format = EBufferFormat::RGB8;
          break;
        case 4:
          _format = EBufferFormat::RGBA8;
          break;
        default:
          OrkAssert(false);
          return;
      }
      break;
    case TypeDesc::UINT16:
      _bytesPerChannel = 2;
      switch (_numcomponents) {
        case 1:
          _format = EBufferFormat::R16;
          break;
        case 3:
          _format = EBufferFormat::RGB16;
          break;
        case 4:
          _format = EBufferFormat::RGBA16;
          break;
        default:
          OrkAssert(false);
          return;
      }
      break;
    default:
      OrkAssert(false);
      return;
  }

  _data = std::make_shared<DataBlock>();
  _data->allocateBlock(_width * _height * _numcomponents * _bytesPerChannel);
  auto pixels = (uint8_t*)_data->data();
  if (_bytesPerChannel == 1) {
    in->read_image(TypeDesc::UINT8, pixels);
  } else if (_bytesPerChannel == 2) {
    in->read_image(TypeDesc::UINT16, pixels);
  }
  in->close();

  if (1) {
    deco::printf(_image_deco, "///////////////////////////////////\n");
    deco::printf(_image_deco, "// Image::initFromInMemoryFile()\n");
    deco::printf(_image_deco, "// _width<%zu>\n", _width);
    deco::printf(_image_deco, "// _height<%zu>\n", _height);
    deco::printf(_image_deco, "// _numcomponents<%zu>\n", _numcomponents);
    deco::printf(_image_deco, "// _bytesPerChannel<%d>\n", _bytesPerChannel);
    deco::printf(_image_deco, "///////////////////////////////////\n");
  }
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

uint8_t* Image::pixel8(int x, int y) {
  int index = (y * _width + x) * _numcomponents;
  return ((uint8_t*)_data->data()) + index;
}
const uint8_t* Image::pixel8(int x, int y) const {
  int index = (y * _width + x) * _numcomponents;
  return ((const uint8_t*)_data->data()) + index;
}
uint16_t* Image::pixel16(int x, int y) {
  int index = (y * _width + x) * _numcomponents;
  return ((uint16_t*)_data->data()) + index;
}
const uint16_t* Image::pixel16(int x, int y) const {
  int index = (y * _width + x) * _numcomponents;
  return ((const uint16_t*)_data->data()) + index;
}

///////////////////////////////////////////////////////////////////////////////

void Image::downsample(Image& imgout) const {
  imgout.init(_width >> 1, _height >> 1, _numcomponents, _bytesPerChannel);
  for (size_t y = 0; y < imgout._height; y++) {
    size_t ya = y * 2;
    size_t yb = ya + 1;
    if (yb > (_height - 1))
      yb = _height - 1;
    for (size_t x = 0; x < imgout._width; x++) {
      size_t xa = x * 2;
      size_t xb = xa + 1;
      if (xb > (_width - 1))
        xb = _width - 1;

      switch (_format) {
        case EBufferFormat::R8:
        case EBufferFormat::RGB8:
        case EBufferFormat::RGBA8: {
          auto outpixel     = imgout.pixel8(x, y);
          auto inppixelXAYA = pixel8(xa, ya);
          auto inppixelXBYA = pixel8(xb, ya);
          auto inppixelXAYB = pixel8(xa, yb);
          auto inppixelXBYB = pixel8(xb, yb);
          for (size_t c = 0; c < _numcomponents; c++) {
            double xaya  = double(inppixelXAYA[c]);
            double xbya  = double(inppixelXBYA[c]);
            double xayb  = double(inppixelXAYB[c]);
            double xbyb  = double(inppixelXBYB[c]);
            double avg   = (xaya + xbya + xayb + xbyb) * 0.25;
            uint8_t uavg = uint8_t(avg);
            outpixel[c]  = uavg;
          }
          break;
        }
        case EBufferFormat::R16:
        case EBufferFormat::RGB16:
        case EBufferFormat::RGBA16: {
          auto outpixel     = imgout.pixel16(x, y);
          auto inppixelXAYA = pixel16(xa, ya);
          auto inppixelXBYA = pixel16(xb, ya);
          auto inppixelXAYB = pixel16(xa, yb);
          auto inppixelXBYB = pixel16(xb, yb);
          for (size_t c = 0; c < _numcomponents; c++) {
            double xaya   = double(inppixelXAYA[c]);
            double xbya   = double(inppixelXBYA[c]);
            double xayb   = double(inppixelXAYB[c]);
            double xbyb   = double(inppixelXBYB[c]);
            double avg    = (xaya + xbya + xayb + xbyb) * 0.25;
            uint16_t uavg = uint16_t(avg);
            outpixel[c]   = uavg;
          }
          break;
        }
        default:
          printf("UNKNOWN FORMAT<%08x>\n", _format);
          OrkAssert(false);
          break;
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

void Image::initRGBA8WithNormalizedFloatBuffer(size_t w, size_t h, size_t numc, const float* buffer) {
  switch (numc) {
    case 1:
      _format = EBufferFormat::R8;
      break;
    case 3:
      _format = EBufferFormat::RGB8;
      break;
    case 4:
      _format = EBufferFormat::RGBA8;
      break;
    default:
      OrkAssert(false);
      break;
  }
  _bytesPerChannel = 1;
  init(w, h, numc, 1);
  auto outptr = (uint8_t*)_data->data();
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int pixelindex = y * w + x;
      int elembase   = pixelindex * numc;
      for (int c = 0; c < numc; c++) {
        outptr[elembase + c] = uint8_t(buffer[elembase + c] * 255.0f);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Image::convertToRGBA(Image& imgout, bool force_8bc) const {

  if (force_8bc) {
    imgout.init(_width, _height, 4, 1);
  } else {
    imgout.init(_width, _height, 4, _bytesPerChannel);
  }

  switch (_numcomponents) {
    case 1:
      if (_bytesPerChannel == 1) {
        auto inp_pixels = (const uint8_t*)_data->data();
        auto out_pixels = (uint8_t*)imgout._data->data();
        for (size_t y = 0; y < imgout._height; y++) {
          for (size_t x = 0; x < imgout._width; x++) {
            auto inppixels = pixel8(x, y);
            auto outpixels = imgout.pixel8(x, y);
            outpixels[0]   = inppixels[0];
            outpixels[1]   = inppixels[0];
            outpixels[2]   = inppixels[0];
            outpixels[3]   = 0xff;
          }
        }
      } else if (_bytesPerChannel == 2) {
        auto inp_pixels = (const uint16_t*)_data->data();
        if (force_8bc) {
          auto out_pixels = (uint8_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel8(x, y);
              outpixels[0]   = uint8_t(inppixels[0] >> 8);
              outpixels[1]   = uint8_t(inppixels[0] >> 8);
              outpixels[2]   = uint8_t(inppixels[0] >> 8);
              outpixels[3]   = 0xff;
            }
          }
        } else {
          auto out_pixels = (uint16_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel16(x, y);
              outpixels[0]   = inppixels[0];
              outpixels[1]   = inppixels[0];
              outpixels[2]   = inppixels[0];
              outpixels[3]   = 0xffff;
            }
          }
        }
      }
      break;
    case 3:
      if (_bytesPerChannel == 1) {
        auto inp_pixels = (const uint8_t*)_data->data();
        auto out_pixels = (uint8_t*)imgout._data->data();
        for (size_t y = 0; y < imgout._height; y++) {
          for (size_t x = 0; x < imgout._width; x++) {
            auto inppixels = pixel8(x, y);
            auto outpixels = imgout.pixel8(x, y);
            outpixels[0]   = inppixels[0];
            outpixels[1]   = inppixels[1];
            outpixels[2]   = inppixels[2];
            outpixels[3]   = 0xff;
          }
        }
      } else if (_bytesPerChannel == 2) {
        auto inp_pixels = (const uint16_t*)_data->data();
        if (force_8bc) {
          auto out_pixels = (uint8_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel8(x, y);
              outpixels[0]   = uint8_t(inppixels[0] >> 8);
              outpixels[1]   = uint8_t(inppixels[1] >> 8);
              outpixels[2]   = uint8_t(inppixels[2] >> 8);
              outpixels[3]   = 0xff;
            }
          }
        } else {
          auto out_pixels = (uint16_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel16(x, y);
              outpixels[0]   = inppixels[0];
              outpixels[1]   = inppixels[1];
              outpixels[2]   = inppixels[2];
              outpixels[3]   = 0xffff;
            }
          }
        }
      }
      break;
    case 4:
      if (_bytesPerChannel == 1) {
        auto inp_pixels = (const uint8_t*)_data->data();
        auto out_pixels = (uint8_t*)imgout._data->data();
        memcpy_fast(out_pixels, inp_pixels, _width * _height * 4);
      } else if (_bytesPerChannel == 2) {
        if (force_8bc) {
          auto out_pixels = (uint8_t*)imgout._data->data();
          for (size_t y = 0; y < imgout._height; y++) {
            for (size_t x = 0; x < imgout._width; x++) {
              auto inppixels = pixel16(x, y);
              auto outpixels = imgout.pixel8(x, y);
              outpixels[0]   = uint8_t(inppixels[0] >> 8);
              outpixels[1]   = uint8_t(inppixels[1] >> 8);
              outpixels[2]   = uint8_t(inppixels[2] >> 8);
              outpixels[3]   = uint8_t(inppixels[3] >> 8);
            }
          }
        } else {
          auto inp_pixels16 = (const uint16_t*)_data->data();
          auto out_pixels16 = (uint16_t*)imgout._data->data();
          memcpy_fast(out_pixels16, inp_pixels16, _width * _height * 4 * 2);
        }
      }
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

void Image::compressDefault(CompressedImage& imgout) const {
#if defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
  uncompressed(imgout);
#else
  if (GfxEnv::supportsBC7()) {
    compressBC7(imgout);
  } else {
    uncompressed(imgout);
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

CompressedImageMipChain Image::compressedMipChainDefault() const {
#if defined(__APPLE__) or defined(ORK_ARCHITECTURE_ARM_64)
  return uncompressedMipChain();
#else
  if (GfxEnv::supportsBC7()) {
    return compressedMipChainBC7();
  } else {
    return uncompressedMipChain();
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_ISPC)

void Image::compressBC7(CompressedImage& imgout) const {
  deco::printf(_image_deco, "///////////////////////////////////\n");
  deco::printf(_image_deco, "// Image::compressBC7(%s)\n", _debugName.c_str());
  deco::printf(_image_deco, "// imgout._width<%zu>\n", _width);
  deco::printf(_image_deco, "// imgout._height<%zu>\n", _height);
  imgout._format = EBufferFormat::RGBA_BPTC_UNORM;
  OrkAssert((_numcomponents == 1) or (_numcomponents == 3) or (_numcomponents == 4));
  imgout._width          = _width;
  imgout._height         = _height;
  imgout._blocked_width  = (_width + 3) & 0xfffffffc;
  imgout._blocked_height = (_height + 3) & 0xfffffffc;
  imgout._numcomponents  = 4;
  //////////////////////////////////////////////////////////////////
  imgout._data = std::make_shared<DataBlock>();

  Image src_as_rgba;
  convertToRGBA(src_as_rgba, true);

  ork::Timer timer;
  timer.Start();
  ////////////////////////////////////////
  // parallel ISPC-BC7 compressor
  ////////////////////////////////////////
  std::atomic<int> pending = 0;
  // auto opgroup      = opq::createCompletionGroup(opq::concurrentQueue(), "BC7ENC");

  size_t src_len = src_as_rgba._data->length();
  size_t dst_len = imgout._blocked_width * imgout._blocked_height;

  auto src_base     = (uint8_t*)src_as_rgba._data->data();
  auto dst_base     = (uint8_t*)imgout._data->allocateBlock(dst_len);
  auto dst_iter = dst_base;
  auto src_iter = src_base;
  size_t src_stride = _width * 4; // 4 BPP
  size_t dst_stride = _width;     // 1 BPP

  size_t num_rows_per_operation = 4;

  for (int y = 0; y < _height; y += num_rows_per_operation) {
    pending.fetch_add(1);
  }
  for (int y = 0; y < _height; y += num_rows_per_operation) {
    int num_rows_this_operation = std::min(num_rows_per_operation, _height - y);
    opq::concurrentQueue()->enqueue([=, &pending]() {
      bc7_enc_settings settings;
      GetProfile_alpha_basic(&settings);
      rgba_surface surface;
      surface.width  = _width;
      surface.height = num_rows_this_operation;
      surface.stride = src_stride;
      surface.ptr    = src_iter;

      CompressBlocksBC7(&surface, dst_iter, &settings);
      pending.fetch_add(-1);
    });
    src_iter += src_stride * num_rows_this_operation;
    dst_iter += dst_stride * num_rows_this_operation;
    ptrdiff_t src_offset = (src_iter - src_base) / 16;
    ptrdiff_t dst_offset = (dst_iter - dst_base) / 16;
    OrkAssert(src_offset<=src_len);
    OrkAssert(dst_offset<=dst_len);
  }
  while (pending.load() > 0) {
    usleep(1000);
  }
  ////////////////////////////////////////

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

#endif

///////////////////////////////////////////////////////////////////////////////

void Image::uncompressed(CompressedImage& imgout) const {
  deco::printf(
      _image_deco,
      "// Image::uncompressed(%s) w<%zu> h<%zu> BPC<%d> _format<%x>\n",
      _debugName.c_str(),
      _width,
      _height,
      _bytesPerChannel,
      _format);
  imgout._format = _format;
  OrkAssert((_numcomponents == 1) or (_numcomponents == 3) or (_numcomponents == 4));
  imgout._width          = _width;
  imgout._height         = _height;
  imgout._blocked_width  = 0;
  imgout._blocked_height = 0;
  imgout._numcomponents  = _numcomponents;
  imgout._data           = std::make_shared<DataBlock>();
  size_t data_size       = _width * _height * _numcomponents * _bytesPerChannel;
  ork::Timer timer;
  timer.Start();
  std::atomic<int> pending = 0;
  size_t src_stride        = _width * _numcomponents * _bytesPerChannel;
  size_t dst_stride        = src_stride;
  auto src_base            = (uint8_t*)this->_data->data();
  auto dst_base            = (uint8_t*)imgout._data->allocateBlock(data_size);
  std::memcpy(dst_base, src_base, data_size);

  float time = timer.SecsSinceStart();
  float MPPS = float(_width * _height) * 1e-6 / time;
  deco::printf(_image_deco, "// compression time<%g> MPPS<%g>\n", time, MPPS);
}

///////////////////////////////////////////////////////////////////////////////

CompressedImageMipChain Image::uncompressedMipChain() const {
  CompressedImageMipChain rval;
  rval._width           = _width;
  rval._height          = _height;
  rval._format          = _format;
  rval._numcomponents   = 4;
  rval._bytesPerChannel = _bytesPerChannel;
  Image imga            = this->clone();
  Image imgb;
  int mipindex = 0;
  while ((imga._width >= 4) and (imga._height >= 4)) {
    CompressedImage cimg;
    imga.uncompressed(cimg);
    rval._levels.push_back(cimg);
    imgb = imga;
    imgb.downsample(imga);
    mipindex++;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::initWithPrecompressedMipLevels(miplevels_t levels) {
  _levels          = levels;
  _width           = levels[0]._width;
  _height          = levels[0]._height;
  _format          = levels[0]._format;
  _numcomponents   = levels[0]._numcomponents;
  _bytesPerChannel = levels[0]._bytesPerChannel;
}

///////////////////////////////////////////////////////////////////////////////

constexpr size_t KXTXVERSION = "xtx-ver0"_crcu;

void CompressedImageMipChain::writeXTX(datablock_ptr_t& out_datablock) {
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

void CompressedImageMipChain::readXTX(datablock_ptr_t datablock) {
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
