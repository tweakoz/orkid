#pragma once

#include "OrkUtils.h"
#include "MayaIncludes.h"

namespace ork { namespace maya { namespace utilmesh {
///////////////////////////////////////////////////////////////////////////////

static const int NOT_CONNECTED = -1;
static const int INVALID_INDEX = -1;

///////////////////////////////////////////////////////////////////////////////
// struct Connections maintains 
//   connection lists between components
///////////////////////////////////////////////////////////////////////////////

struct Connections
{
    void connect( int idx );
    int connected( int idx ) const;
    int numConnected( void ) const;
    bool isConnected( int idx ) const;

    uniquevect<int> _connections;

};

///////////////////////////////////////////////////////////////////////////////
// Component base
//  any component has connections to other components
///////////////////////////////////////////////////////////////////////////////

struct TopoComponent
{   
    TopoComponent();

    Connections _connectedFaces;
    Connections _connectedEdges;
    Connections _connectedVerts;
    int         _componentIndex;
};

///////////////////////////////////////////////////////////////////////////////
// vertex component
///////////////////////////////////////////////////////////////////////////////

struct TopoVertex : public TopoComponent { };

///////////////////////////////////////////////////////////////////////////////
// edge component
///////////////////////////////////////////////////////////////////////////////

struct TopoEdge : public TopoComponent
{
    TopoEdge();
    
    int _vertex[2];
};

///////////////////////////////////////////////////////////////////////////////
// face component
///////////////////////////////////////////////////////////////////////////////

struct TopoFace : public TopoComponent { };

///////////////////////////////////////////////////////////////////////////////
// TopoIndexMap
///////////////////////////////////////////////////////////////////////////////

struct TopoIndexMap
{
    TopoIndexMap();
    
    void add( int srcidx );

    bool isMapped( int idx ) const;
    int size( void ) const { return _indexSet.size(); }
    int mappedIndex( int srcidx ) const;

    private:


    std::map<int,int> _indexMap;
    std::set<int>     _indexSet;

};

///////////////////////////////////////////////////////////////////////////////
// a topomesh is a representation of the
//  connections of a mesh's vertices, faces, and edges
//  no actual geometry is represented.
///////////////////////////////////////////////////////////////////////////////

struct TopoMesh
{
    TopoMesh();
    ~TopoMesh();
    ///////////////////////////////////////////////////////
    const TopoVertex& vertex( int vertindex ) const;
    const TopoEdge& edge( int edgeindex ) const;
    const TopoFace& face( int faceindex ) const;
    //////////////////////////////////////////////
    const TopoVertex& mappedVertex( int vertindex ) const;
    const TopoEdge& mappedEdge( int edgeindex ) const;
    TopoEdge& mappedEdge( int edgeindex );
    ///////////////////////////////////////////////////////    
    void initFromSelection( MDagPath& meshDagPath );
    void computeTopology( MDagPath& meshDagPath );

    TopoIndexMap               _vertexMap;
    TopoIndexMap               _edgeMap;
    TopoIndexMap               _faceMap;

    std::set<int> _endFaces;

    private:    

    std::vector<TopoVertex> _vertices;
    std::vector<TopoEdge> _edges;
    std::vector<TopoFace> _faces;
};

/////////////////////////////////////////////////////////////////////

}}} //namespace ork { namespace maya { namespace utilmesh 
