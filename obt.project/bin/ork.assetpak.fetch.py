#!/usr/bin/env python3

################################################################################

from obt import path, wget, crypt, command
import sys, os, random, numpy, argparse, json, yarl

parser = argparse.ArgumentParser(description='assetpak tester')
parser.add_argument("-p", '--pack', type=str, help='passphrase', required=True)
args = vars(parser.parse_args())

pack = args['pack']

if pack is None:
  print("must supply pack")
  sys.exit(0)

known_packs = {
  "std": "<data>/misc/std_asset_manifest.json",    
}
known_keys = {
  "std": "singularity_rulez"
}

known_locs = {
  "default": yarl.URL("http://tweakoz.com/resources"),
}

known_dests = {
  "stage": path.stage(),
  "share": path.stage()/"share",
}

ork_data_dir = str(path.orkid()/"ork.data")

if pack in known_packs:
  packpath = known_packs[pack]
  if packpath.startswith("<data>"):
    packpath = packpath.replace("<data>",ork_data_dir)
    packobj = json.load(open(packpath))
    for key in packobj:
      #print(f"fetching {key}")
      # get object from name 
      item = packobj[key]
      if item["type"] == "asset_pak":
        dest = item["dest_path"]
        if dest.startswith("<"): 
          # dest_root is name between < and >
          l_angle = dest.find("<")
          r_angle = dest.find(">")
          dest_root = dest[l_angle+1:r_angle]
          if dest_root in known_dests:
            dest = dest.replace(f"<{dest_root}>",str(known_dests[dest_root]))
          #print(f"dest is {dest}")
          fetch_loc = item["loc"]
          filename = item["filename"]
          if fetch_loc.startswith("<"):
            l_angle = fetch_loc.find("<")
            r_angle = fetch_loc.find(">")
            fetch_root = fetch_loc[l_angle+1:r_angle]
            if fetch_root in known_locs:
              fetch_loc = fetch_loc.replace(f"<{fetch_root}>",str(known_locs[fetch_root]))
            fetch_loc += "/" + filename
          print(f"fetching asset_pak {key}")
          fetched = wget.wget(urls=[fetch_loc],output_name=filename,md5val=item["md5"])
          if fetched != None:
            #print(f"fetch {fetch_loc} to {dest}")
            if pack in known_keys:
              dec_key = known_keys[pack]
              decoded = str(fetched)+".dec"
              print(f"decrypting asset_pak {key}")
              crypt.decrypt_file(fetched,decoded,dec_key)
              print(f"extracting asset_pak {key}")
              cmd_list = [
                "tar",
                "xvf",
                decoded,
                "-C",
                dest
              ]
              command.capture(cmd_list)
              
        #wget.fetch(item)
  #print(f"fetching pack {pack} from {packpath}")
  