#!/usr/bin/env python

import os,string
from ork.build.common import TargetPlatform

os.chdir(os.environ["ORKDOTBUILD_WORKSPACE_DIR"])

tool = "ork.tool.test."+TargetPlatform+".release"
print(tool)

def mkdir( actnam ):
	cmd = "mkdir -p ork.data/pc/actors/%s" % actnam
	print(cmd)
	os.system(cmd)

def exp_anim( actnam, anmnam ):
	cmd = "%s -filter dae:xga -in ork.data/src/actors/%s/anims/%s.dae -out ork.data/pc/actors/%s/%s.xga" % (tool,actnam,anmnam,actnam,anmnam)
	print(cmd)
	os.system(cmd)

def exp_actor( actnam ):
	cmd = "%s -filter dae:xgm -in ork.data/src/actors/%s/ref/%s.dae -out ork.data/pc/actors/%s/%s.xgm" % (tool,actnam,actnam,actnam,actnam)
	print(cmd)
	os.system(cmd)

def exp_object( objnam ):
	cmd = "%s -filter dae:xgm -in ork.data/src/environ/%s/ref/%s.dae -out ork.data/pc/environ/%s/%s.xgm" % (tool,objnam,objnam,objnam,objnam)
	print(cmd)
	os.system(cmd)

def do_objects( obj_str ):
 for a in string.split(obj_str):
    mkdir(a)
    exp_object(a)

def do_actors( act_str ):
 for a in string.split(act_str):
    mkdir(a)
    exp_actor(a)

def do_anims( anm_str ):
 for k in anm_str:
    a = anm_str[k]
    for i in string.split(a):
        exp_anim(k, a)

#######################################

#objects = "mtn1"
actors = "4limb rijid frogman iceblob"

anims = dict()
anims["4limb"] = "sprawl"
anims["frogman"] = "an1"

#######################################

do_actors( actors )
do_anims( anims )
#do_objects( objects )

os.system("mkdir ork.data/pc/terrain")
os.system("cp -r ork.data/src/terrain/* ork.data/pc/terrain/")
#######################################
