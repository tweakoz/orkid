////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _GLUTIL_H
#define _GLUTIL_H

///////////////////////////////////////////////////////////////////////////////

void gltexquad( F32 x1, F32 y1, F32 x2, F32 y2, F32 u1, F32 v1, F32 u2, F32 v2 );
void gltext( int x, int y, int w, int h, char *formatstring, ... );
void glinvtext( int x, int y, int w, int h, char *formatstring, ... );

///////////////////////////////////////////////////////////////////////////////

#endif
