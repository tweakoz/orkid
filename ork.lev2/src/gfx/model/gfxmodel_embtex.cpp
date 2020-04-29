///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/datacache.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/pch.h>

#include <ork/file/cfs.inl>
#include <ork/kernel/spawner.h>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

static std::string compressionOptsForUsage(ETextureUsage usage) {
  std::string rval = "???";
  switch (usage) {
    case ETEXUSAGE_COLOR:
#if defined(__APPLE__)
      // rval = "-bc7 -nocuda";
      rval = "-rgb -color";
#else
      // rval = "-bc7 -fast -nocuda";
      rval = "-rgb -color";
#endif
      break;
    case ETEXUSAGE_NORMAL:
      rval = "-rgb -normal";
      break;
    case ETEXUSAGE_DATA:
      rval = "-rgb";
      break;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

datablockptr_t EmbeddedTexture::compressTexture(uint64_t hash) const {
  datablockptr_t dblock = std::make_shared<DataBlock>();
  auto srcpath          = ork::file::generateContentTempPath(hash, _format);
  FILE* fout            = fopen(srcpath.c_str(), "wb");
  fwrite(_srcdata, _srcdatalen, 1, fout);
  fclose(fout);
  std::string compressed_path;
  if (1) { // ISPC compressor (WIP)
    Image img;
    img.initFromInMemoryFile(_format, _srcdata, _srcdatalen);
    img._debugName    = FormatString("emtex_%s", _name.c_str());
    auto cimgchain    = img.compressedMipChainDefault();
    cimgchain._varmap = _varmap;
    compressed_path   = ork::file::generateContentTempPath(hash, "xtx");
    cimgchain.writeXTX(compressed_path);
  } else { // nvtt compressor
    compressed_path = ork::file::generateContentTempPath(hash, "dds");
    auto options    = compressionOptsForUsage(_usage);
    invoke_nvcompress(srcpath, compressed_path, options);
  }
  FILE* fin = fopen(compressed_path.c_str(), "rb");
  fseek(fin, 0, SEEK_END);
  size_t compressed_length = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  dblock->reserve(compressed_length);
  void* compressed_data = malloc(compressed_length);
  size_t numread        = fread(compressed_data, 1, compressed_length, fin);
  OrkAssert(numread == compressed_length);
  fclose(fin);
  dblock->addData(compressed_data, compressed_length);
  free(compressed_data);
  return dblock;
}

///////////////////////////////////////////////////////////////////////////////

void EmbeddedTexture::fetchDDSdata() {

  auto options = compressionOptsForUsage(_usage);

  boost::Crc64 basehasher;
  basehasher.accumulateString(options);
  basehasher.accumulateString(_format);
  basehasher.accumulateString(_name);
  basehasher.accumulateString("version-0");
  basehasher.accumulate(_srcdata, _srcdatalen);
  basehasher.finish();
  uint64_t hashkey  = basehasher.result();
  _ddsdestdatablock = DataBlockCache::findDataBlock(hashkey);

  if (_ddsdestdatablock) {
    chunkfile::InputStream istr(_ddsdestdatablock->data(0), _ddsdestdatablock->length());
  } else {
    _ddsdestdatablock = compressTexture(hashkey);
    DataBlockCache::setDataBlock(hashkey, _ddsdestdatablock);
  }
  _compressionPending = false;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
