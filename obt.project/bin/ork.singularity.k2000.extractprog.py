#!/usr/bin/env python3

import os, sys, json, argparse
from obt import path, command

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-p", "--extractprogram", type=str, required=False, default="Stereo_Grand", help='extract program')
args = vars(parser.parse_args())
extract_program = args["extractprogram"]

a = path.stage()/"share"/"singularity"/"kurzweil"/"k2v3base.json"

o = json.load(open(a,"rt"))
krz = o["KRZ"]
objects = krz["objects"]

keymaps_by_id = {}
keymaps_by_name = {}
samples_by_id = {}
samples_by_name = {}
multisamples_by_id = {}
multisamples_by_name = {}
programs_by_id = {}
programs_by_name = {}
  
for object in objects:
  if "Keymap" in object:
    keymaps_by_id[object["objectID"]] = object
    keymaps_by_name[object["Keymap"]] = object
  elif "Sample" in object:
    samples_by_id[object["objectID"]] = object
    samples_by_name[object["Sample"]] = object
  elif "MultiSample" in object:
    multisamples_by_id[object["objectID"]] = object
    multisamples_by_name[object["MultiSample"]] = object
  elif "Program" in object:
    programs_by_id[object["objectID"]] = object
    programs_by_name[object["Program"]] = object
  
#pretty print
#print(json.dumps(keymaps_by_id,indent=2))
#print(json.dumps(samples_by_id,indent=2))
#print(json.dumps(multisamples_by_id,indent=2))
#print(json.dumps(programs_by_id,indent=2))

#program = programs_by_name[extract_program]
#print(json.dumps(program,indent=2))

out_base = path.orkid()/"ork.data"/"singularity"/"kurzweil"
d = out_base/"prgdumps"
dp = d/"programs"
command.run(["rm","-rf",out_base])
command.run(["mkdir","-p",dp])

for item in programs_by_name:
  program = programs_by_name[item]
  prgdir = dp/item
  command.run(["mkdir","-p",prgdir])
  out_path = prgdir/f"{item}.json"
  layers = program["LAYERS"]
  program["LAYERS"] = []

  with open(out_path,"wt") as f:
    f.write(json.dumps(program,indent=2))
    f.close()
  for i in range(len(layers)):
    layer = layers[i]
    with open(prgdir/f"layer_{i}.json","wt") as f:
      f.write(json.dumps(layer,indent=2))
      f.close()
    #layer["Sample"] = samples_by_id[layer["Sample"]]
    #layers[i] = layer
#keymaps 