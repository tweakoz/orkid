////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#include "hfdflow_module.h"
#include <algorithm>
#include <functional>
#include <vector>
#include <cmath>
#include <cstring>

ImplementReflectionX(ork::lev2::terrain::FillClosedBasinsModuleData, "terrain::FillClosedBasinsModuleData");

namespace ork::lev2::terrain {

///////////////////////////////////////////////////////////////////////////////
// FillClosedBasinsModule — detect + fill CLOSED basins (depressions where water enters
// but can't exit, excluding evaporation), with PERSISTENCE control so nested basins don't
// all collapse to one level (the limitation of plain basin_fill).
//
// A closed basin = a region bounded by HIGHER interior terrain; the MAP EDGE is an OUTLET,
// NOT a wall — a depression that can spill off the edge is open, not closed.
//
// Self-contained CPU build (sorted-cell union-find = the watershed MERGE TREE, exact, handles
// flats — which terrace/lpf produce). Process cells low->high; each new local min starts a basin
// (pit elevation = its floor); when a cell joins two basins it's their SADDLE (pour point) and the
// shallower basin merges into the deeper. Per basin: persistence = pour - pit. `min_depth` (D) keeps
// only basins whose persistence >= D, merging shallower sub-basins into their parent -> the level dial.
// The domain boundary is a virtual OUTLET basin (pit = -inf) so edge-draining regions are never closed.
//
// Outputs:
//   Out       (mono) : the FILLED dem -> each closed basin raised to its (persistence-simplified)
//                      pour point; cells that drain off the edge are left at z.
//   Basin     (RGBA) : R=spill(pour elev) G=depth(spill-z) B=per-basin shade A=closed mask.
//   CenterPit (RGBA) : RGB=3D offset (meters) from the cell to its basin PIT (deepest cell) A=dist(m).
///////////////////////////////////////////////////////////////////////////////

static void _fillClosedBasins(const float* z, int W, int H, float D, float cell_m, float hscale_m,
                              float* o_filled, float* o_basin /*4n*/, float* o_center /*4n*/) {
  const int n = W * H;
  // ---- merge tree (sorted-cell union-find) ---------------------------------------------------
  // basin 0 is the OUTLET (boundary). pit_z=-inf so it's always the deepest -> everything that can
  // reach the edge merges into it (and is therefore NOT closed).
  std::vector<int>   b_pit_cell;  // representative pit cell index per basin (basin 0 = -1)
  std::vector<float> b_pit_z;     // pit (floor) elevation
  std::vector<float> b_pour_z;    // spill elevation (set when the basin merges up); +inf until then
  std::vector<int>   b_parent;    // basin it spilled into (-1 = none yet / outlet)
  auto newBasin = [&](int cell, float pz) -> int {
    b_pit_cell.push_back(cell); b_pit_z.push_back(pz);
    b_pour_z.push_back(3.0e38f); b_parent.push_back(-1);
    return int(b_pit_cell.size()) - 1;
  };
  newBasin(-1, -3.0e38f); // basin 0 = OUTLET

  // union-find over CELLS; each component carries its currently-active (deepest, unmerged) basin id.
  std::vector<int> uf(size_t(n), int(-1));
  for (int i = 0; i < n; i++) uf[i] = -1;   // -1 = unprocessed; else parent cell (root: self)
  std::vector<int> comp_basin(size_t(n), 0);
  std::function<int(int)> find = [&](int x) { while (uf[x] != x) { uf[x] = uf[uf[x]]; x = uf[x]; } return x; };
  std::vector<int> leaf(size_t(n), 0);  // the basin each cell first joined (its leaf basin)

  // process cells by ascending elevation (stable on index -> deterministic on flats)
  std::vector<int> order(size_t(n), 0);
  for (int i = 0; i < n; i++) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) { return z[a] < z[b] || (z[a] == z[b] && a < b); });

  const int dx[4] = {-1, 1, 0, 0};
  const int dy[4] = {0, 0, -1, 1};
  for (int oi = 0; oi < n; oi++) {
    int c  = order[oi];
    int cx = c % W, cy = c / W;
    bool onEdge = (cx == 0 || cy == 0 || cx == W - 1 || cy == H - 1);
    // gather the active basins of already-processed (lower) neighbour components.
    int nbasins[5]; int nbn = 0;
    int nroots[5];  int nrn = 0;
    auto addRoot = [&](int r, int b) {
      for (int k = 0; k < nrn; k++) if (nroots[k] == r) return;       // dedup component
      nroots[nrn++] = r;
      bool seen = false; for (int k = 0; k < nbn; k++) if (nbasins[k] == b) seen = true;
      if (!seen) nbasins[nbn++] = b;
    };
    if (onEdge) { addRoot(-1 /*virtual*/, 0); }                        // edge connects to the OUTLET
    for (int k = 0; k < 4; k++) {
      int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
      int ni = ny * W + nx;
      if (uf[ni] == -1) continue;                                     // not processed yet (higher)
      int r = find(ni); addRoot(r, comp_basin[r]);
    }
    // start this cell's component
    uf[c] = c;
    if (nbn == 0) {                                                   // a brand-new local minimum
      int b = newBasin(c, z[c]); comp_basin[c] = b; leaf[c] = b; continue;
    }
    // deepest active basin among the neighbours = the one c drains into
    int deepest = nbasins[0];
    for (int k = 1; k < nbn; k++) if (b_pit_z[nbasins[k]] < b_pit_z[deepest]) deepest = nbasins[k];
    // every OTHER active basin spills here (z[c] is their pour) and merges into `deepest`
    for (int k = 0; k < nbn; k++) {
      int b = nbasins[k];
      if (b == deepest) continue;
      b_pour_z[b] = z[c];                                             // first (lowest) saddle -> pour
      b_parent[b] = deepest;
    }
    // union the neighbour components (+ this cell) into one, active basin = deepest
    for (int k = 0; k < nrn; k++) { int r = nroots[k]; if (r >= 0) uf[find(r)] = c; }
    uf[c] = c; comp_basin[c] = deepest; leaf[c] = deepest;
  }

  // ---- persistence simplify: survivor[b] = deepest ancestor whose persistence >= D --------------
  const int NB = int(b_pit_cell.size());
  std::vector<int> survivor(size_t(NB), -1);
  std::function<int(int)> surv = [&](int b) -> int {
    if (survivor[b] != -1) return survivor[b];
    if (b == 0) return survivor[b] = 0;                              // outlet always survives
    float pers = b_pour_z[b] - b_pit_z[b];
    int s = (pers >= D || b_parent[b] < 0) ? b : surv(b_parent[b]);
    return survivor[b] = s;
  };
  for (int b = 0; b < NB; b++) surv(b);

  // ---- write outputs ---------------------------------------------------------------------------
  for (int i = 0; i < n; i++) {
    int b = survivor[leaf[i]];
    bool closed = (b != 0);
    float zc = z[i];
    float pour = closed ? b_pour_z[b] : zc;                          // spill level of the surviving basin
    float fill = (pour > zc) ? pour : zc;
    o_filled[i] = fill;
    // basin RGBA
    o_basin[4 * i + 0] = pour;
    o_basin[4 * i + 1] = closed ? (fill - zc) : 0.0f;
    if (closed) { uint32_t hh = uint32_t(b) * 2654435761u; o_basin[4 * i + 2] = 0.15f + 0.80f * float((hh >> 9) & 0xffff) / 65535.0f; }
    else o_basin[4 * i + 2] = 0.0f;
    o_basin[4 * i + 3] = closed ? 1.0f : 0.0f;
    // center -> offset (meters) to the surviving basin's pit cell
    if (closed) {
      int pc = b_pit_cell[b];
      float dxm = float((i % W) - (pc % W)) * cell_m;
      float dzm = float((i / W) - (pc / W)) * cell_m;
      float dhm = (zc - z[pc]) * hscale_m;
      o_center[4 * i + 0] = dxm; o_center[4 * i + 1] = dhm; o_center[4 * i + 2] = dzm;
      o_center[4 * i + 3] = std::sqrt(dxm * dxm + dzm * dzm + dhm * dhm);
    } else { o_center[4 * i + 0] = o_center[4 * i + 1] = o_center[4 * i + 2] = o_center[4 * i + 3] = 0.0f; }
  }
}

