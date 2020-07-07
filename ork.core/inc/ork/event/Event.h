///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/svariant.h>

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
