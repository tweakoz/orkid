////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>

//#include <entity/entity.h>
//#include <ork/entity/EnvironmentCollision.h>

namespace ork {
namespace tool {
		

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// SceneObject
/*
class SceneObjectEditorInterface : public ork::lev2::IEditorInterface
{
	public: //

	SceneObjectEditorInterface()
		: ork::lev2::IEditorInterface()
	{
	}
	virtual bool IsPickable( CObject *pObj ) const
	{
		return (cobject_downcast<Entity>(pObj)!=0);
	}
	virtual std::string GetName( CObject *pObj ) const
	{
		OrkAssert( pObj->GetClass()->IsSubclassOf( SceneObject::GetClassStatic() ) );
		return ((SceneObject*)pObj)->GetName();
	}
	virtual void PropertyChanged( CObject *pObj, CProp *pProp )
	{
		SceneObject *psceneobj = safe_cobject_downcast<SceneObject>(pObj);
		CClass *pclass = pObj->GetClass();
		if( pProp->GetName() == "Name" )
		{	Scene *pscene = psceneobj->GetScene();
			pscene->RemoveSceneObject( psceneobj );		// get rid of binding to old name
			std::string Name = pscene->ComputeNonCollidingName( psceneobj->GetName() );
			if( Name != psceneobj->GetName() ) // name not accepted by scene, use computed name
			{	psceneobj->SetName( Name.c_str() );
			}
			pscene->AddSceneObject( psceneobj );		// rebind to new name
		}
	}
	static IInterface::Context * GetInterface( void )
	{
		static SceneObjectEditorInterface EdIF;
		return (IInterface::Context *)& EdIF;
	}
	static void Register()
	{
		SceneObject::GetClassStatic()->AddInterface( ork::IInterface::EIFID_EDITOR, GetInterface );
	}

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Entity

class EntityTransformInterface : public ork::ITransformInterface
{
	public:

	static IInterface::Context * GetInterface( void )
	{
		static EntityTransformInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}

	virtual const TransformNode3D & GetTransform( CObject *pobj )
	{
		Entity *pent = (Entity*) pobj;
		TransformNode3D& xf = GetEntityTransformNode( pent );
		return xf;
	}

	virtual void SetTransform( CObject *pobj , const TransformNode3D &pMat )
	{
		Entity *pent = (Entity*) pobj;
		TransformNode3D& xf = GetEntityTransformNode( pent );
		xf = pMat;
	}
	static void Register()
	{
		Entity::GetClassStatic()->AddInterface( ork::IInterface::EIFID_XFORM, GetInterface );
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// 2D Collision Vertex

class EcicVertexEditorInterface : public ork::lev2::IEditorInterface
{
	public: //
	EcicVertexEditorInterface()
		: ork::lev2::IEditorInterface()
	{
	}
	virtual bool IsPickable( CObject *pObj ) const { return true; }
	virtual std::string GetName( CObject *pObj ) const
	{
		return CreateFormattedString("ecic_vertex_%08x", pObj );
	}
	virtual void PropertyChanged( CObject *pObj, CProp *pProp )
	{
	}
	static IInterface::Context * GetInterface( void )
	{
		static EcicVertexEditorInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}
	static void Register()
	{
		ecic_vertex::GetClassStatic()->AddInterface( ork::IInterface::EIFID_EDITOR,	GetInterface );
	}
};

class EcicVertexTransformInterface : public ork::ITransformInterface
{
	TransformNode3D transform;

	public:

	virtual const TransformNode3D & GetTransform( CObject *pobj )
	{
		if( EnvironmentCollision2DFactory::GetBoundDagObject() )
		{
			fmtx4 MatW = EnvironmentCollision2DFactory::GetBoundDagObject()->GetTransformNode().GetWorldTransform()->GetMatrix();

			ecic_vertex *pent = (ecic_vertex*) pobj;

			transform.GetWorldTransform()->SetMatrix( fmtx4::Identity );

			fvec4 xlate( pent->GetPosition().GetX(), pent->GetPosition().GetY(), float(0.0f) );

			transform.Translate( TransformNode3D::EMODE_ABSOLUTE, xlate.Transform(MatW).xyz() );
		}				
		return transform;
	}

	virtual void SetTransform( CObject *pobj , const TransformNode3D &pMat )
	{
		if( EnvironmentCollision2DFactory::GetBoundDagObject() )
		{
			fmtx4 MatW = EnvironmentCollision2DFactory::GetBoundDagObject()->GetTransformNode().GetWorldTransform()->GetMatrix();
			fmtx4 MatIW = MatW; MatIW.Inverse();

			ecic_vertex *pent = (ecic_vertex*) pobj;

			fvec4 xlate = pMat.GetWorldTransform()->GetMatrix().GetTranslation();
			xlate = xlate.Transform(MatIW);

			pent->SetPosition( xlate.xyz().GetXY() );
		}
	}
	static IInterface::Context * GetInterface( void )
	{
		static EcicVertexTransformInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}
	static void Register()
	{
		ecic_vertex::GetClassStatic()->AddInterface( ork::IInterface::EIFID_XFORM, GetInterface );
	}
};
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// 2D Collision Area Mesh

class EcicAreaMeshEditorInterface : public ork::lev2::IEditorInterface
{
	public: //
	EcicAreaMeshEditorInterface()
		: ork::lev2::IEditorInterface()
	{
	}
	virtual bool IsPickable( CObject *pObj ) const { return true; }
	virtual std::string GetName( CObject *pObj ) const
	{
		ecic_areamesh* parea = safe_cobject_downcast<ecic_areamesh>(pObj);
		return CreateFormattedString("ecic_areamesh<%s>", parea->GetTag().c_str() );
	}
	virtual void PropertyChanged( CObject *pObj, CProp *pProp )
	{
	}
	static IInterface::Context * GetInterface( void )
	{
		static EcicAreaMeshEditorInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}
	static void Register()
	{
		ecic_areamesh::GetClassStatic()->AddInterface( ork::IInterface::EIFID_EDITOR, GetInterface );
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// 2D Collision Edge Mesh

class EcicEdgeMeshEditorInterface : public ork::lev2::IEditorInterface
{
	public: //
	EcicEdgeMeshEditorInterface()
		: ork::lev2::IEditorInterface()
	{
	}
	virtual bool IsPickable( CObject *pObj ) const { return true; }
	virtual std::string GetName( CObject *pObj ) const
	{
		ecic_edgemesh* pmesh = safe_cobject_downcast<ecic_edgemesh>(pObj);
		return CreateFormattedString("ecic_edgemesh" );
	}
	virtual void PropertyChanged( CObject *pObj, CProp *pProp )
	{
	}
	static IInterface::Context * GetInterface( void )
	{
		static EcicEdgeMeshEditorInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}
	static void Register()
	{
		ecic_edgemesh::GetClassStatic()->AddInterface( ork::IInterface::EIFID_EDITOR, GetInterface );
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// 2D Collision Edge

class EcicEdgeEditorInterface : public ork::lev2::IEditorInterface
{
	public: //
	EcicEdgeEditorInterface()
		: ork::lev2::IEditorInterface()
	{
	}
	virtual bool IsPickable( CObject *pObj ) const { return true; }
	virtual std::string GetName( CObject *pObj ) const
	{
		ecic_edge* pedge = safe_cobject_downcast<ecic_edge>(pObj);
		return CreateFormattedString("ecic_edge<%s>", pedge->mTag.c_str() );
	}
	virtual void PropertyChanged( CObject *pObj, CProp *pProp )
	{
	}
	static IInterface::Context * GetInterface( void )
	{
		static EcicEdgeEditorInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}
	static void Register()
	{
		ecic_edge::GetClassStatic()->AddInterface( ork::IInterface::EIFID_EDITOR, GetInterface );
	}
};

class EcicEdgeTransformInterface : public ork::ITransformInterface
{
	TransformNode3D base_transform;
	TransformNode3D transform;
	fvec2 sva;
	fvec2 svb;

	public:

	virtual const TransformNode3D & GetTransform( CObject *pobj )
	{
		if( EnvironmentCollision2DFactory::GetBoundDagObject() )
		{
			ecic_edge *pent = (ecic_edge*) pobj;
			ecic_vertex* va = pent->mvertexa;
			ecic_vertex* vb = pent->mvertexb;
			transform.GetWorldTransform()->SetMatrix( fmtx4::Identity );
			const fvec2& vpa = va->GetPosition();
			const fvec2& vpb = vb->GetPosition();
			fvec2 Middle = (vpa+vpb)*float(0.5f);
			sva = (vpa-Middle);
			svb = (vpb-Middle);
			fvec3 xlate( Middle.GetX(), Middle.GetY(), float(0.0f) );
			transform.Translate( TransformNode3D::EMODE_ABSOLUTE, xlate );
			base_transform = transform;
		}
		return transform;
	}

	virtual void SetTransform( CObject *pobj , const TransformNode3D &pMat )
	{
		if( EnvironmentCollision2DFactory::GetBoundDagObject() )
		{
			ecic_edge *pent = (ecic_edge*) pobj;
			fvec2 xlate = pMat.GetWorldTransform()->GetMatrix().GetTranslation().GetXY();
			ecic_vertex* va = pent->mvertexa;
			ecic_vertex* vb = pent->mvertexb;
			va->SetPosition( xlate+sva );
			vb->SetPosition( xlate+svb );
		}
	}
	static IInterface::Context * GetInterface( void )
	{
		static EcicEdgeTransformInterface gifd1;
		return (IInterface::Context *)& gifd1;
	}
	static void Register()
	{
		ecic_edge::GetClassStatic()->AddInterface( ork::IInterface::EIFID_XFORM, GetInterface );
	}
};
*/

} }
