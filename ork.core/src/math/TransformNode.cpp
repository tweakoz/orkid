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

void TransformNode::CopyFrom(const TransformNode &oth)
{	
	mTransform.SetMatrix( oth.GetTransform().GetMatrix() );
	mpParent = oth.mpParent;
}
	
TransformNode::TransformNode()
	: mTransform()
	, mpParent( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

void TransformNode::GetMatrix( CMatrix4& mtx ) const
{
	if(mpParent)
	{
		CMatrix4 p;
		mpParent->GetMatrix(p);
		mtx = GetTransform().GetMatrix() * p;
	}
	else
	{
		mtx = GetTransform().GetMatrix();
	}
}
	
void TransformNode::Translate(ETransformHierMode emode, const CVector3 &pos)
{
	if( emode == EMODE_ABSOLUTE )
	{
		GetTransform().SetPosition( pos );
	}
}

void TransformNode::ToPropTypeString( PropTypeString& tstr ) const
{
	const CVector3 pos = GetTransform().GetPosition();
	const CQuaternion quat = GetTransform().GetRotation();
	const CReal scale = GetTransform().GetScale();
	tstr.format("(%g %g %g) (%g %g %g %g) (%g)", pos.GetX(), pos.GetY(), pos.GetZ(),
				quat.GetX(), quat.GetY(), quat.GetZ(), quat.GetW(),
				scale);
}

void TransformNode::FromPropTypeString( const PropTypeString& tstr )
{
	float x, y, z;
	float qx, qy, qz, qw;
	float s;
	sscanf(tstr.c_str(), "(%g %g %g) (%g %g %g %g) (%g)", &x, &y, &z, &qx, &qy, &qz, &qw, &s);
	Translate(TransformNode::EMODE_ABSOLUTE, CVector3(CReal(x), CReal(y), CReal(z)));
	GetTransform().SetRotation(CQuaternion(CReal(qx), CReal(qy), CReal(qz), CReal(qw)));
	GetTransform().SetScale(CReal(s));
}

TransformNode::TransformNode( const TransformNode& oth )
	: mTransform()
	, mpParent( 0 )
{
	CopyFrom(oth);
}

const TransformNode& TransformNode::operator =( const TransformNode& oth )
{
	if(this != &oth)
	{
		CopyFrom(oth);
	}
	
	return *this;
}


TransformNode::~TransformNode()
{
}


///////////////////////////////////////////////////////////////////////////////

void TransformNode::UnParent( void )
{
	if(mpParent != NULL)
	{
		CMatrix4 MatW;
		GetMatrix(MatW);

		///////////////////////////////////

		mpParent = 0;

		///////////////////////////////////
		// Translation

		Translate(EMODE_ABSOLUTE, MatW.GetTranslation());

		///////////////////////////////////
		// Rotation

		CMatrix4 MatR = MatW;
		MatR.SetTranslation(0.0f,0.0f,0.0f);

		CQuaternion NewQ;
		NewQ.FromMatrix(MatR);
		GetTransform().SetRotation(NewQ);
	}
}

void TransformNode::SetParent( const TransformNode* ppar )
{
	mpParent = ppar;
}
void TransformNode::ReParent( const TransformNode* ppar )
{
	OrkAssertI(ppar, "Trying to set Null Parent, if trying to unparent, use the UnParent() call" );

	CMatrix4 MatW = GetTransform().GetMatrix();
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
		Translate(EMODE_ABSOLUTE, CorMatrix.GetTranslation());
	}
}

///////////////////////////////////////////////////////////////////////////////

namespace reflect {
template<>
void Serialize(const TransformNode* in, TransformNode* out, BidirectionalSerializer& bidi)
{
	if(bidi.Serializing())
	{
		bidi | in->GetTransform().GetMatrix();
	}
	else
	{
		CMatrix4 result, temp;
		bidi | temp;
		
		////////////////////////////////////////

		CQuaternion oq;
		CVector3 pos;
		CReal Scale;
		temp.DecomposeMatrix( pos, oq, Scale );
		result.ComposeMatrix( pos, oq, Scale );

		////////////////////////////////////////

		out->GetTransform().SetMatrix(result);

	}
}
}
///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<TransformNode>::meType = EPROPTYPE_TRANSFORMNODE3D;
template<> const char * CPropType<TransformNode>::mstrTypeName		= "TRANSFORMNODE3D";
template<> void CPropType<TransformNode>::ToString( const TransformNode & Value, PropTypeString& tstr)
{
	Value.ToPropTypeString( tstr );
}

template<> TransformNode CPropType<TransformNode>::FromString(const PropTypeString& String)
{
	TransformNode value;
	value.FromPropTypeString( String );
	return value;
}

template class CPropType<TransformNode>;

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

void Transform3DMatrix::SetScale( const float nscale)
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

} // namespace ork
