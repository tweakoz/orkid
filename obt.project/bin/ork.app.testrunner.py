#!/usr/bin/env ork.python
################################################################
# Orkid Test Runner - Visual UI
################################################################

from ork.app.testrunner import TestRunnerApp
from obt import path as obt_path
from pathlib import Path
import os

orkdir = str(obt_path.orkid())
_lev2t = orkdir + "/ork.lev2/pyext/tests"
_lev2e = orkdir + "/ork.lev2/examples/python"

def lev2(*args):
  return ["ork.python", _lev2t + "/" + args[0]] + list(args[1:])
def lev2e(*args):
  return ["ork.python", _lev2e + "/" + args[0]] + list(args[1:])

# Envmap shortnames from scenegraph.cpp aliases (4k variants)
_envmap_options = {
  "none":      [],
  "arena":     ["-e", "arena"],
  "club":      ["-e", "club"],
  "cold":      ["-e", "cold"],
  "pillars":   ["-e", "pillars"],
  "desert":    ["-e", "desert"],
  "ocean":     ["-e", "ocean"],
  "crossroads":["-e", "crossroads"],
  "ethereal":  ["-e", "ethereal"],
  "futcity":   ["-e", "futcity"],
  "nebula":    ["-e", "nebula"],
  "hellscape": ["-e", "hellscape"],
  "caustics":  ["-e", "caustics"],
  "forest":    ["-e", "forest"],
  "city":      ["-e", "city"],
  "courtyard": ["-e", "courtyard"],
  "studio":    ["-e", "studio"],
  "interior":  ["-e", "interior"],
  "night":     ["-e", "night"],
  "sunrise":   ["-e", "sunrise"],
  "sunset":    ["-e", "sunset"],
}

def _build_modelviewer_tests():
  """Build ModelViewer test entries grouped by category from misc_gltf_samples."""
  workspace_dir = os.environ.get("ORKID_WORKSPACE_DIR", "")
  if not workspace_dir:
    return {}
  gltf_dir = Path(workspace_dir) / "ork.data" / "tests" / "misc_gltf_samples"
  if not gltf_dir.exists():
    return {}
  # Group shortnames by subdirectory
  groups = {}
  for glb_file in gltf_dir.rglob("*.glb"):
    shortname = glb_file.stem
    parent_name = glb_file.parent.name
    # Use parent dir name as category, skip top-level .glb files
    if glb_file.parent == gltf_dir:
      category = "Misc"
    else:
      category = parent_name
    if category not in groups:
      groups[category] = {}
    groups[category][shortname] = {
      "_commands": ["ork.modelviewer.py", "-m", shortname],
      "_options": {
        "Fullscreen": ["-f"],
        "SSAA": {
          "none": ["--ssaa", "0"],
          "2x2":  ["--ssaa", "1"],
          "3x3":  ["--ssaa", "2"],
          "4x4":  ["--ssaa", "3"],
          "5x5":  ["--ssaa", "4"],
        },
        "Intensity": {
          "1.0":  ["-i", "1.0"],
          "1.5":  ["-i", "1.5"],
          "2.0":  ["-i", "2.0"],
          "3.0":  ["-i", "3.0"],
          "5.0":  ["-i", "5.0"],
          "0.25": ["-i", "0.25"],
          "0.5":  ["-i", "0.5"],
          "0.75": ["-i", "0.75"],
        },
        "Envmap": _envmap_options,
      },
    }
  return groups

