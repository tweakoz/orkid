///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/kernel/svariant.h>

namespace ork { namespace event {
    
class Event : public Object
{
    RttiDeclareAbstract( Event, Object );
    
public:
};

class VEvent : public Event 
{
    RttiDeclareConcrete( VEvent, Event );
public:
    
    PoolString mCode;
    svar64_t mData;
};

} } // ork::event

