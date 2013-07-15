////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>

#include <orktool/qtui/qtui_tool.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/qtui/qtui.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ImplementMoc( GedInputDialog, QDialog );
ImplementMoc( GedTextEdit, QLineEdit );

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::MocInit()
{
	Moc.AddSlot1( "SlotTextChanged(QString)", & GedInputDialog::SlotTextChanged );
	Moc.AddSlot0( "done()", & GedInputDialog::done );
	Moc.AddSlot0( "canceled()", & GedInputDialog::canceled );
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::MocInit()
{
	Moc.AddSignal0( "SigEditFinished()", & GedTextEdit::SigEditFinished );
	Moc.AddSignal0( "SigCanceled()", & GedTextEdit::SigCanceled );
}

///////////////////////////////////////////////////////////////////////////////

GedTextEdit::GedTextEdit( QWidget* parent )
	: QLineEdit( parent )
{

}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::focusOutEvent( QFocusEvent* pev ) // virtual
{
	SigCanceled();
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::keyPressEvent ( QKeyEvent * pev )
{
	switch( pev->key() )
	{
		case Qt::Key_Enter:
		case Qt::Key_Return:
		{
			SigEditFinished();
			break;
		}
		case Qt::Key_Tab:
		{
			SigCanceled();
			break;
		}
		default:
		{
			QLineEdit::keyPressEvent( pev );
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::SigEditFinished()
{
	Moc.Emit( this, "SigEditFinished()" );
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::SigCanceled()
{
	Moc.Emit( this, "SigCanceled()" );
}

void GedTextEdit::SetText( const char* ptext )
{
	if( ptext )
	{
		setText( QString(ptext) );
	}
}

///////////////////////////////////////////////////////////////////////////////

GedInputDialog::GedInputDialog()
	: QDialog(0, Qt::Dialog|Qt::FramelessWindowHint )
	, mTextEdit( this )
	, mbChanged( false )
	, mResult("")
{
	bool bOK = connect(&mTextEdit, SIGNAL(textChanged(QString)), this, SLOT(SlotTextChanged(QString)));
	OrkAssert(bOK);
	bOK = connect(&mTextEdit, SIGNAL(SigEditFinished()), this, SLOT(done()));
	OrkAssert(bOK);
	bOK = connect(&mTextEdit, SIGNAL(SigCanceled()), this, SLOT(canceled()));
	OrkAssert(bOK);
//	mTextEdit.setTabChangesFocus( true );
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::done( )
{
	QDialog::done(0);
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::canceled( )
{
	QDialog::done(-1);
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::SlotTextChanged( QString newtext )
{
	mResult = mTextEdit.text();
	mbChanged = true;
}

///////////////////////////////////////////////////////////////////////////////

QString GedInputDialog::GetResult()
{
	return mResult;
}

///////////////////////////////////////////////////////////////////////////////

QString GedInputDialog::getText( QMouseEvent* pev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih )
{
	int isx = QCursor::pos().x();
	int isy = QCursor::pos().y();
	int imx = pev->x();
	int imy = pev->y();
	
	int ixb = (isx-imx);
	int iyb = (isy-imy);

	int ixa = ixb+pnode->GetX()+ix;
	int iya = iyb+pnode->GetY()+iy;

	GedInputDialog dialog;
	dialog.setModal( true );

	dialog.setGeometry( ixa, iya, iw, ih );
	dialog.clear();
	dialog.mTextEdit.setGeometry( 0, 0, iw, ih );

	if( defstr )
	{
		dialog.mTextEdit.SetText( defstr );
	}

	int iv = dialog.exec();

	QString res("");

	if( 0 == iv )
	{
		res = dialog.GetResult();
	}
	return res;
}

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {
template class MocImp<ork::tool::ged::GedTextEdit,QLineEdit>;
}}
