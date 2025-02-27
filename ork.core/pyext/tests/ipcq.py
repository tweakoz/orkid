#!/usr/bin/env python
import time, argparse
from orkengine import core
from threading import Thread

parser = argparse.ArgumentParser(description='ipcq test')
parser.add_argument('--server', action='store_true', help='run as server')
parser.add_argument('--count', type=int, default=4<<30, help='count')

args = vars(parser.parse_args())

is_server = args['server']
COUNT = args['count']
core.coreappinit()

if is_server:
  chanA_sendr = core.ipcq.Sender()
  chanA_sendr.create("SHM_TESTA")
  chanB_sendr = core.ipcq.Sender()
  chanB_sendr.create("SHM_TESTB")
  thr_a1 = Thread(target=lambda: chanA_sendr.benchSendPerformance(COUNT) )
  thr_b1 = Thread(target=lambda: chanB_sendr.benchSendPerformance(COUNT) )
  thr_a1.start()
  thr_b1.start()
  thr_a1.join()
  thr_b1.join()
  dblock = core.DataBlock()
  dblock.writeString("hello")
  dblock.writeString("world")
  print( "dblock: ", dblock, dblock.hexdump() )
  chanA_sendr.sendDataBlock(dblock)
else:
  chanA_recvr = core.ipcq.Receiver()
  chanA_recvr.connect("SHM_TESTA")
  chanB_recvr = core.ipcq.Receiver()
  chanB_recvr.connect("SHM_TESTB")
  thr_a2 = Thread(target=lambda: chanA_recvr.benchReceivePerformance(COUNT) )
  thr_b2 = Thread(target=lambda: chanB_recvr.benchReceivePerformance(COUNT) )
  thr_a2.start()
  thr_b2.start()
  thr_a2.join()
  thr_b2.join()
  dblock2 = chanA_recvr.receiveDataBlock()
  stream = core.DataBlockInputStream(dblock2)
  print("dblock2: ",dblock2, dblock2.hexdump() )
  print("str1: ",stream.readString())
  print("str2: ",stream.readString())

