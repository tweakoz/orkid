###############################################################################
# _artifact_names — trace-time {live artifact -> asset name} registry.
#
# Cross-asset particle systems receive LIVE artifacts in their DSL ctor (e.g.
# col_vdb's collision_sdf = a materialized openvdb FloatGrid). The artifact
# itself can't serialize — but its ASSET NAME can (the by-name convention every
# other cross-asset binding uses). The ParticleSystem asset wrapper registers
# {artifact: name} here around the DSL run; op builders that consume live
# artifacts (ops/vdb_collider) consult it and stamp the module's reflected
# asset-reference field — so the traced graph is fully serializable (model B)
# with ZERO change to scene or DSL author surfaces.
#
# Entries hold a strong reference to the artifact so id() stays valid for the
# registration window; `is` re-checks guard against id reuse anyway.
###############################################################################

_BY_ID = {}


def register(artifact, asset_name):
  """Associate a live artifact with its AssetSystem name for the duration of a
  DSL trace. Call clear() when the trace finishes."""
  if artifact is not None and asset_name:
    _BY_ID[id(artifact)] = (artifact, str(asset_name))


def name_for(artifact):
  """The registered asset name for `artifact`, or '' if it wasn't registered
  (standalone / imperative use — the reference field stays empty)."""
  entry = _BY_ID.get(id(artifact))
  if entry is not None and entry[0] is artifact:
    return entry[1]
  return ""


def clear():
  _BY_ID.clear()
