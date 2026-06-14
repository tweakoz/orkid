###############################################################################
# sort_test — verification asset for the GPU stable-sort primitive (MeshSort).
#
# sorttest fills N (key,payload) pairs with a collision-heavy 8-bit hash (N >> 256 so ~N/256 collide,
# stress-testing STABILITY), sorts them with MeshSort (a stable, deterministic 1-bit-radix sort on the
# scan), and writes the sorted (key, payload) into vertex XY. Dump the OBJ ([O]) and check: X (key)
# non-decreasing; within every equal-key run, Y (payload = original index) strictly ascending (proves
# stable/deterministic); {Y} == {0..N-1} (proves the scatter is a permutation, nothing lost/duplicated).
#
#   ./ork.hypermesh.viewer.py sort_test
#
# Infrastructure harness — the emitted points are verification data, not a renderable model.
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh


class SortTestDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    self.output(self.sorttest(252))


__all__ = ["SortTestDemo"]
