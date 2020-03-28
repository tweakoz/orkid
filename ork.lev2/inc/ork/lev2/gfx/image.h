#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/datablock.inl>
#include <ork/kernel/varmap.inl>
#include <ork/file/chunkfile.h>

namespace ork::lev2 {

  struct Image;

  ///////////////////////////////////////////////////////////////////////////////

  struct CompressedImage {

    EBufferFormat _format = EBufferFormat::NONE;
    datablockptr_t _data  = nullptr;
    size_t _width         = 0;
    size_t _height        = 0;
    size_t _depth         = 1;
    size_t _numcomponents = 4; // 3 or 4
    varmap::VarMap _varmap;
  };

  ///////////////////////////////////////////////////////////////////////////////

  struct CompressedImageMipChain{

    void writeXTX( const file::Path& outpath );
    void readXTX( const file::Path& inppath );

    EBufferFormat _format = EBufferFormat::NONE;
    size_t _width         = 0;
    size_t _height        = 0;
    size_t _depth         = 1;
    size_t _numcomponents = 4; // 3 or 4
    varmap::VarMap _varmap;
    std::vector<CompressedImage> _levels;
  };

  ///////////////////////////////////////////////////////////////////////////////

  struct Image {

    void init(size_t w, size_t h, size_t numc);
    void initFromInMemoryFile(std::string fmtguess, const void* src, size_t srclen);
    Image clone() const;
    void downsample(Image& imgout) const;
    void compressBC7(CompressedImage& imgout) const;
    CompressedImageMipChain compressedMipChainBC7() const;

    datablockptr_t _data  = nullptr;
    size_t _width         = 0;
    size_t _height        = 0;
    size_t _depth         = 1;
    size_t _numcomponents = 4; // 3 or 4
    varmap::VarMap _varmap;
  };

} // namespace ork::lev2
