#!/usr/bin/env python3

import os, fileinput
from ork import vivado, path, pathtools

this_dir = path.Path(os.path.dirname(os.path.realpath(__file__)))
pathtools.chdir(this_dir)

INP_NAME = "SYSCLK_P"
INP_FREQ = 300.000 # Mhz
OUT_FREQ = 400.000 # Mhz
INP_PERIOD = round(1000.0 / INP_FREQ,3);
OUT_PHASE_A = 0.000 # Degrees
OUT_PHASE_B = 180.000 # Degrees

########################################################
# TODO: parameterize for single-ended vs differential
# TODO: hoist to infrastructure
########################################################

MMCMDICT = {
  #"CONFIG.PRIMARY_PORT": INP_NAME+"_P",
  "CONFIG.PRIM_SOURCE": "Differential_clock_capable_pin",


  "CONFIG.NUM_OUT_CLKS": 2,
  "CONFIG.CLKOUT2_USED": True,

  "CONFIG.USE_MIN_POWER": True,
  "CONFIG.PRIM_IN_FREQ": INP_FREQ,
  "CONFIG.USE_SAFE_CLOCK_STARTUP": True,
  "CONFIG.JITTER_SEL": "No_Jitter",
  "CONFIG.FEEDBACK_SOURCE": "FDBK_AUTO",

  "CONFIG.MMCM_DIVCLK_DIVIDE": 3,
  "CONFIG.MMCM_CLKOUT0_DUTY_CYCLE": 0.5,

  "CONFIG.MMCM_CLKOUT1_DIVIDE": 2,
  "CONFIG.MMCM_CLKOUT1_DUTY_CYCLE": 0.5,


  "CONFIG.MMCM_CLKIN1_PERIOD": INP_PERIOD,
  "CONFIG.MMCM_CLKIN2_PERIOD": 10.0, # ???

  "CONFIG.CLKIN1_JITTER_PS": 33.330000000000005,

  "CONFIG.CLKOUT1_REQUESTED_OUT_FREQ": OUT_FREQ,
  "CONFIG.CLKOUT1_REQUESTED_PHASE": OUT_PHASE_A,
  "CONFIG.CLKOUT1_DRIVES": "BUFGCE",

  "CONFIG.CLKOUT2_REQUESTED_OUT_FREQ": OUT_FREQ,
  "CONFIG.CLKOUT2_REQUESTED_PHASE": OUT_PHASE_B,
  "CONFIG.CLKOUT2_DRIVES": "BUFGCE",
}

########################################################
########################################################

INSTANCENAME = "systemclocks"
OUTDIR = this_dir/".gen"/INSTANCENAME

os.system("rm -rf %s"%OUTDIR)
os.system("rm -rf %s"%(this_dir/".ip_user_files"))

dirmaps = {
  this_dir: "/tmp/build"
}

vivado.genIP(dirmaps=dirmaps,
             vivworkingdir="/tmp/build",
             tclhostfilename=this_dir/".gen"/"genclock.tcl",
             tclcontfilename=".gen/genclock.tcl",
             IPID="clk_wiz",
             VENDOR="xilinx.com",
             VERSION="6.0",
             LIBRARY="ip",
             PARTNAME="xczu7ev-fbvb900-1-e",
             INSTANCENAME=INSTANCENAME,
             IPPROPERTIES=MMCMDICT)

verilog_out = OUTDIR/("%s_clkwiz.v"%INSTANCENAME)
xcd_out = OUTDIR/("%s.xdc"%INSTANCENAME)

###########################################################
# fixup Differential clock input since the clk_wiz is hosed
#  (it disables PRIMARY_PORT in differential mode)
# https://forums.xilinx.com/xlnx/board/crawl_message?board.id=DEENBD&message.id=13910
###########################################################

with fileinput.FileInput(xcd_out, inplace=True) as file:
    for line in file:
        print(line.replace("clk_in1_p", INP_NAME), end='')
