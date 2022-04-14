////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
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

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

class AudioDevice;
using audiodevice_ptr_t = std::shared_ptr<AudioDevice>;

///////////////////////////////////////////////////////////////////////////////

class AudioDevice {
public:
  virtual void ShutdownNow() {
  }

  static audiodevice_ptr_t instance(void);

  virtual ~AudioDevice();

protected:
  AudioDevice();

  //////////////////
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
