////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include "modular_particles2.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct SphAttractorModuleData : public ParticleModuleData {
  DeclareConcreteX(SphAttractorModuleData, ParticleModuleData);
public:
  SphAttractorModuleData();
  static std::shared_ptr<SphAttractorModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using sphattractormodule_ptr_t = std::shared_ptr<SphAttractorModuleData>;

/////////////////////////////////////////

struct EllipticalAttractorModuleData : public ParticleModuleData {
  DeclareConcreteX(EllipticalAttractorModuleData, ParticleModuleData);
public:
  EllipticalAttractorModuleData();
  static std::shared_ptr<EllipticalAttractorModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using eliattractormodule_ptr_t = std::shared_ptr<EllipticalAttractorModuleData>;

/////////////////////////////////////////

struct PointAttractorModuleData : public ParticleModuleData {
  DeclareConcreteX(PointAttractorModuleData, ParticleModuleData);
public:
  PointAttractorModuleData();
  static std::shared_ptr<PointAttractorModuleData> createShared();
  //static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using pntattractormodule_ptr_t = std::shared_ptr<PointAttractorModuleData>;

/////////////////////////////////////////

struct GravityModuleData : public ParticleModuleData {
  DeclareConcreteX(GravityModuleData, ParticleModuleData);
public:
  GravityModuleData();
  static std::shared_ptr<GravityModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using gravitymodule_ptr_t = std::shared_ptr<GravityModuleData>;

/////////////////////////////////////////

/////////////////////////////////////////
// DirectionalForce — a constant per-particle acceleration applied in the
// given Direction with the given Magnitude. Unlike Gravity (point gravity
// scaled by mass/distance) this is a true uniform force: every particle
// gets the same Δv = Direction * Magnitude * dt regardless of position.
// Direction is renormalized inside compute(); Magnitude in world
// units/sec² (negative flips the direction).
struct DirectionalForceModuleData : public ParticleModuleData {
  DeclareConcreteX(DirectionalForceModuleData, ParticleModuleData);
public:
  DirectionalForceModuleData();
  static std::shared_ptr<DirectionalForceModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using directional_force_module_ptr_t = std::shared_ptr<DirectionalForceModuleData>;

/////////////////////////////////////////

// ExprForceModule (E2.5 S8) — per-particle acceleration authored as a canonical ExprIR TREE
// (context "particles.force"). Three scalar force exprs (x/y/z) are stored as the reflected
// JSON the DSL emits (exprir.encode_json); the instance parses them once and evaluates the
// tree per particle on the CPU, so a sim advance is deterministic. A `Strength` float plug
// scales the whole force (A8-parametric). Symbols: unit_age/age/random/pos.*/vel.*/speed.
struct ExprForceModuleData : public ParticleModuleData {
  DeclareConcreteX(ExprForceModuleData, ParticleModuleData);
public:
  ExprForceModuleData();
  static std::shared_ptr<ExprForceModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
  std::string _force_x, _force_y, _force_z; // canonical particles.force ExprIR JSON per axis
};

using expr_force_module_ptr_t = std::shared_ptr<ExprForceModuleData>;

/////////////////////////////////////////

struct TurbulenceModuleData : public ParticleModuleData {
  DeclareConcreteX(TurbulenceModuleData, ParticleModuleData);
public:
  TurbulenceModuleData();
  static std::shared_ptr<TurbulenceModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using turbulencemodule_ptr_t = std::shared_ptr<TurbulenceModuleData>;

/////////////////////////////////////////

// CurlNoiseForce — divergence-free procedural flow field. Each frame, for
// each particle, samples a 3D vector-potential noise at the particle's
// position and takes the curl via finite differences. The result is a
// vector field with ∇·F=0 → mass is conserved → particles never converge
// to or diverge from a point, just swirl. The signature look of modern
// smoke / fire / fluid effects.
//
// Inputs:
//   Strength   — scalar magnitude (default 1)
//   Frequency  — spatial frequency of the noise (1/scale; default 0.5)
//   Speed      — rate at which the noise field evolves in time (default 0.1)
//   Epsilon    — finite-diff step size for the curl (default 0.05)
//   CellSize   — when > 0, snap sample position to a grid before noise
//                eval. ψ is constant within cells → curl spikes at
//                axis-aligned cell boundaries → grid/circuit-like
//                discharges. Default 0 (smooth).
//   Levels     — when > 0, quantize each ψ component to N discrete
//                levels before the curl. ψ is constant on level sets →
//                curl spikes at organic, dendritic boundaries →
//                arborescent / lightning-like discharges. Default 0.
//                Composes with CellSize (set both for voxelized lightning).
struct CurlNoiseForceModuleData : public ParticleModuleData {
  DeclareConcreteX(CurlNoiseForceModuleData, ParticleModuleData);
public:
  CurlNoiseForceModuleData();
  static std::shared_ptr<CurlNoiseForceModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using curlnoiseforce_ptr_t = std::shared_ptr<CurlNoiseForceModuleData>;

/////////////////////////////////////////

// PolyDrag — polynomial drag force opposing velocity. Decel magnitude is
//   |F| = Constant + Linear·|v| + Quadratic·|v|² + Cubic·|v|³
// applied antiparallel to the velocity. Each term has a clear physical
// reading:
//   Constant   — Coulomb-style friction (decelerates uniformly)
//   Linear     — Stokes drag (laminar, low-Reynolds — small particles)
//   Quadratic  — Newtonian drag (turbulent, high-Reynolds — air resistance)
//   Cubic      — high-velocity nonlinear damping
// All terms default to 0 → default PolyDrag has no effect. Mix freely to
// emulate any drag regime; the per-frame integration clamps so drag never
// reverses the particle's direction.
struct PolyDragModuleData : public ParticleModuleData {
  DeclareConcreteX(PolyDragModuleData, ParticleModuleData);
public:
  PolyDragModuleData();
  static std::shared_ptr<PolyDragModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using polydrag_ptr_t = std::shared_ptr<PolyDragModuleData>;

/////////////////////////////////////////

struct VortexModuleData : public ParticleModuleData {
  DeclareConcreteX(VortexModuleData, ParticleModuleData);
public:
  VortexModuleData();
  static std::shared_ptr<VortexModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using vortexmodule_ptr_t = std::shared_ptr<VortexModuleData>;

/////////////////////////////////////////

struct DragModuleData : public ParticleModuleData {
  DeclareConcreteX(DragModuleData, ParticleModuleData);
public:
  DragModuleData();
  static std::shared_ptr<DragModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using dragmodule_ptr_t = std::shared_ptr<DragModuleData>;

/////////////////////////////////////////

// PlaneCollider — half-space collider. Tests
//   d = dot(pos - Center, Normal)
// Particles with d < 0 are penetrating: snap back along Normal, reflect
// the normal component of velocity scaled by Restitution, dampen the
// tangential component by Friction.
//
// Plugs (uniform-rate):
//   Center      — point on the plane (vec3)
//   Normal      — outward normal direction (vec3; renormalized per frame
//                 so the author can write unnormalized like vec3(0,1,0))
//   Restitution — normal-velocity bounce coefficient. 0 = stick to surface,
//                 1 = elastic, >1 = energy gain (jumpy). Default 0.5.
//   Friction    — tangential-velocity damping each contact. 0 = frictionless
//                 slide, 1 = full stop. Default 0.0.
//
// Effects on particle:
//   mPosition pushed out of half-space (snapped to plane)
//   mVelocity reflected/damped
//   mColliderStates bit 0 set on contact (consumers may read for event-style
//   reactions; cleared by anyone who reads it)
struct PlaneColliderModuleData : public ParticleModuleData {
  DeclareConcreteX(PlaneColliderModuleData, ParticleModuleData);
public:
  PlaneColliderModuleData();
  static std::shared_ptr<PlaneColliderModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using planecollider_ptr_t = std::shared_ptr<PlaneColliderModuleData>;

/////////////////////////////////////////

// SphereCollider — solid-sphere obstacle. Tests
//   delta = pos - Center; dist = length(delta)
// Particles with dist < Radius are penetrating: push out radially to
// the surface, reflect normal-velocity by Restitution, dampen
// tangential by Friction. Only does OUTSIDE collision (sphere is
// solid); an inverted "containment shell" variant can be a future
// SphereContainer module if needed.
//
// Plugs (uniform-rate):
//   Center      — sphere center (vec3)
//   Radius      — sphere radius (float, world units)
//   Restitution — see PlaneCollider; default 0.5
//   Friction    — see PlaneCollider; default 0.0
struct SphereColliderModuleData : public ParticleModuleData {
  DeclareConcreteX(SphereColliderModuleData, ParticleModuleData);
public:
  SphereColliderModuleData();
  static std::shared_ptr<SphereColliderModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using spherecollider_ptr_t = std::shared_ptr<SphereColliderModuleData>;

/////////////////////////////////////////

// VdbCollider — arbitrary-surface collider sampled from an OpenVDB
// signed-distance-field grid (a level set). The grid is passed in as a
// non-plug property (`_sdfGrid`) — typically built once at graph-
// construction time from a mesh via `lev2.openvdb.meshToLevelSet(...)`.
//
// Per particle each frame:
//   sdf = trilinear sample of _sdfGrid at world pos
//   if (sdf < 0)  → penetrating SOLID
//     normal = ∇sdf at that point (6-tap central differences, world space)
//     penetration = -sdf
//     resolve_collision(...)
//
// Use cases:
//   - Funnel / chute geometry (build closed thick-walled mesh, convert)
//   - Static level-collision (load .vdb asset; not wired in v1)
//   - Procedural collision against any closed surface
//
// Limitations:
//   - Grid is set once and never mutated by the collider; if you want
//     animated collision, set the grid before each frame from Python.
//   - The grid must be a proper SDF (negative inside the solid). Binary
//     in/out grids will produce noisy gradients and ugly bounces.
//   - meshToLevelSet requires a CLOSED mesh; open surfaces give
//     garbage inside/outside classification.
//
// Plugs (uniform-rate):
//   Restitution / Friction — same as PlaneCollider / SphereCollider.
// Forward-declared holder so the header doesn't pull in <openvdb>. The
// concrete struct (carrying vdb_floatgrid_ptr_t) is defined in
// modules_force_vdb_collider.cpp and shared with the pyext binding.
struct VdbColliderGridHolder;
using vdbcollider_grid_holder_ptr_t = std::shared_ptr<VdbColliderGridHolder>;

struct VdbColliderModuleData : public ParticleModuleData {
  DeclareConcreteX(VdbColliderModuleData, ParticleModuleData);
public:
  VdbColliderModuleData();
  static std::shared_ptr<VdbColliderModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

