#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a molecular dataset
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal, numpy
from obt import path
import MDAnalysis as mda
from MDAnalysisData import datasets

from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import lev2_pyexdir
from orkengine.core import CrcStringProxy
from orkengine import lev2

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph, createParams

tokens = CrcStringProxy()

################################################################################
parser = argparse.ArgumentParser(description='scenegraph skinning example')
args = vars(parser.parse_args())
################################################################################

def fetchMolecularData():
  # fetch "enzyme Adenylate Kinase in equilibrium without water"
  adk = datasets.fetch_adk_equilibrium()
  u = mda.Universe(adk.topology, adk.trajectory)

  element_colors = {
      'H': [1.0, 1.0, 1.0],  # White
      'C': [0.0, 0.0, 0.0],  # Black
      'N': [0.0, 0.0, 1.0],  # Blue
      'O': [1.0, 0.0, 0.0],  # Red
      'S': [1.0, 1.0, 0.0],  # Yellow
      # Add more elements as needed
  }

  def infer_element(atom_name):
      # Typically, the element is the first character in the atom name
      return atom_name[0]

  # Extract atom positions
  atoms = u.atoms
  positions = atoms.positions
  names = atoms.names  # Use atom names to infer elements

  # Optionally, get radii (if available, otherwise you might want to set a default radius)
  radii = atoms.radius if hasattr(atoms, 'radius') else [1.5] * len(atoms)  # Example default radius

  # Create list of spheres with colors
  spheres = []
  for pos, radius, name in zip(positions, radii, names):
      element = infer_element(name)
      color = element_colors.get(element, [0.5, 0.5, 0.5])  # Default to grey if element not in color map
      sphere = {"position": pos.tolist(), "radius": radius, "color": color}
      spheres.append(sphere)
  return spheres 
     
################################################################################

class MoleculeApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, left=100, top=100, width=960, height=480, ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera( app=self, 
                   eye = vec3(0,0,30), 
                   constrainZ=True, 
                   up=vec3(0,1,0),
                   fov_deg = 110 )

  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    sg_params = createParams()
    sg_params.SkyboxIntensity = 2.0
    #sg_params.SkyboxTexPathStr = "src://envmaps/blender_studio.dds"
    sg_params.preset = "ForwardPBR"

    self.scenegraph = self.ezapp.createScene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")
    self.pbr_common = self.scenegraph.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    self.pbr_common.useDepthPrepass = True

    model= lev2.XgmModel("data://tests/pbr_calib.glb")

    ###################################
    # replace material with white
    ###################################

    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.assignImages(
          ctx,
          color = white,
          normal = normal,
          mtlruf = white,
          doConform=True
        )
        copy.baseColor = vec4(1,1,1,1)
        copy.roughnessFactor = 1.0
        copy.metallicFactor = 0.25
        copy.gpuInit(ctx)
        submesh.material = copy

    ###################################
    # fetch molecular data
    ###################################

    spheres = fetchMolecularData()
    num_instances = len(spheres)

    ###################################
    # create instanced spheres node
    ###################################

    self.sgnode = model.createInstancedNode(num_instances,"node1",self.layer)

    ###################################
    # create instances
    ###################################

    i = 0
    for sphere in fetchMolecularData():
      #print(sphere)
      inp_pos = sphere["position"]
      pos = vec3(inp_pos[0], inp_pos[1], inp_pos[2])
      color = sphere["color"]
      v4color = vec4(color[0], color[1], color[2],1.0)
      radius = sphere["radius"]
      #print("pos<%s> color<%s> radius<%s>"%(pos,color,radius))
      m = mtx4()
      m.setColumn(3,vec4(pos,1))
      m.setColumn(0,vec4(radius,0,0,0))
      m.setColumn(1,vec4(0,radius,0,0))
      m.setColumn(2,vec4(0,0,radius,0))
      self.sgnode.setInstanceMatrix(i,m)
      self.sgnode.setInstanceColor(i,v4color)
      i = i + 1

  ################################################

  def onUpdate(self,updinfo):
    self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    res = lev2.ui.HandlerResult()
    handled = False
    if not handled:
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.camera.copyFrom( self.uicam.cameradata )
    return res

###############################################################################

MoleculeApp().ezapp.mainThreadLoop()

