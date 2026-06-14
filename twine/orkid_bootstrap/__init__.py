"""orkid pip bootstrap.

The umbrella `orkid` wheel ships only this package + console-script entry points.
The actual engine lives in `<site-packages>/orkid/` (installed by the
orkid-engine / orkid-libdeps / orkid-python / ... payload wheels). The launcher
relocates the bundle to wherever pip put it and execs its private interpreter.
"""
__version__ = "0.0.1"