struct FillClosedBasinsModuleInst : public TerrainComputeInst {
  FillClosedBasinsModuleInst(const FillClosedBasinsModuleData* d, dflow::GraphInst* g) : TerrainComputeInst(d, g), _d(d) {}
  void onLink(dflow::GraphInst*) final {
    _output    = typedOutputNamed<HfImagePlugTraits>("Out");
    _outBasin  = typedOutputNamed<HfImagePlugTraits>("Basin");
    _outCenter = typedOutputNamed<HfImagePlugTraits>("CenterPit");
    _input     = typedInputNamed<HfImagePlugTraits>("In");
    _minDepth  = _floatPlug(this, _d, "min_depth");
  }
  void bakeAcquire(dflow::GraphInst* inst) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    auto fxi = env->_ctx->FXI();
    int dim  = env->_w;
    size_t n = size_t(dim) * size_t(dim);
    _allocOut(env.get(), _output->_value);                          // mono filled
    _outBasin->_value->_w = dim;  _outBasin->_value->_h = dim;  _outBasin->_value->_channels = 4;
    _outBasin->_value->_ssbo  = env->createStorageBuffer(n * 4 * sizeof(float));
    _outCenter->_value->_w = dim; _outCenter->_value->_h = dim; _outCenter->_value->_channels = 4;
    _outCenter->_value->_ssbo = env->createStorageBuffer(n * 4 * sizeof(float));
  }
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t) final {
    auto env = inst->_impl.getShared<BakeEnv>();
    if (not env->_per_op_sync) { // ops self-defend: the mid-graph readback below needs per-op submit+wait
      printf("terrain fill_closed_basins<%s>: this graph's driver does NOT sync per op — the CPU "
             "readback would map unsubmitted data. Use it only in a HeightField bake.\n",
             _dgmodule_data->_name.c_str());
      OrkAssert(false);
    }
    auto fxi = env->_ctx->FXI();
    auto in  = _srcImg(_input);
    OrkAssert(in && in->_ssbo);
    int W = env->_w, H = env->_h;
    size_t n = size_t(W) * size_t(H);
    float cell = (W > 0) ? (env->_extent_m / float(W)) : 1.0f;
    auto im = fxi->mapStorageBuffer(in->_ssbo, 0, n * sizeof(float), BufferMapAccess::READ_ONLY);
    const float* z = static_cast<const float*>(im->_mappedaddr);
    std::vector<float> filled(n), basin(n * 4), center(n * 4);
    _fillClosedBasins(z, W, H, _minDepth->value(), cell, env->_height_scale_m,
                      filled.data(), basin.data(), center.data());
    fxi->unmapStorageBuffer(im.get());
    auto wb = [&](FxShaderStorageBuffer* ssbo, const float* src, size_t bytes) {
      auto m = fxi->mapStorageBuffer(ssbo, 0, bytes, BufferMapAccess::WRITE_ONLY);
      std::memcpy(m->_mappedaddr, src, bytes); fxi->unmapStorageBuffer(m.get());
    };
    wb(_output->_value->_ssbo,     filled.data(), n * sizeof(float));
    wb(_outBasin->_value->_ssbo,   basin.data(),  n * 4 * sizeof(float));
    wb(_outCenter->_value->_ssbo,  center.data(), n * 4 * sizeof(float));
  }
  uint64_t cookComputeHash(const std::vector<uint64_t>& ih, uint64_t ctx) const final {
    auto h = DataBlock::createHasher();
    h->accumulateString("terrain.fillclosedbasins.v1"); // CPU union-find merge tree + persistence cut
    h->accumulateItem<float>(_minDepth->value());
    _mixTail(h, ctx, ih);
    h->finish();
    return h->result();
  }

  const FillClosedBasinsModuleData* _d;
  hfimg_outpluginst_ptr_t _output, _outBasin, _outCenter;
  hfimg_inpluginst_ptr_t _input;
  dflow::float_inp_pluginst_ptr_t _minDepth;
};

