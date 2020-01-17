////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/kernel/string/ArrayString.h>

namespace ork {

namespace reflect {

class Command
{
public:
    enum ECommand
    {
        EOBJECT,    // what == classname
        EATTRIBUTE, // what == attributename of object
        EPROPERTY,  // what == propname of object
        EITEM,      // what == "%d" % index_of_item
    };

    Command()
        : mCommand(EOBJECT)
        , mpPrevCommand(NULL)
    { }

    Command(ECommand cmd, PieceString what = PieceString())
        : mCommand(cmd)
        , mpPrevCommand(NULL)
		, mWhat(what)
    {
    }

    void Setup(ECommand cmd, PieceString what = PieceString())
    {
        mCommand = cmd;
		mWhat = what;
    }

    ECommand Type() const { return mCommand; }
    ConstString Name() const { return mWhat; }
    MutableString Name() { return mWhat; }
    const Command *&PreviousCommand() const { return mpPrevCommand; }

private:
    ECommand mCommand;
    const Command mutable * mpPrevCommand;
    ArrayString<64> mWhat;
};

} }

