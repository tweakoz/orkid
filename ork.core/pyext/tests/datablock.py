#!/usr/bin/env python3

import numpy as np
from orkengine.core import *

##############################################
# write datablock
##############################################

a = np.ones([200, 300, 3],dtype = np.uint8)
db = DataBlock()
db.writeString("np.uint8") # type
db.writeInt(3) # ndim
db.writeInt(300) # width
db.writeInt(200) # height
db.writeInt(3) # depth
a_as_bytes = a.tobytes() # convert to bytes
db.writeBytes(a_as_bytes) # write to datablock

print(db)

##############################################
# read datablock
###############################################

istream = DataBlockInputStream(db)
type = istream.readString()
ndim = istream.readInt()
width = istream.readInt()
height = istream.readInt()
depth = istream.readInt()
bytes = istream.readBytes()

###############################################

print(type)
print(ndim)
print(width)
print(height)
print(depth)
print(len(bytes))
