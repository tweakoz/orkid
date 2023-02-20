////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////

enum EPlugDir {
  EPD_INPUT = 0,
  EPD_OUTPUT,
  EPD_BOTH,
  EPD_NONE,
};

enum EPlugRate {
  EPR_EVENT = 0, // plug will not change during the entire duration of an event
  EPR_UNIFORM,   // plug will not change during the entire duration of a single compute call
  EPR_VARYING1,  // plug may change during the entire duration of a single compute call (once per item)
  EPR_VARYING2,  // plug may change more frequently than EPR_VARYING1 (multiple times per item)
};

enum EMorphEventType { 
    EMET_WRITE = 0, 
    EMET_MORPH, 
    EMET_END
};

} //namespace ork { namespace dataflow {

///////////////////////////////////////////////////////////////////////////////
