#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
# Test showing RigidPrimitive with different primitive types:
# POINTS, LINES, TRIANGLES (flat unlit) and TRIANGLESTRIP (Gouraud shaded sphere)
################################################################################

import math, signal, sys
import numpy as np
from orkengine.core import vec3, vec4, mtx4, CrcStringProxy
from orkengine.lev2 import *

sys.path.append(lev2exdir().as_string + "/python")
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph
from lev2utils.primitives import createGridData

tokens = CrcStringProxy()

################################################################################
# Flat unlit shader - solid color, no lighting
################################################################################

FLAT_SHADER = """
fxconfig fxcfg_default { glsl_version = "330"; }

uniform_set ublock_vtx {
  mat4 mvp;
}

uniform_set ublock_frg {
  vec4 modcolor;
}

vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 clr : COLOR0;
  }
  outputs {
    vec3 frg_clr;
  }
}

fragment_interface iface_frg : ublock_frg {
  inputs {
    vec3 frg_clr;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}

vertex_shader vs_flat : iface_vtx {
  gl_Position = mvp * pos;
  gl_PointSize = 10.0;
  frg_clr = clr.rgb;
}

fragment_shader fs_flat : iface_frg {
  out_clr = vec4(frg_clr, 1.0);
}

technique tek_flat {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_flat;
    fragment_shader = fs_flat;
    state_block     = default;
  }
}
"""

################################################################################
# Gouraud shader - smooth shading with vertex colors and lighting
################################################################################

GOURAUD_SHADER = """
fxconfig fxcfg_default { glsl_version = "330"; }

uniform_set ublock_vtx {
  mat4 mvp;
  mat4 m;
  float time;
}

uniform_set ublock_frg {
  vec4 modcolor;
}

vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 nrm : NORMAL;
    vec4 clr : COLOR0;
  }
  outputs {
    vec4 frg_clr;
  }
}

fragment_interface iface_frg : ublock_frg {
  inputs {
    vec4 frg_clr;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}

vertex_shader vs_gouraud : iface_vtx {
  gl_Position = mvp * pos;

  // World space normal
  vec3 N = normalize((m * vec4(nrm.xyz, 0)).xyz);

  // Animated light direction
  float angle = time * 0.5;
  vec3 lightDir = normalize(vec3(cos(angle), 0.7, sin(angle)));

  // Gouraud shading - compute lighting per vertex
  float ambient = 0.15;
  float diffuse = max(0.0, dot(N, lightDir));
  float lighting = ambient + diffuse * 0.85;

  // Use vertex color with lighting
  frg_clr = vec4(clr.rgb * lighting, 1.0);
}

fragment_shader fs_gouraud : iface_frg {
  out_clr = frg_clr;
}

technique tek_gouraud {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_gouraud;
    fragment_shader = fs_gouraud;
    state_block     = default;
  }
}
"""

################################################################################
# Create a simple cube mesh data (shared vertices)
################################################################################

def create_cube_data(size=1.0):
    """Create cube vertices and faces (shared vertices)"""
    s = size / 2.0

    vertices = [
        vec3(-s, -s, -s), vec3( s, -s, -s), vec3( s,  s, -s), vec3(-s,  s, -s),
        vec3(-s, -s,  s), vec3( s, -s,  s), vec3( s,  s,  s), vec3(-s,  s,  s),
    ]

    faces = [
        3, 0, 1, 2,  3, 0, 2, 3,  # Front
        3, 4, 6, 5,  3, 4, 7, 6,  # Back
        3, 0, 3, 7,  3, 0, 7, 4,  # Left
        3, 1, 5, 6,  3, 1, 6, 2,  # Right
        3, 3, 2, 6,  3, 3, 6, 7,  # Top
        3, 0, 4, 5,  3, 0, 5, 1,  # Bottom
    ]

    return vertices, faces

################################################################################

