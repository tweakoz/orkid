#!/usr/bin/env python3
import math, random

########################################
# input data
########################################

#wa env model
#a = [72,52,37,26,50,14]
#b = [.056,.464,2.17,6.94,.544,26.01]

#fullpenv model
#a = [39,56,61,70,82,99]
#b = [3.0,0.5,0.25,0.1,0.025,0.004]

#lowp model
a = [82,99]
b = [0.025,0.004]

#midp model
a = [39,56,61,70,75]
b = [3.0,0.5,0.25,0.1,0.054]

assert(len(a)==len(b))

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
 def __init__(self):
  self.biasa = 1.0
  self.scala = 89.0
  self.diva = 99.0
  self.biasb = 0.01
  self.basec = 1.0
  self.powc = math.pi
  self.scalc = 0.5
  self.error = 1e6
 def permute(self):
  self.biasa += random.uniform(-0.5,0.5)
  self.scala += random.uniform(-2.0,2.0)
  self.diva += random.uniform(-2.0,2.0)
  self.biasb += random.uniform(-0.01,0.1)
  self.basec += random.uniform(-0.5,0.5)
  self.powc += random.uniform(-2.0,2.0)
  self.scalc += random.uniform(-0.49,1)
 def transform(self,value):
  slope = self.biasa+(value*self.scala)/self.diva
  ror = slope2ror(slope,self.biasb)
  computed = math.pow(self.basec/ror,self.powc)*self.scalc
  return computed

bestmodel = model() # initial best

########################################
# find best fit
########################################

for i in range(1,200000):
 m = model()
 m.permute()
 index = 0
 error = 0.0
 for ival in a:
  desired = b[index]
  computed = m.transform(ival)
  error += abs(desired-computed)#/math.pow(desired,0.5)
  #print("ival<%d> slope<%g> ror<%g> desired<%g> computed<%g>"%(ival,slope,ror,desired,computed))
  index+=1
 if(error<bestmodel.error):
  bestmodel = m
  bestmodel.error = error
 if i%10000==0:
   print("iteration<%d>"%i)

########################################
# report results
########################################

print("best error<%g>"%bestmodel.error)
print("best biasa<%g>"%bestmodel.biasa)
print("best scala<%g>"%bestmodel.scala)
print("best diva<%g>"%bestmodel.diva)
print("best biasb<%g>"%bestmodel.biasb)
print("best basec<%g>"%bestmodel.basec)
print("best powc<%g>"%bestmodel.powc)
print("best scalc<%g>"%bestmodel.scalc)

index = 0
for ival in a:
  desired = b[index]
  computed = bestmodel.transform(ival)
  print( "desired<%g> computed<%g>"%(desired,computed))
  index+=1
