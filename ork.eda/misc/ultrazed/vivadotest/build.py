#!/usr/bin/env python3

import os, argparse, sys
from ork import path, pathtools

parser = argparse.ArgumentParser(description='vivadotest build')
parser.add_argument('--clean', action="store_true", help='cleanup transient data' )
parser.add_argument('--genip', action="store_true", help='generate initial IP' )

_args = vars(parser.parse_args())

this_dir = path.Path(os.path.dirname(os.path.realpath(__file__)))
pathtools.chdir(this_dir)

if _args["clean"]:
  os.system("rm -rf %s"%(this_dir/".build"))
  os.system("rm -rf %s"%(this_dir/".gen"))
  _args["genip"]=True

########################################################
########################################################
from ork.eda.xilinx import vivado, mmcm
########################################################
########################################################
vivctx = vivado.Context(hostdir=this_dir,
                        FPGAPART="xczu7ev-fbvb900-1-e")
########################################################
# Generate required procedural IP
#  systemclocks, etc..
########################################################
if _args["genip"]:
  myclocks = [mmcm.OutClock(350.000,0.000),
              mmcm.OutClock(350.000,180.000)]
  mmcm.generate(vivctx,
                INP_NAME="SYSCLK_P",
                INSTANCENAME="systemclocks",
                differential=True,
                INP_FREQ = 300.000, # mhz,
                outclocks = myclocks)
########################################################
# main build
########################################################
vivctx.run_tclscript("cleanbuild.tcl")
