#!/usr/bin/env ork.python
###############################################################################
# ork.vkinfo.py — the parts of vulkaninfo you actually wanted, on one screen.
#
# Bare `vulkaninfo` is ~1500 lines of text for one GPU. This asks it for its
# JSON (the Vulkan Profiles schema — structured, typed, and stable in a way the
# text output is not) and renders only the category you asked for.
#
#   ork.vkinfo.py                      # overview: device, driver, capability digest
#   ork.vkinfo.py --mesh --task        # the amplification pair, features + limits
#   ork.vkinfo.py --stereo             # multiview, incl. the mesh-stage view count
#   ork.vkinfo.py --memory --queue     # heaps/types and queue families
#   ork.vkinfo.py --all                # everything below
#
# WHICH DRIVER ANSWERS is the trap this tool exists to make visible: on macOS
# there is usually more than one MoltenVK on the box (Homebrew's, the SDK's, and
# whatever $OBT_STAGE built), and `vulkaninfo` silently answers from whichever
# ICD the loader resolves. Every run prints the resolved ICD path, and --icd
# points it somewhere explicit:
#
#   ork.vkinfo.py --mesh --icd $OBT_STAGE/builds/moltenvk/Package/Latest/MoltenVK/dylib/macOS/MoltenVK_icd.json
#
# SOURCE NOTE: everything comes from `vulkaninfo --json` EXCEPT --memory. The
# Profiles schema vulkaninfo emits carries features, properties, limits, queue
# families, formats and extensions, but NOT VkPhysicalDeviceMemoryProperties —
# so the memory category (and only that one) parses the text output instead.
# That split is deliberate and load-bearing; do not "unify" it by moving
# everything back to text scraping.
###############################################################################

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

try:
  from rich.console import Console
  from rich.table import Table
  from rich import box
except ImportError:
  sys.stderr.write("ork.vkinfo.py: the 'rich' package is required "
                   "(it ships in the ork.python environment — run this with ork.python)\n")
  sys.exit(2)

console = Console()

###############################################################################
# categories, in display order. name -> (flag help, renderer attr)
###############################################################################

CATEGORIES = [
    ("driver",   "device identity, driver, API version"),
    ("mesh",     "VK_EXT_mesh_shader: mesh stage features + output limits"),
    ("task",     "VK_EXT_mesh_shader: task (amplification) stage features + limits"),
    ("stereo",   "multiview / stereo, including the mesh-stage view count"),
    ("vertex",   "vertex input, draw-indirect, geometry/tessellation stages"),
    ("fragment", "fragment stage: barycentrics, interlock, output attachments"),
    ("compute",  "compute workgroup limits + subgroup properties"),
    ("image",    "image dimensions, array layers, framebuffer + sample counts"),
    ("texture",  "samplers, anisotropy, compressed-format support"),
    ("memory",   "memory heaps and types (parsed from text — see header)"),
    ("queue",    "queue families, flags, counts"),
    ("limits",   "every VkPhysicalDeviceLimits entry"),
    ("ext",      "device extensions (--ext-filter to narrow)"),
]

###############################################################################
# data acquisition
###############################################################################

def _require_vulkaninfo():
  exe = shutil.which("vulkaninfo")
  if exe is None:
    console.print("[bold red]vulkaninfo not found on PATH[/bold red]")
    sys.exit(2)
  return exe


def _resolved_icd():
  """What the loader will actually answer from. Printed on every run because a
  wrong-ICD answer is indistinguishable from a wrong-driver bug."""
  for var in ("VK_DRIVER_FILES", "VK_ICD_FILENAMES"):
    val = os.environ.get(var)
    if val:
      return "%s=%s" % (var, val)
  return "<loader default — no VK_DRIVER_FILES / VK_ICD_FILENAMES set>"


