#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import numpy, time, os, zmq, time, docker, asyncio
import ork.path
import threading
from orkengine.core import *
from orkengine.lev2 import *
from ork.command import run
from PIL import Image
import _shaders
import _submeshes
from signal import signal, SIGINT
from sys import exit

root_dir = ork.path.Path(os.environ["ORKID_WORKSPACE_DIR"])
pyex_dir = root_dir/"ork.lev2"/"examples"/"python"

# trying pymesh from docker...
os.chdir(root_dir)
PORT = 12345

docker_container = None
server_thread = None

################################################################################
def pymesh_server_fn():
  global docker_container
  run(["docker","build",
       "-f",pyex_dir/"pymesh_Dockerfile",
       "--build-arg","PORT=%d"%PORT,
       "-t","orkid/meshproctest",
       pyex_dir
     ])
  dclient = docker.from_env()
  print(" Starting MeshProcServer.")
  docker_container = dclient.containers.run("orkid/meshproctest",
                                            detach=True,
                                            ports={'%d/tcp'%PORT: PORT})
################################################################################
def sig_handler(signal_received, frame):
    print('SIGINT or CTRL-C detected. Exiting gracefully')
    global docker_container
    global server_thread
    docker_container.kill(9)
    server_thread.join()
    # Handle any cleanup here
    exit(0)
################################################################################
if True:
  server_thread = threading.Thread(target=pymesh_server_fn)
  server_thread.start()
else:
  pymesh_server_fn()
###################################
# setup signalhandler
###################################
signal(SIGINT, sig_handler)
###################################
# pymesh client
###################################
time.sleep(1)
zmq_context = zmq.Context()
print("Connecting to pymesh server...")
zmq_socket = zmq_context.socket(zmq.REQ)
zmq_socket.connect ("tcp://localhost:%s" % PORT)
zmq_socket.send_string("connect")
msg = zmq_socket.recv_string()
assert(msg=="connect.OK")
###################################
# get submesh
###################################
if False:
  inp_mesh = meshutil.Mesh()

  #inp_mesh.readFromWavefrontObj("./ork.data/src/actors/rijid/ref/rijid.obj")
  #inp_submesh = inp_mesh.polygroups["polySurface1"]

  inp_mesh.readFromWavefrontObj(str(ork.path.builds()/"igl"/"tutorial"/"data"/"sphere.obj"))
  inp_submesh = inp_mesh.polygroups[""]

  print(inp_mesh.polygroups)
  iglmesh = inp_submesh.toIglMesh(3)
else:
  inp_submesh = _submeshes.FrustumQuads()
  iglmesh = inp_submesh.toIglMesh(4)
###################################
# igl mesh processing
###################################
iglmesh = iglmesh.triangulated()
zmq_socket.send_pyobj({"verts":iglmesh.vertices,
                       "faces":iglmesh.faces})
#  Get the reply.
message = zmq_socket.recv_pyobj()
print(message)

iglmesh.vertices = message["newverts"]
iglmesh.faces = message["newfaces"]
#assert(False)
print("triangulated-numverts: %d"%iglmesh.vertices.shape[0])
print("triangulated-numFaces: %d"%iglmesh.faces.shape[0])
print("genus: %d"%iglmesh.genus)
print("vertexManifold: %s"%iglmesh.isVertexManifold)
print("edgeManifold: %s"%iglmesh.isEdgeManifold)
ue = iglmesh.uniqueEdges
#print("ue.ue2e: %s"%len(ue.ue2e))
#print("ue.E: %s"%ue.E)
#print("ue.uE: %s"%ue.uE)
#print("ue.EMAP: %s"%ue.EMAP)
#print("ue.count: %s"%ue.count)
#me = iglmesh.manifoldExtraction
#print("me.numpatches: %d"%me.numpatches)
#print("me.numcells: %d"%me.numcells)
#print("me.per_patch_cells: %s"%me.per_patch_cells)
#print("me.P: %s"%me.P)
iglmesh = iglmesh.reOriented() # clean up the mesh (changes topology)
iglmesh = iglmesh.cleaned() # clean up the mesh (changes topology)
print(iglmesh.vertices)
print(iglmesh.faces)
#iglmesh = iglmesh.parameterizedSCAF(1,1,0) # autogen UV's (changes topology)
#ao = iglmesh.ambientOcclusion(500)
#curvature = iglmesh.principleCurvature()
#normals = iglmesh.faceNormals()
#iglmesh.normals = normals
#iglmesh.binormals = curvature.k1
#iglmesh.tangents = curvature.k2
#iglmesh.colors = (normals*0.5+0.5) # normals to colors
#iglmesh.uvs = iglmesh.parameterizeHarmonic()*0.5+0.5
#iglmesh.uvs = iglmesh.parameterizeLCSM()*0.5+0.5

#iglmesh.colors = ao # per vertex ambient occlusion
#iglmesh.colors = curvature.k2 # surface curvature (k1, or k2)

###################################
# todo figure out why LCSM broken
###################################
#iglmesh = iglmesh.toSubMesh().toIglMesh(3)
#print(iglmesh.vertices)
#print(iglmesh.faces)
#param = iglmesh.parameterizeLCSM()
#print(param)

#print(iglmesh.faces)
#print(cleaned.faces)

#assert(False)
###################################
# generate primitive
###################################
#tsubmesh = iglmesh.toSubMesh()
#tsubmesh.writeWavefrontObj("meshproc.obj")

docker_container.kill(9)
server_thread.join()
