////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hfdflow_scatter — the C++ SCATTER PLACER (HYPERECS E.2 / review 2.10): runs a
// ScatterSinkData (the reflected D.4 placement contract) against a bake's channel
// images and emits a ScatterSet point Geometry (P / xform / type_id / variant_seed),
// the same artifact Python's scatter.place produced. Hooked into
// HeightFieldGenData::materialize, so a deserialized scene PLACES at load with zero
// Python (the artifact lands at <assetcache>/terrain/<asset>/<sink>.ogeo).
//
// DETERMINISM: placement draws come from a STATELESS counter-based hash RNG keyed
// (seed, cell, stream) — order-independent, language-independent (scatter.py
// implements the IDENTICAL chain in vectorized numpy; the parity gate pins the two
// together: counts / type_id / variant_seed EXACT, float geometry to tolerance).
// The grid is in METERS, so the same graph+seed places identically at any bake dim.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <memory>

namespace ork::meshutil {
struct Geometry;
}

namespace ork::lev2 {
struct ScatterSinkData;
}

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// the shared RNG spec (THE contract — change it nowhere without changing it everywhere):
//   mix(x): splitmix64 finalizer (x ^= x>>30; x *= 0xBF58476D1CE4E5B9;
//                                 x ^= x>>27; x *= 0x94D049BB133111EB; x ^= x>>31)
//   hash(seed, idx, stream) = mix( mix( mix(seed ^ 0x9E3779B97F4A7C15) ^ idx ) ^ stream )
//   u01 = (hash >> 11) * 2^-53          (uniform double in [0,1))
///////////////////////////////////////////////////////////////////////////////

uint64_t scatterHash(uint64_t seed, uint64_t idx, uint64_t stream);
double scatterU01(uint64_t seed, uint64_t idx, uint64_t stream);

// per-cell draw streams (idx = the linear grid-cell index k; i = k % ncx, j = k / ncx)
enum ScatterStream : uint64_t {
  SS_JITTER_X = 0, // in-cell jitter, x
  SS_JITTER_Z = 1, // in-cell jitter, z
  SS_KEEP     = 2, // coverage keep test vs the total weight
  SS_TYPE     = 3, // weighted type pick (inverse-CDF)
  SS_YAW      = 4, // random yaw in [yaw_lo, yaw_hi]
  SS_SCALE    = 5, // uniform per-point scale in [scale_lo, scale_hi]
  SS_VSEED    = 6, // per-instance variant seed (integer, exact across languages)
  SS_PRIO     = 7, // subsample priority (count mode / max_points cap: keep the smallest)
};

///////////////////////////////////////////////////////////////////////////////
// run one sink. `channel_paths` maps channel name -> baked image path ("height" +
// every weight channel the sink's _type_channels references). Returns the ScatterSet
// point Geometry (null on a missing channel — loudly). Output point order = grid-cell
// order (ascending k), also after a subsample (selected-by-priority, then re-sorted).
///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<meshutil::Geometry> scatterPlace(
    const ScatterSinkData& sink,
    const std::map<std::string, std::string>& channel_paths,
    float extent_m,
    float height_m);

} // namespace ork::lev2::terrain
