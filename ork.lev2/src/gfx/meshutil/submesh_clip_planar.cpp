////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh_clip.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_clip = logger()->createChannel("meshutil.clipper", fvec3(.9, .9, 1), true);

const std::unordered_set<int> test_verts = {9, 8, 7, 6, 5, 4};

struct PlanarClipPrimitive : public ClipPrimitiveBase {

  PlanarClipPrimitive(
      SubMeshClipper& clipengine, //
      dplane3 slicing_plane);

  double pointDistance(const dvec3& point) const final;
  bool isPointInFront(const dvec3& point) const final;
  bool doesIntersect(
      const dray3& ray, //
      double& distance, //
      dvec3& isect_point) const final;

  void close();

  dplane3 _slicing_plane;
  SubMeshClipper& _clipengine;
};

///////////////////////////////////////////////////////////////////////////////

PlanarClipPrimitive::PlanarClipPrimitive(
    SubMeshClipper& clipengine,     //
    dplane3 slicing_plane)          //
    : _slicing_plane(slicing_plane) //
    , _clipengine(clipengine) {     //
}

///////////////////////////////////////////////////////////////////////////////

double PlanarClipPrimitive::pointDistance(const dvec3& point) const {
  return _slicing_plane.pointDistance(point);
}

///////////////////////////////////////////////////////////////////////////////

bool PlanarClipPrimitive::isPointInFront(const dvec3& point) const {
  return _slicing_plane.isPointInFront(point);
}

///////////////////////////////////////////////////////////////////////////////

bool PlanarClipPrimitive::doesIntersect(
    const dray3& ray,                 //
    double& distance,                 //
    dvec3& isect_point) const { //
  return _slicing_plane.Intersect(ray, distance, isect_point);
}

///////////////////////////////////////////////////////////////////////////////

