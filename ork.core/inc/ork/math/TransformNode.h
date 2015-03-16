////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_MATH_TRANSFORM_NODE_H
#define _ORK_MATH_TRANSFORM_NODE_H

#include <ork/math/cvector3.h>
#include <ork/math/quaternion.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/tempstring.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Transform3DMatrix 
{
	typedef CVector3		PosType;
	typedef CQuaternion	RotType;
	typedef CReal			ScaType;
	typedef CMatrix4&		MatType;

	void SetMatrix( const CMatrix4& );
	const CMatrix4& GetMatrix() const;
	CVector3		GetPosition() const;
	CQuaternion 	GetRotation() const;
	float			GetScale() const;
	void			SetRotation( const CQuaternion& q );
	void			SetScale( const float scale);
	void			SetPosition( const CVector3& pos );

private:
	
	CMatrix4				mMatrix;	/// matrix in world space

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class TransformNode
{

public:

	TransformNode();
	TransformNode( const TransformNode& oth );
	~TransformNode();
	
	void ToPropTypeString( PropTypeString& tstr ) const;
	void FromPropTypeString( const PropTypeString& tstr );

	const Transform3DMatrix& GetTransform() const { return mTransform; }
	      Transform3DMatrix& GetTransform()       { return mTransform; }

	const TransformNode& operator =( const TransformNode& oth );
		  
	//////////////////////////////////////////////////////////////////////////////

	void GetMatrix( CMatrix4& mtx ) const;

	//////////////////////////////////////////////////////////////////////////////

	enum ETransformHierMode
	{
		EMODE_ABSOLUTE = 0,				/// Set overwrite transform node's current component
		EMODE_LOCAL_RELATIVE,				/// Concatenate
	};

	void Translate( ETransformHierMode emode, const CVector3 & pos );

	//////////////////////////////////////////////////////////////////////////////
	const TransformNode*	GetParent() const { return mpParent; }
	void					SetParent( const TransformNode* ppar );
	//////////////////////////////////////////////////////////////////////////////
	void					UnParent();
	void					ReParent( const TransformNode* ppar );
	//////////////////////////////////////////////////////////////////////////////
private:
	const TransformNode*	mpParent;			
	Transform3DMatrix		mTransform;

	void CopyFrom(const TransformNode &oth);
	//////////////////////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

}
#endif