def create_cube_edges(size=1.0):
    """Create cube vertices and edges (12 edges, no diagonals)"""
    s = size / 2.0

    vertices = [
        vec3(-s, -s, -s), vec3( s, -s, -s), vec3( s,  s, -s), vec3(-s,  s, -s),
        vec3(-s, -s,  s), vec3( s, -s,  s), vec3( s,  s,  s), vec3(-s,  s,  s),
    ]

    # 12 edges of the cube (2 vertices per edge)
    edges = [
        2, 0, 1,  2, 1, 2,  2, 2, 3,  2, 3, 0,  # Front face
        2, 4, 5,  2, 5, 6,  2, 6, 7,  2, 7, 4,  # Back face
        2, 0, 4,  2, 1, 5,  2, 2, 6,  2, 3, 7,  # Connecting edges
    ]

    return vertices, edges

################################################################################
# Create cube with non-shared vertices and per-triangle colors
################################################################################

def create_colored_cube_data(size=1.0):
    """Create cube with unique vertices per triangle and high-contrast colors"""
    s = size / 2.0

    # Corner positions
    corners = [
        vec3(-s, -s, -s),  # 0
        vec3( s, -s, -s),  # 1
        vec3( s,  s, -s),  # 2
        vec3(-s,  s, -s),  # 3
        vec3(-s, -s,  s),  # 4
        vec3( s, -s,  s),  # 5
        vec3( s,  s,  s),  # 6
        vec3(-s,  s,  s),  # 7
    ]

    # 12 triangles (2 per face), each with indices into corners (CCW winding)
    tri_indices = [
        (0, 2, 1), (0, 3, 2),  # Front (-Z)
        (4, 5, 6), (4, 6, 7),  # Back (+Z)
        (0, 7, 3), (0, 4, 7),  # Left (-X)
        (1, 6, 5), (1, 2, 6),  # Right (+X)
        (3, 6, 2), (3, 7, 6),  # Top (+Y)
        (0, 5, 4), (0, 1, 5),  # Bottom (-Y)
    ]

    # High contrast colors for each triangle
    tri_colors = [
        vec3(1.0, 0.0, 0.0),  # Red
        vec3(0.0, 1.0, 0.0),  # Green
        vec3(0.0, 0.0, 1.0),  # Blue
        vec3(1.0, 1.0, 0.0),  # Yellow
        vec3(1.0, 0.0, 1.0),  # Magenta
        vec3(0.0, 1.0, 1.0),  # Cyan
        vec3(1.0, 0.5, 0.0),  # Orange
        vec3(0.5, 0.0, 1.0),  # Purple
        vec3(0.0, 1.0, 0.5),  # Teal
        vec3(1.0, 0.5, 0.5),  # Salmon
        vec3(0.5, 1.0, 0.5),  # Light green
        vec3(0.5, 0.5, 1.0),  # Light blue
    ]

    # Build non-shared vertices (3 per triangle)
    vertices = []
    colors = []
    faces = []

    for i, (i0, i1, i2) in enumerate(tri_indices):
        base_idx = len(vertices)
        vertices.append(corners[i0])
        vertices.append(corners[i1])
        vertices.append(corners[i2])
        # Same color for all 3 vertices of this triangle
        colors.append(tri_colors[i])
        colors.append(tri_colors[i])
        colors.append(tri_colors[i])
        # Face indices
        faces.extend([3, base_idx, base_idx + 1, base_idx + 2])

    return vertices, colors, faces

################################################################################
# Create sphere as triangle strip (latitude bands)
################################################################################

