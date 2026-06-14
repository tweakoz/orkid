////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// hfdflow_scatter.cpp — see hfdflow_scatter.h. A 1:1 port of scatter.py's place()
// with the RandomState draws replaced by the stateless hash RNG (the SAME chain
// scatter.py now uses — the parity gate pins them). Math intentionally mirrors the
// numpy reference operation-for-operation (float32 weight sums, float64 transforms,
// truncating nearest-texel sampling, np.gradient's central/one-sided differences).
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/terrain/dflow/hfdflow_scatter.h>
#include <ork/lev2/gfx/asset_gen.h>
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/math/cvector3.h>
#include <ork/math/cmatrix4.h>

#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// the shared RNG (see the header spec)
///////////////////////////////////////////////////////////////////////////////

static inline uint64_t _mix64(uint64_t x) {
  x ^= x >> 30;
  x *= 0xBF58476D1CE4E5B9ull;
  x ^= x >> 27;
  x *= 0x94D049BB133111EBull;
  x ^= x >> 31;
  return x;
}

uint64_t scatterHash(uint64_t seed, uint64_t idx, uint64_t stream) {
  return _mix64(_mix64(_mix64(seed ^ 0x9E3779B97F4A7C15ull) ^ idx) ^ stream);
}

double scatterU01(uint64_t seed, uint64_t idx, uint64_t stream) {
  return double(scatterHash(seed, idx, stream) >> 11) * (1.0 / 9007199254740992.0); // 2^-53
}

///////////////////////////////////////////////////////////////////////////////
// channel image -> (H,W) float field (channel 0). Mirrors scatter.py _read_channel.
///////////////////////////////////////////////////////////////////////////////

namespace {

struct Field {
  std::vector<float> _v;
  int _w = 0, _h = 0;
  float at(int x, int y) const { return _v[size_t(y) * _w + x]; }
};

bool _readField(const std::string& path, Field& out) {
  auto in = OIIO::ImageInput::open(path);
  if (not in) {
    printf("scatterPlace: channel image MISSING <%s>\n", path.c_str());
    return false;
  }
  const auto& spec = in->spec();
  out._w = spec.width;
  out._h = spec.height;
  std::vector<float> px(size_t(spec.width) * spec.height * spec.nchannels);
  in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, px.data());
  in->close();
  out._v.resize(size_t(out._w) * out._h);
  for (size_t i = 0; i < out._v.size(); i++)
    out._v[i] = px[i * spec.nchannels];
  return true;
}

// nearest-texel sample at uv in [0,1] — numpy: clip((u*w).astype(int64), 0, w-1)
// (astype truncates toward zero; u is non-negative here, so trunc == floor).
inline float _sampleNearest(const Field& f, double u, double v) {
  int xi = int(u * f._w);
  int yi = int(v * f._h);
  xi = std::min(std::max(xi, 0), f._w - 1);
  yi = std::min(std::max(yi, 0), f._h - 1);
  return f.at(xi, yi);
}

