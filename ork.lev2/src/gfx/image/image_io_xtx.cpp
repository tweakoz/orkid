////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <mdspan> the mac is ahead for once ?
#include <math.h>
#include <ork/pch.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/memcpy.inl>
#include <ork/file/file.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/image.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

constexpr size_t KXTXVERSION = "xtx-ver0"_crcu;

void CompressedImageMipChain::writeXTX(chunkfile::OutputStream* header_stream, //
                                       chunkfile::OutputStream* image_stream,  //
                                       chunkfile::Writer& chunkwriter) {       //
  header_stream->AddItem<size_t>(KXTXVERSION);
  header_stream->AddItem<size_t>(_width);
  header_stream->AddItem<size_t>(_height);
  header_stream->AddItem<size_t>(_depth);
  header_stream->AddItem<size_t>(_numcomponents);
  header_stream->AddItem<EBufferFormat>(_format);
  header_stream->AddItem<size_t>(_levels.size());
  header_stream->addVarMap(_varmap, chunkwriter);
  //////////////////////////////////////////
  OrkAssert(_depth == 1); // only 2D for now..
  //////////////////////////////////////////
  for (size_t levidx = 0; levidx < _levels.size(); levidx++) {
    const auto& level = _levels[levidx];
    header_stream->AddItem<size_t>(levidx);
    header_stream->AddItem<size_t>(level._width);
    header_stream->AddItem<size_t>(level._height);

    size_t mipbase   = image_stream->GetSize();
    auto mipdata     = (const void*)level._data->data();
    size_t miplength = level._data->length();

    header_stream->AddItem<size_t>(mipbase);
    header_stream->AddItem<size_t>(miplength);
    image_stream->AddData(mipdata, miplength);
  }
}
  
///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::writeXTX(datablock_ptr_t& out_datablock) {
  //////////////////////////////////////////
  chunkfile::Writer chunkwriter("xtx");
  auto hdrstream = chunkwriter.AddStream("header");
  auto imgstream = chunkwriter.AddStream("image");
  writeXTX(hdrstream, imgstream,chunkwriter);
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
void CompressedImageMipChain::readXTX(   //
  chunkfile::InputStream* header_stream, //
  chunkfile::InputStream* image_stream,  //
  chunkfile::Reader& chunkreader) {      //
  OrkAssert(header_stream);
  OrkAssert(image_stream);

  size_t xtx_version = 0;
  size_t numlevels   = 0;
  header_stream->GetItem<size_t>(xtx_version);
  OrkAssert(xtx_version == KXTXVERSION);
  header_stream->GetItem<size_t>(_width);
  header_stream->GetItem<size_t>(_height);
  header_stream->GetItem<size_t>(_depth);
  header_stream->GetItem<size_t>(_numcomponents);
  header_stream->GetItem<EBufferFormat>(_format);
  header_stream->GetItem<size_t>(numlevels);
  header_stream->getVarMap(_varmap, chunkreader);
  //////////////////////////////////////////
  OrkAssert(_depth == 1); // only 2D for now..
  //////////////////////////////////////////
  for (size_t levidx = 0; levidx < numlevels; levidx++) {
    CompressedImage level;
    size_t lidx      = 0;
    size_t mipbase   = 0;
    size_t miplength = 0;
    header_stream->GetItem<size_t>(lidx);
    header_stream->GetItem<size_t>(level._width);
    header_stream->GetItem<size_t>(level._height);
    header_stream->GetItem<size_t>(mipbase);
    header_stream->GetItem<size_t>(miplength);
    auto mipdata = image_stream->GetDataAt(mipbase);
    level._data  = std::make_shared<DataBlock>(mipdata, miplength);
    level._format = _format;
    level._blocked_width = ((level._width+3)/4)*4;
    level._blocked_height = ((level._height+3)/4)*4;
    level._depth = _depth;
    level._numcomponents = _numcomponents;
    // Derive bytesPerChannel from format
    switch(_format) {
      case EBufferFormat::RGBA32F:
      case EBufferFormat::RGB32F:
      case EBufferFormat::R32F:
        level._bytesPerChannel = 4;
        break;
      case EBufferFormat::RGBA16F:
      case EBufferFormat::RGBA16:
      case EBufferFormat::RGB16:
      case EBufferFormat::R16UI:
        level._bytesPerChannel = 2;
        break;
      default:
        level._bytesPerChannel = 1;
        break;
    }
    _levels.push_back(level);
  }

}

///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::readXTX(const file::Path& inppath) {
  auto dblock = datablockFromFileAtPath(inppath);
  if (dblock) {
    readXTX(dblock);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CompressedImageMipChain::readXTX(datablock_ptr_t datablock) {
  OrkAssert(datablock);
  //////////////////////////////////////////
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(datablock, allocator);
  OrkAssert(chunkreader._chunkfiletype == "xtx");
  if (chunkreader.IsOk()) {
    auto hdrstream     = chunkreader.GetStream("header");
    auto imgstream     = chunkreader.GetStream("image");
    readXTX(hdrstream, imgstream, chunkreader);
  }
}

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
