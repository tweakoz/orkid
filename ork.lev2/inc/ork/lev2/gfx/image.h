#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/datablock.inl>
#include <ork/kernel/varmap.inl>
#include <ork/file/chunkfile.h>

namespace ork::lev2 {

struct Image;

///////////////////////////////////////////////////////////////////////////////

struct CompressedImage {

  EBufferFormat _format  = EBufferFormat::NONE;
  datablockptr_t _data   = nullptr;
  size_t _width          = 0;
  size_t _height         = 0;
  size_t _blocked_width  = 0;
  size_t _blocked_height = 0;
  size_t _depth          = 1;
  size_t _numcomponents  = 4; // 3 or 4
  varmap::VarMap _varmap;
};

///////////////////////////////////////////////////////////////////////////////

struct CompressedImageMipChain {

  typedef std::vector<CompressedImage> miplevels_t;
  void initWithPrecompressedMipLevels(miplevels_t levels);

  void writeXTX(const file::Path& outpath);
  void writeXTX(datablockptr_t& out_datablock);
  void readXTX(const file::Path& inppath);
  void readXTX(datablockptr_t datablock);

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
  void compressBC7(CompressedImage& imgout) const;
  CompressedImageMipChain compressedMipChainBC7() const;
  uint8_t* pixel(int x, int y);
  const uint8_t* pixel(int x, int y) const;
  datablockptr_t _data  = nullptr;
  size_t _width         = 0;
  size_t _height        = 0;
  size_t _depth         = 1;
  size_t _numcomponents = 4; // 3 or 4
  std::string _debugName;
  varmap::VarMap _varmap;
};

} // namespace ork::lev2
