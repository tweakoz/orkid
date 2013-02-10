////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/math/TransformNode.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <>
void TransformNode3D::CopyFrom(const TransformNode3D &oth)
{	
	mTransform->SetMatrix( oth.GetTransform()->GetMatrix() );
	mpParent = oth.mpParent;
}
	
template <>
TransformNode3D::TransformNode()
: mTransform( new Transform3DMatrix )
, mpParent( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

template<>
void TransformNode3D::GetMatrix( CMatrix4& mtx ) const
{
	if(mpParent)
	{
		CMatrix4 p;
		mpParent->GetMatrix(p);
		mtx = GetTransform()->GetMatrix() * p;
	}
	else
	{
		mtx = GetTransform()->GetMatrix();
	}
}
	
template <>
void TransformNode3D::Translate(TransformNode3D::ETransformHierMode emode, const ITransform3D::PosType &pos)
{
	if( emode == TransformNode3D::EMODE_ABSOLUTE )
	{
		GetTransform()->SetPosition( pos );
	}
}

template <>
void TransformNode3D::ToPropTypeString( PropTypeString& tstr ) const
{
	const CVector3 pos = GetTransform()->GetPosition();
	const CQuaternion quat = GetTransform()->GetRotation();
	const CReal scale = GetTransform()->GetScale();
	tstr.format("(%g %g %g) (%g %g %g %g) (%g)", pos.GetX(), pos.GetY(), pos.GetZ(),
				quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW(),
				scale);
}

template <>
void TransformNode3D::FromPropTypeString( const PropTypeString& tstr )
{
	float x, y, z;
	float qx, qy, qz, qw;
	float s;
	sscanf(tstr.c_str(), "(%g %g %g) (%g %g %g %g) (%g)", &x, &y, &z, &qx, &qy, &qz, &qw, &s);
	Translate(TransformNode::EMODE_ABSOLUTE, CVector3(CReal(x), CReal(y), CReal(z)));
	GetTransform()->SetRotation(CQuaternion(CReal(qx), CReal(qy), CReal(qz), CReal(qw)));
	GetTransform()->SetScale(CReal(s));
}

template <>
TransformNode3D::TransformNode( const TransformNode& oth )
: mTransform( new Transform3DMatrix )
, mpParent( 0 )
{
	CopyFrom(oth);
}

template <>
const TransformNode3D& TransformNode3D::operator =( const TransformNode3D& oth )
{
	if(this != &oth)
	{
		CopyFrom(oth);
	}
	
	return *this;
}


template <>
TransformNode3D::~TransformNode()
{
	delete mTransform;
}


///////////////////////////////////////////////////////////////////////////////

namespace reflect {
template<>
void Serialize(const TransformNode3D *in, TransformNode3D *out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi | in->GetTransform()->GetMatrix();
	}
	else
	{
		CMatrix4 result;
		bidi | result;
		out->GetTransform()->SetMatrix(result);
	}
}
}
///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<TransformNode3D>::meType = EPROPTYPE_TRANSFORMNODE3D;
template<> const char * CPropType<TransformNode3D>::mstrTypeName		= "TRANSFORMNODE3D";
template<> void CPropType<TransformNode3D>::ToString( const TransformNode3D & Value, PropTypeString& tstr)
{
	Value.ToPropTypeString( tstr );
}

template<> TransformNode3D CPropType<TransformNode3D>::FromString(const PropTypeString& String)
{
	TransformNode3D value;
	value.FromPropTypeString( String );
	return value;
}

template class CPropType<TransformNode3D>;
//template class CDirectBasicProp<TransformNode3D>;

///////////////////////////////////////////////////////////////////////////////

const CMatrix4& Transform3DMatrix::GetMatrix() const
{
	return mMatrix;
}

void Transform3DMatrix::SetMatrix(const CMatrix4& mat)
{
	mMatrix = mat;
}

CVector3 Transform3DMatrix::GetPosition() const
{
	return mMatrix.GetTranslation();
}

CQuaternion Transform3DMatrix::GetRotation() const
{
	CQuaternion q;
	CVector3 pos;
	CReal Scale;
	mMatrix.DecomposeMatrix( pos, q, Scale );
	return q;
}

CReal Transform3DMatrix::GetScale() const
{
	CQuaternion q;
	CVector3 pos;
	CReal Scale;
	mMatrix.DecomposeMatrix( pos, q, Scale );
	return Scale;
}

void Transform3DMatrix::SetRotation( const CQuaternion &nq )
{
	CQuaternion oq;
	CVector3 pos;
	CReal Scale;
	mMatrix.DecomposeMatrix( pos, oq, Scale );
	mMatrix.ComposeMatrix( pos, nq, Scale );
}

void Transform3DMatrix::SetScale( const CReal& nscale)
{
	CQuaternion q;
	CVector3 pos;
	CReal Scale;
	mMatrix.DecomposeMatrix( pos, q, Scale );
	mMatrix.ComposeMatrix( pos, q, nscale );
}

void Transform3DMatrix::SetPosition( const CVector3& npos )
{
	CQuaternion q;
	CVector3 pos;
	CReal Scale;
	mMatrix.DecomposeMatrix( pos, q, Scale );
	mMatrix.ComposeMatrix( npos, q, Scale );
}

