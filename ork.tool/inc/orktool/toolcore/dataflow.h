////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORKTOOL_DATAFLOW_H
#define _ORKTOOL_DATAFLOW_H
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
namespace ork { namespace tool {
dataflow::scheduler* GetGlobalDataFlowScheduler();
}}
#endif
