////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
        ELEMENT,      // what == "%d" % index_of_item
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

