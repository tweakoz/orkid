#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp> // For noise functions, if needed
#include <cmath> // For std::sin

namespace ork::libnoise {

// Use glm::fract, glm::floor, glm::mix, and glm::smoothstep as direct replacements for GLSL equivalents.

float hash(float n) {
    return glm::fract(std::sin(n) * 10000.0f);
}

float hash(const fvec2& p) {
    return glm::fract(10000.0f * std::sin(17.0f * p.x + p.y * 0.1f) * (0.1f + std::abs(std::sin(p.y * 13.0f + p.x))));
}

float noise(float x) {
    float i = glm::floor(x);
    float f = glm::fract(x);
    float u = f * f * (3.0f - 2.0f * f);
    return glm::mix(hash(i), hash(i + 1.0f), u);
}

float noise_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

template <typename T> T noise_smooth(T input){
    auto three = T::fromScalar(3); // Vector with both components as 3.0
    T two_f = input * 2.0; // Adjusting scalar * vector to vector * scalar
    T temp = three - two_f; // Using vector - vector as required
    T f_squared = input * input; // f * f is already in vector * vector form
    T u = f_squared * temp; // Final multiplication in vector * vector form
    return u;
}

float noise(const fvec2& x) {
    auto i = x.floor();
    auto f = x.fract();

    float a = hash(i);
    float b = hash(i + fvec2(1.0f, 0.0f));
    float c = hash(i + fvec2(0.0f, 1.0f));
    float d = hash(i + fvec2(1.0f, 1.0f));

    //fvec2 u = (f * f) * (3.0f - 2.0f * f);
    fvec2 u = noise_smooth(f);

    return glm::mix(a, b, u.x) + (c - a) * u.y * (1.0f - u.x) + (d - b) * u.x * u.y;
}

// This function has non-ideal tiling properties that might still need tuning
float noise(const fvec3& x) {
    const fvec3 step = fvec3(110.0f, 241.0f, 171.0f);

    fvec3 i = x.floor();
    fvec3 f = x.fract();
    float n = i.dotWith(step);

    //fvec3 u = f * f * (3.0f - 2.0f * f);
    fvec3 u = noise_smooth(f);

    float a1 = noise_lerp(hash(n + step.dotWith(fvec3(0, 0, 0))), hash(n + step.dotWith(fvec3(1, 0, 0))), u.x);
    float a2 = noise_lerp(hash(n + step.dotWith(fvec3(0, 1, 0))), hash(n + step.dotWith(fvec3(1, 1, 0))), u.x);
    float b1 = noise_lerp(hash(n + step.dotWith(fvec3(0, 0, 1))), hash(n + step.dotWith(fvec3(1, 0, 1))), u.x);
    float b2 = noise_lerp(hash(n + step.dotWith(fvec3(0, 1, 1))), hash(n + step.dotWith(fvec3(1, 1, 1))), u.x);
    float c1 = noise_lerp(a1, a2, u.y);
    float c2 = noise_lerp(b1, b2, u.y);
    return noise_lerp(c1, c2, u.z);
}

float snoise(const fvec2& p) {
    fvec2 f = p.fract();
    fvec2 p_floor = p.floor();
    float v = p_floor.x + p_floor.y * 1000.0f;
    fvec4 r = fvec4(v, v + 1.0f, v + 1000.0f, v + 1001.0f);
    r = glm::fract(100000.0f * glm::sin(r * 0.001f));
    f = noise_smooth(f);
    //f = f * f * (3.0f - 2.0f * f);
    return 2.0f * noise_lerp(noise_lerp(r.x, r.y, f.x), noise_lerp(r.z, r.w, f.x), f.y) - 1.0f;
}

}