def create_sphere_strip_data(radius=1.0, lat_segments=24, lon_segments=32):
    """Create sphere vertices in triangle strip order with rainbow vertex colors"""
    vertices = []
    normals = []
    colors = []

    # Generate vertices for triangle strip - alternating between two latitude rows
    for lat in range(lat_segments):
        lat0 = math.pi * lat / lat_segments
        lat1 = math.pi * (lat + 1) / lat_segments

        y0 = math.cos(lat0)
        y1 = math.cos(lat1)
        r0 = math.sin(lat0)
        r1 = math.sin(lat1)

        for lon in range(lon_segments + 1):
            phi = 2.0 * math.pi * lon / lon_segments

            x = math.cos(phi)
            z = math.sin(phi)

            # First vertex (upper latitude)
            p0 = vec3(x * r0 * radius, y0 * radius, z * r0 * radius)
            n0 = vec3(x * r0, y0, z * r0).normalized
            vertices.append(p0)
            normals.append(n0)

            # Rainbow color based on position
            hue = (lat / lat_segments + lon / lon_segments) * 0.5
            r = abs(math.sin(hue * math.pi * 2)) * 0.7 + 0.3
            g = abs(math.sin((hue + 0.33) * math.pi * 2)) * 0.7 + 0.3
            b = abs(math.sin((hue + 0.66) * math.pi * 2)) * 0.7 + 0.3
            colors.append(vec3(r, g, b))

            # Second vertex (lower latitude)
            p1 = vec3(x * r1 * radius, y1 * radius, z * r1 * radius)
            n1 = vec3(x * r1, y1, z * r1).normalized
            vertices.append(p1)
            normals.append(n1)

            # Slightly shifted color for visual interest
            hue2 = hue + 0.1
            r = abs(math.sin(hue2 * math.pi * 2)) * 0.7 + 0.3
            g = abs(math.sin((hue2 + 0.33) * math.pi * 2)) * 0.7 + 0.3
            b = abs(math.sin((hue2 + 0.66) * math.pi * 2)) * 0.7 + 0.3
            colors.append(vec3(r, g, b))

    # For MicroMesh we need dummy faces (not used for TRIANGLESTRIP)
    faces = []

    return vertices, normals, colors, faces

################################################################################

