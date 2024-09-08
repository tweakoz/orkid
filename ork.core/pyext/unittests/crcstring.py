#!/usr/bin/env python3 

import unittest
from orkengine.core import CrcString, CrcStringProxy

class TestCrcString(unittest.TestCase):
  def test_default_constructor(self):
    s = CrcString()
    self.assertEqual(s.hashed, 0)
  ########################################
  #def test_str_constructor(self):
  #  s = CrcString("hello")
  #  self.assertEqual(s.hashed, 0x5e7f612c)
  ########################################

if __name__ == '__main__':
    unittest.main()
