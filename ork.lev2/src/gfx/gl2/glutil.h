////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#ifndef _GLUTIL_H
#define _GLUTIL_H

///////////////////////////////////////////////////////////////////////////////

void gltexquad( F32 x1, F32 y1, F32 x2, F32 y2, F32 u1, F32 v1, F32 u2, F32 v2 );
void gltext( int x, int y, int w, int h, char *formatstring, ... );
void glinvtext( int x, int y, int w, int h, char *formatstring, ... );

///////////////////////////////////////////////////////////////////////////////

#endif