void PlanarClipPrimitive::close() {
  // return;

  // TODO - non convex support
  //
  if (_clipengine._debug) {
    _clipengine._outsubmesh.visitAllVertices([&](vertex_ptr_t v) { //
      double point_distance = _slicing_plane.pointDistance(v->mPos);
      logchan_clip->log("outv%d : %f %f %f point_distance<%f>", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z, point_distance);
    });
    _clipengine._outsubmesh.dumpPolys("preclose");
  }

  _clipengine._backpolys.visit([&](poly_const_ptr_t back_poly) {
    int num_clipped = 0;
    int num_v       = back_poly->numVertices();

    if (_clipengine._debug)
      logchan_clip->log_begin("BACKPOLYVISIT INPUT POLY[");

    for (int iva = 0; iva < num_v; iva++) {
      int ivb                     = (iva + 1) % num_v;
      auto inp_vtx_a              = back_poly->vertex(iva);
      auto inp_vtx_b              = back_poly->vertex(ivb);
      auto he                     = _clipengine._outsubmesh.mergeEdgeForVertices(inp_vtx_a, inp_vtx_b);
      const auto& he_plane_status = _clipengine._outsubmesh.typedVar<SurfaceStatus>(he, "plstatus");
      bool has_clipped            = _clipengine._outsubmesh.hasVar(inp_vtx_a, "clipped_vertex");
      if (has_clipped) {
        num_clipped++;
        if (_clipengine._debug)
          logchan_clip->log_continue(" <%d>", inp_vtx_a->_poolindex);
      } else {
        bool is_back = _clipengine._outsubmesh.tryVarAs<bool>(inp_vtx_a, "back_vertex").value();
        if (is_back) {
          if (_clipengine._debug)
            logchan_clip->log_continue(" (%d)", inp_vtx_a->_poolindex);

        } else {
          if (_clipengine._debug)
            logchan_clip->log_continue(" %d", inp_vtx_a->_poolindex);
        }
      }
      if (he_plane_status._status == ESurfaceStatus::BACK) {
        if (_clipengine._debug)
          logchan_clip->log_continue("!");
      }
      if (auto try_clipped_edge = _clipengine._outsubmesh.tryVarAs<halfedge_ptr_t>(he, "clipped_edge")) {
        auto clipped_edge = try_clipped_edge.value();
        if (_clipengine._debug)
          logchan_clip->log_continue("$");
        _clipengine._vertex_remap[he->_vertexA->_poolindex] = clipped_edge->_vertexA->_poolindex;
        _clipengine._vertex_remap[he->_vertexB->_poolindex] = clipped_edge->_vertexB->_poolindex;
      }
    }

    if (_clipengine._debug)
      logchan_clip->log_continue(" ] num_v<%d> num_clipped<%d>\n", num_v, num_clipped);

    if (_clipengine._vertex_remap.size()) {
      if (_clipengine._debug)
        logchan_clip->log_continue("_vertex_remap [");
      for (auto item : _clipengine._vertex_remap) {
        if (_clipengine._debug)
          logchan_clip->log_continue(" %d->%d", item.first, item.second);
      }
      if (_clipengine._debug)
        logchan_clip->log_continue(" ]\n");
      std::vector<vertex_ptr_t> remapped_verts;
      back_poly->visitVertices([&](vertex_ptr_t vtx) {
        auto it = _clipengine._vertex_remap.find(vtx->_poolindex);
        if (it != _clipengine._vertex_remap.end()) {
          auto remapped_vtx = _clipengine._outsubmesh.vertex(it->second);
          remapped_verts.push_back(remapped_vtx);
        } else {
          remapped_verts.push_back(vtx);
        }
      });
      auto front_poly = _clipengine._outsubmesh.mergePoly(remapped_verts);
    }
    if (num_clipped > 1 and num_clipped == num_v) {
      // all vertices are clipped, so this poly is now a planar poly
      // and shall be added to the front mesh (with orientation flipped)
      vertex_vect_t front_verts;
      back_poly->visitVertices([&](vertex_ptr_t vtx) {

      });
      // auto front_poly = _clipengine._outsubmesh.mergePoly(front_verts);
      // OrkAssert(false);
    }
  });

  /////////////////////////////////////////
  //  take note of edges which lie on the
  //  slicing plane
  /////////////////////////////////////////

  edge_set_t all_edges;
  _clipengine._outsubmesh.visitAllPolys([&](poly_ptr_t poly) { //
    poly->visitEdges([&](edge_ptr_t e) {                       //
      all_edges.insert(e);
    });
  });

  if (_clipengine._debug)
    for (auto e_item : all_edges._the_map) {
      auto e = e_item.second;
      logchan_clip->log("all e[%d %d]", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    }

  _clipengine._surface_verts_pending_close.visit([&](vertex_ptr_t v) {
    if (_clipengine._debug)
      logchan_clip->log("planar v%d : %f %f %f", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
  });

  if (_clipengine._surface_verts_pending_close.size() < 3) {
    return;
  }

  int index = 0;
  edge_set_t planar_edges;

  _clipengine._surface_verts_pending_close.visit([&](vertex_ptr_t v) {
    if (_clipengine._debug)
      logchan_clip->log("planar v%d : %f %f %f", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
  });

  dvec3 planar_center;

  _clipengine._surface_verts_pending_close.visit([&](vertex_ptr_t v) {
    planar_center += v->mPos;
    index++;
  });
  planar_center *= 1.0 / double(index);

  auto first_planar_vpos = _clipengine._surface_verts_pending_close.first()->mPos;
  // printf( "ref vert<%d>\n", _clipengine._surface_verts_pending_close.first()->_poolindex );
  dvec3 planar_reference_direction = (first_planar_vpos - planar_center).normalized();

  std::multimap<double, vertex_ptr_t> verts_by_angle;
  dvec3 R = _slicing_plane.n;

  _clipengine._surface_verts_pending_close.visit([&](vertex_ptr_t v) {
    // using R as the reference direction, compute the angle of the vertex
    // relative to the reference direction
    dvec3 V      = (v->mPos - planar_center).normalized();
    double angle = planar_reference_direction.orientedAngle(V, R);
    verts_by_angle.insert(std::make_pair(angle, v));
  });
  _clipengine._surface_verts_pending_close.clear();

  // we have the verts by polar angle...
  // now create a boundary loop of edges from the sorted vertices

  for (auto v_item : verts_by_angle) {
    double angle = v_item.first;
    auto v       = v_item.second;
    // printf( "angle<%g> v<%d>\n", angle, v->_poolindex );
  }

  for (auto it = verts_by_angle.begin(); it != verts_by_angle.end(); ++it) {
    auto it2 = it;
    ++it2;
    if (it2 == verts_by_angle.end()) {
      it2 = verts_by_angle.begin();
    }
    auto v1 = it->second;
    auto v2 = it2->second;
    auto e  = std::make_shared<edge>(v1, v2);
    planar_edges.insert(e);
  }

  if (_clipengine._debug) {
    for (auto e_item : planar_edges._the_map) {
      auto e = e_item.second;
      logchan_clip->log_continue("planar e %d %d\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    }
  }

  /////////////////////////////////////////
  // we have some edges on the cutting plane
  /////////////////////////////////////////

  if (planar_edges.size()) {

    // link edge chains into edge loops

    EdgeChainLinker _linker;
    _linker._name = _clipengine._outsubmesh.name;
    for (auto edge_item : planar_edges._the_map) {
      auto edge = edge_item.second;
      _linker.add_edge(edge);
    }
    _linker.link();

    // create a new polygon for each edge loop

    dvec3 submesh_centroid = _clipengine._outsubmesh.centerOfPolys();
    vertex submesh_centroid_vertex;
    submesh_centroid_vertex.mPos = submesh_centroid;
    auto centroid_merged         = _clipengine._outsubmesh.mergeVertex(submesh_centroid_vertex);

    auto do_chain = [&](edge_chain_ptr_t chain) { //
      if (chain->_edges.size() < 3)
        return;

      auto ordered_x = chain->orderedVertices();
      vertex_vect_t ordered;
      for (auto vtx : ordered_x) {
        ordered.push_back(_clipengine._outsubmesh.mergeVertex(*vtx));
      }

      bool flip_polygon = false;

      double planar_deviation = chain->planarDeviation();

      // printf( "chain<%p> planarDeviation<%f>\n", chain.get(), planar_deviation );

      if (false) { // winding order from adjacent polys
        // compute correct winding order via the connectivity of
        //  adjacent polygons

        int forward  = 0;
        int backward = 0;

        for (auto e : chain->_edges) {
          auto poly_set = _clipengine._outsubmesh.connectedPolys(e, false);
          printf("edge<%d->%d> poly_set<%d>\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex, int(poly_set.size()));
          size_t num_connected_polys = poly_set.size();
          switch (num_connected_polys) {
            case 0: {
              // no connected polys ?
              // this edge is on the boundary of the mesh
              // so we can't do anything with it
              printf("HAVE BOUNDARY EDGE\n");
              break;
            }
            case 1: {
              int ipoly    = *poly_set.begin();
              auto polygon = _clipengine._outsubmesh.poly(ipoly);
              // find edge in polygon
              polygon->visitEdges([&](edge_ptr_t e_poly) {
                if (e_poly->_vertexA == e->_vertexA and e_poly->_vertexB == e->_vertexB) {
                  // edge is in same direction as polygon
                  forward++;
                } else if (e_poly->_vertexA == e->_vertexB and e_poly->_vertexB == e->_vertexA) {
                  // edge is in opposite direction as polygon
                  backward++;
                }
              });
              break;
            }
            default: {
              printf("HAVE OVERBOOKED EDGE count<%d>!\n", int(num_connected_polys));
              for (auto p : poly_set) {
                printf("poly<%d>\n", p);
              }
              break;
            }
          }
          // auto P = e->_polygon;

        } // for (auto e : chain->_edges) {

        bool all_forward  = (forward == chain->_edges.size());
        bool all_backward = (backward == chain->_edges.size());
        printf("forward %d backward %d\n", forward, backward);
        OrkAssert(all_forward or all_backward);

        if (all_forward) {
          printf("all forward\n");
          // flip polygon
          flip_polygon = true;
        } else if (all_backward) {
          printf("all backward\n");
        }
        if (flip_polygon) {
          std::reverse(ordered.begin(), ordered.end());
        }
        auto P = Polygon(ordered);
        _clipengine._outsubmesh.mergePoly(P);

      } // if (false) { // winding order from adjacent polys
      else {
        // compute correct winding order via the centroid of the polygon
        //  and the centroid of the vertices of the polygon

        if (_clipengine._debug)
          printf("planar_deviation<%g>\n", planar_deviation);

        if (planar_deviation < 0.0001) {

          dvec3 poly_centroid = chain->centroid();
          vertex poly_centroid_vertex;
          poly_centroid_vertex.mPos = poly_centroid;
          _clipengine._outsubmesh.mergeVertex(poly_centroid_vertex);

          dvec3 poly_normal = chain->avgNormalOfFaces();

          dvec3 poly_to_centroid = (poly_centroid - submesh_centroid).normalized();

          double dot = poly_to_centroid.dotWith(poly_normal);

          if (_clipengine._debug)
            printf("submesh center<%g %g %g>\n", submesh_centroid.x, submesh_centroid.y, submesh_centroid.z);
          if (_clipengine._debug)
            printf("poly center<%g %g %g>\n", poly_centroid.x, poly_centroid.y, poly_centroid.z);
          if (_clipengine._debug)
            printf("poly normal<%g %g %g>\n", poly_normal.x, poly_normal.y, poly_normal.z);
          if (_clipengine._debug)
            printf("poly to center<%g %g %g>\n", poly_to_centroid.x, poly_to_centroid.y, poly_to_centroid.z);
          if (_clipengine._debug)
            printf("DOT<%g>\n", dot);

          if (dot < 0.0) {
            flip_polygon = true;
          }

          if (flip_polygon) {
            std::reverse(ordered.begin(), ordered.end());
          }
          auto P = Polygon(ordered);
          _clipengine._outsubmesh.mergePoly(P);
        } else { // triangle fan with correct winding order

          vertex center_vertex;
          center_vertex.mPos = chain->centroid();
          ;
          auto center_merged = _clipengine._outsubmesh.mergeVertex(center_vertex);
          if (_clipengine._debug)
            printf("center_merged poolindex<%d>\n", center_merged->_poolindex);
          // triangle fan
          for (size_t i = 0; i < ordered.size(); i++) {
            auto va     = ordered[i];
            auto vb     = ordered[(i + 1) % ordered.size()];
            auto P      = Polygon(center_merged, va, vb);
            double area = P.signedVolumeWithPoint(submesh_centroid);
            if (area >= 0.0) {
              P = Polygon(center_merged, vb, va);
            }
            _clipengine._outsubmesh.mergePoly(P);
          }
        }
      }

    }; // auto do_chain = [&](edge_chain_ptr_t chain) { //

    for (auto loop : _linker._edge_loops) {
      if (_clipengine._debug) {
        logchan_clip->log_continue("loop [");
        for (auto e : loop->_edges) {
          logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
        }
        logchan_clip->log_continue("]\n");
      }
      do_chain(loop);
    }

    for (auto chain : _linker._edge_chains) {
      if (_clipengine._debug) {
        logchan_clip->log_continue("chain [");
        for (auto e : chain->_edges) {
          logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
        }
        logchan_clip->log_continue("]\n");
      }
      // do_chain(chain);
    }
  } // if (planar_edges.size()) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshClipWithPlane(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_front, //
    bool debug) {

  if (debug) {
    logchan_clip->log_continue("///////////\n");
    inpsubmesh.dumpPolys("inpsubmesh");
  }

  SubMeshClipper clip_engine(inpsubmesh, outsmesh_front, debug);

  auto clip_prim = std::make_shared<PlanarClipPrimitive>(clip_engine, slicing_plane);

  clip_engine.clipWithPrimitive(clip_prim);

  if (close_mesh) {
    clip_prim->close();
  }

  if (debug) {
    outsmesh_front.dumpPolys("clipped_front");
    if (clip_engine._surface_verts_pending_close._the_map.size() > 0) {
      logchan_clip->log_continue("fpv [");
      for (auto v_item : clip_engine._surface_verts_pending_close._the_map) {
        auto v = v_item.second;
        logchan_clip->log_continue(" %d", v->_poolindex);
      }
      logchan_clip->log_continue("]\n");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
