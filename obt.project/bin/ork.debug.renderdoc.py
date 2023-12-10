#!/usr/bin/env python3

import sys, os, json, argparse, re, shutil, json
from obt import path, command
this_dir = path.fileOfInvokingModule()
curwd = os.getcwd()
sys.path.append(str(this_dir))
import _debug_helpers

parser = argparse.ArgumentParser(description="renderdoc wrapper with orkid customizations")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args()

capture_path = path.temp()/"temp.rdc"

exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)

rdoc_dir = path.Path(os.environ["RENDERDOC_DIR"])

renderdoc = rdoc_dir/"bin"/"qrenderdoc"

project_content = {
    "rdocCaptureSettings": 1,
    "settings": {
        "autoStart": False,
        "commandLine": " ".join(exe_args),
        "environment": [
        ],
        "executable": str(exe_path),
        "inject": False,
        "numQueuedFrames": 0,
        "options": {
            "allowFullscreen": True,
            "allowVSync": True,
            "apiValidation": False,
            "captureAllCmdLists": False,
            "captureCallstacks": False,
            "captureCallstacksOnlyDraws": False,
            "debugOutputMute": True,
            "delayForDebugger": 0,
            "hookIntoChildren": False,
            "refAllResources": False,
            "verifyBufferAccess": False
        },
        "queuedFrameCap": 0,
        "workingDir": curwd,
    }
}

project_file_path = path.temp()/"project.cap"
with open(project_file_path, 'w') as f:
    json.dump(project_content, f, indent=4)

cmd_list = [renderdoc, project_file_path]


print(cmd_list)
command.run(cmd_list,do_log=True)
