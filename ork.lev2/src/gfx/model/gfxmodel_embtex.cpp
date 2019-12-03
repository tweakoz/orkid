///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
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

  Spawner s;
  s.mCommandLine = "nvcompress ";
  s.mCommandLine += "-bc3 ";
  s.mCommandLine += srcpath.c_str() + std::string(" ");
  s.mCommandLine += ddspath.c_str() + std::string(" ");

  s.spawnSynchronous();

  FILE* fin = fopen(ddspath.c_str(), "rb");
  fseek(fin, 0, SEEK_END);
  size_t ddslen = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  dblock->reserve(ddslen);
  void* ddsdata = malloc(ddslen);
  fread(ddsdata, ddslen, 1, fin);
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
  _ddsdestdatablock    = DataBlockMgr::findDataBlock(hashkey);

  if (_ddsdestdatablock) {
    const auto& str = _ddsdestdatablock->_data;
    chunkfile::InputStream istr(str.GetData(), str.GetSize());
  } else {
    _ddsdestdatablock = compressTexture(hashkey);
    DataBlockMgr::setDataBlock(hashkey, _ddsdestdatablock);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::MeshUtil
///////////////////////////////////////////////////////////////////////////////
