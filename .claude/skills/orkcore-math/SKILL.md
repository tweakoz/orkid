---
name: orkcore-math
description: Answer questions about orkid's math types including fvec2/3/4, fmtx3/4, fquat, DecompTransform, TransformNode, MultiCurve1D, color utilities, and Klein geometric algebra integration. Use when the user asks about vectors, matrices, quaternions, transforms, or math operations.
user-invocable: false
---

# Orkid Math Types Reference

When answering questions about math types in orkid, consult these files. All under `ork.core/`.

## Key Files

| Component | Header |
|-----------|--------|
| fvec2/dvec2 | `inc/ork/math/cvector2.h` |
| fvec3/dvec3 | `inc/ork/math/cvector3.h` |
| fvec4/dvec4 | `inc/ork/math/cvector4.h` |
| fmtx3/dmtx3 | `inc/ork/math/cmatrix3.h` |
| fmtx4/dmtx4 | `inc/ork/math/cmatrix4.h` |
| fquat/dquat | `inc/ork/math/quaternion.h` |
| TransformNode | `inc/ork/math/TransformNode.h` |
| DecompTransform | `inc/ork/math/TransformNode.h` (same file) |
| MultiCurve1D | `inc/ork/math/multicurve.h` |
| Color Math | `inc/ork/math/colormath.inl` |
| Misc Utils | `inc/ork/math/misc_math.h` |
| Python Bindings | `inc/ork/python/common_bindings/pyext_math_la.inl` |

## Quick Constructor Reference

```python
# Vectors
fvec2(x, y)
fvec3(x, y, z)        # or fvec3(scalar) for all same
fvec4(x, y, z, w)     # or fvec4(fvec3, w=1.0)

# Matrices
fmtx3()               # Identity
fmtx4()               # Identity
fmtx4(fquat)          # From quaternion

# Quaternions
fquat()               # Identity (0,0,0,1)
fquat(x, y, z, w)
fquat(axis_fvec3, angle_radians)
fquat(fmtx4)          # From matrix
```

## Vector Methods (fvec3 example)

```python
v = fvec3(1, 2, 3)
v.x, v.y, v.z         # Component access
v.length               # Magnitude (read-only property)
v.normalized           # Normalized copy (read-only property)
v.normalize()          # In-place normalize
v.dot(other)           # Dot product
v.cross(other)         # Cross product
v.angle(other)         # Angle in radians
v.lerp(a, b, t)        # Linear interpolation
v.clamped(min, max)    # Clamp all components
v.saturated()          # Clamp to [0,1]
v.reflect(normal)      # Reflect around normal
v.transform(fmtx4)    # Transform by matrix → fvec4
v.xy                   # Swizzle to fvec2
```

**Color methods on fvec3:**
```python
v.setHSV(h, s, v)     # Set RGB from HSV
v.convertRgbToHsv()   # RGB → HSV
fvec3.Black(), fvec3.Red(), fvec3.Green(), fvec3.Blue(), fvec3.White()  # Constants
```

## Matrix Methods (fmtx4 example)

```python
m = fmtx4()
m.setTranslation(x, y, z)
m.setScale(x, y, z)
m.setRotateX(radians)  # Also Y, Z
m.translation          # Get as fvec3 (property)
m.inverse              # Get inverse (property)
m.transposed           # Get transpose (property)
m.determinant          # Property
m.xNormal(), m.yNormal(), m.zNormal()  # Basis vectors as fvec3

# Decomposition
pos, rot, scale = m.decompose()        # → fvec3, fquat, float
m.compose(pos_fvec3, rot_fquat, scale) # Set from TRS
m = fmtx4.composed(pos, rot, scale)    # Static factory

# View/Projection
m = fmtx4.perspective(fovy_deg, aspect, near, far)
m = fmtx4.lookAt(eye_fvec3, target_fvec3, up_fvec3)

# Multiplication
result = m1 * m2       # Right-to-left
```

## Quaternion Methods

```python
q = fquat(axis, angle)
q.toMatrix()           # → fmtx4
q.toMatrix3()          # → fmtx3
q.toAxisAngle()        # → fvec4(ax, ay, az, angle)
q.norm()               # Magnitude
q.normalized           # Property
q.conjugate()          # Returns conjugate
q.inverse()            # Returns inverse
q1 * q2                # Quaternion multiplication

# Static interpolation (C++)
fquat.slerp(q1, q2, t)  # Spherical lerp
fquat.lerp(q1, q2, t)   # Linear lerp
```

## DecompTransform

```python
xf = DecompTransform()
xf._translation = fvec3(x, y, z)
xf._rotation = fquat(axis, angle)
xf._uniformScale = 1.0
xf._nonUniformScale = fvec3(sx, sy, sz)
xf._useNonUniformScale = False

mtx = xf.composed()    # → fmtx4
xf.decompose(fmtx4)    # Extract from matrix
xf.lookAt(eye, target, up)
```

## MultiCurve1D

```python
curve = MultiCurve1D()
curve.Init(num_segments)
curve.SetPoint(index, parameter, value)
curve.SetSegmentType(index, type)  # LINEAR, BOX, LOG, EXP
val = curve.Sample(u)  # Sample at parameter u
```

## Klein Geometric Algebra Integration

fvec3, fquat, fmtx4 all interoperate with Klein library:
- `fvec3(kln::point)`, `fvec3(kln::direction)`
- `fvec3.asKleinPoint()`, `fvec3.asKleinDirection()`
- `fquat(kln::rotor)`, `fquat.asKleinRotor()`
- `fmtx4(kln::motor)`, `fmtx4(kln::rotor)`, `fmtx4(kln::translator)`

## Type Aliases

```cpp
using fcolor3 = fvec3;
using fcolor4 = fvec4;
```

## How to Answer

1. For API details: read the specific header (cvector3.h, cmatrix4.h, etc.)
2. For Python API: check `pyext_math_la.inl` for exact binding names
3. Float types (`fvec3`, `fmtx4`, `fquat`) are the primary types used everywhere
4. Double variants (`dvec3`, `dmtx4`, `dquat`) exist but are less common
