////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <deque>

constexpr bool do_front = true;
constexpr bool do_back = true;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////


std::vector<edge_ptr_t> reverse_edgelist(const std::vector<edge_ptr_t>& inp) {
  std::vector<edge_ptr_t> rval;
  for (auto it_e = inp.rbegin(); //
       it_e != inp.rend();       //
       it_e++) {                 //

    auto ne      = std::make_shared<edge>();
    auto e       = *it_e;
    ne->_vertexA = e->_vertexB;
    ne->_vertexB = e->_vertexA;
    rval.push_back(ne);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string dump_edgelist(const std::vector<edge_ptr_t>& inp) {
  std::string rval;
  std::set<vertex_ptr_t> visited_verts;
  for (auto e : inp) {
    auto va   = e->_vertexA;
    auto vb   = e->_vertexB;
    auto itva = visited_verts.find(va);
    if (itva == visited_verts.end()) {
      visited_verts.insert(va);
      rval += FormatString("%d ", va->_poolindex);
    }
    itva = visited_verts.find(vb);
    if (itva == visited_verts.end()) {
      visited_verts.insert(vb);
      rval += FormatString("%d ", vb->_poolindex);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

struct EdgeChain {
  std::vector<edge_ptr_t> _edges;
  std::unordered_set<vertex_ptr_t> _vertices;
};

using edge_chain_ptr_t = std::shared_ptr<EdgeChain>;

///////////////////////////////////////////////////////////////////////////////

struct EdgeLoop {
  std::vector<edge_ptr_t> _edges;
  void reversed(EdgeLoop& out) const {
    out._edges = reverse_edgelist(_edges);
  }
};

using edge_loop_ptr_t = std::shared_ptr<EdgeLoop>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// ChainLinker - links edge chains into edge loops (if possible)
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct ChainLinker {

  //////////////////////////////////////////////////////////
  edge_chain_ptr_t add_edge(edge_ptr_t e) {
    auto va = e->_vertexA;
    auto vb = e->_vertexB;
    _vtxrefcounts[va]++;
    _vtxrefcounts[vb]++;

    //printf("[%s] EDGE va<%d> vb<%d>\n", _name.c_str(), va->_poolindex, vb->_poolindex);

    // printf( "[%s] VA<%g %g %g> <%g %g %g>\n", _name.c_str(), va->mPos.x, va->mPos.y, va->mPos.z, va->mNrm.x, va->mNrm.y,
    // va->mNrm.z ); printf( "[%s] VB<%g %g %g> <%g %g %g>\n", _name.c_str(), vb->mPos.x, vb->mPos.y, vb->mPos.z, vb->mNrm.x,
    // vb->mNrm.y, vb->mNrm.z );

    //////////////////////////////////
    edge_chain_ptr_t dest_chain;
    for (auto c : _edge_chains) {
      auto last_edge = *c->_edges.rbegin();
      if (last_edge->_vertexB == va) {
        dest_chain = c;
      } else if (last_edge->_vertexB == vb) {
        std::swap(va, vb);
        std::swap(e->_vertexA, e->_vertexB);
        dest_chain = c;
      }
    }
    //////////////////////////////////
    // previous dest chain found !
    //////////////////////////////////
    if (dest_chain) {
      // printf("Added to Chain\n");
      //  find position
      dest_chain->_edges.push_back(e);
      dest_chain->_vertices.insert(vb);
    }
    //////////////////////////////////
    // no dest chain found, create a new one
    //////////////////////////////////
    else {
      // printf("Create New Chain vb<%d>\n", vb->_poolindex);
      dest_chain = std::make_shared<EdgeChain>();
      dest_chain->_edges.push_back(e);
      dest_chain->_vertices.insert(vb);
      _edge_chains.push_back(dest_chain);
    }
    return dest_chain;
  }
  //////////////////////////////////////////////////////////
  bool loops_possible() const {
    for (auto vrcitem : _vtxrefcounts) {
      vertex_ptr_t vtx = vrcitem.first;
      int count        = vrcitem.second;
      if (count != 2) {
        return false;
      }
    }
    return true;
  }
  //////////////////////////////////////////////////////////
  edge_chain_ptr_t findChainForVertex(vertex_ptr_t va) {
    for (auto chain : _edge_chains) {
      auto& edges    = chain->_edges;
      auto last_edge = *edges.rbegin();
      if (last_edge->_vertexB == va) {
        return chain;
      }
      auto first_edge = *edges.begin();
      if (first_edge->_vertexA == va) {
        auto reversed = reverse_edgelist(edges);
        chain->_edges = reversed;
        return chain;
      }
    }
    return nullptr;
  }
  //////////////////////////////////////////////////////////
  void removeChain(edge_chain_ptr_t chain_to_remove) {
    // printf( "removeChain chain<%p> numedges<%zu>\n", (void*) chain_to_remove.get(), chain_to_remove->_edges.size() );
    auto the_lambda = std::remove_if(_edge_chains.begin(), _edge_chains.end(), [chain_to_remove](edge_chain_ptr_t testchain) {
      return (testchain == chain_to_remove);
    });
    _edge_chains.erase(the_lambda, _edge_chains.end());
  }
  //////////////////////////////////////////////////////////
  void closeChains() {
    std::unordered_set<edge_chain_ptr_t> closed;
    //////////////////////////////////
    for (auto chain : _edge_chains) {
      auto first_edge = *chain->_edges.begin();
      auto last_edge  = *chain->_edges.rbegin();
      if (first_edge->_vertexA == last_edge->_vertexB) {
        closed.insert(chain);
      }
    }
    //////////////////////////////////
    for (auto chain : closed) {
      removeChain(chain);
      auto loop    = std::make_shared<EdgeLoop>();
      loop->_edges = chain->_edges;
      _edge_loops.push_back(loop);
    }
    //////////////////////////////////
    if (_edge_loops.size() == 0) {
      //printf("[%s] EDGELOOP DEBUG\n", _name.c_str());
      for (auto chain : _edge_chains) {
        auto d = dump_edgelist(chain->_edges);
        //printf("[%s]   CHAIN numedges<%zu> [%s]\n", _name.c_str(), chain->_edges.size(), d.c_str());
      }
    }
  }
  //////////////////////////////////////////////////////////
  void link() {

    // OrkAssert( loops_possible() );

    /////////////////////////////////////////////////////

    //printf("[%s] prelink numchains<%zu>\n", _name.c_str(), _edge_chains.size());
    for (int i = 0; i < _edge_chains.size(); i++) {
      auto d = dump_edgelist(_edge_chains[i]->_edges);
      if(0)printf(
          "[%s] chain %d:%p | numedges<%zu> [%s]\n",
          _name.c_str(),
          i,
          _edge_chains[i].get(),
          _edge_chains[i]->_edges.size(),
          d.c_str());
    }

    //////////////////////////////////////////////////////////////////////////
    // link chains
    //////////////////////////////////////////////////////////////////////////

    bool keep_joining = true;

    while (keep_joining) {

      //////////////////////////////////////////////////
      // find a left and right chain to join
      //////////////////////////////////////////////////

      edge_chain_ptr_t left_chain;
      edge_chain_ptr_t right_chain;

      for (auto c : _edge_chains) {
        auto subj_vtx = (*c->_edges.begin())->_vertexA;
        auto c2       = findChainForVertex(subj_vtx);
        if (c2 != c) {
          left_chain  = c2;
          right_chain = c;
          break;
        }
      }

      //////////////////////////////////////////////////
      // join the left and right chain
      //////////////////////////////////////////////////

      if (left_chain and right_chain) {

        size_t presize = left_chain->_edges.size();

        left_chain->_edges.insert(
            left_chain->_edges.end(),    //
            right_chain->_edges.begin(), //
            right_chain->_edges.end());

        size_t postsize = left_chain->_edges.size();

        if (_edge_chains.size() > 1)
          removeChain(right_chain);

        if(0)printf(
            "[%s] lchain<%p> rchain<%p> presize<%zu> postsize<%zu> chcount<%zu>\n",
            _name.c_str(),
            (void*)left_chain.get(),
            (void*)right_chain.get(),
            presize,
            postsize,
            _edge_chains.size());

        keep_joining = _edge_chains.size() > 1;
      }

      //////////////////////////////////////////////////
      // no joinable chains...
      //////////////////////////////////////////////////

      else {
        keep_joining = false;
      }

      //////////////////////////////////////////////////

    } // while (keep_joining) {

    closeChains();

    //printf("[%s] postlink numchains<%zu>\n", _name.c_str(), _edge_chains.size());
    //printf("[%s] postlink numloops<%zu>\n", _name.c_str(), _edge_loops.size());
  }
  //////////////////////////////////////////////////////////
  std::vector<edge_chain_ptr_t> _edge_chains;
  std::vector<edge_loop_ptr_t> _edge_loops;
  std::unordered_map<vertex_ptr_t, int> _vtxrefcounts;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// submeshClipWithPlane
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshClipWithPlane(
    const submesh& inpsubmesh, //
    fplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_Front, //
    submesh& outsmesh_Back) {

  constexpr float PLANE_EPSILON = 0.01f;

  /////////////////////////////////////////////////////////////////////
  // count sides of the plane to which the input mesh vertices belong
  /////////////////////////////////////////////////////////////////////

  std::unordered_set<vertex_ptr_t> front_verts, back_verts, planar_verts;

  for (auto item : inpsubmesh._vtxpool->_vtxmap) {
    auto vertex          = item.second;
    const auto& pos      = vertex->mPos;
    float point_distance = slicing_plane.pointDistance(pos);
    if (point_distance > 0.0f) {
      front_verts.insert(vertex);
    } else if (point_distance < 0.0f) {
      back_verts.insert(vertex);
    } else { // on plane
      planar_verts.insert(vertex);
    }
  }

  /////////////////////////////////////////////////////////////////////
  // lambda for adding a new poly to the output mesh
  /////////////////////////////////////////////////////////////////////

  auto add_whole_poly = [](poly_ptr_t src_poly, submesh& dest) -> std::unordered_set<vertex_ptr_t> {
    std::vector<vertex_ptr_t> new_verts;
    std::unordered_set<vertex_ptr_t> added;
    //printf("  subm[%s] add poly size<%zu>\n", dest.name.c_str(), src_poly->_vertices.size());
    for (auto v : src_poly->_vertices) {
      OrkAssert(v);
      auto newv = dest.mergeVertex(*v);
      auto pos  = newv->mPos;
      //printf("   subm[%s] add vertex pool<%02d> (%+g %+g %+g)\n", dest.name.c_str(), newv->_poolindex, pos.x, pos.y, pos.z);
      new_verts.push_back(newv);
      added.insert(newv);
    }
    dest.mergePoly(poly(new_verts));
    return added;
  };

  /////////////////////////////////////////////////////////////////////
  // input mesh polygon loop
  /////////////////////////////////////////////////////////////////////

  std::vector<edge_ptr_t> back_planar_edges, front_planar_edges;
  std::deque<vertex_ptr_t> front_planar_verts_deque;
  std::deque<vertex_ptr_t> back_planar_verts_deque;

  for (auto input_poly : inpsubmesh._orderedPolys) {
    int numverts = input_poly->GetNumSides();
    //////////////////////////////////////////////
    // count sides of the plane to which the poly's vertices belong
    //////////////////////////////////////////////
    int front_count  = 0;
    int back_count   = 0;
    int planar_count = 0;
    for (auto v : input_poly->_vertices) {
      if (front_verts.find(v) != front_verts.end()) {
        front_count++;
      }
      if (back_verts.find(v) != back_verts.end()) {
        back_count++;
      }
      if (planar_verts.find(v) != planar_verts.end()) {
        planar_count++;
      }
    }
    //////////////////////////////////////////////
    // input poly statistics
    //////////////////////////////////////////////
    //printf("input poly numv<%d>\n", numverts);
    //printf(" front_count<%d>\n", front_count);
    //printf(" back_count<%d>\n", back_count);
    //printf(" planar_count<%d>\n", planar_count);
    //////////////////////////////////////////////
    // all of this poly's vertices in front ? -> trivially route to outsmesh_Front
    //////////////////////////////////////////////
    if (numverts == front_count) {
      add_whole_poly(input_poly, outsmesh_Front);
    }
    //////////////////////////////////////////////
    // all of this poly's vertices in back ? -> trivially route to outsmesh_Back
    //////////////////////////////////////////////
    else if (numverts == back_count) { // all back ?
      add_whole_poly(input_poly, outsmesh_Back);
    }
    //////////////////////////////////////////////
    // the remaining are those which must be clipped against plane
    //////////////////////////////////////////////
    else {

      mupoly_clip_adapter clip_input;
      mupoly_clip_adapter clipped_front;
      mupoly_clip_adapter clipped_back;

      /////////////////////////////////////////////////
      // fill in mupoly_clip_adapter clip_input
      /////////////////////////////////////////////////

      int inumv = input_poly->GetNumSides();
      for (int iv = 0; iv < inumv; iv++) {
        auto v = inpsubmesh._vtxpool->GetVertex(input_poly->GetVertexID(iv));
        clip_input.AddVertex(v);
      }

      /////////////////////////////////////////////////
      // clip the input poly into clipped_front, clipped_back
      /////////////////////////////////////////////////

      bool ok = slicing_plane.ClipPoly(clip_input, clipped_front, clipped_back);

      ///////////////////////////////////////////

      auto process_clipped_poly = [&](std::vector<vertex>& clipped_poly_vertices,   //
                                      submesh& outsubmesh,                          //
                                      std::deque<vertex_ptr_t>& planar_verts_deque, //
                                      bool flip_nrm) {                              //
        std::vector<vertex_ptr_t> merged_vertices;

        /////////////////////////////////////////
        // classify all points in clipped poly, with respect to plane
        //  put all points which live on plane into planar_verts_deque
        /////////////////////////////////////////

        for (auto& v : clipped_poly_vertices) {

          auto merged_v = outsubmesh.mergeVertex(v);

          merged_vertices.push_back(merged_v);
          float point_dist_to_plane = abs(slicing_plane.pointDistance(merged_v->mPos));
          if (point_dist_to_plane < PLANE_EPSILON) {
            const auto& p  = merged_v->mPos;
            merged_v->mNrm = fvec3(0, 0, 0);
            merged_v->mUV[0].Clear();
            merged_v->mUV[1].Clear();
            //printf("subm[%s] bpv (%+g %+g %+g) \n", outsubmesh.name.c_str(), p.x, p.y, p.z);
            planar_verts_deque.push_back(merged_v);
          } else {
            //printf("subm[%s] REJECT: point_dist_to_plane<%g>\n", outsubmesh.name.c_str(), point_dist_to_plane);
          }
        }

        /////////////////////////////////////////
        // if we have enough merged vertices for a polygon,
        //  then create a polygon
        /////////////////////////////////////////

        if (merged_vertices.size() >= 3) {
          auto out_bpoly = std::make_shared<poly>(merged_vertices);
          add_whole_poly(out_bpoly, outsubmesh);
        }

      };

      ///////////////////////////////////////////

      if (do_front)
        process_clipped_poly(clipped_front.mVerts, outsmesh_Front, front_planar_verts_deque, true);

      if (do_back)
        process_clipped_poly(clipped_back.mVerts, outsmesh_Back, back_planar_verts_deque, false);
    } // clipped ?

  } // for (auto input_poly : inpsubmesh._orderedPolys) {

  ///////////////////////////////////////////////////////////
  // close mesh
  ///////////////////////////////////////////////////////////

  if (close_mesh) {

    auto do_close = [&](submesh& outsubmesh, //
                        std::deque<vertex_ptr_t>& planar_verts_deque,
                        bool test) { //
      /*
      printf("subm[%s] planar_verts_deque[ ", outsubmesh.name.c_str());
      for (auto v : planar_verts_deque) {
        printf("%d ", v->_poolindex);
      }
      printf("]\n");
      */

      /////////////////////////////////////////
      //  take note of edges which lie on the
      //  slicing plane
      /////////////////////////////////////////

      std::vector<edge_ptr_t> planar_edges;

      while (planar_verts_deque.size() >= 2) {
        auto v0 = planar_verts_deque[0];
        auto v1 = planar_verts_deque[1];
        auto e  = std::make_shared<edge>(v0, v1);
        planar_edges.push_back(e);
        planar_verts_deque.pop_front();
        planar_verts_deque.pop_front();
      }

      //printf("subm[%s] stragglers<%zu>\n", outsubmesh.name.c_str(), planar_verts_deque.size());

      if (planar_edges.size()) {

        ChainLinker _linker;
        _linker._name = outsubmesh.name;
        for (auto edge : planar_edges) {
          _linker.add_edge(edge);
        }
        _linker.link();
        for (auto loop : _linker._edge_loops) {

          // EdgeLoop reversed;
          // loop->reversed(reversed);

          std::vector<vertex_ptr_t> vertex_loop;
          //printf("subm[%s] begin edgeloop <%p>\n", outsubmesh.name.c_str(), (void*)loop.get());
          int ie = 0;
          for (auto edge : loop->_edges) {
            vertex_loop.push_back(edge->_vertexA);
            //printf(" subm[%s] edge<%d> vtxi<%d>\n", outsubmesh.name.c_str(), ie, edge->_vertexA->_poolindex);
            ie++;
          }

          ///////////////////////////////////////////
          // compute mesh center
          ///////////////////////////////////////////

          fvec3 mesh_center_pos = inpsubmesh.center();

          ///////////////////////////////////////////
          // compute loop center
          ///////////////////////////////////////////

          vertex temp_loop_center;
          temp_loop_center.center(vertex_loop);
          auto center_vertex = outsubmesh.mergeVertex(temp_loop_center);
          auto loop_center_pos    = temp_loop_center.mPos;
          //printf(" subm[%s] center<%g %g %g>\n", outsubmesh.name.c_str(), loop_center_pos.x, loop_center_pos.y, loop_center_pos.z);

          ///////////////////////////////////////////
          // compute normal based on connected faces
          ///////////////////////////////////////////

          fvec3 avg_n = (loop_center_pos-mesh_center_pos).normalized();

          ///////////////////////////////////////////

          for (auto edge : loop->_edges) {
            auto va = outsubmesh.mergeVertex(*edge->_vertexA);
            auto vb = outsubmesh.mergeVertex(*edge->_vertexB);

            ///////////////////////////////////////////
            // TODO correct winding order
            ///////////////////////////////////////////

            auto dab = (vb->mPos - va->mPos).normalized();
            auto dbc = (center_vertex->mPos - vb->mPos).normalized();
            auto vx  = dab.crossWith(dbc).normalized();

            float d = vx.dotWith(avg_n);

            if ((d < 0.0f) == (test ^ flip_orientation)) {
              outsubmesh.mergeTriangle(vb, va, center_vertex);
            } else {
              outsubmesh.mergeTriangle(va, vb, center_vertex);
            }
          }
        }

      } // if (close_mesh and added_vertices.size()) {

    };

    if(do_front) do_close(outsmesh_Front, front_planar_verts_deque, false);
    if(do_back) do_close(outsmesh_Back,  back_planar_verts_deque,  true);

  } // if(close_mesh){

  ///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
