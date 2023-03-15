#!/usr/bin/env python3

from orkengine.core import *

a = Transform()
a.directMatrix = mtx4.lookAt(vec3(1,1,1),vec3(0,0,0),vec3(0,1,0))

print("a: ", a)
print("a.composed: " , a.composed )

#################################################

fov = 45
aspect = 1
near = .5
far = 3

print("###################")
print( "PERSPECTIVE: FOV<%g> ASPECT<%g> NEAR<%g> FAR<%g>" % (fov, aspect, near, far))
print("###################")

pmatrix = mtx4.perspective(fov,aspect,near,far)

print("###################")
pmatrix.dump("pmtx")
print("###################")

def yo(deco, vspace_v3):
  vspace_v4 = vec4(vspace_v3,1)
  hspace = vspace_v4.transform(pmatrix)
  dspace = hspace.xyz()*(1/hspace.w)
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

print("###################")
print("#### FRUSTUM")
print("###################")

frus = Frustum(mtx4(),pmatrix)
print(frus)