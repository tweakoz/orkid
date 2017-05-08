////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include "OrkUtils.h"
#include "MayaTopoMesh.h"

namespace ork { namespace maya {
///////////////////////////////////////////////////////////////////////////////

class UvRailOverlappedCommand : public MPxCommand
{
   public:
      static void* instantiate();

      UvRailOverlappedCommand() {}
      ~UvRailOverlappedCommand() final {}
      
      bool isUndoable () const final { return false; }
      MStatus doIt (const MArgList&) final;
};

/////////////////////////////////////////////////////////////////////

class UvRailUnitizedCommand : public MPxCommand
{
   public:

      static void* instantiate();

      UvRailUnitizedCommand() {}
      ~UvRailUnitizedCommand() final {}

      bool isUndoable () const final { return false; }
      MStatus doIt (const MArgList&) final;
};

/////////////////////////////////////////////////////////////////////

struct RailFace
{
   RailFace();

   int   _faceIndex;
   int   _vertTL;
   int   _vertTR;
   int   _vertBL;
   int   _vertBR;
   int   _uvTL;
   int   _uvTR;
   int   _uvBL;
   int   _uvBR;
   float _unitL;
   float _unitR;

};

/////////////////////////////////////////////////////////////////////

struct UvRailSelection
{
    std::set<int> _selectedEdges;
    std::set<int> _selectedEdgeVerts;
    std::vector<int> _railStartVerts;
    std::vector<int> _railStartEdges;
};

/////////////////////////////////////////////////////////////////////

struct RailComputeCtx
{
   bool compute( const utilmesh::TopoMesh& umesh,
                 UvRailSelection& railsel,
                 const std::vector<int>& orderedFaces );

   int _selectedEdge;
   int _end;
   std::vector<RailFace> _railFaces;
   std::vector<int> _railVerticesA;
   std::vector<int> _railVerticesB;

};

/////////////////////////////////////////////////////////////////////

enum UvRailUserMode 
{
   UVRMODE_OVERLAPPED = 0,
   UVRMODE_UNITIZED,
   UVRMODE_NONE
};

///////////////////////////////////////////////////////////////////////////////

struct UvRailUserOptions
{
   UvRailUserMode _mode = UVRMODE_NONE;
   bool _flipVdirection = false;
};

///////////////////////////////////////////////////////////////////////////////

enum UvRailGeomType
{
   UVRGEOM_LOOP = 0,
   UVRGEOM_STRIP,
   UVRGEOM_NONE
};

///////////////////////////////////////////////////////////////////////////////

struct UvRailGeomCheck
{
   UvRailGeomCheck();

   void categorize(
         const int selectedEdge,
         const utilmesh::TopoMesh& topomesh,
         const UvRailSelection& railsel,
         MDagPath& meshDagPath );

   int _start;
   int _end;
   std::vector<int> _orderedFaces;
   UvRailGeomType _geometryType;
};

/////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace maya {
