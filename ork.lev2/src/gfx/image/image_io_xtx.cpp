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
  OrkAssert(datablock);
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
      level._format = _format;
      level._blocked_width = ((level._width+3)/4)*4;
      level._blocked_height = ((level._height+3)/4)*4;
      level._depth = _depth;
      level._numcomponents = _numcomponents;
      _levels.push_back(level);
    }
  }
}

} //namespace ork::lev2 {
