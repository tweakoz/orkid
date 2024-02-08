#!/usr/bin/env python3

from obt import command, path, env
#os.system("sudo udevadm control --reload-rules")

stage = path.stage()
bin = path.bin()
sourceroot = path.builds()/"pipewire"
buildroot = sourceroot/".build"

SPA_PLUGIN_DIR=buildroot/"spa"/"plugins"
SPA_DATA_DIR=sourceroot/"spa"/"plugins"
PIPEWIRE_MODULE_DIR=buildroot/"src"/"modules"
#PATH=buildroot/"src"/"examples":$(PATH) 
#PIPEWIRE_CONFIG_DIR=buildroot/"src"/"daemon"
PIPEWIRE_CONFIG_DIR=stage/"share"/"pipewire"
ACP_PATHS_DIR=sourceroot/"spa"/"plugins"/"alsa"/"mixer"/"paths" 
ACP_PROFILES_DIR=sourceroot/"spa"/"plugins"/"alsa"/"mixer"/"profile-sets"
#$(DBG) $(BUILD_ROOT)/src/daemon/pipewire-uninstalled

print("PIPEWIRE_MODULE_DIR<%s>"%PIPEWIRE_MODULE_DIR)
print("PIPEWIRE_CONFIG_DIR<%s>"%PIPEWIRE_CONFIG_DIR)
print("SPA_PLUGIN_DIR<%s>"%SPA_PLUGIN_DIR)
print("SPA_DATA_DIR<%s>"%SPA_DATA_DIR)
print("ACP_PATHS_DIR<%s>"%ACP_PATHS_DIR)
print("ACP_PROFILES_DIR<%s>"%ACP_PROFILES_DIR)

env.set("PIPEWIRE_MODULE_DIR",PIPEWIRE_MODULE_DIR)
env.set("PIPEWIRE_CONFIG_DIR",PIPEWIRE_CONFIG_DIR)
env.set("SPA_PLUGIN_DIR",SPA_PLUGIN_DIR)
env.set("SPA_DATA_DIR",SPA_DATA_DIR)
env.set("ACP_PATHS_DIR",ACP_PATHS_DIR)
env.set("ACP_PROFILES_DIR",ACP_PROFILES_DIR)

command.run([bin/"pipewire","-c",PIPEWIRE_CONFIG_DIR/"ORKPW.conf"], do_log=True)
#command.run(["wireplumber","-c",stage/"share"/"wireplumber"/"wireplumber.conf"], do_log=True)