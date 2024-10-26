////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <mdspan> the mac is ahead for once ?
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/util/logger.h>

#if defined(ENABLE_ISPC)
#include <ispc_texcomp.h>
#endif

namespace ork::lev2 {

extern logchannel_ptr_t logchan_image;


///////////////////////////////////////////////////////////////////////////////

CompressedImage::CompressedImage() {
  _vars = std::make_shared<varmap::VarMap>();
}

void CompressedImage::convertToImage(Image& ref) const{
  ref._format = _format;
  ref._width = _width;
  ref._height = _height;
  ref._numcomponents = _numcomponents;
  ref._data = _data;
  ref._varmap.mergeVars(*_vars);
}

///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_ISPC)
void Image::compressBC7(CompressedImage& imgout) const {
  logchan_image->log("///////////////////////////////////");
  logchan_image->log("// Image::compressBC7(%s)", _debugName.c_str());
  logchan_image->log("// imgout._width<%zu>", _width);
  logchan_image->log("// imgout._height<%zu>", _height);
  imgout._format = EBufferFormat::RGBA_BPTC_UNORM;
  OrkAssert((_numcomponents == 1) or (_numcomponents == 3) or (_numcomponents == 4));
  imgout._width          = _width;
  imgout._height         = _height;
  imgout._blocked_width  = (_width + 3) & 0xfffffffc;
  imgout._blocked_height = (_height + 3) & 0xfffffffc;
  imgout._numcomponents  = 4;
  //////////////////////////////////////////////////////////////////
  imgout._data = std::make_shared<DataBlock>();

  Image src_as_rgba;
  convertToRGBA(src_as_rgba, true);

  ork::Timer timer;
  timer.Start();
  ////////////////////////////////////////
  // parallel ISPC-BC7 compressor
  ////////////////////////////////////////
  std::atomic<int> pending = 0;
  // auto opgroup      = opq::createCompletionGroup(opq::concurrentQueue(), "BC7ENC");

  size_t src_len = src_as_rgba._data->length();
  size_t dst_len = imgout._blocked_width * imgout._blocked_height;

  auto src_base     = (uint8_t*)src_as_rgba._data->data();
  auto dst_base     = (uint8_t*)imgout._data->allocateBlock(dst_len);
  auto dst_iter     = dst_base;
  auto src_iter     = src_base;
  size_t src_stride = _width * 4; // 4 BPP
  size_t dst_stride = _width;     // 1 BPP

  size_t num_rows_per_operation = 4;

  for (int y = 0; y < _height; y += num_rows_per_operation) {
    pending.fetch_add(1);
  }
  for (int y = 0; y < _height; y += num_rows_per_operation) {
    int num_rows_this_operation = std::min(num_rows_per_operation, _height - y);
    opq::concurrentQueue()->enqueue([=, &pending]() {
      bc7_enc_settings settings;
      GetProfile_alpha_basic(&settings);
      rgba_surface surface;
      surface.width  = _width;
      surface.height = num_rows_this_operation;
      surface.stride = src_stride;
      surface.ptr    = src_iter;

      CompressBlocksBC7(&surface, dst_iter, &settings);
      pending.fetch_add(-1);
    });
    src_iter += src_stride * num_rows_this_operation;
    dst_iter += dst_stride * num_rows_this_operation;
    ptrdiff_t src_offset = (src_iter - src_base) / 16;
    ptrdiff_t dst_offset = (dst_iter - dst_base) / 16;
    OrkAssert(src_offset <= src_len);
    OrkAssert(dst_offset <= dst_len);
  }
  while (pending.load() > 0) {
    usleep(1000);
  }
  ////////////////////////////////////////

  float time = timer.SecsSinceStart();
  float MPPS = float(_width * _height) * 1e-6 / time;
  logchan_image->log("// compression time<%g> MPPS<%g>", time, MPPS);
  logchan_image->log("///////////////////////////////////");
}
#endif

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
