#!/usr/bin/env python3 

import unittest
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
import numpy as np


class TestMathMtx3Methods(unittest.TestCase):
  pass
  ########################################
  #def test_mtx3_to_np(self):
  #  m = mtx3() # identity
  #  as_np = np.array(m, copy=False)
  #  for i in range(3):
  #    for j in range(3):
  #      self.assertEqual(as_np[i][j], 1 if i==j else 0)
if __name__ == '__main__':
    unittest.main()
