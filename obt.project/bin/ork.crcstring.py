#!/usr/bin/env python3
###############################################33
# hash a string using orkengine's CrcString
#  and return the hash
#  (useful for debugging)
###############################################33
import sys
from orkengine.core import CrcStringProxy
from ork.deco import Deco
###############################################33
deco = Deco();
token = CrcStringProxy()
###############################################33
str_deco = deco.yellow("str(%s)"%sys.argv[1])
crc_val = token.__getattr__(sys.argv[1])
crc_deco = deco.magenta("%s"%crc_val)
###############################################33
print(str_deco+" -> "+crc_deco)
