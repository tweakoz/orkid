////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
