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
#include <ork/util/generator.inl>

namespace ork {
  namespace chunkfile {
    class OutputStream;
    struct InputStream;
    struct Writer;
    struct Reader;
  }
}

namespace ork::lev2 {

struct Image;

///////////////////////////////////////////////////////////////////////////////

struct MipDimensions{
  operator bool() const { return (_width>0) and (_height>0) and (_depth>0); }
  const size_t numPixels() const { return _width*_height*_depth; }
  size_t _width = 0;
  size_t _height = 0;
  size_t _depth = 0;
  size_t _mipindex = 0;
};

inline ::ork::coroutine::generator<MipDimensions> miplevelgen2D(size_t w, size_t h, const size_t min_tiledim=4) {
  size_t mipindex = 0;
  while(w>=min_tiledim and h>=min_tiledim) {
    MipDimensions mipdim{w,h,1,mipindex};
    co_yield mipdim;
    w>>=1;
    h>>=1;
    mipindex++;
  }
}


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
  void writeXTX(chunkfile::OutputStream* header_stream, 
                chunkfile::OutputStream* image_stream,
                chunkfile::Writer& chunkwriter);
  void readXTX(const file::Path& inppath);
  void readXTX(datablock_ptr_t datablock);
  void readXTX(chunkfile::InputStream* header_stream,
               chunkfile::InputStream* image_stream,
               chunkfile::Reader& chunkreader);

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

  Image();
  ~Image();
  
  void init(size_t w, size_t h, size_t numc, int bytesperchannel);
  void initWithFormat(size_t w, size_t h, EBufferFormat fmt);
  Image clone() const;

  static image_ptr_t createFromFile(const std::string& inpath);
  static image_ptr_t fromSvgString(const std::string& svg, int width, int height);
  static image_ptr_t fromSvgString(const std::string& svg, int size);
  static fvec2 svgIntrinsicSize(const std::string& svg);
  
  //////////////////////////

  bool initFromInMemoryFile(std::string fmtguess, const void* src, size_t srclen);
  bool initFromDataBlock(datablock_ptr_t datablock);
  bool _initFromDataBlockPNG(datablock_ptr_t datablock);
  void initRGBA8WithNormalizedFloatBuffer(size_t w, size_t h, size_t numc, const float* buffer);
  void initRGB8WithColor(size_t w, size_t h, fvec3 color);
  void initRGBA8WithColor(size_t w, size_t h, fvec4 color);

  //////////////////////////

  void resizedOf(const Image& inp, int w, int h);
  void downsample(Image& imgout) const;
  void gaussianBlur(Image& imgout, float kernel_size) const;

  // Separable convolution with optional threshold
  // kernel should be symmetric (e.g., Gaussian)
  // threshold: only process pixels where all components > threshold (default 0 = all pixels)
  void separableConvolve(
    Image& output,
    const std::vector<float>& kernel,
    fvec4 threshold = fvec4(0.0f, 0.0f, 0.0f, 0.0f)
  ) const;

  void lerp(const Image& a, const Image& b, float index);
  void fullBlurOf(const Image& a);
  void invert(uint8_t channel_mask); // Bit 0=R, 1=G, 2=B, 3=A
  void gamma(float gamma_value, uint8_t channel_mask = 0x0F);
  void gammaPerChannel(float gamma_r, float gamma_g, float gamma_b, float gamma_a);
  void dualThreshold(float low_threshold, float set_low, float high_threshold, float set_high, uint8_t channel_mask = 0x0F);
  void contrast(float contrast_value, float midpoint = 0.5f, uint8_t channel_mask = 0x0F);
  void combine(const fmtx4& matrix);

  // Geometric transformations
  image_ptr_t rotated90cw() const;
  image_ptr_t rotated90ccw() const;
  void rotate90cw();
  void rotate90ccw();
  image_ptr_t hFlipped() const;
  image_ptr_t vFlipped() const;
  void hFlip();
  void vFlip();

  //////////////////////////

  void convertToRGBA(Image& imgout,bool force_8bc=false) const;
  Image convertToFormat(EBufferFormat fmt) const;
  void convertFromImageToFormat(const Image& inp, EBufferFormat fmt);

  //////////////////////////

  void uncompressed(CompressedImage& imgout) const;
  CompressedImageMipChain uncompressedMipChain_b() const;
  compressedmipchain_ptr_t uncompressedMipChain() const;
  compressedmipchain_ptr_t uncompressedSingleMipChain() const;

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
  float* pixel32f(int x, int y);
  const float* pixel32f(int x, int y) const;

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

struct ImageProvider {
  using img_prov_fn_t = std::function<image_ptr_t()>;
  img_prov_fn_t _func;
};
} // namespace ork::lev2
