###############################################################################
# scatter_consumer — render a baked ScatterSet (.ogeo) as instanced drawables.
#
# DECOUPLED from the producer: scatter (the placer) emits ONLY data (matrices +
# instance attrs). This consumer reads that .ogeo, shards the points by `type_id`,
# and creates ONE InstancedRigidPrimitiveDrawable SG node per type — filling the
# instance matrices from the baked `xform` channel. The per-type DRAWABLE (mesh +
# material) is supplied by the caller (the ASSET owns it); this stays generic.
#
# (A future consumer could feed the SAME .ogeo to a mesh/task pipeline instead — the
# format is consumer-agnostic; this is just the instanced-drawable handoff.)
###############################################################################
import numpy as np


def install(scatterset_path, prim_by_type, layer, ctx, *, name="scatter", cull=True):
    """Instance a ScatterSet .ogeo onto `layer`. `prim_by_type` is an ordered list indexed by
    type_id: each entry is a (RigidPrimitive, material) recipe or None (skip that type). Returns
    the created SG node handles (the caller keeps them alive). MESH-AGNOSTIC: the meshes/materials
    come from the caller; this only reads matrices + type_id from the baked ScatterSet."""
    from orkengine.lev2 import Geometry
    geo   = Geometry.read(str(scatterset_path))
    xform = np.array(geo.point["xform"])      # (N,4,4) per-instance TRS (== the proxy layout)
    tid   = np.array(geo.point["type_id"])    # (N,)
    nodes = []
    for ti, recipe in enumerate(prim_by_type):
        if recipe is None:
            continue
        prim, mtl = recipe
        sel = (tid == ti)
        cnt = int(np.count_nonzero(sel))
        if cnt == 0:
            continue
        node  = prim.createInstancedNode(cnt, "%s_%d" % (name, ti), layer, mtl, cull=cull)
        idata = node.instanceData
        # the buffers are now COUNT-sized (cnt rows). Fill the matrices (baked TRS).
        mats = np.array(idata.matrices, copy=False)    # (cnt,4,4)
        mats[:cnt] = xform[sel]                         # baked TRS -> the drawn instances
        # matrices-only materials have NO per-instance color array (empty) — skip it.
        if not getattr(mtl, "instanceMatricesOnly", False):
            cols = np.array(idata.colors, copy=False)   # (cnt,4); per-instance tint
            cols[:cnt] = 1.0                            # white (let the material color show)
        nodes.append(node)
    return nodes


def install_hypermeshes(scatterset_path, hm_by_type, layer, ctx, *, name="scatter"):
  """Instance a baked ScatterSet .ogeo as HYPERMESHES. E.2: this is now ONE DSL WIRE per type —
  `asset.instance_source(ogeo_path=..., type_id=ti)` puts a ScatterSource module IN the asset's
  graph; the C++ module loads + type-filters the set and fills the InstanceSet (matrices + TYPED
  attrs SSBOs: x=type_id, y=seed01 -> frg_clr; the matrix-bottom-row smuggle is retired);
  make_drawable auto-instances from the graph's set. The numpy shard/smuggle glue this function
  used to be is GONE — the signature survives as the convenience loop over types:
      (hypermesh_asset, material_cls)            # default material kwargs
      (hypermesh_asset, material_cls, albedo)    # + per-type albedo (vec3 / hsv())
  Returns (nodes, lives, assets); keep nodes+lives alive (the live GpuMesh owns the pooled
  SSBOs) and drive each animated asset's onUpdate(updinfo) per frame as before."""
  from ork.hypergraph.dflow.hypermesh import make_drawable
  nodes, lives, assets = [], [], []
  for ti, recipe in enumerate(hm_by_type):
    if recipe is None:
      continue
    asset, mtl_cls = recipe[0], recipe[1]
    albedo         = recipe[2] if len(recipe) > 2 else None
    asset.instance_source(ogeo_path=str(scatterset_path), type_id=ti)   # THE wire (replace semantics)
    live = asset.materialize_live(ctx)            # ONE graph eval -> one GpuMesh + this type's InstanceSet
    if int(getattr(live, "instance_count", 0)) == 0:
      continue                                    # this type placed nothing — no node
    cdd, _ = make_drawable(live, ctx, animated=bool(asset.is_animated), material_cls=mtl_cls, albedo=albedo)
    node = layer.createDrawableNodeFromData("%s_hm_%d" % (name, ti), cdd)
    nodes.append(node)
    lives.append(live)                            # KEEP ALIVE: the GpuMesh owns the SSBOs
    assets.append(asset)                          # so the caller can drive its onUpdate (animated types)
  return nodes, lives, assets
