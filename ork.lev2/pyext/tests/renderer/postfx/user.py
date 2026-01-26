#!/usr/bin/env ork.python

################################################################################
# User PostFx Node Example - Custom Post-Processing Effects
#
# This example demonstrates how to create custom post-processing effects using
# PostFxNodeUser nodes in a compositor chain. It showcases:
#
# 1. **Double-Buffered Feedback Effects**: Using texture_provider for temporal
#    effects that reference previous frames safely in Vulkan.
#
# 2. **Custom Shader Integration**: Loading custom GLSL/SPIR-V shaders via
#    the .fxv2 format for post-processing.
#
# 3. **Compositor Chain Architecture**: Building a multi-stage post-processing
#    pipeline where effects are applied sequentially.
#
# Architecture:
#   Scene Render → C0 (Radial Distortion) → C1 (Feedback) → Display
#                       ↑                          ↓
#                       └──── Feedback Loop ───────┘
#
# Key Concepts:
# - PostFxNodeUser: User-defined post-processing nodes with custom shaders
# - double_buffer: Ping-pong rendering for safe feedback texture sampling
# - texture_provider: Deferred texture evaluation for correct Vulkan layouts
# - flip_vertical: Coordinate system adjustment for Vulkan compatibility
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from obt import path
from pathlib import Path
from orkengine.core import vec3, vec4, mtx4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

tokens = CrcStringProxy()
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))

################################################################################

class USERPOSTNODEAPP(ComponentizedApplication):
  """
  Demonstrates custom post-processing effects with temporal feedback.

  This application creates a scene with two post-processing nodes:
  1. Radial Distortion: Time-animated radial displacement effect
  2. Feedback: Temporal blending with previous frame for motion trails

  The feedback node uses double buffering to safely sample from the previous
  frame's output, avoiding Vulkan image layout conflicts.
  """

  def __init__(self):
    super().__init__()

    ############################################################################
    # POST-PROCESSING NODE 0: Radial Distortion Effect
    ############################################################################
    # Creates animated radial displacement based on polar coordinates.
    # This node doesn't require double buffering since it doesn't use feedback.
    radial = lev2.PostFxNodeUser()
    radial.shader_path = str(this_dir / "usertest.fxv2")
    radial.technique = "tek_radial_distort"  # Uses ps_radial fragment shader

    # Shader parameters (uploaded to uniform blocks)
    radial.params.mvp = mtx4()                # Model-View-Projection matrix
    radial.params.modcolor = vec4(1,0,0,1)    # Modulation color (unused in this shader)
    radial.params.frg_time = 0.0              # Animated time value (updated per frame)

    # Vulkan compatibility: flip texture coordinates vertically
    # (Vulkan Y-axis points down in NDC, OpenGL points up)
    radial.flip_vertical = True
    self.pfx_radial = radial

    ############################################################################
    # POST-PROCESSING NODE 1: Temporal Feedback Effect
    ############################################################################
    # Blends current frame with previous frame for motion trail/echo effect.
    # REQUIRES double buffering to avoid Vulkan layout conflicts.
    feedback = lev2.PostFxNodeUser()
    feedback.shader_path = str(this_dir / "usertest.fxv2")
    feedback.technique = "tek_feedback"  # Uses ps_feedback fragment shader

    # CRITICAL: Enable double buffering for safe feedback texture sampling
    # Without this, the texture would be in GENERAL layout instead of
    # SHADER_READ_ONLY_OPTIMAL, causing Vulkan validation errors.
    feedback.double_buffer = True

    # Vulkan compatibility
    feedback.flip_vertical = True

    feedback.params.mvp = mtx4()
    self.pfx_feedback = feedback

    ############################################################################
    # Scene Graph Component Setup
    ############################################################################
    # Creates a standard scene graph with the post-processing chain.
    # Rendering order: Scene → radial (C0) → feedback (C1) → Display
    self.SGC = self.addComponent("SGC",
                                 StandardSceneGraphComponent,
                                 grid_variant=None,
                                 eye = vec3(5,1,5),
                                 post_nodes = [radial,feedback])  # Order matters!
    self.createEzApp(name="UserPostFxNode", fullscreen=False,
                      use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ############################################################################
  # GPU Initialization
  ############################################################################
  # Called once when the graphics context becomes available.
  # This is where we:
  # 1. Load GPU resources (models, textures)
  # 2. Set up scene graph nodes
  # 3. Connect post-processing feedback loops
  ############################################################################
  def _onGpuInit(self,ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    #######################################
    # Load 3D Model
    #######################################
    # Load a glTF model to render in the scene (provides visual input
    # for the post-processing effects to operate on)
    self.model = lev2.XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0,0,0)

    #######################################
    # Set Up Feedback Loop
    #######################################
    # Get references to compositor post nodes (C0 = radial, C1 = feedback)
    C0 = SG.compositorpostnode(0)  # radial node
    C1 = SG.compositorpostnode(1)  # feedback node

    # CRITICAL: Use texture_provider instead of direct texture access!
    #
    # texture_provider provides deferred texture evaluation, ensuring that:
    # 1. The texture is sampled from the correct double-buffered frame
    # 2. The texture has been transitioned to SHADER_READ_ONLY_OPTIMAL layout
    # 3. We avoid race conditions in Vulkan
    #
    # Both nodes sample from C1's output, creating a feedback loop where
    # the feedback node's previous frame influences the current frame.
    self.pfx_feedback.params.FeedbackMap = C1.texture_provider
    self.pfx_radial.params.FeedbackMap = C1.texture_provider

  ############################################################################
  # Per-Frame Update
  ############################################################################
  # Called every frame to update time-varying parameters.
  ############################################################################
  def _onUpdate(self,updinfo):
    time = updinfo.absolutetime

    # Update animated time parameter for radial distortion
    # The distortion amount oscillates based on sin(time*10)
    self.pfx_radial.params.frg_time = time*1.0
    
###############################################################################

app = USERPOSTNODEAPP()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
