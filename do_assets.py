#!/usr/bin/python

import os

tool = "ork.tool.test.osx.release"

def exp_anim( actnam, anmnam ):
	cmd = "%s -filter dae:xga -in data/src/actors/%s/anims/%s.dae -out data/pc/actors/%s/%s.xga" % (tool,actnam,anmnam,actnam,anmnam)
	print cmd
	os.system(cmd)

def exp_actor( actnam ):
	cmd = "%s -filter dae:xgm -in data/src/actors/%s/ref/%s.dae -out data/pc/actors/%s/%s.xgm" % (tool,actnam,actnam,actnam,actnam)
	print cmd
	os.system(cmd)


#ork.tool.test.osx.release -filter dae:xgm -in data/src/actors/Diver/ref/diver.dae -out data/pc/actors/Diver/diver.xgm
#ork.tool.test.osx.release -filter dae:xga -in data/src/actors/Diver/anims/idle.dae -out data/pc/actors/Diver/idle.xga
#ork.tool.test.osx.release -filter dae:xga -in data/src/actors/Diver/anims/walk.dae -out data/pc/actors/Diver/walk.xga
#ork.tool.test.osx.release -filter dae:xgm -in data/src/actors/frogman/mesh_export/sk3.dae -out data/pc/actors/frogman/sk3.xgm

#exp_anim("Diver", "idle")
#exp_anim("Diver", "walk")
#exp_anim("Diver", "jump")
#exp_anim("Diver", "run")
#exp_anim("houdtest", "a1")

exp_actor("Diver")
#exp_actor("houdtest")
