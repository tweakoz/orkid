###############################################################################
# ork.dflow._context_classes — Python-side mirror of the C++
# ContextVariableRegistry (orkengine.core.dataflow.context_variables).
#
# The C++ registry knows the (DSL name, output plug, policy) tuple but doesn't
# expose a usable construct/find API in headless contexts — sharedFactory() is
# only set during describeX which only runs after lev2appinit's GPU init.
#
# The Python emitter needs to call graphdata.create() / findModuleByClass()
# both of which take a Python module class object. This file holds the
# DSL-name → Python class mapping populated by each family's __init__ at
# import time, with cross-validation against the C++ registry to catch drift.
###############################################################################

_PY_CLASSES = {}


def register_python_class(dsl_name, py_class):
    """Register the Python module class that materializes `dsl_name` in the
    graph. Verifies the C++ registry has a matching entry (raises otherwise)
    so a missing or renamed C++ registration surfaces immediately, not at
    bind time."""
    from orkengine.core import dataflow
    spec = dataflow.context_variables.lookup(dsl_name)
    if spec is None:
        raise RuntimeError(
            f"register_python_class({dsl_name!r}, {py_class.__name__!r}): "
            f"no matching C++ registration. Verify the static initializer "
            f"in the engine module's .cpp file is present and that the "
            f"dylib loaded.")
    _PY_CLASSES[dsl_name] = py_class


def python_class_for(dsl_name):
    """Return the Python module class registered for `dsl_name`, or None."""
    return _PY_CLASSES.get(dsl_name)


def registered_names():
    """All DSL names that have a Python class mapping registered."""
    return list(_PY_CLASSES.keys())
