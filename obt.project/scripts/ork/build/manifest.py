import os, sys, common, json

deco = common.deco

class Manifest:
    def __init__(self,pth):
      self._path = pth
      with open(pth,"r") as f:
        self._obj = json.load(f)
        print(self._obj)
