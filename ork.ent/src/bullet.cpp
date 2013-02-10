////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/bullet.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>

#include<ork/math/PIDController.h>


///////////////////////////////////////////////////////////////////////////////

static const bool USE_GIMPACT = true;
extern bool USE_THREADED_RENDERER;
extern bool bFIXEDTIMESTEP;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

btVector3 orkv3tobtv3( const ork::CVector3& v3 )
{
	return btVector3( v3.GetX(), v3.GetY(), v3.GetZ() );
}
ork::CVector3 btv3toorkv3( const btVector3& v3 )
{
	return ork::CVector3( float(v3.x()), float(v3.y()), float(v3.z()) );
}

///////////////////////////////////////////////////////////////////////////////

btTransform orkmtx4tobtmtx4( const ork::CMatrix4& mtx )
{	btTransform xf;
	ork::CVector3 position = mtx.GetTranslation();
	xf.setOrigin( ! position );
	btMatrix3x3& mtx33 = xf.getBasis();
	for( int i=0; i<3; i++ )
	{	float fx = mtx.GetElemYX(i,0);
		float fy = mtx.GetElemYX(i,1);
		float fz = mtx.GetElemYX(i,2);
		mtx33[i] = btVector3( fx, fy, fz );
	}
	return xf;
}

ork::CMatrix4 btmtx4toorkmtx4( const btTransform& mtx )
{	ork::CMatrix4 rval;
	ork::CVector3 position = ! mtx.getOrigin();
	rval.SetTranslation( position );
	const btMatrix3x3& mtx33 = mtx.getBasis();
	for( int i=0; i<3; i++ )
	{	const btVector3& vec = mtx33.getColumn(i);
		rval.SetElemXY(i,0,float(vec.x()));
		rval.SetElemXY(i,1,float(vec.y()));
		rval.SetElemXY(i,2,float(vec.z()));
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

btMatrix3x3 orkmtx3tobtbasis( const ork::CMatrix3& mtx )
{	btMatrix3x3 xf;
	for( int i=0; i<3; i++ )
	{	float fx = mtx.GetElemYX(i,0);
		float fy = mtx.GetElemYX(i,1);
		float fz = mtx.GetElemYX(i,2);
		xf[i] = btVector3( fx, fy, fz );
	}
	return xf;
}

ork::CMatrix3 btbasistoorkmtx3( const btMatrix3x3& mtx )
{	ork::CMatrix3 rval;
	for( int i=0; i<3; i++ )
	{	const btVector3& vec = mtx.getColumn(i);
		rval.SetElemXY(i,0,float(vec.x()));
		rval.SetElemXY(i,1,float(vec.y()));
		rval.SetElemXY(i,2,float(vec.z()));
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

EntMotionState::EntMotionState(const btTransform &initialpos, ork::ent::Entity *entity)
{
    mEntity = entity;
    mTransform = initialpos;
}

void EntMotionState::getWorldTransform(btTransform &transform) const
{
    transform = mTransform;
}

void EntMotionState::setWorldTransform(const btTransform &transform)
{
    if(mEntity)
	{
		/*const btVector3 &btOrigin = transform.getOrigin();
		btQuaternion btRotation = transform.getRotation();

		ork::CVector3 position = ! btOrigin;

		mEntity->GetDagNode().GetTransformNode().GetTransform()->SetPosition(position);

		ork::CQuaternion rotationQuat(float(btRotation.x()), float(btRotation.y()), float(btRotation.z()), float(btRotation.w()) );
		ork::CMatrix4 rotationMat;
		rotationMat.FromQuaternion(rotationQuat);

		mEntity->GetDagNode().GetTransformNode().GetTransform()->SetRotation(rotationMat);*/

		mEntity->GetDagNode().GetTransformNode().GetTransform()->SetMatrix( ! transform );
		mTransform = transform;
	}
}

static ork::PoolString sWorldString;

btBoxShape *XgmModelToBoxShape(const ork::lev2::XgmModel *xgmmodel,float fscale)
{
	const ork::CVector3 &xyz = xgmmodel->GetBoundingAA_XYZ();
	const ork::CVector3 &whd = xgmmodel->GetBoundingAA_WHD();
	const ork::CVector3 &center = xgmmodel->GetBoundingCenter();
	float radius = xgmmodel->GetBoundingRadius();
	float fsc = 0.5f*fscale;

	// Assumes center of box is at origin


	return new btBoxShape(btVector3(whd.GetX() * fsc, whd.GetY() * fsc, whd.GetZ() * fsc));
}

///////////////////////////////////////////////////////////////////////////////

btSphereShape *XgmModelToSphereShape(const ork::lev2::XgmModel *xgmmodel,float fscale)
{
	const ork::CVector3 &xyz = xgmmodel->GetBoundingAA_XYZ();
	const ork::CVector3 &whd = xgmmodel->GetBoundingAA_WHD();
	const ork::CVector3 &center = xgmmodel->GetBoundingCenter();
	float radius = xgmmodel->GetBoundingRadius()*fscale;

	// Assumes center of sphere is at origin
	return new btSphereShape(radius);
}

///////////////////////////////////////////////////////////////////////////////

btTriangleIndexVertexArray* XgmClusterToTriVertArray(const ork::lev2::XgmCluster &xgmcluster,float fscale)
{
	ork::lev2::GfxTarget* pTARG = ork::lev2::GfxEnv::GetRef().GetLoaderTarget();

	void* ploadtoken = pTARG->BeginLoad();

	btTriangleIndexVertexArray *indexVertexArrays = new btTriangleIndexVertexArray;

	const ork::lev2::VertexBufferBase* pVB = xgmcluster.GetVertexBuffer();

	for(int pg = 0; pg < xgmcluster.GetNumPrimGroups(); pg++)
	{
		const ork::lev2::XgmPrimGroup &xgmprimgroup = xgmcluster.RefPrimGroup(pg);

		lev2::EPrimitiveType ept = xgmprimgroup.GetPrimType();

		btIndexedMesh mesh;

		int inumindices = xgmprimgroup.miNumIndices;
		int inumvertices = pVB->GetNumVertices();

		const ork::lev2::IndexBufferBase*	ibase = xgmprimgroup.GetIndexBuffer();

		const void* pIBDATA = pTARG->GBI()->LockIB( *ibase );
		const void* pVBDATA = pTARG->GBI()->LockVB( *pVB );
	
		if( pIBDATA && pVBDATA )
		{
			std::vector<u16> Indices;
		
			const u16* psrcIDC = (const u16*) pIBDATA;
					
			switch( ept )
			{
				case lev2::EPRIM_TRIANGLESTRIP:
				{	
					for( int ii=0; ii<(inumindices-2); ii++ )
					{
						u16 idx0 = psrcIDC[ii+0];
						u16 idx1 = psrcIDC[ii+1];
						u16 idx2 = psrcIDC[ii+2];
						
						bool bOK = 
							( idx0!=idx1 )
							&& ( idx0!=idx2 )
							&& ( idx1!=idx2 );
						
						if( bOK )
						{
							Indices.push_back( idx0 );
							Indices.push_back( idx1 );
							Indices.push_back( idx2 );
						}
					}
					break;
				}
				case lev2::EPRIM_TRIANGLES:
				{	
					for( int ii=0; ii<inumindices; ii+=3 )
					{
						u16 idx0 = psrcIDC[ii+0];
						u16 idx1 = psrcIDC[ii+1];
						u16 idx2 = psrcIDC[ii+2];
						
						bool bOK = 
							( idx0!=idx1 )
							&& ( idx0!=idx2 )
							&& ( idx1!=idx2 );
						
						if( bOK )
						{
							Indices.push_back( idx0 );
							Indices.push_back( idx1 );
							Indices.push_back( idx2 );
						}
					}
					break;
				}
				default:
					OrkAssert(false);
					break;
			}
			inumindices = Indices.size();
			OrkAssert( (inumindices%3) == 0 );

			////////////////////////////////////////////////////////

			U16* pnewU16 = new U16[ inumindices ];
			for( int i=0; i<inumindices; i++ )
			{
				//printf( "index<%d>\n", Indices[i] );
				pnewU16[i] = Indices[i];
			}
			mesh.m_triangleIndexBase = (const unsigned char *)pnewU16;
			mesh.m_triangleIndexStride = 3 * sizeof(U16);
			mesh.m_indexType = PHY_SHORT;
			mesh.m_numVertices = inumvertices;
			mesh.m_numTriangles = inumindices/3;

			////////////////////////////////////////////////////////

			lev2::EVtxStreamFormat efmt = pVB->GetStreamFormat();

			switch( efmt )
			{
				////////////////////////////////////////////////////////
				case lev2::EVTXSTREAMFMT_V12N12B12T8C4:
				{	const int knfloats = 9;
					btScalar* pVERTS = new btScalar[ mesh.m_numVertices*knfloats ];
					mesh.m_vertexBase = (const unsigned char*) pVERTS;
					mesh.m_vertexStride = knfloats*sizeof(btScalar);
					const ork::lev2::SVtxV12N12B12T8C4* pVBDATAT = (const ork::lev2::SVtxV12N12B12T8C4*) pVBDATA;
					for( int i=0; i<inumvertices; i++ )
					{	const ork::lev2::SVtxV12N12B12T8C4& src_vtx = pVBDATAT[i];
						float fv = src_vtx.mUV0.GetY();
						int j = i*knfloats;
						pVERTS[j+0] = src_vtx.mPosition.GetX()*fscale;
						pVERTS[j+1] = (src_vtx.mPosition.GetY()*fscale);//-(fv*0.1f);
						pVERTS[j+2] = src_vtx.mPosition.GetZ()*fscale;
						pVERTS[j+3] = src_vtx.mNormal.GetX();
						pVERTS[j+4] = src_vtx.mNormal.GetY();
						pVERTS[j+5] = src_vtx.mNormal.GetZ();
						pVERTS[j+6] = src_vtx.mBiNormal.GetX();
						pVERTS[j+7] = src_vtx.mBiNormal.GetY();
						pVERTS[j+8] = src_vtx.mBiNormal.GetZ();
					}
					break;
				}
				////////////////////////////////////////////////////////
				case lev2::EVTXSTREAMFMT_V12N12T16C4:
				{	const int knfloats = 3;
					btScalar* pVERTS = new btScalar[ mesh.m_numVertices*knfloats ];
					mesh.m_vertexBase = (const unsigned char*) pVERTS;
					mesh.m_vertexStride = knfloats*sizeof(btScalar);
					const ork::lev2::SVtxV12N12T16C4* pVBDATAT = (const ork::lev2::SVtxV12N12T16C4*) pVBDATA;
					for( int i=0; i<inumvertices; i++ )
					{	const ork::lev2::SVtxV12N12T16C4& src_vtx = pVBDATAT[i];
						float fv = src_vtx.mUV0.GetY();
						int j = i*knfloats;
						pVERTS[j+0] = src_vtx.mPosition.GetX()*fscale;
						pVERTS[j+1] = (src_vtx.mPosition.GetY()*fscale);//-(fv*0.1f);
						pVERTS[j+2] = src_vtx.mPosition.GetZ()*fscale;
						//pVERTS[j+3] = src_vtx.mNormal.GetX();
						//pVERTS[j+4] = src_vtx.mNormal.GetY();
						//pVERTS[j+5] = src_vtx.mNormal.GetZ();
					}
					break;
				}
				////////////////////////////////////////////////////////
				default:
					printf( "VertexFormat<%d> not supported for physics\n", int(efmt) );
					OrkAssert( false );
					break;
			}
			indexVertexArrays->addIndexedMesh(mesh, PHY_SHORT);

		}
		pTARG->GBI()->UnLockIB( *ibase );
		pTARG->GBI()->UnLockVB( *pVB );
	}

	pTARG->EndLoad(ploadtoken);
	
	return indexVertexArrays;
}

///////////////////////////////////////////////////////////////////////////////

btCompoundShape *XgmModelToCompoundShape(const ork::lev2::XgmModel *xgmmodel,float fscale)
{
	btCompoundShape *compoundShape = new btCompoundShape;

	for(int m = 0; m < xgmmodel->GetNumMeshes(); m++)
	{
		const ork::lev2::XgmMesh *xgmmesh = xgmmodel->GetMesh(m);

		btCompoundShape *subCompoundShape = XgmMeshToCompoundShape(xgmmesh,fscale);

		btTransform tr;
		tr.setIdentity();
		compoundShape->addChildShape(tr, subCompoundShape);
	}

	return compoundShape;
}

///////////////////////////////////////////////////////////////////////////////

btCompoundShape *XgmMeshToCompoundShape(const ork::lev2::XgmMesh *xgmmesh,float fscale)
{
	btCompoundShape *compoundShape = new btCompoundShape;

	for(int sm = 0; sm < xgmmesh->GetNumSubMeshes(); sm++)
	{
		const ork::lev2::XgmSubMesh *submesh = xgmmesh->GetSubMesh(sm);
		for(int c = 0; c < submesh->GetNumClusters(); c++)
		{
			const ork::lev2::XgmCluster &xgmcluster = submesh->RefCluster(c);

			if(btCollisionShape* shape = XgmClusterToBvhTriangleMeshShape(xgmcluster,fscale))
			{
				btTransform tr;
				tr.setIdentity();
				compoundShape->addChildShape(tr, shape);
			}
		}
	}

	return compoundShape;
}

///////////////////////////////////////////////////////////////////////////////

btCollisionShape *XgmClusterToBvhTriangleMeshShape(const ork::lev2::XgmCluster &xgmcluster,float fscale)
{
	btTriangleIndexVertexArray* arrays = XgmClusterToTriVertArray(xgmcluster,fscale);
	
	btVector3 aabbMin, aabbMax;
	arrays->calculateAabbBruteForce(aabbMin, aabbMax);

	btBvhTriangleMeshShape* pshape = new btBvhTriangleMeshShape(arrays
//		, /*useQuantizedAabbCompression=*/false, aabbMin, aabbMax);
		, /*useQuantizedAabbCompression=*/true, aabbMin, aabbMax);
	//pshape->buildOptimizedBvh();
	pshape->getOptimizedBvh();

	printf( "MeshAABB min<%f %f %f> max<%f %f %f>\n",
		aabbMin.getX(), aabbMin.getY(), aabbMin.getZ(),
		aabbMax.getX(), aabbMax.getY(), aabbMax.getZ() );		

	return pshape ;
}

///////////////////////////////////////////////////////////////////////////////

btCollisionShape* XgmClusterToGimpactMeshShape(const ork::lev2::XgmCluster &xgmcluster,float fscale)
{
	btCollisionShape* rval = 0;

	btTriangleIndexVertexArray* arrays = XgmClusterToTriVertArray(xgmcluster,fscale);

	btVector3 aabbMin, aabbMax;
	arrays->calculateAabbBruteForce(aabbMin, aabbMax);

	if( USE_GIMPACT )
	{
	btVector3 scale(fscale,fscale,fscale);
		btGImpactConvexDecompositionShape* pshape = new btGImpactConvexDecompositionShape(arrays,scale);
		pshape->updateBound();
		rval = pshape;
	}
	else
	{	btGImpactMeshShape* pshape = new btGImpactMeshShape(arrays);
		rval = pshape;
	}
 //
	printf( "MeshAABB min<%f %f %f> max<%f %f %f>\n",
		aabbMin.getX(), aabbMin.getY(), aabbMin.getZ(),
		aabbMax.getX(), aabbMax.getY(), aabbMax.getZ() );		

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

btCompoundShape* XgmMeshToGimpactShape(const ork::lev2::XgmMesh *xgmmesh,float fscale)
{
	btCompoundShape *compoundShape = new btCompoundShape;

	for(int sm = 0; sm < xgmmesh->GetNumSubMeshes(); sm++)
	{
		const ork::lev2::XgmSubMesh *submesh = xgmmesh->GetSubMesh(sm);
		for(int c = 0; c < submesh->GetNumClusters(); c++)
		{
			const ork::lev2::XgmCluster &xgmcluster = submesh->RefCluster(c);

			if(btCollisionShape* shape = XgmClusterToGimpactMeshShape(xgmcluster,fscale))
			{
				btTransform tr;
				tr.setIdentity();
				compoundShape->addChildShape(tr, shape);
			}
		}
	}

	return compoundShape;
}

///////////////////////////////////////////////////////////////////////////////

btCollisionShape* XgmModelToGimpactShape(const ork::lev2::XgmModel *xgmmodel,float fscale)
{
	btCompoundShape *compoundShape = new btCompoundShape;

	for(int m = 0; m < xgmmodel->GetNumMeshes(); m++)
	{
		const ork::lev2::XgmMesh *xgmmesh = xgmmodel->GetMesh(m);

		btCollisionShape *subCompoundShape = XgmMeshToGimpactShape(xgmmesh,fscale);

		btTransform tr;
		tr.setIdentity();
		compoundShape->addChildShape(tr, subCompoundShape);
	}

	return compoundShape;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} } // namespace ork { namespace ent
