#include "pyext.h"
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <pybind11/eigen.h>
#include <ork/lev2/gfx/meshutil/igl.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
using namespace meshutil;
using rigidprim_t = RigidPrimitive<SVtxV12N12B12T8C4>;
void pyinit_meshutil(py::module& module_lev2) {
  auto meshutil = module_lev2.def_submodule("meshutil", "Mesh operations");
  //////////////////////////////////////////////////////////////////////////////
  py::class_<IglMesh, iglmesh_ptr_t>(meshutil, "IglMesh") //
      .def(py::init([](const Eigen::MatrixXd& verts,      //
                       const Eigen::MatrixXi& faces) {    //
        return std::make_shared<IglMesh>(verts, faces);
      }))
      .def(py::init([](submesh_ptr_t subm, int numsides) { //
        return std::make_shared<IglMesh>(*subm, numsides);
      }))
      .def(
          "__repr__",
          [](iglmesh_ptr_t iglmesh) -> std::string {
            fxstring<64> fxs;
            fxs.format("IglMesh(%p)", iglmesh.get());
            return fxs.c_str();
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
          "reOriented",
          [](iglmesh_constptr_t inpmesh) -> iglmesh_ptr_t { //
            return inpmesh->reOriented();
          })
      .def(
          "cleaned",
          [](iglmesh_constptr_t inpmesh) -> iglmesh_ptr_t { //
            return inpmesh->cleaned();
          })
      .def(
          "ambientOcclusion",
          [](iglmesh_constptr_t inpmesh, int numsamples) -> Eigen::VectorXd { //
            return inpmesh->ambientOcclusion(numsamples);
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
  py::class_<IglPrincipleCurvature, iglprinciplecurvature_ptr_t>(meshutil, "IglCurvatureDirection")
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
  py::class_<rigidprim_t>(meshutil, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](submesh& submesh, ctx_t context) {
        auto prim = std::unique_ptr<rigidprim_t>(new rigidprim_t);
        prim->fromSubMesh(submesh, context.get());
        return prim;
      }))
      .def("fromSubMesh", [](rigidprim_t& prim, submesh_ptr_t submesh, Context* context) { prim.fromSubMesh(*submesh, context); })
      .def("renderEML", [](rigidprim_t& prim, ctx_t context) { prim.renderEML(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<submesh, submesh_ptr_t>(meshutil, "SubMesh")
      .def(py::init<>())
      .def("igl_test", [](submesh_ptr_t submesh) { return submesh->igl_test(); })
      .def(
          "triangulate",
          [](submesh_constptr_t inpsubmesh) -> submesh_ptr_t {
            submesh_ptr_t rval = std::make_shared<submesh>();
            submeshTriangulate(*inpsubmesh, *rval);
            return rval;
          })
      .def("toIglMesh", [](submesh_ptr_t submesh, int numsides) -> iglmesh_ptr_t { return submesh->toIglMesh(numsides); })
      .def("numPolys", [](submesh_constptr_t submesh, int numsides) -> int { return submesh->GetNumPolys(numsides); })
      .def("numVertices", [](submesh_constptr_t submesh) -> int { return submesh->_vtxpool.GetNumVertices(); })
      .def(
          "writeWavefrontObj",
          [](submesh_constptr_t submesh, const std::string& outpath) { return submeshWriteObj(*submesh, outpath); })
      .def(
          "addQuad", [](submesh_ptr_t submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3) { return submesh->addQuad(p0, p1, p2, p3); })
      .def(
          "addQuad",
          [](submesh_ptr_t submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec4 c) {
            return submesh->addQuad(p0, p1, p2, p3, c);
          })
      .def(
          "addQuad",
          [](submesh_ptr_t submesh, fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c) {
            return submesh->addQuad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, c);
          })
      .def(
          "addQuad",
          [](submesh_ptr_t submesh,
             fvec3 p0,
             fvec3 p1,
             fvec3 p2,
             fvec3 p3,
             fvec3 n0,
             fvec3 n1,
             fvec3 n2,
             fvec3 n3,
             fvec2 uv0,
             fvec2 uv1,
             fvec2 uv2,
             fvec2 uv3,
             fvec4 c) { return submesh->addQuad(p0, p1, p2, p3, n0, n1, n2, n3, uv0, uv1, uv2, uv3, c); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vertexpool>(meshutil, "VertexPool").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<poly>(meshutil, "Poly").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<edge>(meshutil, "Edge").def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<UniqueEdges, unique_edges_ptr_t>(meshutil, "UniqueEdges")
      .def_property_readonly("count", [](unique_edges_ptr_t ue) -> int { return ue->_count; })
      .def_property_readonly("ue2e", [](unique_edges_ptr_t ue) -> std::vector<std::vector<size_t>> { return ue->_ue2e; })
      .def_property_readonly("E", [](unique_edges_ptr_t ue) -> Eigen::MatrixXi { return ue->E; })
      .def_property_readonly("uE", [](unique_edges_ptr_t ue) -> Eigen::MatrixXi { return ue->uE; })
      .def_property_readonly("EMAP", [](unique_edges_ptr_t ue) -> Eigen::MatrixXi { return ue->EMAP; });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ManifoldExtraction, manifold_extraction_ptr_t>(meshutil, "ManifoldExtraction")
      .def_property_readonly("numpatches", [](manifold_extraction_ptr_t me) -> int { return me->_numpatches; })
      .def_property_readonly("numcells", [](manifold_extraction_ptr_t me) -> int { return me->_numcells; })
      .def_property_readonly("per_patch_cells", [](manifold_extraction_ptr_t me) -> Eigen::MatrixXi { return me->per_patch_cells; })
      .def_property_readonly("P", [](manifold_extraction_ptr_t me) -> Eigen::VectorXi { return me->P; });

  //////////////////////////////////////////////////////////////////////////////
  py::class_<Mesh, mesh_ptr_t>(meshutil, "Mesh") //
      .def(py::init<>())
      .def_property_readonly(
          "polygroups",                              //
          [](mesh_ptr_t the_mesh) -> submesh_lut_t { //
            return the_mesh->_submeshesByPolyGroup;
          })
      .def("readFromWavefrontObj", [](mesh_ptr_t the_mesh, std::string pth) { //
        the_mesh->ReadFromWavefrontObj(pth);
      });
  //////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2
