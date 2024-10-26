#!/usr/bin/env python3

from orkengine.core import *
from orkengine.lev2 import *
import math 

a = Transform()
a.directMatrix = mtx4.lookAt(vec3(1,1,1),vec3(0,0,0),vec3(0,1,0))

print("a: ", a)
print("a.composed: " , a.composed )

constants = mathconstants()

#################################################

fov = 45
aspect = 1
near = .5
far = 3

print("###################")
print( "PERSPECTIVE: FOV<%g> ASPECT<%g> NEAR<%g> FAR<%g>" % (fov, aspect, near, far))
print("###################")

pmatrix = mtx4.perspective(fov*constants.DTOR,aspect,near,far)

print("###################")
pmatrix.dump("pmtx")
print("###################")

def yo(deco, vspace_v3):
  vspace_v4 = vec4(vspace_v3,1)
  hspace = vspace_v4.transform(pmatrix)
  dspace = hspace.xyz*(1/hspace.w)
  print("## %s" % deco)
  print("vspace_v3: %s" % vspace_v3)
  print("vspace_v4: %s" % vspace_v4)
  print("hspace: %s" % hspace)
  print("dspace: %s" % dspace)

print("###################")
print("#### AT Near eye,corners")
print("###################")
yo("eye", vec3(0,0,-near))
yo("BL", vec3(-1,-1,-near))
yo("TL", vec3(-1,1,-near))
yo("TR", vec3(1,1,-near))
yo("BR", vec3(1,-1,-near))

print("###################")
print("#### AT Far eye,corners")
print("###################")
yo("eye",vec3(0,0,-far))
yo("BL",vec3(-1,-1,-far))
yo("TL", vec3(-1,1,-far))
yo("TR", vec3(1,1,-far))
yo("BR", vec3(1,-1,-far))

#########################################################################################

def do_frustum_check(fov_degrees = 0.0,
                     aspect = 1.0,
                     near = 0.5,
                     far = 3.0 ):
  
  fov_radians = fov_degrees*constants.DTOR
  print( "######################################################################")
  print( "PERSPECTIVE: FOV(deg)<%g> ASPECT<%g> NEAR<%g> FAR<%g>" % (fov_degrees, aspect, near, far))
  print( "######################################################################")

  tan_half_angle = math.tan(fov_radians / 2)
  a = 1 / (aspect * tan_half_angle)
  b = 1 / tan_half_angle

  print(">> frustum.tan_half_angle: ",tan_half_angle)
  print(">> frustum.a: ",a)
  print(">> frustum.b: ",b)

  # 
  pmatrix = mtx4.perspective(fov_radians,aspect,near,far)
  pmatrix.dump("pmtx")
  frus = frustum(mtx4(),pmatrix)


  #print("frustum.xnormal: ",frus.xNormal)
  #print("frustum.ynormal: ",frus.yNormal)
  #print("frustum.znormal: ",frus.zNormal)
  #print("frustum.nearPlane: ",frus.nearPlane)
  #print("frustum.farPlane: ",frus.farPlane)
  #print("frustum.leftPlane: ",frus.leftPlane)
  #print("frustum.rightPlane: ",frus.rightPlane)
  #print("frustum.topPlane: ",frus.topPlane)
  #print("frustum.bottomPlane: ",frus.bottomPlane)

  # get frustum corner edge direction vectors (far-near)

  c0 = (frus.farCorner(0)-frus.nearCorner(0)).normalized
  c1 = (frus.farCorner(1)-frus.nearCorner(1)).normalized
  c2 = (frus.farCorner(2)-frus.nearCorner(2)).normalized
  c3 = (frus.farCorner(3)-frus.nearCorner(3)).normalized
  #print("frustum.c0: ",c0)
  #print("frustum.c1: ",c1)
  #print("frustum.c2: ",c2)
  #print("frustum.c3: ",c3)

  # get frustum side edge direction vectors (centered horiz,vertical)
  cL = (c0+c3).normalized
  cR = (c1+c2).normalized
  cT = (c0+c1).normalized
  cB = (c2+c3).normalized
  #print("frustum.cL: ",cL)
  #print("frustum.cR: ",cR)
  #print("frustum.cT: ",cT)
  #print("frustum.cB: ",cB)

  measure_horiz_fov = cL.orientedAngle(cR,vec3(0,-1,0))/constants.DTOR
  measure_verti_fov = cT.orientedAngle(cB,vec3(-1,0,0))/constants.DTOR
  print(">> frustum.measured_horiz_fov: ",measure_horiz_fov)
  print(">> frustum.measured_verti_fov: ",measure_verti_fov)
  
#########################################################################################

def aspect_convert(fovY, fov_ratio):
  slope_a = math.tan(fovY*constants.DTOR*0.5)
  slope_b = math.tan(fovY*fov_ratio*constants.DTOR*0.5)
  r = slope_b/slope_a
  print("slope_a: ",slope_a)
  print("slope_b: ",slope_b)
  print("r: ",r)
  return r

def do_frustum_check_with_ratio(fovy_degrees=0.0, fov_ratio=1.0 ):
  do_frustum_check(fov_degrees=fovy_degrees, aspect=aspect_convert(fovy_degrees,fov_ratio))

#########################################################################################

print("###################")
print("#### FRUSTUM CHECKS")
print("###################")
#                

do_frustum_check_with_ratio(fovy_degrees=45)
do_frustum_check_with_ratio(fovy_degrees=90)
do_frustum_check_with_ratio(fovy_degrees=75)
do_frustum_check_with_ratio(fovy_degrees=120)
do_frustum_check_with_ratio(fovy_degrees=150,fov_ratio=0.5)
do_frustum_check_with_ratio(fovy_degrees=75,fov_ratio=2.0)