// np.gradient port over a double field (spacing 1): central differences in the
// interior, one-sided at the edges. `axis` 0 = rows (z), 1 = cols (x).
std::vector<double> _gradient(const std::vector<double>& a, int W, int H, int axis) {
  std::vector<double> g(a.size());
  if (axis == 1) { // d/dcol
    for (int r = 0; r < H; r++) {
      const double* row = a.data() + size_t(r) * W;
      double* go        = g.data() + size_t(r) * W;
      if (W == 1) { go[0] = 0.0; continue; }
      go[0]     = row[1] - row[0];
      go[W - 1] = row[W - 1] - row[W - 2];
      for (int c = 1; c < W - 1; c++)
        go[c] = (row[c + 1] - row[c - 1]) * 0.5;
    }
  } else { // d/drow
    for (int c = 0; c < W; c++) {
      if (H == 1) { g[c] = 0.0; continue; }
      g[size_t(0) * W + c]       = a[size_t(1) * W + c] - a[size_t(0) * W + c];
      g[size_t(H - 1) * W + c]   = a[size_t(H - 1) * W + c] - a[size_t(H - 2) * W + c];
      for (int r = 1; r < H - 1; r++)
        g[size_t(r) * W + c] = (a[size_t(r + 1) * W + c] - a[size_t(r - 1) * W + c]) * 0.5;
    }
  }
  return g;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<meshutil::Geometry> scatterPlace(
    const ScatterSinkData& sink,
    const std::map<std::string, std::string>& channel_paths,
    float extent_m_f,
    float height_m_f) {

  const double extent_m = double(extent_m_f);
  const double height_m = double(height_m_f);
  const uint64_t seed   = uint64_t(uint32_t(sink._seed)); // non-negative, matches py (seed & mask)

  // --- channels ---------------------------------------------------------------
  auto it_h = channel_paths.find("height");
  if (it_h == channel_paths.end()) {
    printf("scatterPlace<%s>: no 'height' channel path supplied\n", sink._name.c_str());
    return nullptr;
  }
  Field height;
  if (not _readField(it_h->second, height))
    return nullptr;
  const int W = height._w, H = height._h;

  const int K = int(sink._type_channels.size());
  OrkAssert(K >= 1);
  std::vector<Field> weights(K);
  for (int t = 0; t < K; t++) {
    auto it = channel_paths.find(sink._type_channels[t]);
    if (it == channel_paths.end()) {
      printf("scatterPlace<%s>: weight channel <%s> path missing\n",
             sink._name.c_str(), sink._type_channels[t].c_str());
      return nullptr;
    }
    if (not _readField(it->second, weights[t]))
      return nullptr;
  }

  const bool count_mode = (sink._count > 0);
  const double density  = double(sink._density);

  // --- jittered grid (cells in METERS -> resolution-independent) ---------------
  int ncx;
  if (not count_mode)
    ncx = std::max(1, int(std::lround(extent_m * std::sqrt(density))));
  else // count mode: oversample, then priority-subsample the kept set to `count`
    ncx = std::max(1, int(std::ceil(std::sqrt(double(std::max(1, sink._count)) * 4.0))));
  const double cell_m = extent_m / double(ncx);
  const double jitter = double(sink._jitter);

  struct Kept {
    int64_t k;      // cell index (output order key)
    int type_id;
    double wx, wz, u, v;
    double prio;    // subsample priority (keep smallest)
  };
  std::vector<Kept> kept;
  kept.reserve(size_t(ncx) * ncx / 4 + 16);

  const double cutoff = std::max(double(sink._cutoff), 1e-9);
  const int64_t ncells = int64_t(ncx) * int64_t(ncx);
  std::vector<float> ws(K);
  for (int64_t k = 0; k < ncells; k++) {
    const double i = double(k % ncx); // meshgrid 'xy' raveled: ci varies fastest
    const double j = double(k / ncx);
    const double wx = (i + 0.5 + (scatterU01(seed, k, SS_JITTER_X) - 0.5) * jitter) * cell_m - extent_m * 0.5;
    const double wz = (j + 0.5 + (scatterU01(seed, k, SS_JITTER_Z) - 0.5) * jitter) * cell_m - extent_m * 0.5;
    const double u  = wx / extent_m + 0.5;
    const double v  = wz / extent_m + 0.5;
    float Wsum = 0.0f; // float32 accumulation, declaration order (== the numpy reference)
    for (int t = 0; t < K; t++) {
      float w = _sampleNearest(weights[t], u, v);
      ws[t]   = (w < 0.0f) ? 0.0f : w;
      Wsum += ws[t];
    }
    if (double(Wsum) < cutoff)
      continue;
    const double keep_p = std::min(std::max(double(Wsum), 0.0), 1.0);
    if (not(scatterU01(seed, k, SS_KEEP) < keep_p))
      continue;
    // weighted type pick: inverse-CDF over the per-point weights (float32 cumsum)
    int type_id = 0;
    {
      std::vector<float> cdf(K);
      float acc = 0.0f;
      for (int t = 0; t < K; t++) { acc += ws[t]; cdf[t] = acc; }
      const double r = scatterU01(seed, k, SS_TYPE) * double(cdf[K - 1]);
      int ge = 0;
      for (int t = 0; t < K; t++)
        if (r >= double(cdf[t]))
          ge++;
      type_id = std::min(std::max(ge, 0), K - 1);
    }
    kept.push_back(Kept{k, type_id, wx, wz, u, v, scatterU01(seed, k, SS_PRIO)});
  }

  // --- cap (or count-mode subsample): keep the `target` SMALLEST priorities,
  //     then restore grid-cell order (the spec'd, language-neutral output order) ---
  const int64_t target =
      count_mode ? std::min(int64_t(sink._count), int64_t(sink._max_points)) : int64_t(sink._max_points);
  if (int64_t(kept.size()) > target) {
    std::sort(kept.begin(), kept.end(), [](const Kept& a, const Kept& b) {
      return (a.prio != b.prio) ? (a.prio < b.prio) : (a.k < b.k);
    });
    kept.resize(size_t(target));
    std::sort(kept.begin(), kept.end(), [](const Kept& a, const Kept& b) { return a.k < b.k; });
  }
  const int N = int(kept.size());

  auto geo = std::make_shared<meshutil::Geometry>();
  if (N == 0)
    return geo;

  // --- normals from the height gradient (physical meters), align == "normal" ---
  const bool align_normal = (sink._align == "normal");
  std::vector<double> gx, gz;
  if (align_normal) {
    std::vector<double> hm(size_t(W) * H);
    for (size_t i = 0; i < hm.size(); i++)
      hm[i] = double(height._v[i]) * height_m;
    gz = _gradient(hm, W, H, 0); // d/drow (z)
    gx = _gradient(hm, W, H, 1); // d/dcol (x)
  }
  auto sampleD = [&](const std::vector<double>& f, double u, double v) -> double {
    int xi = int(u * W), yi = int(v * H);
    xi = std::min(std::max(xi, 0), W - 1);
    yi = std::min(std::max(yi, 0), H - 1);
    return f[size_t(yi) * W + xi];
  };

  // --- channels ----------------------------------------------------------------
  auto chP  = geo->_point.createChannel<fvec3>("P");
  auto chX  = geo->_point.createChannel<fmtx4>("xform");
  auto chT  = geo->_point.createChannel<int>("type_id");
  auto chS  = geo->_point.createChannel<int>("variant_seed");
  // PER-ITEM physics proxy (the collider shape rides the DATA): kind -1 = none,
  // 0 = sphere(d0) 1 = capsule(d0 radius, d1 height) 2 = box(d0,d1,d2 half-extents).
  // Parsed from the sink's per-type "kind:d0:d1:d2" declarations; consumers
  // (BulletShapeScatter) read ONLY these channels — nothing hardcoded downstream.
  auto chPK = geo->_point.createChannel<int>("proxy_kind");
  auto chPD = geo->_point.createChannel<fvec3>("proxy_dims");
  std::vector<int> tkind(K, -1);
  std::vector<fvec3> tdims(K, fvec3(0, 0, 0));
  for (int t = 0; t < K; t++) {
    auto it = sink._type_colliders.find(sink._type_names[t]);
    if (it == sink._type_colliders.end())
      continue;
    int k = -1;
    float d0 = 0, d1 = 0, d2 = 0;
    if (sscanf(it->second.c_str(), "%d:%f:%f:%f", &k, &d0, &d1, &d2) >= 2) {
      tkind[t] = k;
      tdims[t] = fvec3(d0, d1, d2);
    }
  }
  chP->_data.resize(N);
  chX->_data.resize(N);
  chT->_data.resize(N);
  chS->_data.resize(N);
  chPK->_data.resize(N);
  chPD->_data.resize(N);

  const double yaw_lo = double(sink._yaw_lo), yaw_hi = double(sink._yaw_hi);
  const double sc_lo  = double(sink._scale_lo), sc_hi = double(sink._scale_hi);
  const double lift   = double(sink._lift);

  for (int n = 0; n < N; n++) {
    const auto& pt = kept[n];
    const double px = pt.wx, pz = pt.wz;
    const double py = double(_sampleNearest(height, pt.u, pt.v)) * height_m;

    // normal (double math, the numpy float64 path)
    double nx = 0.0, ny = 1.0, nz = 0.0;
    if (align_normal) {
      nx = -sampleD(gx, pt.u, pt.v) / (extent_m / double(W));
      nz = -sampleD(gz, pt.u, pt.v) / (extent_m / double(H));
    }
    {
      const double il = 1.0 / std::sqrt(nx * nx + ny * ny + nz * nz);
      nx *= il; ny *= il; nz *= il;
    }

    const double yaw = yaw_lo + (yaw_hi - yaw_lo) * scatterU01(seed, pt.k, SS_YAW);
    const double sc  = sc_lo + (sc_hi - sc_lo) * scatterU01(seed, pt.k, SS_SCALE);

    // RIGHT-HANDED yaw-rotated basis about the up axis (the scatter.py construction)
    const double rx = (std::abs(nz) > 0.9) ? 1.0 : 0.0;
    const double rz = (std::abs(nz) > 0.9) ? 0.0 : 1.0;
    // t = cross(ref, up), normalized
    double tx = 0.0 * nz - rz * ny, ty = rz * nx - rx * nz, tz = rx * ny - 0.0 * nx;
    {
      const double il = 1.0 / std::sqrt(tx * tx + ty * ty + tz * tz);
      tx *= il; ty *= il; tz *= il;
    }
    // b = cross(t, up)  (right-handed, det=+1)
    const double bx = ty * nz - tz * ny;
    const double by = tz * nx - tx * nz;
    const double bz = tx * ny - ty * nx;
    const double cy = std::cos(yaw), sy = std::sin(yaw);
    const double t2x = cy * tx + sy * bx, t2y = cy * ty + sy * by, t2z = cy * tz + sy * bz;
    const double b2x = -sy * tx + cy * bx, b2y = -sy * ty + cy * by, b2z = -sy * tz + cy * bz;

    // glm COLUMNS: col0 = t2*sc, col1 = up*sc, col2 = b2*sc, col3 = pos + nrm*lift.
    // ROUNDING ORDER matches the numpy reference exactly: P is float32 FIRST, then the
    // (double) lift term adds onto the rounded value (numpy: float32 pos array + nrm*lift).
    // At large world extents (32km) the difference is a visible float32 ULP (~1e-3).
    const float pxf = float(px), pyf = float(py), pzf = float(pz);
    fmtx4 m;
    m.setColumn(0, fvec4(float(t2x * sc), float(t2y * sc), float(t2z * sc), 0.0f));
    m.setColumn(1, fvec4(float(nx * sc), float(ny * sc), float(nz * sc), 0.0f));
    m.setColumn(2, fvec4(float(b2x * sc), float(b2y * sc), float(b2z * sc), 0.0f));
    m.setColumn(3, fvec4(float(double(pxf) + nx * lift), float(double(pyf) + ny * lift),
                         float(double(pzf) + nz * lift), 1.0f));

    chP->_data[n] = fvec3(pxf, pyf, pzf);
    chX->_data[n] = m;
    chT->_data[n] = pt.type_id;
    chPK->_data[n] = tkind[pt.type_id];
    chPD->_data[n] = tdims[pt.type_id];
    // integer variant seed — EXACT across languages: top 30 bits of the hash
    chS->_data[n] = int((scatterHash(seed, pt.k, SS_VSEED) >> 34) & 0x3FFFFFFFull);
  }
  return geo;
}

} // namespace ork::lev2::terrain