///////////////////////////////////////////////////////////////////////////////
/*
void TransformNode<ITransform3D>::CalcMatrix( void ) const
{
	//////////////////////////////////////

	if( mFlags & kRotFlag )
	{
		CMatrix4 MatR;

		switch( mXfType )
		{
			case EXF_QUATERNION:
			{
				MatR = mRotation.ToMatrix();
				break;
			}
		}

#if defined( EDITORTRANSFORMS )
		mLocalMatrix.SetElemYX(0,0,MatR.GetElemYX(0,0)*mScale);
		mLocalMatrix.SetElemYX(1,0,MatR.GetElemYX(1,0)*mScale);
		mLocalMatrix.SetElemYX(2,0,MatR.GetElemYX(2,0)*mScale);
		mLocalMatrix.SetElemYX(0,1,MatR.GetElemYX(0,1)*mScale);
		mLocalMatrix.SetElemYX(1,1,MatR.GetElemYX(1,1)*mScale);
		mLocalMatrix.SetElemYX(2,1,MatR.GetElemYX(2,1)*mScale);
		mLocalMatrix.SetElemYX(0,2,MatR.GetElemYX(0,2)*mScale);
		mLocalMatrix.SetElemYX(1,2,MatR.GetElemYX(1,2)*mScale);
		mLocalMatrix.SetElemYX(2,2,MatR.GetElemYX(2,2)*mScale);
#else
		mWorldMatrix.SetElemYX(0,0,MatR.GetElemYX(0,0)*mScale);
		mWorldMatrix.SetElemYX(1,0,MatR.GetElemYX(1,0)*mScale);
		mWorldMatrix.SetElemYX(2,0,MatR.GetElemYX(2,0)*mScale);
		mWorldMatrix.SetElemYX(0,1,MatR.GetElemYX(0,1)*mScale);
		mWorldMatrix.SetElemYX(1,1,MatR.GetElemYX(1,1)*mScale);
		mWorldMatrix.SetElemYX(2,1,MatR.GetElemYX(2,1)*mScale);
		mWorldMatrix.SetElemYX(0,2,MatR.GetElemYX(0,2)*mScale);
		mWorldMatrix.SetElemYX(1,2,MatR.GetElemYX(1,2)*mScale);
		mWorldMatrix.SetElemYX(2,2,MatR.GetElemYX(2,2)*mScale);
#endif
	}

	//CalcWorldMatrix();

	mFlags &= kNotDirtyMask;
}

///////////////////////////////////////////////////////////////////////////////

void TransformNode::SetMatrix( const CMatrix4 & mat )
{
	OrkAssert( EXF_MATRIX == mXfType );
	//mLocalMatrix = mat;
	mWorldMatrix = mat;
	mPosition.SetX( mat.GetElemYX(0,3) );
	mPosition.SetY( mat.GetElemYX(1,3) );
	mPosition.SetZ( mat.GetElemYX(2,3) );
	mFlags &= kNotDirtyMask;
}

///////////////////////////////////////////////////////////////////////////////

void TransformNode::Translate( ETransformHierMode emode, const CVector3 & pos )
{
	switch( emode )
	{
		case EMODE_ABSOLUTE:
		{
			mPosition = pos; 
			SetMatrixTrans();
			//CalcWorldMatrix();
			break;
		}
		case EMODE_LOCAL_RELATIVE:
		{
			mPosition += pos; 
			SetMatrixTrans();
			//CalcWorldMatrix();
			break;
		}
	}
}*/

///////////////////////////////////////////////////////////////////////////////

template<>
void TransformNode<ITransform3D>::UnParent( void )
{
	if(mpParent != NULL)
	{
		CMatrix4 MatW;
		GetMatrix(MatW);

		///////////////////////////////////

		mpParent = 0;

		///////////////////////////////////
		// Translation

		Translate(TransformNode3D::EMODE_ABSOLUTE, MatW.GetTranslation());

		///////////////////////////////////
		// Rotation

		CMatrix4 MatR = MatW;
		MatR.SetTranslation(0.0f,0.0f,0.0f);

		CQuaternion NewQ;
		NewQ.FromMatrix(MatR);
		GetTransform()->SetRotation(NewQ);
	}
}

template<>
void TransformNode3D::SetParent( const TransformNode3D *ppar )
{
	mpParent = ppar;
}
template<>
void TransformNode3D::ReParent( const TransformNode3D *ppar )
{
	OrkAssertI(ppar, "Trying to set Null Parent, if trying to unparent, use the UnParent() call" );

	CMatrix4 MatW = GetTransform()->GetMatrix();
	CMatrix4 MatParentW;
	ppar->GetMatrix(MatParentW);

	if(NULL == mpParent)
	{
		mpParent = ppar;

		CMatrix4 CorMatrix;
		CorMatrix.CorrectionMatrix( MatParentW, MatW );

		CVector3 NewPos;
		CQuaternion NewQ;
		CReal NewScale;

		CorMatrix.DecomposeMatrix( NewPos, NewQ, NewScale );
		
		//ppar->GetWorldTransform()->SetRotation( NewQ );

		///////////////////////////////////
		// Translation

		CVector3 WorldPos = MatW.GetTranslation();
		CVector3 ParentWorldPos = MatParentW.GetTranslation();
		Translate(TransformNode3D::EMODE_ABSOLUTE, CorMatrix.GetTranslation());
	}
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
