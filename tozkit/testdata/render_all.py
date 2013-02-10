#!/usr/bin/python
import glob
import os
import sys

ndivs = 0
divi = 0
do_divs = False

if len(sys.argv)==3:
 ndivs = int(sys.argv[1])
 divi = int(sys.argv[2])
 print "ndivs<%d> divi<%d>" % (ndivs,divi)
 do_divs = True

cwd = os.getcwd()

os.system("rm -rf job_files/*")

def send_job(ribf):
	os.system("renderdl -progress %s"%ribf )

files = sorted(glob.glob("output/*.rib"))
#print files

idx = int(0)

for file in files:
  do_job = True
  if do_divs:
    do_job = (idx%ndivs)==divi
  if do_job:
    send_job(file)
  idx=idx+1
