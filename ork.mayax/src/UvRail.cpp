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

static MStatus UvRailImpl(const UvRailUserOptions& options);

///////////////////////////////////////////////////////////////////////////////

void* UvRailOverlappedCommand::instantiate()
{
   return new UvRailOverlappedCommand;
}

///////////////////////////////////////////////////////////////////////////////

MStatus UvRailOverlappedCommand::doIt(const MArgList& args) // final
{
    UvRailUserOptions opt;
    opt._mode = UVRMODE_OVERLAPPED;
	return UvRailImpl(opt);
}

///////////////////////////////////////////////////////////////////////////////

void* UvRailUnitizedCommand::instantiate()
{
   return new UvRailUnitizedCommand;
}

///////////////////////////////////////////////////////////////////////////////

MStatus UvRailUnitizedCommand::doIt(const MArgList& args) // final
{
    UvRailUserOptions opt;
    opt._mode = UVRMODE_UNITIZED;
    return UvRailImpl(opt);
}

///////////////////////////////////////////////////////////////////////////////

static UvRailSelection UvRailGetSelected(MDagPath& meshDagPath)
{
    UvRailSelection output;
	MSelectionList selList;
	MGlobal::getActiveSelectionList(selList);
	MItSelectionList edgeselList( selList, MFn::kMeshEdgeComponent );

	while ( false == edgeselList.isDone() )
	{   MObject edgeComponents;
		edgeselList.getDagPath(meshDagPath, edgeComponents);
		MItMeshEdge itEdge( meshDagPath, edgeComponents );
		while( false == itEdge.isDone() )
		{   MStatus Status;
			int edgeIndex = itEdge.index( & Status );
			if( false == itemInSet(output._selectedEdges, edgeIndex) )
			{   output._selectedEdges.insert( edgeIndex );
				output._railStartEdges.push_back( edgeIndex );
				int vtx0 = itEdge.index( 0, & Status );
				int vtx1 = itEdge.index( 1, & Status );
				if( output._selectedEdgeVerts.end() == output._selectedEdgeVerts.find( vtx0 ) )
				{   output._selectedEdgeVerts.insert( vtx0 );
					output._railStartVerts.push_back( vtx0 );
				}
				if( output._selectedEdgeVerts.end() == output._selectedEdgeVerts.find( vtx1 ) )
				{   output._selectedEdgeVerts.insert( vtx1 );
					output._railStartVerts.push_back( vtx1 );
				}
			}
			itEdge.next();
		}
		edgeselList.next();
	}
    return output;
}

///////////////////////////////////////////////////////////////////////////////

