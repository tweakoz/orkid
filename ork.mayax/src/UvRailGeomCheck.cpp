////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "UvRail.h"
#include <assert.h>

namespace ork { namespace maya {

using namespace utilmesh;

///////////////////////////////////////////////////////////////////////////////

UvRailGeomCheck::UvRailGeomCheck() 
    : _geometryType(UVRGEOM_NONE)
    , _start(INVALID_INDEX)
    , _end(INVALID_INDEX)
{

}

///////////////////////////////////////////////////////////////////////////////

void UvRailGeomCheck::categorize(
    const int selectedEdge,
    const TopoMesh& topomesh,
    const UvRailSelection& railsel,
    MDagPath& meshDagPath )
{   
    size_t inumendface = topomesh._endFaces.size();
    MStatus status;
    auto meshobj = meshDagPath.node();
    MItMeshEdge itEdge( meshobj, & status );
    int previndex = -1;
    status = itEdge.setIndex( selectedEdge, previndex );
    int numfacesconnectedtostartingedge = 0;
    status = itEdge.numConnectedFaces( numfacesconnectedtostartingedge );
    MIntArray edgefaces;
    int inum = itEdge.getConnectedFaces( edgefaces, & status );

    logMessage( "UvRail: GeomCheck - inumendface<%d> numfacesconnectedtostartingedge<%d>", inumendface, numfacesconnectedtostartingedge );

    if(    inumendface>=2 
        && numfacesconnectedtostartingedge!=1 )
    {
        logError( "UvRail: Error - Check that you did not select a middle edge (use an endpoint edge for strips)" );
        _geometryType = UVRGEOM_NONE;
        return;
    }
    switch( inumendface )
    {   ///////////////////////
        // zero end faces means a loop was selected
        ///////////////////////
        case 0: 
        {   if( numfacesconnectedtostartingedge == 2 )
            {   _start = edgefaces[0];
                _end = edgefaces[1];
                _geometryType = UVRGEOM_LOOP;
            }
            break;
        }
        ///////////////////////
        // two end faces means a strip was selected
        ///////////////////////
        case 2: 
        {   auto it = topomesh._endFaces.begin();
            _start = *it;
            it++;
            _end = *it;
            int rsvtx0 = railsel._railStartVerts[0];
            int rsvtx1 = railsel._railStartVerts[1];
            int mapvtxidx0 = topomesh._vertexMap.mappedIndex( rsvtx0 );
            int mapvtxidx1 = topomesh._vertexMap.mappedIndex( rsvtx1 );
            const auto& vtx0 = topomesh.vertex( mapvtxidx0 );
            const auto& vtx1 = topomesh.vertex( mapvtxidx1 );
            if( vtx0._connectedFaces.isConnected(_end))
            {   if( vtx1._connectedFaces.isConnected( _end ) )
                {   _geometryType = UVRGEOM_STRIP;
                    // must start at selected edge
                    std::swap(_start,_end);
                }
            }
            else if(vtx0._connectedFaces.isConnected(_start ))
            {   if(vtx1._connectedFaces.isConnected(_start ))
                    _geometryType = UVRGEOM_STRIP;
            }
            break;
        }
        ///////////////////////
        // we only do loops and strips
        ///////////////////////
        default:
            break;
    }

    ///////////////////////////////
    // output debug info
    ///////////////////////////////

    switch( _geometryType )
    {
        case UVRGEOM_STRIP:
            logMessage( "UvRail: Using Strip Operation" );
            break;
        case UVRGEOM_LOOP:
            logMessage( "UvRail: Using Loop Operation" );
            break;
        default:
            logError( "UvRail: Error - Invalid Operation (Make sure only quads are selected)" );
            break;
    }

    logMessage( "UvRail: _start<%d> _end<%d>", _start, _end );

    ///////////////////////////////
    // order faces
    //  ensure geometry is traversable
    ///////////////////////////////

    std::set<int> visited;
    int currentFace = _start;

    logMessage( "UvRail: ordering faces.." );

    if( _geometryType!=UVRGEOM_NONE)
    {
        while( INVALID_INDEX != currentFace )
        {   size_t orderedFaceIndex = _orderedFaces.size();
            //logMessage( "currentFace<%d>", currentFace );
            //logMessage( "orderedFaceIndex<%d>", orderedFaceIndex );
            visited.insert(currentFace);
            _orderedFaces.push_back( currentFace );
            int mapped = topomesh._faceMap.mappedIndex( currentFace );
            //logMessage( "mapped<%d>", mapped );
            if( INVALID_INDEX != mapped )
            {   const auto& face = topomesh.face( mapped ); 
                int cface0 = face._connectedFaces.connected(0);
                int cface1 = face._connectedFaces.connected(1);
                //logMessage( "cface0<%d>", cface0 );
                //logMessage( "cface1<%d>", cface1 );
                if(    (UVRGEOM_LOOP == _geometryType) 
                    && (0==orderedFaceIndex) ) 
                {   if( cface0 == _end ) cface0=cface1;
                    if( cface1 == _end ) cface1=cface0;
                    if( cface1 == _end ) _geometryType = UVRGEOM_NONE;
                }
                if( ! itemInSet(visited,cface0) )
                    currentFace = cface0;
                else if( ! itemInSet(visited,cface1) )
                    currentFace = cface1;
                //logMessage( "new-currentFace<%d>", currentFace );
            }
        }
    }
}

}} //namespace ork { namespace maya {
