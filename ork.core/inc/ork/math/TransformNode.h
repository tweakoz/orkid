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
//#include <ork/kernel/prop.h>
#include <ork/kernel/tempstring.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ITransform3D
{
public:

	typedef CVector3		PosType;
	typedef CQuaternion	RotType;
	typedef CReal			ScaType;
	typedef CMatrix4&		MatType;

	virtual void SetMatrix( const CMatrix4& ) = 0;
	virtual const CMatrix4& GetMatrix() const = 0;
	virtual CVector3		GetPosition() const = 0;
	virtual CQuaternion 	GetRotation() const = 0;
	virtual CReal			GetScale() const = 0;
	virtual void			SetRotation( const CQuaternion& q ) = 0;
	virtual void			SetScale( const CReal& scale) = 0;
	virtual void			SetPosition( const CVector3& pos ) = 0;

	virtual ~ITransform3D() {}
//	virtual void ToPropTypeString( PropTypeString& pstr ) const;
//	virtual void FromPropTypeString( const PropTypeString& pstr );

};
	
///////////////////////////////////////////////////////////////////////////////

class Transform3DMatrix : public ITransform3D
{
	CMatrix4				mMatrix;	/// matrix in world space

	virtual void SetMatrix( const CMatrix4& );
	virtual const CMatrix4& GetMatrix() const;
	virtual CVector3		GetPosition() const;
	virtual CQuaternion 	GetRotation() const;
	virtual CReal			GetScale() const;
	virtual void			SetRotation( const CQuaternion &q );
	virtual void			SetScale( const CReal& scale);
	virtual void			SetPosition( const CVector3& pos );
};

///////////////////////////////////////////////////////////////////////////////

class Transform3DDecomposed : public ITransform3D
{
public:
	enum EFlags
	{
		EFLAG_TRANS_DIRTY=0,				/// Transform's components have been changed, rebuild the matrix
		EFLAG_ROTATE_DIRTY,				/// Transform's components have been changed, rebuild the matrix
		EFLAG_SCALE_DIRTY,				/// Transform's components have been changed, rebuild the matrix
	};

	void					CheckDirty() const;
	void					SetFlag( EFlags eflag );
	void					ClrFlag( EFlags eflag );

private:
	static const U32 kRotFlag = (1<<EFLAG_ROTATE_DIRTY);
	static const U32 kTraFlag = (1<<EFLAG_TRANS_DIRTY);
	static const U32 kScaFlag = (1<<EFLAG_SCALE_DIRTY);
	static const U32 kDirtyMask = kRotFlag | kTraFlag | kScaFlag;
	static const U32 kNotDirtyMask = ~kDirtyMask;

	CVector3				mPosition;
	CReal					mScale;
	CQuaternion				mRotation;
	CMatrix4				mMatrix;
	mutable U32				mFlags;
	
	virtual void SetMatrix( const CMatrix4& );
	virtual const CMatrix4& GetMatrix() const;
	virtual CVector3		GetPosition() const;
	virtual CQuaternion 	GetRotation() const;
	virtual CReal			GetScale() const;
	virtual void			SetRotation( const CQuaternion & q );
	virtual void			SetScale( const CReal& scale);
	virtual void			SetPosition( const CVector3& pos );
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename IXF>
class TransformNode
{

public:

	TransformNode();
	TransformNode( const TransformNode& oth );
	~TransformNode();
	
	void ToPropTypeString( PropTypeString& tstr ) const;
	void FromPropTypeString( const PropTypeString& tstr );

	const IXF *				GetTransform() const { return mTransform; }
	      IXF *				GetTransform()       { return mTransform; }
		  IXF *				mTransform;

	const TransformNode& operator =( const TransformNode& oth );
		  
	//////////////////////////////////////////////////////////////////////////////

	void GetMatrix( CMatrix4& mtx ) const;

	//////////////////////////////////////////////////////////////////////////////

	enum ETransformHierMode
	{
		EMODE_ABSOLUTE = 0,				/// Set overwrite transform node's current component
		EMODE_LOCAL_RELATIVE,				/// Concatenate
	};

	void Translate( ETransformHierMode emode, const typename IXF::PosType & pos );

	//////////////////////////////////////////////////////////////////////////////
	const TransformNode*	GetParent() const { return mpParent; }
	void					SetParent( const TransformNode *ppar );
	//////////////////////////////////////////////////////////////////////////////
	void					UnParent();
	void					ReParent( const TransformNode *ppar );
	//////////////////////////////////////////////////////////////////////////////
private:
	const TransformNode*	mpParent;			

	void CopyFrom(const TransformNode &oth);
	//////////////////////////////////////////////////////////////////////////////
};

typedef TransformNode<ITransform3D> TransformNode3D;

///////////////////////////////////////////////////////////////////////////////

}
#endif
