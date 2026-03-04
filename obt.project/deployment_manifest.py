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
        "ork.data",
    ],
    "files": [
        "orkid.cmake",
    ],
    "apps": [
        {
            "name": "OrkTestRunner",
            "command": ["ork.app.testrunner.py"],
            "mode": "gui",
            "bundle_id": "com.tweakoz.orkid.testrunner",
        },
    ],
    "environment": [
        "ORKID_AUDIO_FRAMESIZE",
        "ORKID_AUDIO_INPUT_DEVICE",
        "ORKID_AUDIO_OUTPUT_DEVICE",
        "ORKID_GRAPHICS_API",
        "MVK_CONFIG_LOG_LEVEL",
    ],
}
