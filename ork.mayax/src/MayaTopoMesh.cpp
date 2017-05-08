////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "OrkUtils.h"
#include "MayaTopoMesh.h"

#include <assert.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace maya { namespace utilmesh {
///////////////////////////////////////////////////////////////////////////////
TopoComponent::TopoComponent()
    : _componentIndex(NOT_CONNECTED)
{
}
//////////////////////////////////////////////
TopoEdge::TopoEdge()
    : TopoComponent()
{   _vertex[0] = NOT_CONNECTED;
    _vertex[1] = NOT_CONNECTED;
}
/////////////////////////////////////////////////////
void Connections::connect( int idx )
{   _connections.merge(idx);
}
/////////////////////////////////////////////////////
int Connections::connected( int idx ) const
{   int rval = NOT_CONNECTED;
    if( idx < numConnected() )
        rval = _connections.itemAtIndex(idx);
    return rval;
}
/////////////////////////////////////////////////////
int Connections::numConnected( void ) const
{   return _connections.size();
}
/////////////////////////////////////////////////////
bool Connections::isConnected( int idx ) const
{   return itemInSet(_connections._set,idx);
}
/////////////////////////////////////////////////////
TopoIndexMap::TopoIndexMap() { }
///////////////////////////////////////////////////////
int TopoIndexMap::mappedIndex( int srcidx ) const
{   auto it = _indexMap.find(srcidx);
    return  (it==_indexMap.end()) 
          ? int(NOT_CONNECTED) 
          : it->second;
}
///////////////////////////////////////////////////////
bool TopoIndexMap::isMapped( int idx ) const
{   return ( _indexSet.end() != _indexSet.find( idx ) );
}
///////////////////////////////////////////////////////
void TopoIndexMap::add( int srcidx )
{   int mapped_index = NOT_CONNECTED;
    if( ! isMapped( srcidx ) )
    {   mapped_index = size();
        _indexSet.insert( srcidx );
        _indexMap.insert( std::make_pair( srcidx, mapped_index ) );
    }
}
//////////////////////////////////////////////
TopoMesh::TopoMesh() { }
//////////////////////////////////////////////
TopoMesh::~TopoMesh() { }
//////////////////////////////////////////////
void TopoMesh::initFromSelection( MDagPath& mayamesh )
{
	MSelectionList msellist;
	MGlobal::getActiveSelectionList(msellist);

	MItSelectionList selectedFaces( msellist, MFn::kMeshPolygonComponent );

    while ( false == selectedFaces.isDone() )
    {   
        MObject faces;
        selectedFaces.getDagPath(mayamesh, faces);
        MItMeshPolygon itFace(mayamesh, faces);
        
        while( false == itFace.isDone() )
        {   
            /////////////////////////////////////////////////////////////
            // merge faces
            /////////////////////////////////////////////////////////////

			MStatus status;
            int faceidx = itFace.index( & status );
            _faceMap.add( faceidx );

            /////////////////////////////////////////////////////////////
            // merge vertices
            /////////////////////////////////////////////////////////////

            MIntArray mverts;
            status = itFace.getVertices( mverts );
            int inumverts = mverts.length();
            for( int iv=0; iv<inumverts; iv++ )
            {
                int ivertidx = mverts[ iv ];
                _vertexMap.add( ivertidx );
            }

            /////////////////////////////////////////////////////////////
            // merge edges
            /////////////////////////////////////////////////////////////

            MIntArray medges;
            status = itFace.getEdges( medges );
            int inumedges = medges.length();
            for( int ie=0; ie<inumedges; ie++ )
            {
                int iedgeidx = medges[ ie ];
                _edgeMap.add( iedgeidx );
            }

            /////////////////////////////////////////////////////////////

            itFace.next();

        }

        selectedFaces.next();
    }
}
///////////////////////////////////////////////////////////////////////////////
// compute Topology
///////////////////////////////////////////////////////////////////////////////
void TopoMesh::computeTopology( MDagPath& mayamesh )
{
    logMessage( "TopoMesh::computeTopology\n" );

	MSelectionList msellist;
	MGlobal::getActiveSelectionList(msellist);

    _vertices.resize(_vertexMap.size());
    _edges.resize(_edgeMap.size());
    _faces.resize(_faceMap.size());

    //////////////////////////////////////
    // get the selected faces
    //////////////////////////////////////

	MItSelectionList selectedFaces( msellist, MFn::kMeshPolygonComponent );
    int facecnt = 0;
	MStatus status;

    logMessage( "getting selected faces" );

    while ( false == selectedFaces.isDone() )
    {   MObject faceComponents;
        selectedFaces.getDagPath(mayamesh, faceComponents);
        MItMeshPolygon itFace(mayamesh, faceComponents);

        //////////////////////////////////////
        // iterate through selected faces
        //////////////////////////////////////

        while( false == itFace.isDone() )
        {   
            int faceidx = itFace.index( & status );
            auto& face = _faces[facecnt++];
            face._componentIndex = faceidx;

            //logMessage( "faceidx<%d>", faceidx );

            //////////////////////////////////////
            // register face to face connections
            //////////////////////////////////////

            MIntArray mconface; 
            status = itFace.getConnectedFaces( mconface );
            int numcons = mconface.length();
            for( int i=0; i<numcons; i++ )
            {   int con = mconface[ i ];
                if( _faceMap.isMapped( con ) )
                {
                    //logMessage( "  con face<%d> -> mayaidx<%d>", i, con );
                    face._connectedFaces.connect( con );
                }
            }

            //////////////////////////////////////
            // detect end faces
            //  (faces which only have 1 connected face)
            //////////////////////////////////////

            if( face._connectedFaces.numConnected() == 1 ) 
            {   
                if( _endFaces.end() == _endFaces.find( faceidx ) )
                    _endFaces.insert( faceidx );
            }

            /////////////////////////////////////////////////////////////
            // register face to vertex connections
            /////////////////////////////////////////////////////////////

            MIntArray mverts;
            status = itFace.getVertices( mverts );
            int inumverts = mverts.length();

			if( inumverts!=4 )
				logMessage( "non-quad found! faceIndex<%d>", faceidx );

            for( int iv=0; iv<inumverts; iv++ )
            {   int mayavtxindex = mverts[ iv ];
                int mappedvtxindex = _vertexMap.mappedIndex( mayavtxindex );
                //logMessage( "  con vtx<%d> -> mayaidx<%d> mapidx<%d>", iv, mayavtxindex, mappedvtxindex );

                auto& vtx = _vertices[mappedvtxindex];
                face._connectedVerts.connect( mayavtxindex );
                vtx._connectedFaces.connect( faceidx );
            }

            /////////////////////////////////////////////////////////////
            // register face to edge connections
            /////////////////////////////////////////////////////////////

            MIntArray medges;
            status = itFace.getEdges( medges );
            int inumedges = medges.length();
            for( int E=0; E<inumedges; E++ )
            {   int mayaedgeindex = medges[ E ];
                face._connectedEdges.connect( mayaedgeindex );
                int mappededgeindex = _edgeMap.mappedIndex( mayaedgeindex );
                auto& meshedge = _edges[mappededgeindex];
                auto meshobj = mayamesh.node();
                MItMeshEdge itEdge( meshobj, & status );
				int previndex;
                itEdge.setIndex( mayaedgeindex, previndex );
                //////////////////////////////////////////////////////////
                MIntArray connectedFaces;
                int inumconfaces = itEdge.getConnectedFaces( connectedFaces, & status );
                for( int C=0; C<inumconfaces; C++ )
                {   int mayaconfaceindex = connectedFaces[ C ];
                    int mappedfaceindex = _faceMap.mappedIndex( mayaconfaceindex );
                    if( NOT_CONNECTED != mappedfaceindex ) 
                    {   meshedge._connectedFaces.connect( mayaconfaceindex );
                        auto& F = _faces[mappedfaceindex];
                        F._connectedEdges.connect( mayaedgeindex );
                    }
                }
                //////////////////////////////////////////////////////////
                int edgevertex0 = itEdge.index( 0, & status );
                meshedge._vertex[0] = edgevertex0;
                meshedge._connectedVerts.connect( edgevertex0 );
                int vertex0mapped = _vertexMap.mappedIndex( edgevertex0 );
                _vertices[vertex0mapped]._connectedEdges.connect( mayaedgeindex );
                //////////////////////////////////////////////////////////
                int edgevertex1 = itEdge.index( 1, & status );
                meshedge._vertex[1] = edgevertex1;
                meshedge._connectedVerts.connect( edgevertex1 );
                int vertex1mapped = _vertexMap.mappedIndex( edgevertex1 );
                _vertices[vertex1mapped]._connectedEdges.connect( mayaedgeindex );
            }
            /////////////////////////////////////////////////////////////
            itFace.next();
        }
        selectedFaces.next();
    }

	/////////////////////////////////////////////
	// register edge to edge connections
    /////////////////////////////////////////////

	int inumv = _vertexMap.size();
	for( int V=0; V<inumv; V++ )
	{   const auto& vtx = vertex(V);
	    const Connections& eset = vtx._connectedEdges;
		int inume = eset.numConnected();
		for( int ie=0; ie<inume; ie++ )
		{   int edgeamidx = eset.connected(ie);
			auto& edgea = mappedEdge( edgeamidx );
			int eav0 = edgea._vertex[0];
			int eav1 = edgea._vertex[1];
			
			if( eav0 != NOT_CONNECTED )
			{    for( int iv2=0; iv2<inumv; iv2++ )
				{   const auto& V2 = vertex(iv2);
					const Connections& eset2 = vtx._connectedEdges;
					int v2numconedges = eset2.numConnected();
					for( int ie2=0; ie2<v2numconedges; ie2++ )
					{  int edgebmidx = eset.connected(ie2);
						const auto& edgeb = mappedEdge( edgebmidx );
						int ebv0 = edgeb._vertex[0];
						int ebv1 = edgeb._vertex[1];
						if(    (edgeamidx!=edgebmidx) 
                            && (ebv0==eav0 || ebv1==eav1) )
							edgea._connectedEdges.connect(edgebmidx);
					}
				}
				
			}

			if( eav1 != NOT_CONNECTED )
			{    for( int iv2=0; iv2<inumv; iv2++ )
				{   const auto& V2 = vertex(iv2);
					const Connections& eset2 = vtx._connectedEdges;
					int v2numconedges = eset2.numConnected();
					for( int ie2=0; ie2<v2numconedges; ie2++ )
					{   int edgebmidx = eset.connected(ie2);
						const auto& edgeb = mappedEdge( edgebmidx );
						int ebv0 = edgeb._vertex[0];
						int ebv1 = edgeb._vertex[1];
						if(    (edgeamidx!=edgebmidx) 
                            && (ebv0==eav0 || ebv1==eav1) )
							edgea._connectedEdges.connect( edgebmidx );
					}
				}
				
			}
		}
	}
}
//////////////////////////////////////////////
const TopoFace& TopoMesh::face( int faceindex ) const
{   return _faces[ faceindex ];
}
//////////////////////////////////////////////
const TopoVertex& TopoMesh::vertex( int vertindex ) const
{   return _vertices[ vertindex ];
}
//////////////////////////////////////////////
const TopoEdge& TopoMesh::edge( int edgeindex ) const
{   return _edges[ edgeindex ];
}
//////////////////////////////////////////////
const TopoVertex& TopoMesh::mappedVertex( int vertindex ) const
{   int mapped = _vertexMap.mappedIndex(vertindex);
    return vertex(mapped);
}
//////////////////////////////////////////////
TopoEdge& TopoMesh::mappedEdge( int edgeindex )
{   int mapped = _edgeMap.mappedIndex(edgeindex);
    return _edges[mapped];
}
//////////////////////////////////////////////
const TopoEdge& TopoMesh::mappedEdge( int edgeindex ) const
{   int mapped = _edgeMap.mappedIndex(edgeindex);
    return edge(mapped);
}
/////////////////////////////////////////////////////
}}} //namespace ork { namespace maya {

