#!/usr/bin/env python3

import numpy as np
from orkengine.core import *

a = np.ones([200, 300, 3],dtype = np.uint8)
db = DataBlock()
db.addData(a)
print(db.size)
print(a.shape)
print(db)