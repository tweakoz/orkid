////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

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

///////////////////////////////////////////////////////////////////////////////

class AudioDevice {
public:
  virtual void ShutdownNow() {
  }

  static AudioDevice* GetDevice(void);

  virtual ~AudioDevice() {
  }

protected:
  AudioDevice();

  static AudioDevice* gpDevice;

  //////////////////
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
