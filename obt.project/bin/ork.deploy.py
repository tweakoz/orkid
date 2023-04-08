#!/usr/bin/env python3

import sys
import os, argparse
import ork.host
import ork.dep
from ork import path, pathtools
from ork.command import Command, run
from ork import buildtrace
import ork._globals as _glob

parser = argparse.ArgumentParser(description='orkid deploy to folder')

parser.add_argument("-d","--destdir", type=str, default=str(path.stage()/".test-deploy"), help = "deployment dir")

_args = vars(parser.parse_args())

this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)

stage_dir = path.stage()

deploy_destination = path.Path(_args["destdir"])

print(deploy_destination)

#########################
num_dup_bytes = 0
num_bytes_total = 0
#############################################
class CatalogItem:
    def __init__(self, path,wildcard="*"):
        self.path = path
        self.items = pathtools.EnumDirInfo(path,wildcard)
        self.num_bytes_dupe = self.items.numDuplicateBytes()
        self.num_bytes_total = self.items.numBytes()
class Catalog:
    def __init__(self):
        self.items = {}
        self.total_catalog_bytes = 0
        self.total_catalog_bytes_dupe = 0
        self.num_files = 0
    def enumerate(self, path, wildcard="*"):
        print( "enumerating %s ...." % path)
        self.items[path]=CatalogItem(path,wildcard=wildcard)
        self.total_catalog_bytes += self.items[path].num_bytes_total
        self.total_catalog_bytes_dupe += self.items[path].num_bytes_dupe
        for k in self.items.keys():
            item = self.items[k]
            self.num_files += len(item.items.contents_by_path)
    def summary(self):
        print("#############################################")
        print("total_catalog_num_files<%d>" % self.num_files)
        print("total_catalog_bytes<%d>" % self.total_catalog_bytes)
        print("total_catalog_bytes_dupe<%d>" % self.total_catalog_bytes_dupe)
        print("total_catalog_bytes_unique<%d>" % (self.total_catalog_bytes-self.total_catalog_bytes_dupe))
        print("total_catalog_items<%d>" % len(self.items))
        print("total_catalog_pct_dupe<%f>" % (100*float(self.total_catalog_bytes_dupe)/float(self.total_catalog_bytes)))
        print("#############################################")
        for k in self.items.keys():
            item = self.items[k]
            print("  item<%s> num_bytes_dupe<%d> num_bytes<%d>" % (item.path, item.num_bytes_dupe, item.num_bytes_total))

#############################################
PYTHON = ork.dep.instance("python")
pyvenv_dir = PYTHON.virtualenv_dir
#############################################
c = Catalog()
c.enumerate(path.includes(),"*")
c.enumerate(path.libs(),"*")
c.enumerate(path.bin(),"*")
c.enumerate(pyvenv_dir,"*")
#############################################
c.summary()
