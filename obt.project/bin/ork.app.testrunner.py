#!/usr/bin/env ork.python
################################################################
# Orkid Test Runner - Visual UI
################################################################

from ork.app.testrunner import TestRunnerApp
from obt import path as obt_path

orkdir = str(obt_path.orkid())
_lev2t = orkdir + "/ork.lev2/pyext/tests"
_lev2e = orkdir + "/ork.lev2/examples/python"

def lev2(*args):
  return ["ork.python", _lev2t + "/" + args[0]] + list(args[1:])
def lev2e(*args):
  return ["ork.python", _lev2e + "/" + args[0]] + list(args[1:])

tests = {
    "Core": {
        "Math":       ["ork.test.python.unittests.all.py"],
    },
    "Lev2": {
        "GFX": {
            "Datasources": {
                "Compute Shader":  lev2("renderer/datasources/computeshader.py"),
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
                "Imposter 1": lev2("renderer/imposters/i1.py"),
                "Imposter 5": lev2("renderer/imposters/i5.py"),
            },
            "Primitives": {
                "Primitive Types": lev2("renderer/primitives/primitive_types.py"),
            },
            "Misc": {
                "Models": lev2e("scenegraph/models.py"),
            }
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
                "SGUI":     lev2("ui/sgui_secondary_window.py"),
                "EzApp":    lev2("ui/secondary_window.py"),
            },
            "PrimCanvas": {
                "Clock":    lev2("ui/prim_canvas_clock.py"),
                "Invaders": lev2("ui/prim_canvas_invaders.py"),
                "Layers":   lev2("ui/prim_canvas_layers.py"),
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
            "Simple Wave":    lev2("singularity/simplewave.py"),
            "KRZ Minimal":    lev2("singularity/krz_minimal.py"),
            "TX81Z Minimal":  lev2("singularity/minimal_tx81z.py"),
            "Loaded Waveforms": lev2("singularity/waveforms_load.py"),
            "Computed Waveforms": lev2("singularity/waveforms_compute.py"),
        },
    },
    "ECS": {
        "Physics": {
            "FPS":     ["ork.python", orkdir + "/ork.ecs/examples/python/physics/FPS.py"],
            "SubMesh": ["ork.python", orkdir + "/ork.ecs/examples/python/physics/submesh.py"],
        },
        "ScenePlayer": {
            "ecsscn2": ["ork.ecsplay.py", "-s", orkdir + "/ork.data/ecsscenes/ecsscn2.json", "-f"],
        },
    },
}

TestRunnerApp(tests, title="Orkid Test Runner").run()
