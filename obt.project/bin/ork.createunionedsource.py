#!/usr/bin/env python3
##########################################################################
# create a unioned source filetree from the different modules
#  to aid metadevelopment tasks such as doxygen, easier browsing, etc.
#  this unioned filetree is disposable and should never be commited.....
##########################################################################

import os, string, pathlib

from pathlib import Path

base = Path(os.environ["ORKDOTBUILD_WORKSPACE_DIR"])
stage = Path(os.environ["ORKDOTBUILD_STAGE_DIR"])
mrgsrc = stage/"unioned_src"

o_inc_ork = base/"ork.core"/"inc"/"ork"
o_inc_lev2 = base/"ork.lev2"/"inc"/"ork"/"lev2"
o_inc_orktool = base/"ork.tool"/"inc"/"orktool"
o_inc_ent = base/"ork.ent"/"inc"/"pkg"/"ent"
o_inc_enttool = base/"ork.tool"/"inc"/"pkg"/"ent"

o_src_core = base/"ork.core"/"src"
o_src_lev2 = base/"ork.lev2"/"src"
o_src_tool = base/"ork.tool"/"src"
o_src_ent  = base/"ork.ent"/"src"

m_inc_ork     = mrgsrc/"inc"/"ork"
m_inc_pkg     = mrgsrc/"inc"/"pkg"
m_inc_ent     = m_inc_pkg/"ent"
m_inc_lev2    = mrgsrc/"inc"/"ork"/"lev2"
m_inc_enttool = m_inc_ent
m_inc_orktool = mrgsrc/"inc"/"orktool"

m_src_core = mrgsrc/"src"/"core"
m_src_lev2 = mrgsrc/"src"/"lev2"
m_src_tool = mrgsrc/"src"/"tool"
m_src_ent  = mrgsrc/"src"/"ent"

print(base)
print(stage)
print(mrgsrc)
print(m_inc_ork)

##########################################################################
# make directories
##########################################################################

os.system("rm -rf %s"%mrgsrc)
for item in [m_inc_ork,m_inc_lev2,m_inc_orktool,m_inc_orktool,m_inc_ent,m_src_core,m_src_lev2,m_src_tool,m_src_ent]:
    print("mkdir %s"%item)
    os.system("mkdir -p %s"%item)

##########################################################################

def do_item( src, target ):
    os.chdir(target)
    relative = os.path.relpath(src,target)
    print(relative)
    os.system("ln -s %s/* ./"% relative)

##########################################################################
# symlink items
##########################################################################

do_item(o_inc_ork,     m_inc_ork)
do_item(o_inc_lev2,    m_inc_lev2)
do_item(o_inc_orktool, m_inc_orktool)
do_item(o_inc_ent,     m_inc_ent)
do_item(o_inc_enttool, m_inc_enttool)

do_item(o_src_core, m_src_core)
do_item(o_src_lev2, m_src_lev2)
do_item(o_src_tool, m_src_tool)
do_item(o_src_ent,  m_src_ent)
