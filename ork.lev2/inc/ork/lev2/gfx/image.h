#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/datablock.inl>
#include <ork/kernel/varmap.inl>

namespace ork::lev2 {

  ///////////////////////////////////////////////////////////////////////////////

  struct CompressedImage {

    EBufferFormat _format = EBUFFMT_END;
    datablockptr_t _data  = nullptr;
    size_t _width         = 0;
    size_t _height        = 0;
    size_t _numcomponents = 4; // 3 or 4
    varmap::VarMap _varmap;
  };

  ///////////////////////////////////////////////////////////////////////////////

  struct Image {

    void init(size_t w, size_t h, size_t numc);
    void initFromInMemoryFile(std::string fmtguess, const void* src, size_t srclen);
    void downsample(Image& imgout) const;
    void compressBC7(CompressedImage& imgout) const;

    datablockptr_t _data  = nullptr;
    size_t _width         = 0;
    size_t _height        = 0;
    size_t _numcomponents = 4; // 3 or 4
    varmap::VarMap _varmap;
  };

} // namespace ork::lev2
