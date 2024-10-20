////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector2.h>
#include <ork/math/misc_math.h>

using namespace ork;

static const float MyEPSILON = 5.0e-07f;//std::numeric_limits<float>::epsilon();

TEST(Vector2DefaultConstructor)
{
	fvec2 v;
	CHECK_CLOSE(0.0f, v.x, MyEPSILON);
	CHECK_CLOSE(0.0f, v.y, MyEPSILON);
}

TEST(Vector2Constructor)
{
	fvec2 v(1.0f, 2.0f);
	CHECK_CLOSE(1.0f, v.x, MyEPSILON);
	CHECK_CLOSE(2.0f, v.y, MyEPSILON);
}

TEST(Vector2CopyConstructor)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 copy(v);
	CHECK_CLOSE(1.0f, copy.x, MyEPSILON);
	CHECK_CLOSE(2.0f, copy.y, MyEPSILON);
}

TEST(Vector2Rotate)
{
	fvec2 v(0.0f, 1.0f);
	v.rotate(PI / 2.0f);
	CHECK_CLOSE(0.0f, v.x, MyEPSILON);
	CHECK_CLOSE(1.0f, v.y, MyEPSILON);
}

TEST(Vector2Add)
{
	fvec2 v(1.0f, 0.0f);
	Vector2 res = v + Vector2(0.0f, 1.0f);
	CHECK_CLOSE(1.0f, res.x, MyEPSILON);
	CHECK_CLOSE(1.0f, res.y, MyEPSILON);
}

TEST(Vector2AddTo)
{
	fvec2 v(1.0f, 0.0f);
	v += Vector2(0.0f, 1.0f);
	CHECK_CLOSE(1.0f, v.x, MyEPSILON);
	CHECK_CLOSE(1.0f, v.y, MyEPSILON);
}

TEST(Vector2Subtract)
{
	fvec2 v(1.0f, 0.0f);
	Vector2 res = v - Vector2(0.0f, 1.0f);
	CHECK_CLOSE(1.0f, res.x, MyEPSILON);
	CHECK_CLOSE(-1.0f, res.y, MyEPSILON);
}

TEST(Vector2SubtractFrom)
{
	fvec2 v(1.0f, 0.0f);
	v -= Vector2(0.0f, 1.0f);
	CHECK_CLOSE(1.0f, v.x, MyEPSILON);
	CHECK_CLOSE(-1.0f, v.y, MyEPSILON);
}

TEST(Vector2Multiply)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 res = v * Vector2(3.0f, 2.0f);
	CHECK_CLOSE(3.0f, res.x, MyEPSILON);
	CHECK_CLOSE(4.0f, res.y, MyEPSILON);
}

TEST(Vector2MultiplyTo)
{
	fvec2 v(1.0f, 2.0f);
	v *= Vector2(3.0f, 2.0f);
	CHECK_CLOSE(3.0f, v.x, MyEPSILON);
	CHECK_CLOSE(4.0f, v.y, MyEPSILON);
}

TEST(Vector2Divide)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 res = v / Vector2(3.0f, 2.0f);
	CHECK_CLOSE(0.333333f, res.x, MyEPSILON);
	CHECK_CLOSE(1.0f, res.y, MyEPSILON);
}

TEST(Vector2DivideTo)
{
	fvec2 v(1.0f, 2.0f);
	v /= Vector2(3.0f, 2.0f);
	CHECK_CLOSE(0.333333f, v.x, MyEPSILON);
	CHECK_CLOSE(1.0f, v.y, MyEPSILON);
}

TEST(Vector2Scale)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 res = v * 3.0f;
	CHECK_CLOSE(3.0f, res.x, MyEPSILON);
	CHECK_CLOSE(6.0f, res.y, MyEPSILON);
}

TEST(Vector2ScaleTo)
{
	fvec2 v(1.0f, 2.0f);
	v *= 3.0f;
	CHECK_CLOSE(3.0f, v.x, MyEPSILON);
	CHECK_CLOSE(6.0f, v.y, MyEPSILON);
}

TEST(Vector2ScalePre)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 res = 3.0f * v;
	CHECK_CLOSE(3.0f, res.x, MyEPSILON);
	CHECK_CLOSE(6.0f, res.y, MyEPSILON);
}

TEST(Vector2InvScale)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 res = v / 3.0f;
	CHECK_CLOSE(0.333333f, res.x, MyEPSILON);
	CHECK_CLOSE(0.666667f, res.y, MyEPSILON);
}

TEST(Vector2InvScaleTo)
{
	fvec2 v(1.0f, 2.0f);
	v /= 3.0f;
	CHECK_CLOSE(0.333333f, v.x, MyEPSILON);
	CHECK_CLOSE(0.666667f, v.y, MyEPSILON);
}

TEST(Vector2EqualCompare)
{
	fvec2 v1(1.0f, 2.0f);
	fvec2 v2(1.0f, 2.0f);
	fvec2 v3(4.0f, 3.0f);
	CHECK_EQUAL(true, v1 == v2);
	CHECK_EQUAL(false, v1 == v3);
}

TEST(Vector2NotEqualCompare)
{
	fvec2 v1(1.0f, 2.0f);
	fvec2 v2(1.0f, 2.0f);
	fvec2 v3(4.0f, 3.0f);
	CHECK_EQUAL(false, v1 != v2);
	CHECK_EQUAL(true, v1 != v3);
}

TEST(Vector2Dot)
{
	fvec2 v1(1.0f, 2.0f);
	fvec2 v2(2.0f, 1.0f);
	float res = v1.dotWith(v2);
	CHECK_CLOSE(4.0f, res, MyEPSILON);
}

TEST(Vector2Normalize)
{
	fvec2 v(1.0f, 2.0f);
	v.normalizeInPlace();
	CHECK_CLOSE(0.447214f, v.x, MyEPSILON);
	CHECK_CLOSE(0.894427f, v.y, MyEPSILON);
}

TEST(Vector2Normal)
{
	fvec2 v(1.0f, 2.0f);
	Vector2 res = v.normalized();
	CHECK_CLOSE(0.447214f, res.x, MyEPSILON);
	CHECK_CLOSE(0.894427f, res.y, MyEPSILON);
}

TEST(Vector2Mag)
{
	fvec2 v(3.0f, 4.0f);
	float res = v.magnitude();
	CHECK_CLOSE(5.0f, res, MyEPSILON);
}

TEST(Vector2MagSquared)
{
	fvec2 v(3.0f, 4.0f);
	float res = v.magnitudeSquared();
	CHECK_CLOSE(25.0f, res, MyEPSILON);
}