  // The SDF grid. Set from Python via `.sdf_grid = <FloatGrid>` on the
  // pyext binding (lev2::particle::VdbCollider). Held via an opaque
  // shared holder so this header has no <openvdb> dependency.
  // RUNTIME-only — never serialized (derived voxel data; see _sdf_asset_name).
  vdbcollider_grid_holder_ptr_t _sdfGrid;

  // ASSET-REFERENCE plug: the AssetSystem name of the SDF asset whose materialized
  // FloatGrid fills _sdfGrid at load (the by-name convention every other cross-asset
  // binding uses). REFLECTED — this is what makes a graph holding a live grid fully
  // serializable (model B): the recipe rides the scene as its gen (ImplicitSdfGenData
  // etc.), the graph carries only this NAME, and the host resolves it from the artifact
  // registry post-deserialize (the Python wire today; the C++ host's VarMap at D.5).
  // Stamped automatically at trace time (the ParticleSystem wrapper registers
  // {artifact -> asset_name}; the DSL op builder consults it). Empty = grid supplied
  // out-of-band (standalone/imperative use).
  std::string _sdf_asset_name;

  // PLACEMENT offset of the SDF surface in its (possibly entity-local) sampling
  // frame, in METERS: the collision surface moves BY +_sdf_offset relative to the
  // grid as authored (sampling happens at p - offset). Lets the COLLISION surface
  // sit slightly off the VISUAL surface built from the same shared grid (e.g.
  // saddle2: +1cm up so resting water doesn't peek through the underside).
  fvec3 _sdf_offset;

  // Optional name of a published entity transform — looked up at
  // compute() time via GraphInst::_resolveEntityXf. When non-empty +
  // resolvable, the SDF is treated as living in that entity's local
  // frame: particles are mapped by inv(host_xf) before SDF sampling and
  // the gradient normal is rotated back by host_xf for the collision
  // response. Empty (default) or unresolved → world-space sampling
  // (legacy behavior). See SpawnData::_publishxf_name for the publisher
  // side and Simulation's xf registry for the lookup pipeline.
  std::string _follow_entity;
};

using vdbcollider_ptr_t = std::shared_ptr<VdbColliderModuleData>;

/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
