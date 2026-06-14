###############################################################################
# ork.hypergraph.assets.terrain — terrain heightfield graph assets (HeightField
# DSL subclasses). The HeightField scene-asset wrapper resolves these by bare
# name (e.g. dsl_file="hf1") via ork.hypergraph.dflow.terrain.resolve.
###############################################################################
from .hf1 import HF1

__all__ = ["HF1"]
