////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/lev2_types.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/asset/Asset.h>
#include <ork/util/endian.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/tempstring.h>
#include <ork/dataflow/dataflow.h>
#include <ork/kernel/orkpool.h>
#include <ork/math/multicurve.h>
#include <ork/math/TransformNode.h>
#include <ork/math/basicfilters.h>
#include <ork/kernel/any.h>
#include <ork/kernel/varmap.inl>
#include <ork/application/application.h>
#include <ork/kernel/concurrent_queue.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudioInputChunk {
  AudioInputChunk(size_t channel_count=1);
  void setNumChannels(size_t channel_count);
  std::vector<input_frames_t> _channels;
  size_t _chunk_index = 0;
  size_t _num_frames = 0;
};

struct AudioInputChunkSource {
  virtual ~AudioInputChunkSource() {}
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual audioinputchunk_ptr_t getChunk() = 0;
};

struct StreamingAudioInputChunkSource : public AudioInputChunkSource {

  void start() final;
  void stop() final;
  lev2::audioinputchunk_ptr_t getChunk() final;
  MpMcBoundedQueue<lev2::audioinputchunk_ptr_t,16> _inputqueue;
  svar64_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct AudioDevice {

  static audiodevice_ptr_t createInstance(appinitdata_wkptr_t appinitd);

  AudioDevice(appinitdata_wkptr_t appinitd);
  virtual ~AudioDevice();
  virtual void startup();
  virtual void shutdown();

  appinitdata_wkptr_t _appinitdata;
  svar64_t _impl;
  varmap::varmap_ptr_t _vars;
  audio_input_handler_t _input_handler;
  size_t _num_input_channels = 0;
  size_t _num_output_channels = 0;
  std::string _inp_dev_name;
  std::string _out_dev_name;
  //////////////////
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
