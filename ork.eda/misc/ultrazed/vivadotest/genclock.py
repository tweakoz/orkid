#!/usr/bin/env python3

import os
from ork import vivado, path, pathtools

this_dir = path.Path(os.path.dirname(os.path.realpath(__file__)))
pathtools.chdir(this_dir)

print(os.getcwd())

IPID = "clk_wiz"
VENDOR = "xilinx.com"
VERSION = "6.0"
LIBRARY = "ip"
PARTNAME = "xczu7ev-fbvb900-1-e"
INSTANCENAME = "systemclocks"
INPFREQ = 300.000 # Mhz
OUTFREQ = 400.000 # Mhz
INP_PERIOD = round(1000.0 / INPFREQ,3);
OUT_PHASE_A = 0.000 # Degrees
OUT_PHASE_B = 180.000 # Degrees

MMCMDICT = {
  "CONFIG.NUM_OUT_CLKS": 2,
  "CONFIG.CLKOUT2_USED": True, # CONFIG.CLKOUT2_USED {true}

  "CONFIG.USE_MIN_POWER": True, # CONFIG.USE_MIN_POWER {true}
  "CONFIG.PRIM_IN_FREQ": INPFREQ, # CONFIG.PRIM_IN_FREQ {$INPFREQ}
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

  "CONFIG.CLKOUT1_REQUESTED_OUT_FREQ": OUTFREQ,
  "CONFIG.CLKOUT1_REQUESTED_PHASE": OUT_PHASE_A,
  "CONFIG.CLKOUT1_DRIVES": "BUFGCE",

  "CONFIG.CLKOUT2_REQUESTED_OUT_FREQ": OUTFREQ,
  "CONFIG.CLKOUT2_REQUESTED_PHASE": OUT_PHASE_B,
  "CONFIG.CLKOUT2_DRIVES": "BUFGCE",

 # "CONFIG.MMCM_CLKFBOUT_MULT_F": 8.000,

 # "CONFIG.MMCM_CLKOUT0_DIVIDE_F": 2.000,

  #"CONFIG.CLKOUT1_JITTER": 109.830,
  #"CONFIG.CLKOUT1_PHASE_ERROR": 114.212,

  #"CONFIG.CLKOUT2_JITTER": 109.830,
  #"CONFIG.CLKOUT2_PHASE_ERROR": 114.212,

  #"CONFIG.CLKOUT3_DRIVES": "BUFGCE",
  #"CONFIG.CLKOUT4_DRIVES": "BUFGCE",
  #"CONFIG.CLKOUT5_DRIVES": "BUFGCE",
  #"CONFIG.CLKOUT6_DRIVES": "BUFGCE",
  #"CONFIG.CLKOUT7_DRIVES": "BUFGCE",

  #"CONFIG.MMCM_CLKOUT1_PHASE": OUT_PHASE_B,
  #"CONFIG.MMCM_CLKOUT1_PHASE": OUT_PHASE_B,
}

IPPROPERTIES = ''.join(['%s {%s} ' % (key, str(value)) for (key, value) in MMCMDICT.items()])
IPPROPERTIES = IPPROPERTIES.replace("True","true")
IPPROPERTIES = IPPROPERTIES.replace("False","false")
print(IPPROPERTIES)

os.system("rm -rf %s"%(this_dir/".gen"/INSTANCENAME))
os.system("rm -rf %s"%(this_dir/".ip_user_files"))

dirmaps = {
  this_dir: "/tmp/build"
}

vivado.genIP(dirmaps=dirmaps,
             vivworkingdir="/tmp/build",
             tclhostfilename=this_dir/".gen"/"genclock.tcl",
             tclcontfilename=".gen/genclock.tcl",
             IPID=IPID,
             VENDOR=VENDOR,
             VERSION=VERSION,
             LIBRARY=LIBRARY,
             PARTNAME=PARTNAME,
             INSTANCENAME=INSTANCENAME,
             IPPROPERTIES=IPPROPERTIES)
