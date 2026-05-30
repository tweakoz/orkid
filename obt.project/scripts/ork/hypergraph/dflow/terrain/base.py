###############################################################################
# ork.hypergraph.dflow.terrain.base — HeightField family base class.
#
# User code subclasses HeightField and builds the heightfield DAG in __init__ via
# the expression-first terrain DSL. The base class:
#  - allocates a fresh dflow.GraphData on construction
#  - opens a trace context so DSL ops/operators target that graph
#  - exposes self.capture(node, channel) to record output channels (multi-sink:
#    a terrain bake emits several channels — height, slope, masks, ...)
#  - generatedflow() closes the trace and returns the populated GraphData
#
# Materialization is a BAKE: lev2.terrain.bake_heightfield(graphdata, ctx, dim)
# runs the compute DAG once and writes one EXR/PNG per capture channel. The base
# stays out of the bake — the asset wrapper (later) assigns per-channel paths via
# set_capture_path() then calls the driver.
#
# __init__ takes **kwargs so the same class is reusable parameterized: loop
# bounds / conditionals in __init__ read kwargs the asset/scene supplies (Python
# control flow runs at TRACE time and unrolls into DAG topology).
###############################################################################

from orkengine.core import dataflow as _dflow
from orkengine.lev2 import terrain as _terrain
from .._trace import enter_trace, leave_trace, current_graph
from ._node import TerrainNode, anon_name


class HeightField:
    """Family base for HyperSyn terrain heightfield graphs (expression-first DSL).

    Subclass and implement __init__:

        from ork.hypergraph.dflow.terrain import HeightField
        from ork.hypergraph.dflow import terrain as T

        class RollingHills(HeightField):
          def __init__(self, octaves=5, steps=6):
            super().__init__()                       # opens the trace
            h = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5
            self.capture(T.Terrace(h, steps=steps), "height")

    Then:

        hf = RollingHills()
        g  = hf.generatedflow()                      # the populated dflow.GraphData
        hf.set_capture_path("height", "/tmp/h.exr")  # asset/bake harness supplies paths
        stats = lev2.terrain.bake_heightfield(g, ctx, 1024)
    """

    def __init__(self, **kwargs):
        # Fresh empty graph; DSL ops populate it during user __init__.
        self.graphdata = _dflow.GraphData.createShared()
        # channel name -> CaptureModule (the sink). dict preserves declaration
        # order so the bake's FieldStats list lines up with channels().
        self._captures = {}
        # **kwargs accepted + ignored at the base — subclasses opt in by
        # declaring their own signature (parameterized terrain). Open the trace;
        # generatedflow() closes it.
        self._prev_trace = enter_trace(self.graphdata)

    def capture(self, node, channel):
        """Record an output channel. Creates a CaptureModule fed by `node`,
        keyed by `channel`. Repeatable for multi-channel bakes; the on-disk path
        is supplied later via set_capture_path() (asset wrapper / bake harness)."""
        if not isinstance(node, TerrainNode):
            raise TypeError(
                f"HeightField.capture() expects a terrain node (output of a T.* op "
                f"or operator); got {type(node).__name__}")
        if not self._is_tracing():
            raise RuntimeError(
                f"capture({channel!r}) called outside a trace context — call "
                f"super().__init__() at the top of your HeightField subclass __init__.")
        if channel in self._captures:
            raise ValueError(f"duplicate capture channel {channel!r}")
        g = self.graphdata
        cap = g.create(anon_name("capture", g), _terrain.CaptureModule)
        g.connect(cap.inputs.In, node.output_plug)
        self._captures[channel] = cap

    @property
    def channels(self):
        """The declared channel names, in declaration order (one EXR each)."""
        return tuple(self._captures.keys())

    def set_capture_path(self, channel, path):
        """Assign a channel's on-disk output path before bake_heightfield()."""
        if channel not in self._captures:
            raise KeyError(f"no such capture channel {channel!r}; have {self.channels}")
        self._captures[channel].path = str(path)

    def generatedflow(self):
        """Close the trace and return the populated dflow.GraphData. Idempotent —
        re-calling just returns the same graph."""
        if self._is_tracing():
            leave_trace(self._prev_trace)
            self._prev_trace = None
        return self.graphdata

    def _is_tracing(self):
        return current_graph() is self.graphdata
