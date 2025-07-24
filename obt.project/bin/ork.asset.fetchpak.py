#!/usr/bin/env python3

################################################################################

from ork import assets
import sys, argparse

parser = argparse.ArgumentParser(description='assetpak fetcher')
parser.add_argument("-p", '--pack', type=str, help='asset ID as namespace.asset_id (e.g., singularity.std)', required=True)
args = vars(parser.parse_args())

pack = args['pack']

if pack is None:
  print("must supply pack")
  sys.exit(0)

# Fetch the asset pack(s)
fetch_count = assets.fetch_pak(pack)

# Exit with error if nothing was fetched
if fetch_count == 0:
  sys.exit(1)