RailFace::RailFace()
  : _faceIndex(-1)
  , _vertTL(-1)
  , _vertTR(-1)
  , _vertBL(-1)
  , _vertBR(-1)
  , _uvTL(-1)
  , _uvTR(-1)
  , _uvBL(-1)
  , _uvBR(-1)
  , _unitL(0.0f)
  , _unitR(0.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

bool RailComputeCtx::compute( const TopoMesh& topomesh,
                              UvRailSelection& railsel,
                              const std::vector<int>& orderedFaces )
{
    size_t inumorderedfaces = orderedFaces.size();
	logMessage( "UvRail: inumorderedfaces<%d>", inumorderedfaces );

    if( inumorderedfaces < 5 )
    {
        logError( "UvRail: Error - must have at least 5 faces selected" );
        return false;
    }
    else
    {   
        int numstartverts = railsel._railStartVerts.size();

        if( numstartverts != 2 )
        {
            logError( "UvRail: Error - must have 2 start verts, there are now [%d]", numstartverts );
            return false;
        }
        else
        {
            //////////////////////////////////////////////
            // find the first edges
            //////////////////////////////////////////////

			const int ivTL = railsel._railStartVerts[0];
			const int ivBL = railsel._railStartVerts[1];
			_railVerticesA.push_back( ivTL );
            _railVerticesB.push_back( ivBL );

            //////////////////////////////////////////////
            logMessage( "UvRail: first edges ivTL<%d> ivBL<%d>\n", ivTL, ivBL );
            //////////////////////////////////////////////

            //////////////////////////////////////////////
            logMessage( "UvRail: traversing rail faces" );
            //////////////////////////////////////////////

			std::set<int> topSet;
			std::set<int> botSet;
			topSet.insert(ivTL);
			botSet.insert(ivBL);
			int ivaTL = ivTL;
			int ivbTL = ivTL;
			int ivaBL = ivBL;
			int ivbBL = ivBL;
			for( int orderedFaceIndex=0; 
                 orderedFaceIndex<int(inumorderedfaces);
                 orderedFaceIndex++ )
			{
				int mface = orderedFaces[orderedFaceIndex];
				int imf = topomesh._faceMap.mappedIndex( mface );
				const auto& the_face = topomesh.face(imf);
				const auto& edges = the_face._connectedEdges;

				auto ComputeRailFace = [](	const TopoMesh& topomesh,
    										const Connections& edges,
	   									    std::set<int>& ExclSet,
		  								    std::vector<int>& RailVect,
										    int& ivTHIS,
										    int& ivOTH )

				{	auto eset = edges._connections._set;
					while( eset.empty() == false )
					{	
                        int iee = *eset.begin();
						eset.erase(eset.begin());
						const auto& the_edge = topomesh.mappedEdge(iee);
						int iv0 = the_edge._vertex[0];
						int iv1 = the_edge._vertex[1];
						bool bhastl = (iv0==ivTHIS) || (iv1==ivTHIS);
						bool bhasbl = (iv0==ivOTH) || (iv1==ivOTH);
						
                        /////////////////////////////////////////////////
						// the correct top edge is an edge which is 
                        // connected to ivTL, is not connected to ivBL
						// and has a vert not yet in topSet
						/////////////////////////////////////////////////
						
                        if( bhastl && (false==bhasbl) )
						{	int inewv = -1;
							if( ! itemInSet(ExclSet,iv0))
							{	inewv = iv0;
								assert( inewv != ivTHIS );
							}
							else if( ! itemInSet(ExclSet,iv1))
							{	inewv = iv1;
								assert( inewv != ivTHIS );
							}
							if( inewv>=0 )
							{	ExclSet.insert(inewv);
								RailVect.push_back(inewv);
								ivTHIS = inewv;
								eset.clear();
								
                                ///////////////////
								// update ivBL
								// new ivBL is on an edge connected to inewv
								// it is also connected to an edge connected to ivBL
								///////////////////
								
                                auto eset2 = edges._connections._set;
								eset2.erase( eset2.find(iee) );
								while( eset2.empty() == false )
								{	int iee = *eset2.begin();
									eset2.erase(eset2.begin());
									const TopoEdge& the_edge = topomesh.mappedEdge(iee);
									if( the_edge._connectedVerts.isConnected(inewv) )
									{	int iv2 = the_edge._vertex[0];
										if( iv2 == inewv ) iv2 = the_edge._vertex[1];
										ivOTH = iv2;
										eset2.clear();
									}
								}
							}
						}
					}			
				};
				/////////////////////////////////////////////////////////////////
				// Compute Rails
				/////////////////////////////////////////////////////////////////
				ComputeRailFace( topomesh, edges, topSet, _railVerticesA, ivaTL, ivaBL );
				ComputeRailFace( topomesh, edges, botSet, _railVerticesB, ivbBL, ivbTL );
                /////////////////////////////////////////////////////////////////
			} 
        } 
    } 
	
    ///////////////////////////////////
    // rail vertices have been created,
    //   now generate rail faces
    ///////////////////////////////////

    logMessage( "UvRail: generating rail faces" );

	int numA = int(_railVerticesA.size());
	int numB = int(_railVerticesB.size());
	int numF = int(orderedFaces.size());

	logMessage( "UvRail: numA<%d> numB<%d> numF<%d>", numA, numB, numF );

	for( int f=0; f<numF; f++ )
	{
		int l = f+0;
		int r = f+1;

		float fL = float(l)/float(numA-1);
		float fR = float(r)/float(numA-1);

		RailFace rf;
		rf._faceIndex = orderedFaces[f];
		
		rf._vertTL = _railVerticesA[l%_railVerticesA.size()];
		rf._vertTR = _railVerticesA[r%_railVerticesA.size()];
		rf._vertBL = _railVerticesB[l%_railVerticesB.size()];
		rf._vertBR = _railVerticesB[r%_railVerticesB.size()];
		rf._unitL = float(f)/float(numF);
		rf._unitR = float(f+1)/float(numF);

		_railFaces.push_back( rf );

	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

static MStatus UvRailImpl(const UvRailUserOptions& options)
{
    bool overlapped = options._mode==UVRMODE_OVERLAPPED;

    MStatus Status;
    TopoMesh topomesh;

    ////////////////////////////////////////////////////////
    // get mesh
    ////////////////////////////////////////////////////////

    MSelectionList selList;
    MGlobal::getActiveSelectionList(selList);
    MDagPath meshDagPath;
    selList.getDagPath( 0, meshDagPath);
    MFnMesh fnMesh( meshDagPath, &Status );

    ////////////////////////////////////////////////////////
    // get uvset info
    ////////////////////////////////////////////////////////

    MString uvsetName( "map1" ); // default
    fnMesh.getCurrentUVSetName( uvsetName ); // override if present

    ////////////////////////////////////////////////////////
    // initialize topomesh
    ////////////////////////////////////////////////////////

    topomesh.initFromSelection( meshDagPath );

    ////////////////////////////////////////////////////////
    // Get Selected Edge (Starting point of UV strip)
    ////////////////////////////////////////////////////////

    auto railsel = UvRailGetSelected( meshDagPath );

    int numfaces = topomesh._faceMap.size();
    int numedges = topomesh._edgeMap.size();
    int numvertices = topomesh._vertexMap.size();

    logMessage( "UvRail: numFaces<%d> numEdges<%d> numVertices<%d>", numfaces, numedges, numvertices );

    /////////////////////////////////////////////////////////
    // Check to see that we have 1 selected edge
    /////////////////////////////////////////////////////////

	if( railsel._selectedEdges.size() == 1 )
    {
		////////////////////////////////////////////////////////////////
		// compute mesh topology
		////////////////////////////////////////////////////////////////

        int selectedEdgeIndex = *railsel._selectedEdges.begin();

		topomesh.computeTopology(meshDagPath);

        //////////////////////////////////////////////
        // further categorize selected geometry
        //  (check if its a loop or strip, etc...)
        //////////////////////////////////////////////

		UvRailGeomCheck geomcheck;
		geomcheck.categorize(selectedEdgeIndex,topomesh,railsel,meshDagPath);

        //////////////////////////////////////////////
        // set the new UV data on the mesh
        //////////////////////////////////////////////

        if( UVRGEOM_NONE != geomcheck._geometryType )
        {	
			///////////////////////////////////////////////
			// compute the uv rails
            ///////////////////////////////////////////////

            RailComputeCtx ctx;
			ctx._selectedEdge = selectedEdgeIndex;
			bool railsCreatedOK = ctx.compute( topomesh,railsel,geomcheck._orderedFaces );
			
            ///////////////////////////////////////////////
            // if rails computed successfully,
            //  apply to maya mesh
            ///////////////////////////////////////////////

            if( railsCreatedOK )
            {   
                int uvbase = fnMesh.numUVs( uvsetName, & Status  );
				int numfaces = int( ctx._railFaces.size() );
				int newuvindex = uvbase;

				///////////////////////////////////////////////
				// Generate new UVS
				///////////////////////////////////////////////

                bool do_flip = options._flipVdirection;

				const float v_t = do_flip 
                                ? 1.0f 
                                : 0.0f;
				const float v_b = do_flip
                                ? 0.0f 
                                : 1.0f;
				
                logMessage( "UvRail: generate UVS do_flip<%d> numfaces<%d>", int(do_flip), numfaces );

                for( int irf=0; irf<numfaces; irf++ )
				{	
                    RailFace& rf = ctx._railFaces[irf];
					
                    /////////////////////////////////////////
                    // overlap? - also do left side (for all 4)
                    /////////////////////////////////////////

                    bool bdoleft = overlapped ? (irf%16==0) : irf==0;
										
                    if( bdoleft ) 
					{	int iTL = rf._vertTL;
						int iBL = rf._vertBL;
						rf._uvTL = newuvindex++;
						rf._uvBL = newuvindex++;
						rf._unitL = 0.0f;
						rf._unitR = 1.0f;
                        Status = fnMesh.setUV( rf._uvTL, 0.0f, v_t, & uvsetName );
                        Status = fnMesh.setUV( rf._uvBL, 0.0f, v_b, & uvsetName );
					}
					
                    /////////////////////////////////////////
					else // irf is definately not 0, 
                         //  reuse right side of last
					/////////////////////////////////////////
					
                    {	const RailFace& prf = ctx._railFaces[irf-1];
						rf._uvTL = prf._uvTR;
						rf._uvBL = prf._uvBR;
						rf._unitL = prf._unitR;
						rf._unitR = prf._unitR+1.0f;
					}
					
                    /////////////////////////////////////////
					// do right side
					/////////////////////////////////////////
					
                    rf._uvTR = newuvindex++;
					rf._uvBR = newuvindex++;
                    Status = fnMesh.setUV( rf._uvTR, rf._unitR, v_t, & uvsetName );
                    Status = fnMesh.setUV( rf._uvBR, rf._unitR, v_b, & uvsetName );
				}
				
                ///////////////////////////////////////////////
				// Assign UVS
				///////////////////////////////////////////////
				
                int numuvs = fnMesh.numUVs(&Status);

                logMessage( "UvRail: assign UVS numuvs<%d>", numuvs );
                
                for( int irf=0; irf<numfaces; irf++ )
				{	const RailFace& rf = ctx._railFaces[irf];
                    MItMeshPolygon itFace( meshDagPath.node(), & Status );
					int previndex = -1;
					Status = itFace.setIndex( rf._faceIndex, previndex );
                    int numverticesinface = itFace.polygonVertexCount( & Status );

                    for( int v=0; v<numverticesinface; v++ )
                    {	
						int vi = itFace.vertexIndex( v, &Status );

						if( vi == rf._vertTL )
                        {   assert( rf._uvTL<numuvs );
							assert( rf._uvTL>=0 );
							fnMesh.assignUV( rf._faceIndex, v, rf._uvTL, & uvsetName );
						}
                        if( vi == rf._vertTR )
                        {   assert( rf._uvTR<numuvs );
							assert( rf._uvTR>=0 );
							fnMesh.assignUV( rf._faceIndex, v, rf._uvTR, & uvsetName );
                        }
                        if( vi == rf._vertBL )
                        {   assert( rf._uvBL<numuvs );
							assert( rf._uvBL>=0 );
							fnMesh.assignUV( rf._faceIndex, v, rf._uvBL, & uvsetName );
                        }
                        if( vi == rf._vertBR )
                        {   assert( rf._uvBR<numuvs );
							assert( rf._uvBR>=0 );
							fnMesh.assignUV( rf._faceIndex, v, rf._uvBR, & uvsetName );
                        }
                    }
				}
            }
        }
    }
    ////////////////////////////////////////////////////////////////
	else // uh oh, didnt have 1 edge selected
    ////////////////////////////////////////////////////////////////
    {
		logError( "UvRail: Error - only 1 starting edge can be selected!" );
        return MS::kFailure;
    }

    logMessage( "UvRail complete..." );

	return MS::kSuccess;
}

}} //namespace ork { namespace maya {
