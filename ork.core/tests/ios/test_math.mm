////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <ork/math/plane.h>
#include <ork/util/logger.h>

using namespace ork;

void runMathTests(void) {
    auto logchan = logchannel("MATH");

    logchan->log("========================================");
    logchan->log("Starting Math Tests");
    logchan->log("========================================");

    // Vector2 tests
    logchan->log("");
    logchan->log("--- Vector2 Tests ---");
    fvec2 v2a(1.0f, 2.0f);
    fvec2 v2b(3.0f, 4.0f);
    fvec2 v2c = v2a + v2b;
    logchan->log("v2a(%.2f, %.2f) + v2b(%.2f, %.2f) = (%.2f, %.2f)",
                 v2a.x, v2a.y, v2b.x, v2b.y, v2c.x, v2c.y);

    float len2 = v2a.length();
    logchan->log("v2a.length() = %.2f", len2);

    // Vector3 tests
    logchan->log("");
    logchan->log("--- Vector3 Tests ---");
    fvec3 v3a(1.0f, 2.0f, 3.0f);
    fvec3 v3b(4.0f, 5.0f, 6.0f);
    fvec3 v3c = v3a + v3b;
    logchan->log("v3a(%.2f, %.2f, %.2f) + v3b(%.2f, %.2f, %.2f) = (%.2f, %.2f, %.2f)",
                 v3a.x, v3a.y, v3a.z, v3b.x, v3b.y, v3b.z, v3c.x, v3c.y, v3c.z);

    float len3 = v3a.length();
    logchan->log("v3a.length() = %.2f", len3);

    fvec3 normalized = v3a.normalized();
    logchan->log("v3a.normalized() = (%.4f, %.4f, %.4f)", normalized.x, normalized.y, normalized.z);

    // Vector4 tests
    logchan->log("");
    logchan->log("--- Vector4 Tests ---");
    fvec4 v4a(1.0f, 2.0f, 3.0f, 4.0f);
    fvec4 v4b(5.0f, 6.0f, 7.0f, 8.0f);
    fvec4 v4c = v4a + v4b;
    logchan->log("v4a + v4b = (%.2f, %.2f, %.2f, %.2f)", v4c.x, v4c.y, v4c.z, v4c.w);

    // Matrix4 tests
    logchan->log("");
    logchan->log("--- Matrix4 Tests ---");
    fmtx4 identity;
    identity.setToIdentity();
    logchan->log("Identity matrix created");

    fmtx4 translation;
    translation.setTranslation(5.0f, 10.0f, 15.0f);
    logchan->log("Translation matrix created for offset (5, 10, 15)");

    fmtx4 rotation;
    rotation.setRotateY(PI * 0.5f); // 90 degrees
    logchan->log("Rotation matrix created for 90° around Y axis");

    // Quaternion tests
    logchan->log("");
    logchan->log("--- Quaternion Tests ---");
    fquat q1;
    q1.fromAxisAngle(fvec4(0.0f, 1.0f, 0.0f, PI * 0.5f)); // 90 degrees around Y
    logchan->log("Quaternion created from axis-angle");

    fquat q2;
    q2.fromAxisAngle(fvec4(1.0f, 0.0f, 0.0f, PI * 0.25f)); // 45 degrees around X
    fquat q3 = q1 * q2;
    logchan->log("Combined quaternion rotation created");

    // Plane tests
    logchan->log("");
    logchan->log("--- Plane Tests ---");
    fplane3 plane(fvec3(0.0f, 1.0f, 0.0f), 5.0f); // XZ plane at Y=5
    fvec3 testPoint(10.0f, 8.0f, 10.0f);
    float distance = plane.pointDistance(testPoint);
    logchan->log("Distance from point (%.2f, %.2f, %.2f) to plane = %.2f",
                 testPoint.x, testPoint.y, testPoint.z, distance);

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Math Tests Complete");
    logchan->log("========================================");
}
