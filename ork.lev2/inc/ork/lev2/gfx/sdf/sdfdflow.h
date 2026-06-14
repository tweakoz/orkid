////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// sdfdflow.h — the SDFGRID dflow family (HYPERECS E.7 / review 3.4). GPU signed-
// distance fields as graph citizens, carried on the family-neutral SdfGrid
// interchange plug (dflow/interchange.h — dense brick v1, NanoVDB blob in M3).
//
// RATIFIED CONTRACT (2026-06-12):
//  - DENSE BRICK = the GPU-WRITABLE substrate (voxelize / CSG / marching cubes —
//    the per-frame boolean-modeling chain). NANOVDB = the sparse READ/transport
//    blob (baked assets, track-scale sampling; one buffer = cook DataBlock =
//    sidecar = SSBO). OpenVDB is CPU build/oracle ONLY — never per-frame.
//  - v1 ENV: SDF modules ride INSIDE hypermesh graphs (the boolean chain's home)
//    and use the GraphInst's MeshEnv (ctx + the family-neutral pow2 SSBO pool).
//    A standalone-SDF-graph driver is deferred until something needs one.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/dataflow/all.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dflow/interchange.h>
#include <ork/lev2/gfx/hypermesh/hmdflow.h> // MeshEnv (the v1 env contract) + pool

