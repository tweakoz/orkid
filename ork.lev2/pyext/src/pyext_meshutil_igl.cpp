////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <pybind11/eigen.h>
#include <ork/lev2/gfx/meshutil/igl.h>

#if defined(ENABLE_IGL)

namespace ork::lev2 {
using namespace meshutil;


void pyinit_meshutil_igl(py::module& module_meshutil) {
  py::class_<IglMesh, iglmesh_ptr_t>(module_meshutil, "IglMesh") //
      .def(py::init([] {                                       //
        return std::make_shared<IglMesh>();
      }))
      .def(py::init([](const Eigen::MatrixXd& verts,   //
                       const Eigen::MatrixXi& faces) { //
        return std::make_shared<IglMesh>(verts, faces);
      }))
      .def(py::init([](submesh_ptr_t subm, int numsides) { //
        return std::make_shared<IglMesh>(*subm, numsides);
      }))
      .def(
          "__repr__",
          [](iglmesh_ptr_t iglmesh) -> std::string {
            return FormatString(
                "IglMesh(%p) numv<%d> numf<%d>", iglmesh.get(), int(iglmesh->_verts.size()), int(iglmesh->_faces.size()));
          })
      .def_property(
          "vertices",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_verts;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_verts = inp;
          })
      .def_property(
          "faces",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXi { //
            return iglm->_faces;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXi inp) { //
            iglm->_faces = inp;
          })
      .def_property(
          "normals",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_normals;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_normals = inp;
          })
      .def_property(
          "binormals",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_binormals;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_binormals = inp;
          })
      .def_property(
          "tangents",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_tangents;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_tangents = inp;
          })
      .def_property(
          "colors",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_colors;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_colors = inp;
          })
      .def_property(
          "uvs",
          [](iglmesh_ptr_t iglm) -> Eigen::MatrixXd { //
            return iglm->_uvs;
          },
          [](iglmesh_ptr_t iglm, Eigen::MatrixXd inp) { //
            iglm->_uvs = inp;
          })
      .def_property_readonly(
          "uniqueEdges",
          [](iglmesh_constptr_t iglm) -> unique_edges_ptr_t { //
            return iglm->uniqueEdges();
          })
      .def_property_readonly(
          "piecewiseConstantWindingNumber",
          [](iglmesh_constptr_t iglm) -> bool { //
            return iglm->piecewiseConstantWindingNumber();
          })

      .def_property_readonly(
          "manifoldExtraction",
          [](iglmesh_constptr_t iglm) -> manifold_extraction_ptr_t { //
            return iglm->extractManifolds();
          })
      .def_property_readonly(
          "genus",
          [](iglmesh_constptr_t iglm) -> int { //
            return iglm->genus();
          })
      .def_property_readonly(
          "isVertexManifold",
          [](iglmesh_constptr_t iglm) -> bool { //
            return iglm->isVertexManifold();
          })
      .def_property_readonly(
          "isEdgeManifold",
          [](iglmesh_constptr_t iglm) -> bool { //
            return iglm->isEdgeManifold();
          })
      .def(
          "triangulated",
          [](iglmesh_constptr_t inpmesh) -> iglmesh_ptr_t { //
            return inpmesh->triangulated();
          })
      .def(
          "decimated",
          [](iglmesh_constptr_t inpmesh, float amount) -> iglmesh_ptr_t { //
            return inpmesh->decimated(amount);
          })
      .def(
          "parameterizedSCAF",
          [](iglmesh_ptr_t inpmesh, int numiters, double scale, double bias) -> iglmesh_ptr_t { //
            return inpmesh->parameterizedSCAF(numiters, scale, bias);
          })
      .def(
          "toSubMesh",
          [](iglmesh_constptr_t inpmesh) -> submesh_ptr_t { //
            return inpmesh->toSubMesh();
          })
      .def(
          "averageEdgeLength",
          [](iglmesh_constptr_t inpmesh) -> double { //
            return inpmesh->averageEdgeLength();
          })
      .def(
          "gaussianCurvature",
          [](iglmesh_constptr_t inpmesh) -> Eigen::VectorXd { //
            return inpmesh->computeGaussianCurvature();
          })
      .def(
          "principleCurvature",
          [](iglmesh_constptr_t inpmesh) -> iglprinciplecurvature_ptr_t { //
            return inpmesh->computePrincipleCurvature();
          })
      .def(
          "parameterizeHarmonic",
          [](iglmesh_constptr_t inpmesh) -> Eigen::MatrixXd { //
            return inpmesh->parameterizeHarmonic();
          })
      .def(
          "parameterizeLCSM",
          [](iglmesh_ptr_t inpmesh) -> Eigen::MatrixXd { //
            return inpmesh->parameterizeLCSM();
          })
      .def(
          "areaStatistics",
          [](iglmesh_constptr_t inpmesh) -> fvec4 { //
            return inpmesh->computeAreaStatistics();
          })
      .def(
          "angleStatistics",
          [](iglmesh_constptr_t inpmesh) -> fvec4 { //
            return inpmesh->computeAngleStatistics();
          })
      .def(
          "irregularVertexCount",
          [](iglmesh_constptr_t inpmesh) -> int { //
            return inpmesh->countIrregularVertices();
          })
      .def(
          "cleaned",
          [](iglmesh_constptr_t inpmesh) -> iglmesh_ptr_t { //
            return inpmesh->cleaned();
          })
      .def(
          "reOriented",
          [](iglmesh_constptr_t inpmesh) -> iglmesh_ptr_t { //
            return inpmesh->reOriented();
          })
      .def(
          "ambientOcclusion",
          [](iglmesh_constptr_t inpmesh, int numsamples) -> Eigen::VectorXd { //
            return inpmesh->ambientOcclusion(numsamples);
          })
      .def(
          "booleanOf",
          [](iglmesh_ptr_t inpmesh, iglmesh_constptr_t a, crcstring_ptr_t operation, iglmesh_constptr_t b) -> bool { //
            auto iglmesh_operation = BooleanOperation(operation->hashed());
            return inpmesh->booleanOf(a, iglmesh_operation, b);
          })
      .def(
          "faceNormals",
          [](iglmesh_constptr_t inpmesh) -> Eigen::MatrixXd { //
            return inpmesh->computeFaceNormals();
          })
      .def(
          "vertexNormals",
          [](iglmesh_constptr_t inpmesh) -> Eigen::MatrixXd { //
            return inpmesh->computeVertexNormals();
          })
      .def("cornerNormals", [](iglmesh_constptr_t inpmesh, float dihedral_angle) -> Eigen::MatrixXd { //
        return inpmesh->computeCornerNormals(dihedral_angle);
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<IglPrincipleCurvature, iglprinciplecurvature_ptr_t>(module_meshutil, "IglCurvatureDirection")
      .def_property_readonly(
          "k1",
          [](iglprinciplecurvature_ptr_t igldir) -> Eigen::MatrixXd { //
            return igldir->PD1;
          })
      .def_property_readonly(
          "k2",
          [](iglprinciplecurvature_ptr_t igldir) -> Eigen::MatrixXd { //
            return igldir->PD2;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<UniqueEdges, unique_edges_ptr_t>(module_meshutil, "UniqueEdges")
      .def_property_readonly("count", [](unique_edges_ptr_t ue) -> int { return ue->_count; })
      .def_property_readonly("ue2e", [](unique_edges_ptr_t ue) -> std::vector<std::vector<size_t>> { return ue->_ue2e; })
      .def_property_readonly("E", [](unique_edges_ptr_t ue) -> Eigen::MatrixXi { return ue->E; })
      .def_property_readonly("uE", [](unique_edges_ptr_t ue) -> Eigen::MatrixXi { return ue->uE; })
      .def_property_readonly("EMAP", [](unique_edges_ptr_t ue) -> Eigen::MatrixXi { return ue->EMAP; });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ManifoldExtraction, manifold_extraction_ptr_t>(module_meshutil, "ManifoldExtraction")
      .def_property_readonly("numpatches", [](manifold_extraction_ptr_t me) -> int { return me->_numpatches; })
      .def_property_readonly("numcells", [](manifold_extraction_ptr_t me) -> int { return me->_numcells; })
      .def_property_readonly("per_patch_cells", [](manifold_extraction_ptr_t me) -> Eigen::MatrixXi { return me->per_patch_cells; })
      .def_property_readonly("P", [](manifold_extraction_ptr_t me) -> Eigen::VectorXi { return me->P; });


} // void pyinit_meshutil_igl(py::module& module_meshutil) {

} // namespace ork::lev2

#endif //#if defined(ENABLE_IGL)
