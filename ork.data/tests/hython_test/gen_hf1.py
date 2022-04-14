#!/usr/bin/env hython3.7

import hou
import ork

print("hello")

#hython $HFS/houdini/python3.7libs/opnode_sum.py

def print_tree(node, indent=0):
  for child in node.children():
    indent_str = " " * indent
    print ( "%s%s : %s" % (indent_str,child.name(),child.type().name()) )
    print_tree(child, indent + 3)

def print_parms(node):
  for p in node.parms():
    p_name = p.name()
    p_val = p.eval()
    print("%s : %s" % (p_name, p_val))

########################################################
# create geometry
########################################################

hou.hipFile.clear()
top_geo = hou.node('/obj').createNode('geo')
box = top_geo.createNode("box")
subd = top_geo.createNode("subdivide")
subd.parm('iterations').set(1)
subd.parm('smoothvertex').set(0)
print("############################################")
print_parms(subd)
print("############################################")
subd.setFirstInput(box)
subd.moveToGoodPosition()
subd.setDisplayFlag(True)
subd.setRenderFlag(True)
subd.setCurrent(True, clear_all_selected=True)

########################################################
# create GLTF export node
########################################################

glb_export = top_geo.createNode("rop_gltf")
glb_export.setFirstInput(subd)

print_tree(hou.node('/'))

print("############################################")
print(dir(glb_export))
print("############################################")
glb_export.parm('file').set("hf1_generated.glb")
print("############################################")
print_parms(glb_export)
print("############################################")

glb_export.render()