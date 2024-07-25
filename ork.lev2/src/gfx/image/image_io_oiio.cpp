////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>
#include <math.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/memcpy.inl>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

extern logchannel_ptr_t logchan_image;

///////////////////////////////////////////////////////////////////////////////

bool Image::_initFromDataBlockPNG(datablock_ptr_t datablock) {
  return initFromInMemoryFile("png", datablock->data(), datablock->length());
}

///////////////////////////////////////////////////////////////////////////////

bool Image::initFromInMemoryFile( std::string fmtguess, //
                                  const void* srcdata,  //
                                  size_t srclen ) {     //
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
        case 3: {
          auto channames = spec.channelnames;
          if (channames[0] == "R" and channames[1] == "G" and channames[2] == "B")
            _format = EBufferFormat::RGB8;
          else if (channames[0] == "B" and channames[1] == "G" and channames[2] == "R")
            _format = EBufferFormat::BGR8;
          else
            OrkAssert(false);
          break;
        }
        case 4: {
          auto channames = spec.channelnames;
          if (channames[0] == "R" and channames[1] == "G" and channames[2] == "B" and channames[3] == "A")
            _format = EBufferFormat::RGBA8;
          else if (channames[0] == "B" and channames[1] == "G" and channames[2] == "R" and channames[3] == "A")
            _format = EBufferFormat::BGRA8;
          else
            OrkAssert(false);
          break;
        }
        default:
          OrkAssert(false);
          return false;
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
          return false;
      }
      break;
    default:
      OrkAssert(false);
      return false;
      
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
    logchan_image->log("///////////////////////////////////");
    logchan_image->log("// Image::initFromInMemoryFile()");
    logchan_image->log("// _width<%zu>", _width);
    logchan_image->log("// _height<%zu>", _height);
    logchan_image->log("// _numcomponents<%zu>", _numcomponents);
    logchan_image->log("// _bytesPerChannel<%d>", _bytesPerChannel);
    logchan_image->log("///////////////////////////////////");
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void Image::writeToFile(const ork::file::Path& outpath) const {
  auto cstrpath = outpath.c_str();
  auto out      = ImageOutput::create(cstrpath);
  if (!out)
    return;
  ImageSpec spec(_width, _height, _numcomponents, TypeDesc::UINT8);
  switch (_format) {
    case EBufferFormat::R8:
      spec.format       = TypeDesc::UINT8;
      spec.channelnames = {"R"};
      spec.nchannels    = 1;
      break;
    case EBufferFormat::RGB8:
      spec.format       = TypeDesc::UINT8;
      spec.nchannels    = 3;
      spec.channelnames = {"R", "G", "B"};
      break;
    case EBufferFormat::BGR8:
      spec.format       = TypeDesc::UINT8;
      spec.nchannels    = 3;
      spec.channelnames = {"B", "G", "R"};
      break;
    case EBufferFormat::RGBA8:
      spec.format       = TypeDesc::UINT8;
      spec.nchannels    = 4;
      spec.channelnames = {"R", "G", "B", "A"};
      break;
    case EBufferFormat::BGRA8:
      spec.format       = TypeDesc::UINT8;
      spec.nchannels    = 4;
      spec.channelnames = {"B", "G", "R", "A"};
      break;
    case EBufferFormat::R16:
      spec.format       = TypeDesc::UINT16;
      spec.nchannels    = 1;
      spec.channelnames = {"R"};
      break;
    case EBufferFormat::RGB16:
      spec.format       = TypeDesc::UINT16;
      spec.nchannels    = 3;
      spec.channelnames = {"R", "G", "B"};
      break;
    case EBufferFormat::RGBA16:
      spec.format       = TypeDesc::UINT16;
      spec.nchannels    = 4;
      spec.channelnames = {"R", "G", "B", "A"};
      break;
    default:
      OrkAssert(false);
      break;
  }

  out->open(cstrpath, spec);
  out->write_image(TypeDesc::UINT8, _data->data());
  out->close();
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