static void _reshapeFCBIOs(dataflow::moduledata_ptr_t data) {
  dflow::ModuleData::createInputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "In");
  // persistence threshold (normalized height units): keep basins with (pour - pit) >= min_depth;
  // shallower sub-basins merge into their parent. 0 = finest (every pit); larger = coarser.
  dflow::ModuleData::createInputPlug<dflow::FloatPlugTraits>(data, dflow::EPR_UNIFORM, "min_depth")->setValue(0.0f);
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Out");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "Basin");
  dflow::ModuleData::createOutputPlug<HfImagePlugTraits>(data, dflow::EPR_UNIFORM, "CenterPit");
}
FillClosedBasinsModuleData::FillClosedBasinsModuleData() {}
std::shared_ptr<FillClosedBasinsModuleData> FillClosedBasinsModuleData::createShared() {
  auto d = std::make_shared<FillClosedBasinsModuleData>(); _reshapeFCBIOs(d); return d;
}
dflow::dgmoduleinst_ptr_t FillClosedBasinsModuleData::createInstance(dflow::GraphInst* g) const {
  return std::make_shared<FillClosedBasinsModuleInst>(this, g);
}
void FillClosedBasinsModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return FillClosedBasinsModuleData::createShared(); });
  clazz->annotateTyped<dataflow::moduleIOreshape_fn_t>("reshapeIOs",
      [](dataflow::moduledata_ptr_t m) { _reshapeFCBIOs(m); });
}

} // namespace ork::lev2::terrain
