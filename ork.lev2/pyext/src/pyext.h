////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/python/pycodec.inl>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>
#include <ork/object/Object.h>
#include <ork/file/fileenv.h>
#include <ork/file/filedevcontext.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/primitives.inl>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/lev2_types.h>
#include <ork/math/TransformNode.h>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::lev2;

namespace ork::lev2 {

using ctx_t               = ork::python::unmanaged_ptr<Context>;
using dwi_t               = ork::python::unmanaged_ptr<DrawingInterface>;
using fbi_t               = ork::python::unmanaged_ptr<FrameBufferInterface>;
using gbi_t               = ork::python::unmanaged_ptr<GeometryBufferInterface>;
using fxi_t               = ork::python::unmanaged_ptr<FxInterface>;
using txi_t               = ork::python::unmanaged_ptr<TextureInterface>;
using ci_t                = ork::python::unmanaged_ptr<ComputeInterface>;
using capbuf_t            = ork::python::unmanaged_ptr<CaptureBuffer>;
using pyfxshader_ptr_t    = ork::python::unmanaged_ptr<FxShader>;
using pyfxcomputeshader_ptr_t    = ork::python::unmanaged_const_ptr<FxComputeShader>;
using pyfxparam_ptr_t     = ork::python::unmanaged_const_ptr<FxShaderParam>;
using pyfxuniblk_ptr_t     = ork::python::unmanaged_const_ptr<FxUniformBlock>;
using pyfxstorage_ptr_t     = ork::python::unmanaged_const_ptr<FxShaderStorageBlock>;
using pyfxtechnique_ptr_t = ork::python::unmanaged_const_ptr<FxShaderTechnique>;
using fxparammap_t        = std::map<std::string, pyfxparam_ptr_t>;
using fxtechniquemap_t    = std::map<std::string, pyfxtechnique_ptr_t>;
using vtxa_t              = SVtxV12N12B12T8C4;
using vb_static_vtxa_t    = StaticVertexBuffer<vtxa_t>;
using vw_vtxa_t           = VtxWriter<vtxa_t>;
using cstrref_t           = const std::string&;
using rcfd_t              = RenderContextFrameData;
using decxf_t             = ork::decompxf_ptr_t;
using idxbuf_rawptr_t     = ork::python::unmanaged_ptr<IndexBufferBase>;
using fxshaderstoragebuffer_ptr_t = ork::python::unmanaged_ptr<FxShaderStorageBuffer>;
using compositordrawdata_ptr_t = ork::python::unmanaged_ptr<CompositorDrawData>;
using viewdata_ptr_t           = std::shared_ptr<ViewData>;
using pyfn_ptr_t = std::shared_ptr<py::function>;


} // namespace ork::lev2
