////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/varmap.inl>

namespace ork::lev2 {

struct Image;

///////////////////////////////////////////////////////////////////////////////

struct CompressedImage {

  CompressedImage();

  void convertToImage(Image& ref) const;
  EBufferFormat _format  = EBufferFormat::NONE;
  datablock_ptr_t _data   = nullptr;
  size_t _width          = 0;
  size_t _height         = 0;
  size_t _blocked_width  = 0;
  size_t _blocked_height = 0;
  size_t _depth          = 1;
  size_t _numcomponents  = 4; // 3 or 4
  size_t _bytesPerChannel = 1;
  varmap::varmap_ptr_t _vars;
};

///////////////////////////////////////////////////////////////////////////////

struct CompressedImageMipChain {

  typedef std::vector<CompressedImage> miplevels_t;
  void initWithPrecompressedMipLevels(miplevels_t levels);

  void writeXTX(const file::Path& outpath);
  void writeXTX(datablock_ptr_t& out_datablock);
  void readXTX(const file::Path& inppath);
  void readXTX(datablock_ptr_t datablock);

  void readDDS(datablock_ptr_t datablock);

  EBufferFormat _format = EBufferFormat::NONE;
  size_t _width         = 0;
  size_t _height        = 0;
  size_t _depth         = 1;
  size_t _bytesPerChannel = 1;
  size_t _numcomponents = 4; // 3 or 4
  varmap::VarMap _varmap;
  miplevels_t _levels;
};

///////////////////////////////////////////////////////////////////////////////

struct Image {

  void init(size_t w, size_t h, size_t numc, int bytesperchannel);
  Image clone() const;

  static image_ptr_t createFromFile(const std::string& inpath);
  
  //////////////////////////

  bool initFromInMemoryFile(std::string fmtguess, const void* src, size_t srclen);
  bool initFromDataBlock(datablock_ptr_t datablock);
  bool _initFromDataBlockPNG(datablock_ptr_t datablock);
  void initRGBA8WithNormalizedFloatBuffer(size_t w, size_t h, size_t numc, const float* buffer);
  void initRGB8WithColor(size_t w, size_t h, fvec3 color, EBufferFormat fmt);
  void initRGBA8WithColor(size_t w, size_t h, fvec4 color, EBufferFormat fmt);

  //////////////////////////

  void resizedOf(const Image& inp, int w, int h);
  void downsample(Image& imgout) const;

  //////////////////////////

  void convertToRGBA(Image& imgout,bool force_8bc=false) const;
  Image convertToFormat(EBufferFormat fmt) const;
  void convertFromImageToFormat(const Image& inp, EBufferFormat fmt);

  //////////////////////////

  void uncompressed(CompressedImage& imgout) const;
  CompressedImageMipChain uncompressedMipChain_b() const;
  compressedmipchain_ptr_t uncompressedMipChain() const;

  #if defined(ENABLE_ISPC)
  void compressBC7(CompressedImage& imgout) const;
  CompressedImageMipChain compressedMipChainBC7_b() const;
  compressedmipchain_ptr_t compressedMipChainBC7() const;
  #endif

  CompressedImageMipChain compressedMipChainDefault_b() const;
  void compressDefault(CompressedImage& imgout) const;

  compressedmipchain_ptr_t compressedMipChainDefault() const;

  //////////////////////////

  void writeToFile(const ork::file::Path& outpath) const;
  bool readFromFile(const ork::file::Path& inpath);

  //////////////////////////

  uint8_t* pixel8(int x, int y);
  const uint8_t* pixel8(int x, int y) const;
  uint16_t* pixel16(int x, int y);
  const uint16_t* pixel16(int x, int y) const;

  //////////////////////////

  datablock_ptr_t _data  = nullptr;
  EBufferFormat _format  = EBufferFormat::NONE;
  size_t _width         = 0;
  size_t _height        = 0;
  size_t _depth         = 1;
  size_t _numcomponents = 4; // 3 or 4
  size_t _bytesPerChannel = 1;
  mutable uint64_t _contentHash = 0;
  std::string _debugName;
  varmap::VarMap _varmap;
  mutable compressedmipchain_ptr_t _cmipchain;
};

} // namespace ork::lev2
