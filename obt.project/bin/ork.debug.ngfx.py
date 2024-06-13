#!/usr/bin/env python3 

import sys, os, json, argparse, re, shutil, json, uuid
from obt import path, command
this_dir = path.fileOfInvokingModule()
curwd = os.getcwd()
sys.path.append(str(this_dir))
import _debug_helpers

parser = argparse.ArgumentParser(description="nsight wrapper with orkid customizations")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args()

capture_path = path.temp()/"temp.rdc"

exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)

# Generate a UUID
generated_uuid = str(uuid.uuid4())
the_uuid = "{" + generated_uuid + "}"

PYTHONPATH = os.environ.get("PYTHONPATH")
PATH = os.environ.get("PATH")
LD_LIBRARY_PATH = os.environ.get("LD_LIBRARY_PATH")
ORKID_WORKSPACE_DIR = os.environ.get("ORKID_WORKSPACE_DIR")

# Function to generate the Nsight GFX project JSON
project_content = {
    "extension": "ngfx-proj",
    "files": [],
    "launcher": {
        "activity/cpp/capturemode": "Manual",
        "activity/cpp/d3d/d3d12replayfencebehavior": "Default",
        "activity/cpp/d3d/enablecachedpipelinestatesupport": False,
        "activity/cpp/d3d/forceminimalshaderbindtables": False,
        "activity/cpp/d3d/limitshaderbindtablecapturesize": "Unlimited",
        "activity/cpp/d3d/replaycapturedexecuteindirectbuffer": False,
        "activity/cpp/d3d/reportforcefailedqueryinterfaces": True,
        "activity/cpp/d3d/reportunknownobjects": True,
        "activity/cpp/d3d/revisionzerodatacollection": "Auto",
        "activity/cpp/d3d/syncinterval": "0",
        "activity/cpp/d3d/syncshadercollection": False,
        "activity/cpp/d3d/usewritewatch": "Auto",
        "activity/cpp/forcerepaint": False,
        "activity/cpp/hudposition": "Top Left",
        "activity/cpp/opengl/allowprogrambinaries": True,
        "activity/cpp/opengl/delimiter": "SwapBuffers",
        "activity/cpp/opengl/noerror": "Application Controlled",
        "activity/cpp/opengl/reportnullclientsidebuffer": True,
        "activity/cpp/raytracing/acceleration_structure_collect_refits": "Auto",
        "activity/cpp/raytracing/acceleration_structure_collect_to_vidmem": "Auto",
        "activity/cpp/raytracing/acceleration_structure_geometry_tracking_mode": "Auto",
        "activity/cpp/raytracing/acceleration_structure_report_shallow_geometry_tracking_warnings": True,
        "activity/cpp/raytracing/force_raytracing_dimensions_to_zero": False,
        "activity/cpp/targethud": True,
        "activity/cpp/troubleshooting/blockonfirstincompatibility": "Auto",
        "activity/cpp/troubleshooting/crashreporting": True,
        "activity/cpp/troubleshooting/disableinterception": False,
        "activity/cpp/troubleshooting/ignoreincompatibilities": False,
        "activity/cpp/troubleshooting/replaythreadpausestrategy": "Auto",
        "activity/cpp/troubleshooting/serialization": True,
        "activity/cpp/troubleshooting/threading": False,
        "activity/cpp/vulkan/bufferdeviceaddresscapturereplay": True,
        "activity/cpp/vulkan/coherentbuffercollection": True,
        "activity/cpp/vulkan/forcecapturereplaydevicememory": False,
        "activity/cpp/vulkan/forcevalidation": False,
        "activity/cpp/vulkan/fullmemoryserialization": False,
        "activity/cpp/vulkan/hostvisiblevideomemorymode": "Demote to SYSMEM",
        "activity/cpp/vulkan/ignoredxoglovervklibraries": "Auto",
        "activity/cpp/vulkan/replayexpandmultidrawindirectcommands": False,
        "activity/cpp/vulkan/reserveheap": 0,
        "activity/cpp/vulkan/revisionzerodatacollection": "Auto",
        "activity/cpp/vulkan/safeobjectlookup": "Auto",
        "activity/cpp/vulkan/serializationobjectset": "Only Active",
        "activity/cpp/vulkan/unsafelayers": False,
        "activity/cpp/vulkan/unsafepnext": False,
        "activity/cpp/vulkan/unweavethreads": False,
        "activity/cpp/vulkan/usewritewatch": "Auto",
        "activity/fd/capturemode": "Manual",
        # Add more parameters as needed
        # Add your nsight parameters here
    },
    "uuid": "{}".format(the_uuid),
    "version": "1.3"
}
    
launcher = {
    "activity/cpp/capturemode": "Manual",
    # Other launcher parameters...
    # Add your nsight parameters here
    "platform/linux/arguments": " ".join(exe_args),
    "platform/linux/arguments/history": [" ".join(exe_args)],  # Add history for arguments
    "platform/linux/autoconnect": True,
    "platform/linux/environment": f"PYTHONPATH={PYTHONPATH}:$PYTHONPATH,PATH={PATH}:$PATH,LD_LIBRARY_PATH={LD_LIBRARY_PATH}:$LD_LIBRARY_PATH,ORKID_WORKSPACE_DIR={ORKID_WORKSPACE_DIR}",
    "platform/linux/environment/history": [  # Add history for environment
        f"PYTHONPATH={PYTHONPATH}:$PYTHONPATH,PATH={PATH}:$PATH,LD_LIBRARY_PATH={LD_LIBRARY_PATH}:$LD_LIBRARY_PATH,ORKID_WORKSPACE_DIR={ORKID_WORKSPACE_DIR}"
    ],
    "platform/linux/executable": exe_path,
    "platform/linux/executable/history": [exe_path],  # Add history for executable
}        
# Add executable and working directory
project_content["launcher"].update(launcher)
    

#nsight_gfx_project = generate_nsight_gfx_project(executable_name, exec_args, curwd)
print(json.dumps(project_content, indent=4))
project_file_path = path.temp()/"ngfx-project.ngfx-proj"
os.chdir(path.temp())
with open(project_file_path, 'w') as f:
    json.dump(project_content, f, indent=4)

nsight_dir = path.Path(os.environ["NSIGHT_DIR"])
nsight = nsight_dir/"ngfx-ui"

cmd_list = [nsight, project_file_path]

print(cmd_list)
command.run(cmd_list,do_log=True)
