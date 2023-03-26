////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/svariant.h>

namespace ork::ui {

  struct UpdateData {
    double _dt      = 0.0;
    double _abstime = 0.0;
  };
  using updatedata_ptr_t = std::shared_ptr<UpdateData>;

}

namespace ork { namespace event {

class Event {
public:
  virtual ~Event() {
  }
};

class VEvent : public Event {

public:
  PoolString mCode;
  svar64_t mData;
};

}} // namespace ork::event