namespace ork::lev2::sdf {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// module base — mirrors MeshModuleData/MeshComputeInst at minimal scale.
///////////////////////////////////////////////////////////////////////////////

struct SdfModuleData : public dflow::DgModuleData {
  DeclareAbstractX(SdfModuleData, dflow::DgModuleData);
  SdfModuleData() = default;
};

///////////////////////////////////////////////////////////////////////////////
// SdfEval — evaluate an authored GLSL distance expression (in terms of `vec3 p`,
// world space) into a dense brick. The DSL's analytic-SDF algebra (sphere/box/
// csg operators) lowers to this expression — no grid exists until something
// needs uniform-cost sampling or a mesh gets voxelized in (M1).
// Plugs (ALL runtime — re-read every eval, pokeable):
//   dim    (int)  : cubic brick resolution (brick reallocs from the pool on change)
//   extent (float): world cube edge length
//   center (vec3) : world center of the brick
// Reflected: `expression` (BAKED into the kernel — a change recompiles).
///////////////////////////////////////////////////////////////////////////////

struct SdfEvalData : public SdfModuleData {
  DeclareConcreteX(SdfEvalData, SdfModuleData);
  SdfEvalData() = default;
  static std::shared_ptr<SdfEvalData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  std::string _expression = "length(p) - 1.0"; // GLSL float expr of `vec3 p`
};
using sdfevaldata_ptr_t = std::shared_ptr<SdfEvalData>;

///////////////////////////////////////////////////////////////////////////////
// MeshToSdf (M1) — GPU voxelize: a GpuMesh -> a dense SDF brick. THE foundational
// primitive of the SDF-boolean track (per-frame animatable; openvdb is the
// offline oracle, never the runtime).
//   distance: per-voxel nearest feature over the mesh's faces (convex faces
//   fan-decompose EXACTLY in-kernel; fan diagonals classify as face-interior).
//   sign (the ratified strategy): angle-weighted PSEUDONORMAL of the nearest
//   feature — exact on closed manifold input (face normal / edge pn from the
//   MeshEdges table / vertex pn accumulated per-corner with fixed-point
//   atomics) — with the WINDING-NUMBER fallback (solid-angle sum, |w|>1/2)
//   selected IN-KERNEL when the edge build counts boundary edges (the free
//   probe), or forced via `sign_mode`.
// Plugs (runtime): dim (cubic), extent (<=0 = AUTO from a position readback at
// topology rebuild, +pad), center (used when extent>0), pad (auto-bound margin).
// Reflected: sign_mode (-1 AUTO / 0 pseudonormal / 1 winding) — a runtime ctl.
///////////////////////////////////////////////////////////////////////////////

struct MeshToSdfData : public SdfModuleData {
  DeclareConcreteX(MeshToSdfData, SdfModuleData);
  MeshToSdfData() = default;
  static std::shared_ptr<MeshToSdfData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _sign_mode = -1; // -1 AUTO (boundary probe) / 0 pseudonormal / 1 winding
};
using meshtosdfdata_ptr_t = std::shared_ptr<MeshToSdfData>;

///////////////////////////////////////////////////////////////////////////////
// Csg (M2) — boolean composite of two SDF bricks on the dense substrate. Output
// rides A's frame; B is sampled TRILINEARLY at A's world positions (so the two
// inputs need not share a frame). op: 0 union=min, 1 intersect=max, 2 subtract=
// max(A,-B), 3 smooth-union (polynomial smin, fillet ~k). `k` is a runtime plug.
///////////////////////////////////////////////////////////////////////////////

struct CsgData : public SdfModuleData {
  DeclareConcreteX(CsgData, SdfModuleData);
  CsgData() = default;
  static std::shared_ptr<CsgData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _op = 0; // 0 union / 1 intersect / 2 subtract / 3 smooth-union
};
using csgdata_ptr_t = std::shared_ptr<CsgData>;

///////////////////////////////////////////////////////////////////////////////
// Redistance (M4a) — GPU eikonal redistance via jump-flooding: re-normalize an
// SDF brick to a TRUE |grad phi|=1 field while PRESERVING the zero-set (sign
// copied from the input). Needed because the dense brick is the field every M4b
// consumer samples, and CSG (esp. smooth_union) is not a true SDF away from the
// zero-set. Single In(SdfGrid) -> Out(SdfGrid); inherits the input's frame (the
// Csg-inherits-A pattern). Explicit DSL verb (NOT auto-in-CSG, owner-ratified).
// Reflected: max_iterations (0 = auto = ceil(log2(next-pow2(dim))) JFA passes).
///////////////////////////////////////////////////////////////////////////////

struct RedistanceData : public SdfModuleData {
  DeclareConcreteX(RedistanceData, SdfModuleData);
  RedistanceData() = default;
  static std::shared_ptr<RedistanceData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  int _max_iterations = 0; // 0 = auto (ceil(log2 next-pow2 dim)); >0 forces the JFA pass count
};
using redistancedata_ptr_t = std::shared_ptr<RedistanceData>;

///////////////////////////////////////////////////////////////////////////////
// SdfToMesh (M2) — marching cubes: a dense SDF brick -> an INDEXED GpuMesh (tris).
// It is a MESH PRODUCER (SdfGrid "In" -> Mesh "Out"), so it is a hypermesh
// MeshComputeInst: the mesh driver gives it onTopologyReady (the MC output size
// is data-dependent — eval 1 classifies + scans + emits into worst-case buffers,
// onTopologyReady reads the live counts back to the CPU), and it joins the mesh
// cook-cache + render path like any other generated mesh. v1 emits per-cell
// triangle SOUP; the MeshSort position-weld (shared verts) layers on next.
// Reflected: iso (the level), weld (bool — dedup coincident verts via MeshSort).
///////////////////////////////////////////////////////////////////////////////

struct SdfToMeshData : public hypermesh::MeshModuleData {
  DeclareConcreteX(SdfToMeshData, hypermesh::MeshModuleData);
  SdfToMeshData();
  static std::shared_ptr<SdfToMeshData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  bool _weld   = true;  // MeshSort position-weld -> shared (watertight, indexed) verts
  bool _blocky = false; // CUBERILLE: axis-aligned voxel faces (pure blocky, no interpolation/smoothing)
                        // instead of marching-tetrahedra. Forces weld OFF (hard per-face normals must
                        // NOT be merged). Block size = the brick voxel (RES/extent).
};
using sdftomeshdata_ptr_t = std::shared_ptr<SdfToMeshData>;

///////////////////////////////////////////////////////////////////////////////
// gates
///////////////////////////////////////////////////////////////////////////////

// M0 oracle — builds SdfEval graphs in C++, runs them through the real dispatch
// machinery (MeshEnv + pool + phases), reads the bricks back and asserts every
// voxel against the ANALYTIC distance (sphere / two-sphere union), plus layout
// (x-fastest indexing), sign at known inside/outside points, and plug-driven
// re-evaluation (poke dim -> brick reallocs + recomputes). Returns failure count.
int sdfGridSelfTest(Context* ctx);

// M1 oracle — voxelizes hypermesh primitives (box / icosphere) through the real
// graph machinery and gates the bricks against openvdb::meshToLevelSet built
// from the SAME readback geometry (sign agreement everywhere vdb is resolved;
// near-band distance agreement), plus FORCED pseudonormal-vs-winding sign-field
// equivalence on clean meshes. Returns failure count.
int sdfVoxelizeSelfTest(Context* ctx);

// M2 oracle — builds two analytic SDF bricks (SdfEval) and a Csg of them, reads
// the result brick back and asserts EVERY voxel against the analytic boolean
// (union=min, intersect=max, subtract=max(A,-B)) within the trilinear-sample
// tolerance. Returns failure count.
int sdfCsgSelfTest(Context* ctx);

// M2 oracle — marching-cubes a known analytic brick (sphere) into a GpuMesh and
// asserts: a non-trivial triangle count, EVERY emitted vertex lies on the iso
// surface (|sdf(v)| < tol via the analytic field), and (when welded) the mesh is
// watertight (every edge shared by exactly two triangles). Returns failure count.
int sdfMeshSelfTest(Context* ctx);

// M4a oracle — redistance a SCALED sphere SDF (|grad|=3, non-unit) + a smooth-union
// blob, read the brick back, and assert: the zero-set is preserved (sign matches the
// input voxel-for-voxel), the redistanced field matches the TRUE analytic sphere
// distance in the near band (JFA sub-voxel-seed accuracy), and |grad phi| -> 1 in the
// interior. Returns failure count.
int sdfRedistanceSelfTest(Context* ctx);

} // namespace ork::lev2::sdf
