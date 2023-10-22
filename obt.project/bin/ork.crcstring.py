#!/usr/bin/env python3
###############################################33
# hash a string using orkengine's CrcString
#  and return the hash
#  (useful for debugging)
###############################################33
import sys, os, re
import argparse
from orkengine.core import CrcStringProxy
from obt.deco import Deco
from obt import pathtools, path, env
###############################################33
deco = Deco();
token = CrcStringProxy()
###############################################33
encodings = ['utf-8', 'windows-1252', 'ISO-8859-1']
def read_file(filename):
  content = None
  for encoding in encodings:
    try:
      with open(filename, 'r', encoding=encoding) as file:
        content = file.read()
      break
    except UnicodeDecodeError:
      pass
  return content
###############################################33
parser = argparse.ArgumentParser(description="Orkid CRCSTRING util")
parser.add_argument("--hash", help="string -> hash")
parser.add_argument("--string", help="hash -> string")
args = parser.parse_args()
print(args)
if args.hash != None: # string->hash
  str_deco = deco.yellow("str(%s)"%args.hash)
  crc_val = token.__getattr__(args.hash)
  crc_deco = deco.magenta("%s"%crc_val)
  print(str_deco+" -> "+crc_deco)
elif args.string != None: # hash->string
  pattern = r'CrcEnum\((.*?)\)'
  p = path.Path(os.environ["ORKID_WORKSPACE_DIR"])
  l = pathtools.recursive_glob(str(p))
  for item in l:
    if (item.find(".h")>=0) or (item.find(".inl")>=0):
      with open(item, 'r') as file:
        content = read_file(item)
        matches = re.findall(pattern, content)
        for match in matches:
          crc_val = str(token.__getattr__(match).hashed)
          #print(crc_val)
          if(crc_val==args.string):
            str_deco = deco.yellow("hash(%s)"%args.string)
            crc_deco = deco.magenta("%s"%crc_val)
            print(str_deco+" -> "+crc_deco,match)
            sys.exit(0)
###############################################33
