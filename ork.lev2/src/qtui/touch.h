////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


typedef struct SmtPoint { float x,y; } MtPoint;
typedef struct { MtPoint pos,vel; } MtReadout;

typedef struct SmtFinger {
  int frame;
  double timestamp;
  int identifier, state, foo3, foo4;
  MtReadout normalized;
  float size;
  int zero1;
  float angle, majorAxis, minorAxis; // ellipsoid
  MtReadout mm;
  int zero2[2];
  float unk2;
} MtFinger;

struct ITouchReciever
{
	ITouchReciever(void*pctx) : mpCTX(pctx) {}
	void* mpCTX;
	
	virtual void OnTouchBegin(const MtFinger* finger) = 0;
	virtual void OnTouchUpdate(const MtFinger* finger) = 0;
	virtual void OnTouchEnd(const MtFinger* finger) = 0;
};
