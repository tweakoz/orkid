////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
