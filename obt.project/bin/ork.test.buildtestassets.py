#!/usr/bin/env python3

import os,string
import ork.host
import ork.command
from ork.path import Path

base = Path(os.environ["ORKID_WORKSPACE_DIR"])

tool = "ork.tool.release"

def mkdir( dirname ):
    cmd = ["mkdir","-p",base/"ork.data"/"pc"/"test"/dirname]
    ork.command.run(cmd)
    print(cmd)

def exp_anim( name ):
    src = base/"ork.data"/"tests"/name
    dst = (base/"ork.data"/"pc"/"test"/name).with_suffix('.xga')
    cmd = [tool,"--filter","ass:xga", "--in",src, "--out", dst]
    ork.command.run(cmd)
    print(cmd)

def exp_mesh( name ):
    src = base/"ork.data"/"tests"/name
    dst = (base/"ork.data"/"pc"/"test"/name).with_suffix('.xgm')
    cmd = [tool,"--filter","ass:xgm", "--in",src, "--out", dst]
    ork.command.run(cmd)
    print(cmd)

mkdir("chartest")
mkdir("hfstest")
mkdir("pbr1")

animlist = ["bonetest_anim.dae","rigtest_anim.gltf","chartest/char_anim.gltf","hfstest/hfs_rigtest_anim.fbx"]
for item in animlist:
    exp_anim(item)

meshlist = ["pbr_calib.gltf","pbr1/pbr1.gltf","bonetest_mesh.gltf","rigtest_exp.gltf","chartest/char_mesh.gltf","hfstest/hfs_rigtest.fbx","hfstest/hfs_chartest.fbx","hfstest/bakeprocedural.gltf"]
for item in meshlist:
    exp_mesh(item)
