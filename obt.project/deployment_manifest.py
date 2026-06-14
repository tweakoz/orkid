"""Deploy manifest for orkid.

Declares what files/dirs from the source tree should be included
in a relocatable deployment.
"""

manifest = {
    "dirs": [
        "obt.project",
        "ork.core/pyext/tests",
        "ork.lev2/examples/python",
        "ork.lev2/pyext/tests",
        "ork.ecs/examples/python",
    ],
    "optional_dirs": [
    ],
    "files": [
        "orkid.cmake",
    ],
    # ork.data is NOT shipped wholesale (~2.7GB: tests/sounds/misc + most of
    # src/ and platform_lev2/textures are dev-only). Ship a curated subset.
    # Each entry is relative to the tree root and is a dir (wholesale), a glob,
    # or a file. __pycache__ is stripped globally after copy (Phase 5), so the
    # wholesale dirs below need no per-file curation.
    "tree_whitelists": {
        "ork.data": [
            # individual files pulled from otherwise-dropped trees
            "tests/pbr_calib.glb",  # data://tests/pbr_calib.glb (PBR calib model)
            "tests/pbr_calib_lopoly.glb",  # data://tests/pbr_calib_lopoly.glb (lo-poly PBR calib)
            "tests/monkey_pbr.glb",  # data://tests/monkey_pbr.glb
            # lightmap room test: the glb + its orkid sidecar + 6 cubemap bake
            # faces. NOT roomtest_lightmaps.* (would pull the .blend source).
            "tests/environ/roomtest_lightmaps.glb",
            "tests/environ/roomtest_lightmaps.orkid.json",
            "tests/environ/BAKE.*.png",
            # wholesale runtime/content dirs
            "asset_manifests",
            "ecsscenes",
            "scenes",
            "particles",
            "grammars",
            "dox",
            "cdntest",
            "cdntest2",
            # platform_lev2: built-in shaders (wholesale) + only the fonts/grid
            # textures the engine needs (drops ~71 dev tga/dds/xcf).
            "platform_lev2/shaders/dummy",
            "platform_lev2/shaders/fxv2",
            "platform_lev2/textures/Inconsolata*.png",
            "platform_lev2/textures/gridcell_blue.png",
            "platform_lev2/textures/transponder24.png",
            "platform_lev2/textures/exp2.dds",  # lev2://textures/exp2
            # src: only particle textures + lua/export scripts (drops ~358MB of
            # effect_textures/materials/actors/terrain/environ/audio).
            "src/particle_textures",
            "src/scripts",
            # effect_textures: only the utility textures referenced in code
            # (white/normal/noise/particle/knob/uvmap/etc.) — NOT the full 126MB
            # dir. Source list from: ork.find.py "src://effect_textures".
            "src/effect_textures/white.dds",
            "src/effect_textures/white_64.dds",
            "src/effect_textures/black.dds",
            "src/effect_textures/green.dds",
            "src/effect_textures/blue_64.dds",
            "src/effect_textures/default_normal.dds",
            "src/effect_textures/NoiseKern.dds",
            "src/effect_textures/voltex_pn2.dds",
            "src/effect_textures/knob2.png",
            "src/effect_textures/spinner.dds",
            "src/effect_textures/uvmap_A.dds",
            "src/effect_textures/uvmap_A.png",
            "src/effect_textures/adama.png",
            "src/effect_textures/L0D.png",
            "src/effect_textures/ptc1.png",
            "src/effect_textures/ptc3.png",
            "src/effect_textures/ptc4.png",
        ],
    },
    "environment": [
        "ORKID_AUDIO_FRAMESIZE",
        "ORKID_AUDIO_INPUT_DEVICE",
        "ORKID_AUDIO_OUTPUT_DEVICE",
        "ORKID_GRAPHICS_API",
        "MVK_CONFIG_LOG_LEVEL",
    ],
}