class PrimitiveTypesApp:
    def __init__(self):
        super().__init__()
        self.ezapp = OrkEzApp.create(self, height=720, width=1280)
        self.ezapp.setRefreshPolicy(RefreshFastest, 0)
        setupUiCamera(app=self, eye=vec3(10, 8, 10), tgt=vec3(0, 0, 0))

        signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
        self.time = 0.0

    def onGpuInit(self, ctx):
        self.context = ctx
        createSceneGraph(app=self)

        # Create grid
        self.grid_data = createGridData()
        self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)

        # Create FLAT pipeline for points/lines/triangles
        flat_mtl = FreestyleMaterial()
        flat_mtl.gpuInitFromShaderText(ctx, "flat_shader", FLAT_SHADER)
        flat_mtl.rasterstate.culltest = tokens.PASS_FRONT
        flat_mtl.rasterstate.depthtest = tokens.LEQUALS

        flat_permu = FxPipelinePermutation(rendermodel="ForwardPBR")
        flat_permu.technique = flat_mtl.shader.technique("tek_flat")

        self.flat_pipeline = flat_mtl.fxcache.findPipeline(flat_permu)
        self.flat_pipeline.bindParam(flat_mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
        self.flat_pipeline.bindParam(flat_mtl.param("modcolor"), tokens.RCFD_MODCOLOR)
        self.flat_pipeline.sharedMaterial = flat_mtl

        # Create GOURAUD pipeline for sphere strip
        gouraud_mtl = FreestyleMaterial()
        gouraud_mtl.gpuInitFromShaderText(ctx, "gouraud_shader", GOURAUD_SHADER)
        gouraud_mtl.rasterstate.culltest = tokens.PASS_FRONT
        gouraud_mtl.rasterstate.depthtest = tokens.LEQUALS

        gouraud_permu = FxPipelinePermutation(rendermodel="ForwardPBR")
        gouraud_permu.technique = gouraud_mtl.shader.technique("tek_gouraud")

        self.gouraud_pipeline = gouraud_mtl.fxcache.findPipeline(gouraud_permu)
        self.gouraud_pipeline.bindParam(gouraud_mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
        self.gouraud_pipeline.bindParam(gouraud_mtl.param("m"), tokens.RCFD_M)
        self.gouraud_pipeline.bindParam(gouraud_mtl.param("modcolor"), tokens.RCFD_MODCOLOR)
        self.time_param = gouraud_mtl.param("time")
        self.gouraud_pipeline.bindParam(self.time_param, float(0.0))
        self.gouraud_pipeline.sharedMaterial = gouraud_mtl

        # Get cube data
        verts, faces = create_cube_data(size=1.5)

        # TRIANGLES - Per-triangle colored cube (left back)
        colored_verts, colored_colors, colored_faces = create_colored_cube_data(size=1.5)
        self.tri_prim = RigidPrimitive()
        tri_mesh = MicroMesh.fromVertAndFaceLists(colored_verts, colored_faces)
        tri_mesh.computeNormals()
        colors_np = np.array([[c.x, c.y, c.z] for c in colored_colors], dtype=np.float32)
        tri_mesh.updateColors(colors_np)
        self.tri_prim.updateWithMicroMesh(tri_mesh, ctx, tokens.TRIANGLES)
        self.tri_node = self.tri_prim.createNode("triangles", self.layer1, self.flat_pipeline)
        self.tri_node.worldTransform.translation = vec3(-3, 1, -3)
        self.tri_node.modcolor = vec4(1.0, 1.0, 1.0, 1)  # White (use vertex colors)

        # LINES - Bright green wireframe cube (right back, no diagonals)
        line_verts, line_edges = create_cube_edges(size=1.5)
        self.line_prim = RigidPrimitive()
        line_mesh = MicroMesh.fromVertAndFaceLists(line_verts, line_edges)
        self.line_prim.updateWithMicroMesh(line_mesh, ctx, tokens.LINES)
        self.line_node = self.line_prim.createNode("lines", self.layer1, self.flat_pipeline)
        self.line_node.worldTransform.translation = vec3(3, 1, -3)
        self.line_node.modcolor = vec4(0.2, 1.0, 0.2, 1)  # Green

        # POINTS - Bright cyan points cube (left front)
        self.pts_prim = RigidPrimitive()
        pts_mesh = MicroMesh.fromVertAndFaceLists(verts, faces)
        pts_mesh.computeNormals()
        self.pts_prim.updateWithMicroMesh(pts_mesh, ctx, tokens.POINTS)
        self.pts_node = self.pts_prim.createNode("points", self.layer1, self.flat_pipeline)
        self.pts_node.worldTransform.translation = vec3(-3, 1, 3)
        self.pts_node.modcolor = vec4(0.2, 1.0, 1.0, 1)  # Cyan

        # TRIANGLESTRIP - Rainbow Gouraud sphere (right front, larger)
        sphere_verts, sphere_normals, sphere_colors, sphere_faces = create_sphere_strip_data(radius=2.0)

        self.strip_prim = RigidPrimitive()
        strip_mesh = MicroMesh.fromVertAndFaceLists(sphere_verts, sphere_faces)

        # Set normals for Gouraud shading
        normals_np = np.array([[n.x, n.y, n.z] for n in sphere_normals], dtype=np.float32)
        strip_mesh.updateNormals(normals_np)

        self.strip_prim.updateWithMicroMesh(strip_mesh, ctx, tokens.TRIANGLESTRIP)
        self.strip_node = self.strip_prim.createNode("tristrip", self.layer1, self.gouraud_pipeline)
        self.strip_node.worldTransform.translation = vec3(3, 2, 3)
        self.strip_node.modcolor = vec4(1, 1, 1, 1)

        print("Primitive Types Demo:")
        print("  Back-Left  (multi):  TRIANGLES - per-triangle colored cube")
        print("  Back-Right (green):  LINES - wireframe cube")
        print("  Front-Left (cyan):   POINTS - point cloud cube")
        print("  Front-Right (white): TRIANGLESTRIP - Gouraud sphere")

        self.scene.lightingmanager.gpuInit(ctx)

    def onGpuUpdate(self, ctx):
        # Update time for animated lighting on sphere
        self.gouraud_pipeline.bindParam(self.time_param, float(self.time))

    def onUiEvent(self, uievent):
        handled = self.uicam.uiEventHandler(uievent)
        if handled:
            self.camera.copyFrom(self.uicam.cameradata)
        return ui.HandlerResult()

    def onUpdate(self, updinfo):
        self.time = updinfo.absolutetime
        self.scene.updateScene(self.cameralut)

################################################################################

if __name__ == "__main__":
    app = PrimitiveTypesApp()
    app.ezapp.mainThreadLoop()
