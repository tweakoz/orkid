////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <mdspan> the mac is ahead for once ?
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
  // self-defend (ops fail-loud): an empty/too-small buffer makes OIIO's ImageInput::open
  // fail and return null; the old in->spec() deref would then crash silently. Refuse loudly.
  if (srcdata == nullptr or srclen == 0) {
    throw std::runtime_error(FormatString(
        "Image::initFromInMemoryFile: empty image buffer fmtguess<%s> srclen<%zu>",
        fmtguess.c_str(),
        srclen));
  }
  ImageSpec config;                                          // ImageSpec describing input configuration options
  Filesystem::IOMemReader memreader((void*)srcdata, srclen); // I/O proxy object
  void* ptr = &memreader;
  config.attribute("oiio:ioproxy", TypeDesc::PTR, &ptr);
  // orkid image data is STRAIGHT (unassociated) alpha end-to-end: writeToFile dumps the raw
  // RGBA channels and the GPU samplers do their own alpha math. PNG stores unassociated alpha,
  // but OIIO's reader defaults to ASSOCIATING it (RGB *= alpha) into its internal convention on
  // load — so a section-bake albedo captured with alpha=0 (only RGB is authored) came back with
  // RGB premultiplied to ZERO, blacking out the whole texture (the WARM section-array cache load
  // graying). Request unassociated alpha so RGB is returned exactly as stored (a no-op for the
  // opaque alpha=255 majority). LDR-only: EXR/HDR carry associated alpha by convention.
  if (fmtguess == "png")
    config.attribute("oiio:UnassociatedAlpha", 1);

  auto name = std::string("inmem.") + fmtguess;

  printf("Image::initFromInMemoryFile: fmtguess<%s> srclen<%zu>\n", fmtguess.c_str(), srclen);

  auto in = ImageInput::open(name, &config);
  if (not in) {
    throw std::runtime_error(FormatString(
        "Image::initFromInMemoryFile: OIIO could not open in-memory image fmtguess<%s> srclen<%zu> oiio_error<%s>",
        fmtguess.c_str(),
        srclen,
        OIIO::geterror().c_str()));
  }
  const ImageSpec& spec = in->spec();
  _width                = spec.width;
  _height               = spec.height;
  _numcomponents        = spec.nchannels;
  int native_nc         = spec.nchannels;

  printf("oiio read: w<%zu> h<%zu> nc<%zu> fmt<%s>\n", _width, _height, _numcomponents, spec.format.c_str());
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
          _format = EBufferFormat::R16UI;
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
    case TypeDesc::HALF:
      _bytesPerChannel = 2;
      switch (_numcomponents) {
        case 3:
          _format = EBufferFormat::RGBA16F;
          _numcomponents = 4; // promote to 4-channel for GPU compatibility
          break;
        case 4:
          _format = EBufferFormat::RGBA16F;
          break;
        default:
          OrkAssert(false);
          return false;
      }
      break;
    case TypeDesc::FLOAT:
      _bytesPerChannel = 4;
      switch (_numcomponents) {
        case 1:
          _format = EBufferFormat::R32F; // single-channel float (e.g. heightfield EXR)
          break;
        case 3:
          _format = EBufferFormat::RGBA32F;
          _numcomponents = 4; // promote to 4-channel for GPU compatibility
          break;
        case 4:
          _format = EBufferFormat::RGBA32F;
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
  bool needs_expand = (_numcomponents == 4 && native_nc == 3);

  if (needs_expand) {
    // Read native 3 channels into temp buffer, then expand to 4
    size_t num_pixels = _width * _height;
    auto tmp = std::vector<uint8_t>(num_pixels * 3 * _bytesPerChannel);
    TypeDesc read_type = (_bytesPerChannel == 4) ? TypeDesc::FLOAT : TypeDesc::HALF;
    in->read_image(0, 0, 0, 3, read_type, tmp.data());
    in->close();

    // Expand 3→4 channels, alpha = 1.0
    if (_bytesPerChannel == 4) {
      auto src = reinterpret_cast<const float*>(tmp.data());
      auto dst = reinterpret_cast<float*>(pixels);
      for (size_t i = 0; i < num_pixels; i++) {
        dst[i * 4 + 0] = src[i * 3 + 0];
        dst[i * 4 + 1] = src[i * 3 + 1];
        dst[i * 4 + 2] = src[i * 3 + 2];
        dst[i * 4 + 3] = 1.0f;
      }
    } else { // HALF
      auto src = reinterpret_cast<const uint16_t*>(tmp.data());
      auto dst = reinterpret_cast<uint16_t*>(pixels);
      uint16_t one_half = 0x3C00; // 1.0 in half-float
      for (size_t i = 0; i < num_pixels; i++) {
        dst[i * 4 + 0] = src[i * 3 + 0];
        dst[i * 4 + 1] = src[i * 3 + 1];
        dst[i * 4 + 2] = src[i * 3 + 2];
        dst[i * 4 + 3] = one_half;
      }
    }
  } else if (_bytesPerChannel == 1) {
    in->read_image(TypeDesc::UINT8, pixels);
    in->close();
  } else if (_format == EBufferFormat::RGBA16F) {
    in->read_image(0, 0, 0, 4, TypeDesc::HALF, pixels);
    in->close();
  } else if (_format == EBufferFormat::RGBA32F) {
    in->read_image(0, 0, 0, 4, TypeDesc::FLOAT, pixels);
    in->close();
  } else if (_format == EBufferFormat::R32F) {
    in->read_image(0, 0, 0, 1, TypeDesc::FLOAT, pixels);
    in->close();
  } else if (_bytesPerChannel == 2) {
    in->read_image(TypeDesc::UINT16, pixels);
    in->close();
  } else {
    in->close();
  }

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

bool Image::writeToFile(const ork::file::Path& outpath, bool linear_colorspace) const {
  auto cstrpath = outpath.c_str();
  auto out      = ImageOutput::create(cstrpath);
  // self-defend (ops fail-loud): OIIO returns null when it cannot map the path's
  // extension to an output plugin (an extension-less / unrecognized path). The old
  // silent `return` wrote NOTHING yet the void signature reported success — a capture
  // "written" to a bad path produced no file and callers reported a spurious result.
  // Refuse loudly (path + OIIO reason) and hand the caller a `false` it can act on;
  // never a silent no-op. (Mirrors the initFromInMemoryFile OIIO-null guard above.)
  if (!out) {
    auto oiio_err = OIIO::geterror();
    if (oiio_err.empty())
      oiio_err = "unrecognized output extension (no OIIO plugin for this path)";
    fprintf(stderr, "ork.lev2 Image::writeToFile FAILED path<%s> reason<%s>\n", cstrpath, oiio_err.c_str());
    logchan_image->log("Image::writeToFile FAILED path<%s> reason<%s>", cstrpath, oiio_err.c_str());
    return false;
  }
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
    case EBufferFormat::R16UI:
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
    case EBufferFormat::RGBA16F:
      spec.format       = TypeDesc::HALF;
      spec.nchannels    = 4;
      spec.channelnames = {"R", "G", "B", "A"};
      break;
      case EBufferFormat::RGB32F:
      spec.format       = TypeDesc::FLOAT;
      spec.nchannels    = 3;
      spec.channelnames = {"R", "G", "B"};
      break;
    case EBufferFormat::RGBA32F:
      spec.format       = TypeDesc::FLOAT;
      spec.nchannels    = 4;
      spec.channelnames = {"R", "G", "B", "A"};
      break;
    case EBufferFormat::R32F:
      spec.format       = TypeDesc::FLOAT;
      spec.nchannels    = 1;
      spec.channelnames = {"R"};
      break;
      default:
      OrkAssert(false);
      break;
  }

  if (linear_colorspace) {
    // tag as linear data (heightmaps/masks). Without this, integer formats like
    // PNG default to an sRGB chunk; a data consumer that honors the tag would then
    // apply an unwanted sRGB->linear curve. Gamma 1.0 == linear gAMA chunk.
    spec.attribute("oiio:ColorSpace", "Linear");
    spec.attribute("oiio:Gamma", 1.0f);
  }

  // OIIO's output plugins stamp the ENCODE wall-clock into "DateTime" when the spec
  // lacks it (EXR capDate, PNG tIME). The epoch sentinel preempts that unconditionally
  // (owner policy 2026-07-18): engine-written images are byte-reproducible.
  spec.attribute("DateTime", "1970:01:01 00:00:00");

  // open + write also fail-loud: OIIO signals these via a false return + per-object
  // geterror(). Ignoring them is the same silent-success lie as the create case.
  if (not out->open(cstrpath, spec)) {
    fprintf(stderr, "ork.lev2 Image::writeToFile FAILED (open) path<%s> reason<%s>\n", cstrpath, out->geterror().c_str());
    logchan_image->log("Image::writeToFile FAILED (open) path<%s> reason<%s>", cstrpath, out->geterror().c_str());
    return false;
  }
  if (not out->write_image(spec.format, _data->data())) {
    fprintf(stderr, "ork.lev2 Image::writeToFile FAILED (write) path<%s> reason<%s>\n", cstrpath, out->geterror().c_str());
    logchan_image->log("Image::writeToFile FAILED (write) path<%s> reason<%s>", cstrpath, out->geterror().c_str());
    out->close();
    return false;
  }
  out->close();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
