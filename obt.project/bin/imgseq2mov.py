#!/usr/bin/python

import os, sys

nargs = len(sys.argv)
if 3 != nargs:
	print "usage: imgseq2mov.py <seq_basename> <mov_name>"
	print "   will generate an mp4 wrapped x264 yuv420p stream"
	sys.exit(0)

imgseqnam = sys.argv[1]
movoutnam = sys.argv[2]

cmd = "ffmpeg -i %s_%%04d.png -f mp4 -vcodec libx264 -pix_fmt yuv420p %s" % (imgseqnam,movoutnam)

os.system(cmd)

