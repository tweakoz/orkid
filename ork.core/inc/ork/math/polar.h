////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _POLAR_H
#define _POLAR_H

///////////////////////////////////////////////////////////////////////////////

F32 pol2rect_x( F32 ang, F32 rad );
F32 pol2rect_y( F32 ang, F32 rad );
F32 rect2pol_ang( F32 x, F32 y );
F32 rect2pol_angr( F32 x, F32 y );
F32 rect2pol_rad( F32 x, F32 y );

#endif