def load_json(gpu):
  """`vulkaninfo --json` writes VP_VULKANINFO_<device>_<ver>.json into the CWD
  rather than stdout, so run it in a scratch dir and read what it dropped."""
  exe = _require_vulkaninfo()
  with tempfile.TemporaryDirectory() as td:
    rc = subprocess.run([exe, "--json=%d" % gpu],
                        cwd=td, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    hits = glob.glob(os.path.join(td, "VP_VULKANINFO_*.json"))
    if not hits:
      err = rc.stderr.decode("utf-8", "replace").strip()
      console.print("[bold red]vulkaninfo --json produced no profile for gpu %d[/bold red]" % gpu)
      if err:
        console.print("[dim]%s[/dim]" % err)
      sys.exit(2)
    with open(hits[0], "r") as f:
      return json.load(f)


def load_memory_text(gpu):
  """VkPhysicalDeviceMemoryProperties is absent from the JSON profile, so this
  one category comes from the text output. Returns (heaps, types)."""
  exe = _require_vulkaninfo()
  out = subprocess.run([exe], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
  lines = out.stdout.decode("utf-8", "replace").splitlines()

  # Take the gpu'th VkPhysicalDeviceMemoryProperties block.
  starts = [i for i, ln in enumerate(lines) if ln.startswith("VkPhysicalDeviceMemoryProperties")]
  if gpu >= len(starts):
    return [], []
  body = lines[starts[gpu]:]

  heaps, types, cur = [], [], None
  for ln in body[1:]:
    if ln and not ln.startswith(("\t", "=", "-")) and not ln.startswith("memory"):
      break                                   # left the block
    s = ln.strip()
    mh = re.match(r"^memoryHeaps\[(\d+)\]:$", s)
    mt = re.match(r"^memoryTypes\[(\d+)\]:$", s)
    if mh:
      cur = {"idx": int(mh.group(1)), "flags": []}
      heaps.append(cur)
      continue
    if mt:
      cur = {"idx": int(mt.group(1)), "flags": []}
      types.append(cur)
      continue
    if cur is None:
      continue
    kv = re.match(r"^(size|budget|usage|heapIndex)\s*=\s*(\d+)", s)
    if kv:
      cur[kv.group(1)] = int(kv.group(2))
    elif s.startswith("MEMORY_"):
      cur["flags"].append(s)
  return heaps, types

###############################################################################
# formatting helpers
###############################################################################

def yn(v):
  if v is True:
    return "[bold green]yes[/bold green]"
  if v is False:
    return "[red]no[/red]"
  return "[dim]—[/dim]"


def human_bytes(n):
  try:
    n = int(n)
  except (TypeError, ValueError):
    return str(n)
  for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
    if n < 1024 or unit == "TiB":
      return "%.2f %s" % (n, unit) if unit != "B" else "%d B" % n
    n /= 1024.0
  return str(n)


def fmt(v):
  if isinstance(v, bool):
    return yn(v)
  if isinstance(v, list):
    if not v:
      return "[dim]—[/dim]"
    if all(isinstance(x, (int, float)) for x in v):
      return " x ".join(str(x) for x in v)
    return ", ".join(str(x).replace("VK_", "") for x in v)
  if isinstance(v, dict):
    return ", ".join("%s=%s" % (k, v[k]) for k in v)
  return str(v)


def api_version(v):
  """JSON gives apiVersion as a packed int or an already-formatted string."""
  if isinstance(v, str):
    return v
  try:
    v = int(v)
  except (TypeError, ValueError):
    return str(v)
  return "%d.%d.%d" % ((v >> 22) & 0x7F, (v >> 12) & 0x3FF, v & 0xFFF)


def table(title, cols=("field", "value")):
  t = Table(title=title, box=box.SIMPLE_HEAD, title_style="bold cyan",
            header_style="bold magenta", title_justify="left", expand=False)
  t.add_column(cols[0], style="white", no_wrap=True)
  for c in cols[1:]:
    t.add_column(c, style="bright_white")
  return t


def emit(t, rows):
  """Render only if there is something to say — an empty table is noise."""
  if not rows:
    return False
  for r in rows:
    t.add_row(*r)
  console.print(t)
  console.print()
  return True

###############################################################################
# accessors over the profile document
###############################################################################

class Caps:
  def __init__(self, doc):
    dev = doc.get("capabilities", {}).get("device", {})
    self.features   = dev.get("features", {})
    self.properties = dev.get("properties", {})
    self.queues     = dev.get("queueFamiliesProperties", [])
    self.formats    = dev.get("formats", {})
    self.extensions = dev.get("extensions", {})
    # macos-specific block carries the portability-subset bits on MoltenVK
    self.macos      = doc.get("capabilities", {}).get("macos-specific", {})

  def feat(self, group, name):
    return self.features.get(group, {}).get(name)

  def prop(self, group, name):
    return self.properties.get(group, {}).get(name)

  def has(self, group, kind="features"):
    return group in (self.features if kind == "features" else self.properties)

  def limit(self, name):
    return self.properties.get("VkPhysicalDeviceProperties", {}).get("limits", {}).get(name)

  def rows(self, group, names, kind="features"):
    """(label, value) rows for the names present in a group; silently skips
    fields this driver/loader did not report rather than printing '—' noise."""
    src = (self.features if kind == "features" else self.properties).get(group, {})
    out = []
    for n in names:
      if n in src:
        out.append((n, fmt(src[n])))
    return out

  def limit_rows(self, names, humanize=()):
    lim = self.properties.get("VkPhysicalDeviceProperties", {}).get("limits", {})
    out = []
    for n in names:
      if n in lim:
        out.append((n, human_bytes(lim[n]) if n in humanize else fmt(lim[n])))
    return out

###############################################################################
# category renderers
###############################################################################

MESH_F = "VkPhysicalDeviceMeshShaderFeaturesEXT"
MESH_P = "VkPhysicalDeviceMeshShaderPropertiesEXT"
V11F   = "VkPhysicalDeviceVulkan11Features"
V11P   = "VkPhysicalDeviceVulkan11Properties"
V12P   = "VkPhysicalDeviceVulkan12Properties"
CORE_F = "VkPhysicalDeviceFeatures"
CORE_P = "VkPhysicalDeviceProperties"


def cat_driver(c):
  p = c.properties.get(CORE_P, {})
  rows = [
      ("deviceName", "[bold]%s[/bold]" % p.get("deviceName", "?")),
      ("deviceType", fmt(p.get("deviceType", "?"))),
      ("apiVersion", api_version(p.get("apiVersion"))),
      ("driverVersion", fmt(p.get("driverVersion"))),
      ("vendorID", hex(p["vendorID"]) if isinstance(p.get("vendorID"), int) else fmt(p.get("vendorID"))),
      ("deviceID", hex(p["deviceID"]) if isinstance(p.get("deviceID"), int) else fmt(p.get("deviceID"))),
  ]
  for n in ("driverID", "driverName", "driverInfo", "conformanceVersion"):
    v = c.prop(V12P, n)
    if v is not None:
      rows.append((n, fmt(v)))
  rows.append(("extensions", "%d device extensions" % len(c.extensions)))
  emit(table("driver / device"), rows)


def cat_mesh(c):
  if not c.has(MESH_F):
    console.print("[yellow]VK_EXT_mesh_shader not advertised by this driver — no mesh stage.[/yellow]\n")
    return
  emit(table("mesh shader — features"),
       c.rows(MESH_F, ["meshShader", "multiviewMeshShader",
                       "primitiveFragmentShadingRateMeshShader", "meshShaderQueries"]))
  emit(table("mesh shader — output limits"),
       c.rows(MESH_P, ["maxMeshWorkGroupTotalCount", "maxMeshWorkGroupCount",
                       "maxMeshWorkGroupInvocations", "maxMeshWorkGroupSize",
                       "maxMeshOutputVertices", "maxMeshOutputPrimitives",
                       "maxMeshOutputComponents", "maxMeshOutputLayers",
                       "maxMeshSharedMemorySize", "maxMeshOutputMemorySize",
                       "maxMeshPayloadAndSharedMemorySize", "maxMeshPayloadAndOutputMemorySize",
                       "maxMeshMultiviewViewCount",
                       "maxPreferredMeshWorkGroupInvocations",
                       "prefersLocalInvocationVertexOutput",
                       "prefersLocalInvocationPrimitiveOutput"], kind="properties"))


def cat_task(c):
  if not c.has(MESH_F):
    console.print("[yellow]VK_EXT_mesh_shader not advertised — no task (amplification) stage.[/yellow]\n")
    return
  ts = c.feat(MESH_F, "taskShader")
  note = ""
  if ts is False:
    note = "  [dim](mesh stage is TASKLESS on this driver)[/dim]"
  emit(table("task shader — features"), [("taskShader", yn(ts) + note)])
  emit(table("task shader — limits"),
       c.rows(MESH_P, ["maxTaskWorkGroupTotalCount", "maxTaskWorkGroupCount",
                       "maxTaskWorkGroupInvocations", "maxTaskWorkGroupSize",
                       "maxTaskPayloadSize", "maxTaskSharedMemorySize",
                       "maxTaskPayloadAndSharedMemorySize",
                       "maxPreferredTaskWorkGroupInvocations"], kind="properties"))


def cat_stereo(c):
  rows = c.rows(V11F, ["multiview", "multiviewGeometryShader", "multiviewTessellationShader"])
  rows += c.rows(V11P, ["maxMultiviewViewCount", "maxMultiviewInstanceIndex"], kind="properties")
  if c.has(MESH_F):
    rows += c.rows(MESH_F, ["multiviewMeshShader"])
    rows += c.rows(MESH_P, ["maxMeshMultiviewViewCount"], kind="properties")
  rows += c.rows(CORE_F, ["multiViewport"])
  rows += c.limit_rows(["maxViewports", "maxFramebufferLayers"])
  emit(table("stereo / multiview"), rows)


def cat_vertex(c):
  emit(table("vertex + geometry pipeline — features"),
       c.rows(CORE_F, ["geometryShader", "tessellationShader", "multiDrawIndirect",
                       "drawIndirectFirstInstance", "vertexPipelineStoresAndAtomics",
                       "shaderClipDistance", "shaderCullDistance", "depthClamp", "depthBiasClamp"]))
  emit(table("vertex — limits"),
       c.limit_rows(["maxVertexInputAttributes", "maxVertexInputBindings",
                     "maxVertexInputAttributeOffset", "maxVertexInputBindingStride",
                     "maxVertexOutputComponents", "maxDrawIndexedIndexValue",
                     "maxDrawIndirectCount", "maxGeometryOutputVertices",
                     "maxTessellationGenerationLevel", "maxTessellationPatchSize"]))


def cat_fragment(c):
  rows = c.rows(CORE_F, ["fragmentStoresAndAtomics", "sampleRateShading", "dualSrcBlend",
                         "independentBlend", "alphaToOne", "logicOp"])
  for g, n in (("VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR", "fragmentShaderBarycentric"),
               ("VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT", "fragmentShaderSampleInterlock"),
               ("VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT", "fragmentShaderPixelInterlock"),
               ("VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT", "fragmentShaderShadingRateInterlock")):
    v = c.feat(g, n)
    if v is not None:
      rows.append((n, yn(v)))
  emit(table("fragment — features"), rows)
  emit(table("fragment — limits"),
       c.limit_rows(["maxFragmentInputComponents", "maxFragmentOutputAttachments",
                     "maxFragmentDualSrcAttachments", "maxFragmentCombinedOutputResources",
                     "maxColorAttachments", "framebufferColorSampleCounts",
                     "framebufferDepthSampleCounts", "framebufferStencilSampleCounts",
                     "sampledImageColorSampleCounts", "maxSampleMaskWords"]))


def cat_compute(c):
  emit(table("compute — limits"),
       c.limit_rows(["maxComputeSharedMemorySize", "maxComputeWorkGroupCount",
                     "maxComputeWorkGroupInvocations", "maxComputeWorkGroupSize"],
                    humanize=("maxComputeSharedMemorySize",)))
  emit(table("compute — storage features"),
       c.rows(CORE_F, ["shaderStorageImageExtendedFormats", "shaderStorageImageMultisample",
                       "shaderStorageImageReadWithoutFormat", "shaderStorageImageWriteWithoutFormat",
                       "shaderStorageBufferArrayDynamicIndexing", "shaderInt64", "shaderInt16",
                       "shaderFloat64"]))
  emit(table("subgroup"),
       c.rows(V11P, ["subgroupSize", "subgroupSupportedStages", "subgroupSupportedOperations",
                     "subgroupQuadOperationsInAllStages"], kind="properties"))


def cat_image(c):
  emit(table("image — limits"),
       c.limit_rows(["maxImageDimension1D", "maxImageDimension2D", "maxImageDimension3D",
                     "maxImageDimensionCube", "maxImageArrayLayers",
                     "maxFramebufferWidth", "maxFramebufferHeight", "maxFramebufferLayers",
                     "maxPerStageDescriptorStorageImages", "maxDescriptorSetStorageImages",
                     "bufferImageGranularity"], humanize=("bufferImageGranularity",)))
  emit(table("image — features"),
       c.rows(CORE_F, ["imageCubeArray", "sparseBinding", "sparseResidencyImage2D",
                       "sparseResidencyImage3D", "shaderStorageImageMultisample"]))
  console.print("[dim]formats reported: %d (use `vulkaninfo --show-formats` for the full matrix)[/dim]\n"
                % len(c.formats))


def cat_texture(c):
  emit(table("texture / sampler — features"),
       c.rows(CORE_F, ["samplerAnisotropy", "textureCompressionBC", "textureCompressionETC2",
                       "textureCompressionASTC_LDR", "shaderSampledImageArrayDynamicIndexing",
                       "occlusionQueryPrecise"]))
  emit(table("texture / sampler — limits"),
       c.limit_rows(["maxSamplerAnisotropy", "maxSamplerLodBias", "maxSamplerAllocationCount",
                     "maxPerStageDescriptorSamplers", "maxPerStageDescriptorSampledImages",
                     "maxDescriptorSetSamplers", "maxDescriptorSetSampledImages",
                     "maxTexelBufferElements", "minTexelOffset", "maxTexelOffset"]))


def cat_memory(c, gpu):
  heaps, types = load_memory_text(gpu)
  if not heaps and not types:
    console.print("[yellow]no memory properties reported[/yellow]\n")
    return
  t = table("memory heaps", cols=("heap", "size", "budget", "usage", "flags"))
  rows = []
  for h in heaps:
    rows.append((str(h["idx"]),
                 human_bytes(h.get("size", 0)),
                 human_bytes(h["budget"]) if "budget" in h else "[dim]—[/dim]",
                 human_bytes(h["usage"]) if "usage" in h else "[dim]—[/dim]",
                 ", ".join(f.replace("MEMORY_HEAP_", "").replace("_BIT", "") for f in h["flags"]) or "[dim]—[/dim]"))
  emit(t, rows)

  t = table("memory types", cols=("type", "heap", "properties"))
  rows = []
  for m in types:
    rows.append((str(m["idx"]), str(m.get("heapIndex", "?")),
                 ", ".join(f.replace("MEMORY_PROPERTY_", "").replace("_BIT", "") for f in m["flags"]) or "[dim]—[/dim]"))
  emit(t, rows)


def cat_queue(c):
  t = table("queue families", cols=("family", "count", "flags", "timestampValidBits", "granularity"))
  rows = []
  for i, q in enumerate(c.queues):
    qp = q.get("VkQueueFamilyProperties", q)
    g = qp.get("minImageTransferGranularity", {})
    rows.append((str(i),
                 str(qp.get("queueCount", "?")),
                 ", ".join(f.replace("VK_QUEUE_", "").replace("_BIT", "") for f in qp.get("queueFlags", [])),
                 str(qp.get("timestampValidBits", "?")),
                 "%sx%sx%s" % (g.get("width", "?"), g.get("height", "?"), g.get("depth", "?"))))
  emit(t, rows)


def cat_limits(c):
  lim = c.properties.get(CORE_P, {}).get("limits", {})
  emit(table("VkPhysicalDeviceLimits (%d)" % len(lim)),
       [(k, fmt(lim[k])) for k in sorted(lim)])


def cat_ext(c, filt):
  names = sorted(c.extensions)
  if filt:
    names = [n for n in names if filt.lower() in n.lower()]
  t = table("device extensions (%d%s)" % (len(names), " matching %r" % filt if filt else ""),
            cols=("extension", "spec"))
  emit(t, [(n, str(c.extensions[n])) for n in names])

###############################################################################

RENDERERS = {
    "driver": cat_driver, "mesh": cat_mesh, "task": cat_task, "stereo": cat_stereo,
    "vertex": cat_vertex, "fragment": cat_fragment, "compute": cat_compute,
    "image": cat_image, "texture": cat_texture, "queue": cat_queue,
    "limits": cat_limits,
}


def overview(c):
  """No flags: identity plus a one-line digest of the things most likely to
  decide whether a given orkid render path can run at all."""
  cat_driver(c)
  rows = []
  mesh = c.feat(MESH_F, "meshShader")
  task = c.feat(MESH_F, "taskShader")
  rows.append(("mesh shader", yn(mesh) if mesh is not None else "[red]ext absent[/red]"))
  rows.append(("task shader (amplification)", yn(task) if task is not None else "[red]ext absent[/red]"))
  rows.append(("multiview", yn(c.feat(V11F, "multiview"))))
  mvmesh = c.feat(MESH_F, "multiviewMeshShader")
  if mvmesh is not None:
    rows.append(("multiview mesh shader", yn(mvmesh)))
  rows.append(("geometry shader", yn(c.feat(CORE_F, "geometryShader"))))
  rows.append(("tessellation shader", yn(c.feat(CORE_F, "tessellationShader"))))
  rows.append(("sampler anisotropy", yn(c.feat(CORE_F, "samplerAnisotropy"))))
  rows.append(("queue families", str(len(c.queues))))
  emit(table("capability digest"), rows)
  console.print("[dim]category flags: %s[/dim]"
                % "  ".join("--%s" % n for n, _ in CATEGORIES))


def main():
  ap = argparse.ArgumentParser(
      prog="ork.vkinfo.py",
      description="Brief, categorized Vulkan device info (vulkaninfo without the 1500 lines).",
      formatter_class=argparse.RawDescriptionHelpFormatter)
  for name, helptext in CATEGORIES:
    ap.add_argument("--%s" % name, action="store_true", help=helptext)
  ap.add_argument("--all", action="store_true", help="show every category")
  ap.add_argument("--gpu", type=int, default=0, help="gpu index for multi-GPU systems (default 0)")
  ap.add_argument("--icd", metavar="PATH",
                  help="explicit ICD json to query (sets VK_ICD_FILENAMES/VK_DRIVER_FILES)")
  ap.add_argument("--ext-filter", metavar="SUBSTR", help="with --ext, show only matching extensions")
  args = ap.parse_args()

  if args.icd:
    if not os.path.exists(args.icd):
      console.print("[bold red]--icd path does not exist: %s[/bold red]" % args.icd)
      sys.exit(2)
    os.environ["VK_ICD_FILENAMES"] = args.icd
    os.environ["VK_DRIVER_FILES"] = args.icd

  # soft_wrap: an ICD path is long and rich's default wrap breaks it mid-token
  # onto a line that reads like a separate field.
  console.print("[dim]icd: %s[/dim]" % _resolved_icd(), soft_wrap=True)
  console.print()

  doc = load_json(args.gpu)
  caps = Caps(doc)

  wanted = [n for n, _ in CATEGORIES if getattr(args, n.replace("-", "_"))]
  if args.all:
    wanted = [n for n, _ in CATEGORIES]

  if not wanted:
    overview(caps)
    return

  for name in wanted:
    if name == "memory":
      cat_memory(caps, args.gpu)
    elif name == "ext":
      cat_ext(caps, args.ext_filter)
    else:
      RENDERERS[name](caps)


if __name__ == "__main__":
  main()
