################################################################################
# Scene I/O - Load/Save .osgr scene files
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os
import json
from orkengine.core import vec3, quat, Transform
from orkengine import lev2

################################################################################

class SceneLoader:
  """Standalone scene loader for .osgr files.

  Can be used in demos without the full editor infrastructure.
  """

  @staticmethod
  def load(path, scenegraph, layer, model_cache=None):
    """Load .osgr scene into existing scenegraph/layer.

    Args:
      path: Path to .osgr file
      scenegraph: Target scenegraph instance
      layer: Target layer for nodes
      model_cache: Optional dict to cache loaded models (model_path -> XgmModel)
                   If provided, models are cached for reuse.

    Returns:
      dict with 'nodes' and 'lights' lists
    """
    if not os.path.exists(path):
      raise FileNotFoundError(f"Scene file not found: {path}")

    with open(path, 'r') as f:
      data = json.load(f)

    if model_cache is None:
      model_cache = {}

    result = {'nodes': [], 'lights': []}

    # Load drawable nodes
    for name, nd in data.get("nodes", {}).items():
      model_path = nd.get("model_path")
      if not model_path:
        print(f"Warning: Node '{name}' has no model_path, skipping")
        continue

      # Load or get cached model
      if model_path not in model_cache:
        try:
          model_cache[model_path] = lev2.XgmModel(model_path)
        except Exception as e:
          print(f"Warning: Failed to load model '{model_path}' for node '{name}': {e}")
          continue

      model = model_cache[model_path]
      drawable = model.createDrawable()
      node = scenegraph.createDrawableNodeOnLayers([layer], name, drawable)

      # Apply transform
      xform = Transform()
      t = nd.get("translation", [0, 0, 0])
      xform.translation = vec3(t[0], t[1], t[2])
      o = nd.get("orientation", [0, 0, 0, 1])
      xform.orientation = quat(o[0], o[1], o[2], o[3])
      xform.scale = nd.get("scale", 1.0)
      node.worldTransform = xform

      # Store metadata in userdata
      node.user.model_path = model_path

      result['nodes'].append(node)

    # Load point lights
    for name, ld in data.get("lights", {}).items():
      light_data = lev2.PointLightData()
      c = ld.get("color", [100, 100, 100])
      light_data.color = vec3(c[0], c[1], c[2])

      light_node = light_data.createNode(name, layer)

      # Apply transform
      xform = Transform()
      t = ld.get("translation", [0, 5, 0])
      xform.translation = vec3(t[0], t[1], t[2])
      light_node.worldTransform = xform
      light_node.setMatrix(xform.composed)

      # Store lightdata in userdata for editor access
      light_node.user.lightdata = light_data

      result['lights'].append(light_node)

    return result

################################################################################

class SceneSaver:
  """Scene saver for .osgr files."""

  @staticmethod
  def save(path, scenegraph, node_type_token, light_type_token):
    """Save scenegraph to .osgr file.

    Args:
      path: Output path (will add .osgr extension if missing)
      scenegraph: Source scenegraph instance
      node_type_token: CrcString token for drawable node type (e.g., tokens.model)
      light_type_token: CrcString token for light node type (e.g., tokens.pointlight)

    Returns:
      Path to saved file
    """
    if not path.endswith('.osgr'):
      path += '.osgr'

    data = {"nodes": {}, "lights": {}}

    # Save drawable nodes
    for node in scenegraph.drawableNodesWithType(node_type_token):
      xform = node.worldTransform
      t, o = xform.translation, xform.orientation

      node_data = {
        "translation": [t.x, t.y, t.z],
        "orientation": [o.x, o.y, o.z, o.w],
        "scale": xform.scale,
      }

      # Include model_path if available
      if hasattr(node.user, 'model_path'):
        node_data["model_path"] = node.user.model_path

      data["nodes"][node.name] = node_data

    # Save point lights
    for node in scenegraph.lightNodesWithType(light_type_token):
      xform = node.worldTransform
      t = xform.translation

      light_data = {"translation": [t.x, t.y, t.z]}

      # Get color from lightdata if available
      if hasattr(node.user, 'lightdata'):
        c = node.user.lightdata.color
        light_data["color"] = [c.x, c.y, c.z]

      data["lights"][node.name] = light_data

    with open(path, 'w') as f:
      json.dump(data, f, indent=2)

    return path
