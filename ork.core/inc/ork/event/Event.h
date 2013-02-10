///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>

namespace ork { namespace event {
    
class Event : public Object
{
    RttiDeclareAbstract( Event, Object );
    
public:
};


} } // ork::event

