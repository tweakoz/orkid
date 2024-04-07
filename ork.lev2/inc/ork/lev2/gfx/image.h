////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/varmap.inl>
#include <ork/file/chunkfile.h>

namespace ork::lev2 {

struct Image;

///////////////////////////////////////////////////////////////////////////////

struct CompressedImage {

  CompressedImage();
  EBufferFormat _format  = EBufferFormat::NONE;
  datablock_ptr_t _data   = nullptr;
  size_t _width          = 0;
  size_t _height         = 0;
  size_t _blocked_width  = 0;
  size_t _blocked_height = 0;
  size_t _depth          = 1;
  size_t _numcomponents  = 4; // 3 or 4
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

  EBufferFormat _format = EBufferFormat::NONE;
  size_t _width         = 0;
  size_t _height        = 0;
  size_t _depth         = 1;
  size_t _numcomponents = 4; // 3 or 4
  varmap::VarMap _varmap;
  miplevels_t _levels;
};

///////////////////////////////////////////////////////////////////////////////

struct Image {

  void init(size_t w, size_t h, size_t numc);
  void initFromInMemoryFile(std::string fmtguess, const void* src, size_t srclen);
  void initWithNormalizedFloatBuffer(size_t w, size_t h, size_t numc, const float* buffer);
  void writeToFile(ork::file::Path outpath) const;
  Image clone() const;
  void convertToRGBA(Image& imgout) const;
  void downsample(Image& imgout) const;

  void compressDefault(CompressedImage& imgout) const;
  CompressedImageMipChain compressedMipChainDefault() const;

  #if defined(ENABLE_ISPC)
  void compressBC7(CompressedImage& imgout) const;
  CompressedImageMipChain compressedMipChainBC7() const;
  #endif

  void compressRGBA(CompressedImage& imgout) const;
  CompressedImageMipChain compressedMipChainRGBA() const;
  uint8_t* pixel(int x, int y);
  const uint8_t* pixel(int x, int y) const;
  datablock_ptr_t _data  = nullptr;
  size_t _width         = 0;
  size_t _height        = 0;
  size_t _depth         = 1;
  size_t _numcomponents = 4; // 3 or 4
  std::string _debugName;
  varmap::VarMap _varmap;
};

} // namespace ork::lev2
