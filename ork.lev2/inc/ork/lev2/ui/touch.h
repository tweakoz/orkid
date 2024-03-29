////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

struct MultiTouchPoint
{
    enum PointState
    {
        PS_UP=0,
        PS_PUSHED,
        PS_DOWN,
        PS_RELEASED,
    };
    
    float mfOrigX;
    float mfOrigY;
    float mfPrevX;
    float mfPrevY;
    float mfCurrX;
    float mfCurrY;
    float mfPressure;
    int   mID;
    PointState mState;
    
    MultiTouchPoint()
        : mfOrigX(0.0f)
        , mfOrigY(0.0f)
        , mfPrevX(0.0f)
        , mfPrevY(0.0f)
        , mfCurrX(0.0f)
        , mfCurrY(0.0f)
        , mfPressure(0.0f)
        , mID(-1)
        , mState(PS_UP)
    {
    }
};

}}