tests = {
    "Core": {
        "Math":       ["ork.test.python.unittests.all.py"],
    },
    "Lev2": {
        "GFX": {
            "Datasources": {
                "Compute Shader": {
                    "_commands": lev2("renderer/datasources/computeshader.py"),
                    "_description": "GPU water sim (compute shader)",
                    "_options": {
                        "Fullscreen": ["-f"],
                    },
                },
                "Marching Cubes2": lev2("renderer/datasources/marchingcubes2.py"),
                "Pixel Art":       lev2("renderer/datasources/pixelart.py"),
                "Vector Field":    lev2("renderer/datasources/vectorfield.py"),
            },
            "Lighting": {
                "Lightmap":         lev2("renderer/lighting/lightmap1.py"),
                "Probe":            lev2("renderer/lighting/probe.py"),
                "Spotlight Rigid":  lev2("renderer/lighting/spotlight_rigid_model.py"),
                "Spotlight Skinned":lev2("renderer/lighting/spotlight_skinned_model.py"),
            },
            "Particles": {
                "Elliptical":  lev2("renderer/particles/ptc_elliptical.py"),
                "Elliptical2": lev2("renderer/particles/ptc_elliptical2.py"),
                "Elliptical3": lev2("renderer/particles/ptc_elliptical3.py"),
                "Emitter Line":lev2("renderer/particles/ptc_emitter_line.py"),
                "Lights":      lev2("renderer/particles/ptc_lights.py"),
                "Sprite":      lev2("renderer/particles/ptc_sprite.py"),
                "Streak":      lev2("renderer/particles/ptc_streak.py"),
                "Tex Grid":    lev2("renderer/particles/ptc_texgrid.py"),
            },
            "Imposters": {
                "Imposter 1": {
                    "_commands": lev2("renderer/imposters/i1.py"),
                    "_options": { "Envmap": _envmap_options },
                },
                "Imposter 5": {
                    "_commands": lev2("renderer/imposters/i5.py"),
                    "_options": { "Envmap": _envmap_options },
                },
            },
            "Primitives": {
                "Primitive Types": lev2("renderer/primitives/primitive_types.py"),
            },
            "GeoClipMesh": {
                "_options": { "Fullscreen": ["-f"] },
                "Basic": { "_commands": lev2("renderer/geoclip/geoclipmesh_basic.py") },
                "Water": { "_commands": lev2("renderer/geoclip/geoclipmesh_water.py") },
            },
            "MoviePlayback": {
                "HwDecodeBasic": {
                    "_commands": lev2("movie/hwdec_basic.py"),
                    "_options": { "Fullscreen": ["-f"], "Audio": ["-a"] },
                },
                "HwDecodeStress": {
                    "_commands": lev2("movie/hwdec_stresstest.py"),
                    "_options": { "Fullscreen": ["-f"], "Audio": ["-a"] },
                },
            },
            "SubMesh": {
                "Boolean": lev2("submesh/boolean.py"),
                "ConvexDecomp": lev2("submesh/convex_decomp.py"),
                "ConvexHull2": lev2("submesh/convex_hull_2.py"),
                "PlanarClip": lev2("submesh/planar_clip.py"),
            },
            "Misc": {
                "Models": lev2e("scenegraph/models.py"),
                "ShaderBalls": {
                    "_commands": lev2e("scenegraph/shaderballs.py"),
                    "_options": {
                        "Envmap": _envmap_options,
                        "Intensity": {
                          "1.5":  ["-i", "1.5"],
                          "1.0":  ["-i", "1.0"],
                          "2.0":  ["-i", "2.0"],
                          "3.0":  ["-i", "3.0"],
                          "5.0":  ["-i", "5.0"],
                          "0.5":  ["-i", "0.5"],
                        },
                    },
                },
                "Skinning4": {
                    "_commands": lev2e("scenegraph/skinning4.py"),
                    "_options": {
                        "Envmap": _envmap_options,
                        "Intensity": {
                          "1.0":  ["-i", "1.0"],
                          "1.5":  ["-i", "1.5"],
                          "2.0":  ["-i", "2.0"],
                          "3.0":  ["-i", "3.0"],
                          "0.5":  ["-i", "0.5"],
                        },
                    },
                },
                "PseudoWire": lev2e("scenegraph/pseudowire.py"),
            },
            "ModelViewer": _build_modelviewer_tests(),
        },
        "UI": {
            "SGUI": {
                "Single":           lev2("ui/sgui_single.py"),
                "Dual View":        lev2("ui/sgui_dualview.py"),
                "Quad Single":      lev2("ui/sgui_quad_single.py"),
                "Quad Multi":       lev2("ui/sgui_quad_multi.py"),
            },
            "UiEmbedding": {
                "Emb SceneGraph":   lev2("ui/sgui_emb_sg.py"),
                "Emb WorldGrid":    lev2("ui/sgui_emb_worldgrid.py"),
                "Emb ViewGrid":     lev2("ui/sgui_emb_viewgrid.py"),
                "Emb MpLib":        lev2("ui/sgui_emb_mplib.py"),
            },
            "Secondary Window": {
                "SGUI": {
                    "_commands": lev2("ui/sgui_secondary_window.py"),
                    "_options": { "Fullscreen": ["-f"] },
                },
                "EzApp": {
                    "_commands": lev2("ui/secondary_window.py"),
                    "_options": { "Fullscreen": ["-f"] },
                },
                "Global Events": {
                    "_commands": lev2("ui/globalevents.py"),
                },
            },
            "PrimCanvas": {
                "Clock":    lev2("ui/prim_canvas_clock.py"),
                "Invaders": lev2("ui/prim_canvas_invaders.py"),
                "Layers":   lev2("ui/prim_canvas_layers.py"),
                "Pacman":   lev2("ui/prim_canvas_pacman.py"),
                "Quads":    lev2("ui/prim_canvas_quads.py"),
                "Sprites":  lev2("ui/prim_canvas_sprites.py"),
                "Text":     lev2("ui/prim_canvas_text.py"),
                "Tri List":  lev2("ui/prim_canvas_trilist.py"),
                "Tri Strip": lev2("ui/prim_canvas_tristrip.py"),
            },
            "Layout": {
                "Grid":       lev2("ui/layouttest_grid.py"),
                "RC Replace": lev2("ui/layouttest_rc_replace.py"),
                "RC Std":     lev2("ui/layouttest_rc_std.py"),
                "Split":      lev2("ui/layouttest_split.py"),
                "Fixed Pack": lev2("ui/fixed_pack.py"),
                "Widget Pack":lev2("ui/widget_pack.py"),
            },
            "Widgets": {
                "Outliner":        lev2("ui/outliner.py"),
                "Outliner Model":  lev2("ui/outliner_model.py"),
                "GraphView":       lev2("ui/graphview.py"),
                "DynaGrid":        lev2("ui/dynagrid.py"),
                "Toolbar":         lev2("ui/toolbar.py"),
                "Themes":          lev2("ui/themes.py"),
                "Property Sheet":  lev2("ui/property_sheet.py"),
                "Overlay Dropdown":lev2("ui/overlay_dropdown_test.py"),
                "Page Widget":     lev2("ui/page_widget_example.py"),
            },
            "UiRendering": {
                "Font Test":        lev2("ui/fonttest.py"),
                "SDF Prims":        lev2("ui/sdf_prims.py"),
                "SDF Img Simple":   lev2("ui/sdf_img_simple.py"),
                "SDF Img Texture":  lev2("ui/sdf_img_texture.py"),
                "SDF Img CharCells":lev2("ui/sdf_img_charcells.py"),
            },
        },
        "Audio": {
            "Simple Wave": {
                "_commands": lev2("singularity/simplewave.py"),
            },
            "KRZ Minimal": {
                "_commands": lev2("singularity/krz_minimal.py"),
            },
            "TX81Z Minimal": {
                "_commands": lev2("singularity/minimal_tx81z.py"),
            },
            "Loaded Waveforms": {
                "_commands": lev2("singularity/waveforms_load.py"),
            },
            "Computed Waveforms": {
                "_commands": lev2("singularity/waveforms_compute.py"),
            },
        },
    },
    "ECS": {
        "Physics": {
            "FPS": {
                "_commands": ["ork.python", orkdir + "/ork.ecs/examples/python/physics/FPS.py"],
                "_options": {
                    "Fullscreen": ["--fullscreen"],
                    "Envmap": _envmap_options,
                },
            },
            "FPS2": {
                "_commands": ["ork.python", orkdir + "/ork.ecs/examples/python/physics/FPS2.py"],
                "_options": {
                    "Fullscreen": ["--fullscreen"],
                    "Envmap": _envmap_options,
                },
            },
            "SubMesh": ["ork.python", orkdir + "/ork.ecs/examples/python/physics/submesh.py"],
        },
        "ScenePlayer": {
            "ecsscn2": {
                "_commands": ["ork.ecsplay.py", "-s", orkdir + "/ork.data/ecsscenes/ecsscn2.ecs"],
                "_options": { "Fullscreen": ["-f"] },
            },
        },
    },
}

TestRunnerApp(tests, title="Orkid Test Runner").run()
