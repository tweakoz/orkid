#!/usr/bin/env python3
import math, random
import numpy as np
import concurrent.futures
opq = concurrent.futures.ThreadPoolExecutor(max_workers=8)
# cant use ProcessPoolExecutor because multiprocessing is broken
#  so we are stuck with the GIL for now
#  this should get more performant when using numpi for crunching
#  the numbers

########################################
# input data
########################################

class dataset:
  def __init__(self,name):
    self._name = name
    self._keys = []
    self._vals = []
  def check(self):
    assert(len(self._keys)==len(self._vals))

wa_env_data = dataset("waveamp")
wa_env_data._keys = [72,52,37,26,50,14]
wa_env_data._vals = [.056,.464,2.17,6.94,.544,26.01]
wa_env_data.check()

fullp_env_data = dataset("full-p")
fullp_env_data._keys = [39,56,61,70,82,99]
fullp_env_data._vals = [3.0,0.5,0.25,0.1,0.025,0.004]
fullp_env_data.check()

lowp_env_data = dataset("low-p")
lowp_env_data._keys = [82,99]
lowp_env_data._vals = [0.025,0.004]
lowp_env_data.check()

midp_env_data = dataset("mid-p")
midp_env_data._keys = [39,56,61,70,75]
midp_env_data._vals = [3.0,0.5,0.25,0.1,0.054]
midp_env_data.check()

hip_env_data = dataset("hi-p")
hip_env_data._keys = [5,10]
hip_env_data._vals = [135,79]
hip_env_data.check()

########################################
# math utils
########################################

def clamp(inp,min_value,max_value):
  return max(min(inp, max_value), min_value)

def slope2ror(slope,bias):
  clamped     = clamp(slope + bias, 0.0, 90.0 - bias);
  riseoverrun = math.tan(clamped * math.pi / 180.0);
  return riseoverrun

########################################
# model
########################################

class model:
 ##################################
 def __init__(self):
  self.inpbias = 1.0
  self.inpscale = 89.0
  self.inpdiv = 99.0
  self.rorbias = 0.01
  self.basenumer = 1.0
  self.power = 3.0
  self.scalar = 0.5
  self.error = 1e6
 ##################################
 def permute(self):
  self.inpbias += random.uniform(-0.5,0.5)
  self.inpscale += random.uniform(-2.0,2.0)
  self.inpdiv += random.uniform(-2.0,2.0)
  self.rorbias += random.uniform(-0.01,0.1)
  self.basenumer += random.uniform(-0.5,0.5)
  self.power += random.uniform(-2.0,2.0)
  self.scalar += random.uniform(-0.49,1)
 ##################################
 def transform(self,value):
  slope = self.inpbias+(value*self.inpscale)/self.inpdiv
  ror = slope2ror(slope,self.rorbias)
  computed = math.pow(self.basenumer/ror,self.power)*self.scalar
  return computed
 ##################################
 def report(self,dset):
  print("########################################")
  print(dset._name+" error<%g>"%self.error)
  print(dset._name+" inpbias<%g>"%self.inpbias)
  print(dset._name+" inpscale<%g>"%self.inpscale)
  print(dset._name+" inpdiv<%g>"%self.inpdiv)
  print(dset._name+" rorbias<%g>"%self.rorbias)
  print(dset._name+" basenumer<%g>"%self.basenumer)
  print(dset._name+" power<%g>"%self.power)
  print(dset._name+" scalar<%g>"%self.scalar)
  print("##")
  index = 0
  for ival in dset._keys:
   desired = dset._vals[index]
   computed = self.transform(ival)
   print(dset._name+" desired<%g> computed<%g>"%(desired,computed))
   index+=1
  print("########################################")
 ##################################

########################################
# find best fit for one dataset
########################################

def perform_dataset(dset):
 bestmodel = model() # initial best
 for i in range(1,1250000):
  m = model()
  m.permute()
  index = 0
  error = 0.0
  for ival in dset._keys:
   desired = dset._vals[index]
   computed = m.transform(ival)
   error += abs(desired-computed)/math.pow(desired,0.25)
   index+=1
  if(error<bestmodel.error):
   bestmodel = m
   bestmodel.error = error
  if i%10000==0:
   print("iteration<%d>"%i)
 return bestmodel

########################################
# submit jobs
########################################

async0 = opq.submit(perform_dataset, wa_env_data)
async1 = opq.submit(perform_dataset, lowp_env_data)
async2 = opq.submit(perform_dataset, midp_env_data)
async3 = opq.submit(perform_dataset, hip_env_data)
async4 = opq.submit(perform_dataset, fullp_env_data)

########################################
# report jobs
########################################

async0.result().report(wa_env_data)
async1.result().report(lowp_env_data)
async2.result().report(midp_env_data)
async3.result().report(hip_env_data)
