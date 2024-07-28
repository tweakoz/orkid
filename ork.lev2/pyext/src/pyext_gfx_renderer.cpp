////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_renderer(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto rcfd_type_t = py::class_<RenderContextFrameData, rcfd_ptr_t>(
                         module_lev2,                                 //
                         "RenderContextFrameData")                    //
                         .def(py::init([](ctx_t& ctx) -> rcfd_ptr_t { //
                           return std::make_shared<RenderContextFrameData>(ctx.get());
                         }))
                         .def_property_readonly(
                             "topCompositor", [](rcfd_ptr_t the_rcfd) -> compositorimpl_ptr_t { return the_rcfd->topCompositor(); })
                         .def(
                             "pushCompositor",
                             [](rcfd_ptr_t the_rcfd, compositorimpl_ptr_t cimpl) { //
                               the_rcfd->pushCompositor(cimpl);
                             })
                         .def(
                             "popCompositor",
                             [](rcfd_ptr_t the_rcfd) { //
                               the_rcfd->popCompositor();
                             })
                         .def(
                             "setRenderingModel",
                             [](rcfd_ptr_t the_rcfd, std::string rendermodel) { //
                               auto as_crc               = CrcString(rendermodel.c_str());
                               the_rcfd->_renderingmodel = (uint32_t)as_crc._hashed;
                             })
                         .def("setUserProperty", [](rcfd_ptr_t the_rcfd, uint32_t crc, py::object obj) { //
                           // rcfd->setUserProperty("vrcam"_crc, (const CameraData*) gpurec->_camdata.get() );
                         });
  type_codec->registerStdCodec<rcfd_ptr_t>(rcfd_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto rcid_type_t = py::class_<RenderContextInstData, rcid_ptr_t>(
                         module_lev2,                                          //
                         "RenderContextInstData")                              //
                         .def(py::init([](rcfd_ptr_t the_rcfd) -> rcid_ptr_t { //
                           return RenderContextInstData::create(the_rcfd);
                         }))
                         .def(
                             "pipeline",                                                       //
                             [](rcid_ptr_t the_rcid, material_ptr_t mtl) -> fxpipeline_ptr_t { //
                               auto cache = mtl->pipelineCache();
                               return cache->findPipeline(*the_rcid);
                             })
                         .def(
                             "genMatrix",                                 //
                             [](rcid_ptr_t the_rcid, py::object method) { //
                               auto py_callback     = method.cast<pybind11::object>();
                               the_rcid->_genMatrix = [py_callback]() -> fmtx4 {
                                 py::gil_scoped_acquire acquire;
                                 py::object mtx_attempt = py_callback();
                                 printf("YAY..\n");
                                 return mtx_attempt.cast<fmtx4>();
                               };
                             })
                         .def(
                             "forceTechnique",                                  //
                             [](rcid_ptr_t the_rcid, pyfxtechnique_ptr_t tek) { //
                               the_rcid->forceTechnique(tek.get());
                             });
  /*.def_property("fxcache",
      [](rcid_ptr_t the_rcid) -> fxpipelinecache_constptr_t { //
        return the_rcid->_pipeline_cache;
      },
      [](rcid_ptr_t the_rcid, fxpipelinecache_constptr_t cache) { //
        the_rcid->_pipeline_cache = cache;
      }
  )*/
  type_codec->registerStdCodec<rcid_ptr_t>(rcid_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto camdattype = //
      py::class_<CameraData, cameradata_ptr_t>(module_lev2, "CameraData")
          .def(py::init([] -> cameradata_ptr_t { return std::make_shared<CameraData>(); }))
          .def(
              "perspective",                                                    //
              [](cameradata_ptr_t camera, float near, float ffar, float fovy) { //
                camera->Persp(near, ffar, fovy);
              })
          .def(
              "lookAt",                                                        //
              [](cameradata_ptr_t camera, fvec3& eye, fvec3& tgt, fvec3& up) { //
                camera->Lookat(eye, tgt, up);
              })
          .def(
              "fromPoseMatrix",                            //
              [](cameradata_ptr_t camera, fmtx4 posemtx) { //
                camera->fromPoseMatrix(posemtx);
              })
          .def(
              "copyFrom",                                                //
              [](cameradata_ptr_t camera, cameradata_ptr_t src_camera) { //
                //*camera = *src_camera;
                camera->mEye    = src_camera->mEye;
                camera->mTarget = src_camera->mTarget;
                camera->mUp     = src_camera->mUp;
                camera->_xnormal = src_camera->_xnormal;
                camera->_ynormal = src_camera->_ynormal;
                camera->_znormal = src_camera->_znormal;
                camera->_left = src_camera->_left;
                camera->_right = src_camera->_right;
                camera->_top = src_camera->_top;
                camera->_bottom = src_camera->_bottom;
                camera->mAper = src_camera->mAper;
                camera->mHorizAper = src_camera->mHorizAper;
                camera->mNear = src_camera->mNear;
                camera->mFar = src_camera->mFar;
              })
          .def(
              "projectDepthRay",                                              //
              [](cameradata_ptr_t camera, fvec2 pos2d, float aspect) -> fray3 { //
                auto cammat = camera->computeMatrices(aspect);
                fray3 rval;
                cammat.projectDepthRay(pos2d, rval);
                return rval;
              })
          .def("computeMatrices", [](cameradata_ptr_t camera, float aspect) -> CameraMatrices { //
            return camera->computeMatrices(aspect);
          })
          .def("pixelLengthVectors", [](cameradata_ptr_t camera, fvec3 inpos, fvec2 vp) -> py::list { //
              float aspect = vp.x/vp.y;
              auto cammat = camera->computeMatrices(aspect);
              fvec3 out_x, out_y;
              cammat.GetPixelLengthVectors(inpos, vp, out_x, out_y);
              py::list rval;
              rval.append(out_x);
              rval.append(out_y);
              return rval;
          })
          .def("vMatrix", [](cameradata_ptr_t camera) -> fmtx4 { //
              return camera->computeViewMatrix();
          })
          .def("pMatrix", [](cameradata_ptr_t camera, float aspect) -> fmtx4 { //
              auto matrices = camera->computeMatrices(aspect);
              return matrices._pmatrix;
          })
          .def("vpMatrix", [](cameradata_ptr_t camera, float aspect) -> fmtx4 { //
              auto matrices = camera->computeMatrices(aspect);
              return matrices._vpmatrix;
          })
          .def("project", [](cameradata_ptr_t camera, float aspect, fvec3 wpos) -> fvec3 { //
              auto matrices = camera->computeMatrices(aspect);
              auto VP = matrices._vpmatrix;
              auto hpos = fvec4(wpos,1).transform(VP);
              hpos.perspectiveDivideInPlace();
              return hpos.xyz();
          })
          .def_property_readonly("eye", [](cameradata_ptr_t camera) -> fvec3 { return camera->mEye; })
          .def_property_readonly("target", [](cameradata_ptr_t camera) -> fvec3 { return camera->mTarget; })
          .def_property_readonly("up", [](cameradata_ptr_t camera) -> fvec3 { return camera->mUp; })
          .def_property_readonly("xnormal", [](cameradata_ptr_t camera) -> fvec3 { return camera->_xnormal; })
          .def_property_readonly("ynormal", [](cameradata_ptr_t camera) -> fvec3 { return camera->_ynormal; })
          .def_property_readonly("znormal", [](cameradata_ptr_t camera) -> fvec3 { return camera->_znormal; })
          .def_property("fovy", [](cameradata_ptr_t camera) -> float { 
              return camera->mAper;
              },
              [](cameradata_ptr_t camera, float fovy_degrees) {
              camera->mAper = fovy_degrees;
              })
          .def_property("near", [](cameradata_ptr_t camera) -> float { 
              return camera->mNear;
              },
              [](cameradata_ptr_t camera, float near) {
              camera->mNear = near;
              })
          .def_property("far", [](cameradata_ptr_t camera) -> float { 
            return camera->mFar; 
            },
            [](cameradata_ptr_t camera, float far) {
              camera->mFar = far;
            });
  type_codec->registerStdCodec<cameradata_ptr_t>(camdattype);
  /////////////////////////////////////////////////////////////////////////////////
  auto camdatluttype = //
      py::class_<CameraDataLut, cameradatalut_ptr_t>(module_lev2, "CameraDataLut")
          .def(py::init<>())
          .def("addCamera", [](cameradatalut_ptr_t lut, std::string key, cameradata_constptr_t camera) { (*lut)[key] = camera; })
          .def("create", [](cameradatalut_ptr_t lut, std::string key) -> cameradata_ptr_t {
            auto camera = lut->create(key);
            return camera;
          });
  type_codec->registerStdCodec<cameradatalut_ptr_t>(camdatluttype);
  /////////////////////////////////////////////////////////////////////////////////
  auto cammatstype = //
      py::class_<CameraMatrices, cameramatrices_ptr_t>(module_lev2, "CameraMatrices")
          .def(py::init([] -> cameramatrices_ptr_t { //
            return std::make_shared<CameraMatrices>();
          }))
          .def(
              "setCustomProjection",                           //
              [](cameramatrices_ptr_t cammats, fmtx4 matrix) { //
                cammats->setCustomProjection(matrix);
              })
          .def(
              "setCustomView",                                 //
              [](cameramatrices_ptr_t cammats, fmtx4 matrix) { //
                cammats->setCustomView(matrix);
              });
  type_codec->registerStdCodec<cameramatrices_ptr_t>(cammatstype);
}
} // namespace ork::lev2
