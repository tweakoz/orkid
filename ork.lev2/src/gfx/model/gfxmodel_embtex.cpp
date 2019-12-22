///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/datacache.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/pch.h>

#include <ork/file/cfs.inl>
#include <ork/kernel/spawner.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

datablockptr_t EmbeddedTexture::compressTexture(uint64_t hash) const {
  datablockptr_t dblock = std::make_shared<DataBlock>();
  auto srcpath          = ork::file::generateContentTempPath(hash, _format);
  auto ddspath          = ork::file::generateContentTempPath(hash, "dds");
  FILE* fout            = fopen(srcpath.c_str(), "wb");
  fwrite(_srcdata, _srcdatalen, 1, fout);
  fclose(fout);

  invoke_nvcompress(srcpath,ddspath,"rgb");

  FILE* fin = fopen(ddspath.c_str(), "rb");
  fseek(fin, 0, SEEK_END);
  size_t ddslen = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  dblock->reserve(ddslen);
  void* ddsdata = malloc(ddslen);
  size_t numread = fread(ddsdata, ddslen, 1, fin);
  OrkAssert(numread==ddslen);
  fclose(fin);
  dblock->addData(ddsdata, ddslen);
  free(ddsdata);

  return dblock;
}

///////////////////////////////////////////////////////////////////////////////

void EmbeddedTexture::fetchDDSdata() {

  boost::Crc64 basehasher;
  basehasher.accumulateString(_format);
  basehasher.accumulateString(_name);
  basehasher.accumulate(_srcdata, _srcdatalen);
  basehasher.finish();
  uint64_t hashkey = basehasher.result();
  _ddsdestdatablock    = DataBlockCache::findDataBlock(hashkey);

  if (_ddsdestdatablock) {
    chunkfile::InputStream istr(_ddsdestdatablock->data(0), _ddsdestdatablock->length());
  } else {
    _ddsdestdatablock = compressTexture(hashkey);
    DataBlockCache::setDataBlock(hashkey, _ddsdestdatablock);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::MeshUtil
///////////////////////////////////////////////////////////////////////////////
