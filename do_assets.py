#!/usr/bin/python

import os,string

tool = "ork.tool"

def mkdir( actnam ):
	cmd = "mkdir -p data/pc/actors/%s" % actnam
	print cmd
	os.system(cmd)

def exp_anim( actnam, anmnam ):
	cmd = "%s -filter dae:xga -in data/src/actors/%s/anims/%s.dae -out data/pc/actors/%s/%s.xga" % (tool,actnam,anmnam,actnam,anmnam)
	print cmd
	os.system(cmd)

def exp_actor( actnam ):
	cmd = "%s -filter dae:xgm -in data/src/actors/%s/ref/%s.dae -out data/pc/actors/%s/%s.xgm" % (tool,actnam,actnam,actnam,actnam)
	print cmd
	os.system(cmd)

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

actors = "4limb rijid frogman"

anims = dict()
anims["4limb"] = "sprawl"
anims["frogman"] = "an1"

#######################################

do_actors( actors )
do_anims( anims )

#######################################


