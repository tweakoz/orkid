#!/usr/bin/env python3
import pymesh
import os, sys, traceback
import zmq
import time

PORT = int(os.environ["PORT"])
context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind("tcp://*:%s" % PORT)

message = socket.recv_string()
assert(message=="connect")
socket.send_string("connect.OK")

while True:
    message = socket.recv_pyobj()

    verts = message["verts"]
    faces = message["faces"]

    mesh = pymesh.form_mesh(verts, faces)
    mesh.enable_connectivity();

    try:
      new_mesh, info = pymesh.remove_duplicated_faces(mesh)
      socket.send_pyobj({"newverts":new_mesh.vertices,
                         "newfaces":new_mesh.faces,
                         "info": info})
    except:
      socket.send_pyobj({"server.error":sys.exc_info()[0],
                         "backtrace":traceback.format_exc()})
