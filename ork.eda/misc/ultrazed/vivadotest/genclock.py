#!/usr/bin/env python3

import os, fileinput
from ork import vivado, path, pathtools

this_dir = path.Path(os.path.dirname(os.path.realpath(__file__)))
pathtools.chdir(this_dir)

########################################################
# TODO: hoist to infrastructure
########################################################

class OutClock:
 def __init__(self,freq,phaseoffset): # Mhz, Degrees
   self._frequency = freq
   self._phaseoffset = phaseoffset

def genMMCMdict(differential=False,
                INP_FREQ=None,
                outputs=[]):
 num_outputs = len(outputs)
 assert(num_outputs>=1)
 assert(num_outputs<=4)
 INP_PERIOD = round(1000.0 / INP_FREQ,3);
 MMCMDICT = {
  "CONFIG.NUM_OUT_CLKS": num_outputs,
  "CONFIG.USE_MIN_POWER": True,
  "CONFIG.PRIM_IN_FREQ": INP_FREQ,
  "CONFIG.USE_SAFE_CLOCK_STARTUP": True,
  "CONFIG.JITTER_SEL": "No_Jitter",
  "CONFIG.FEEDBACK_SOURCE": "FDBK_AUTO",
  "CONFIG.MMCM_CLKIN1_PERIOD": INP_PERIOD,
 }
 #############################
 if differential:
   MMCMDICT["CONFIG.PRIM_SOURCE"]="Differential_clock_capable_pin"
 #############################
 index = 0
 for item in outputs:
   out = outputs[index]
   index += 1
   MMCMDICT["CONFIG.CLKOUT%d_REQUESTED_OUT_FREQ"%index]=out._frequency
   MMCMDICT["CONFIG.CLKOUT%d_REQUESTED_PHASE"%index]=out._phaseoffset
   MMCMDICT["CONFIG.CLKOUT%d_DRIVES"%index]="BUFGCE" # BUFG,BUFGCE
   MMCMDICT["CONFIG.CLKOUT%d_USED"%index] = True
 #############################
 return MMCMDICT
########################################################
MMCMDICT=genMMCMdict(differential=True,
                     INP_FREQ = 300.000, # Mhz
                     outputs = [OutClock(350.000,0.000),
                                OutClock(350.000,180.000)])
########################################################

INP_NAME = "SYSCLK_P"
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
