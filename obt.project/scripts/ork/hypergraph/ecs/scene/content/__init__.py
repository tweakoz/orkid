###############################################################################
# ork.hypergraph.ecs.scene.content — SCENE CONTENT that lives in the library
# rather than in ork.data/scenes: whole scenes (or scene bases) other scenes
# subclass, with every recipe they need — species grammars, material recipes,
# terrain/bake config, tuned constants.
#
# WHY A PACKAGE AND NOT A SCENE FILE: ork.data/scenes is the RUNNABLE surface
# (the resolver lists it, the viewer loads it by bare name). A scene that exists
# to be subclassed is a library asset; leaving it in the runnable directory is
# what let the family drift — a scope label that outlived its commit.
#
# Re-exported here so a consumer imports the short form:
#     from ork.hypergraph.ecs.scene.content import ForestScene
###############################################################################

from ork.hypergraph.ecs.scene.content.forest import ForestScene

__all__ = ["ForestScene"]
