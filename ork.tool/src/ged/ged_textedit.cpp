////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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

GedTextEdit::GedTextEdit( QWidget* parent )
	: QLineEdit( parent ){

}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::focusOutEvent( QFocusEvent* pev ) {
	emit canceled();
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::keyPressEvent ( QKeyEvent * pev ) {
	switch( pev->key() ) {
		case Qt::Key_Enter:
		case Qt::Key_Return: {
			emit editFinished();
			break;
		}
		case Qt::Key_Tab: {
			emit canceled();
			break;
		}
		default: {
			QLineEdit::keyPressEvent( pev );
			break;
		}
	}
}

void GedTextEdit::_setText( const char* ptext ){
	if( ptext ){
		setText( QString(ptext) );
	}
}

///////////////////////////////////////////////////////////////////////////////

GedInputDialog::GedInputDialog()
	: QDialog(0, Qt::Dialog|Qt::FramelessWindowHint )
	, mTextEdit( this )
	, mbChanged( false )
	, mResult("") {
	bool bOK = connect(&mTextEdit, SIGNAL(textChanged(QString)), this, SLOT(textChanged(QString)));
	OrkAssert(bOK);
	bOK = connect(&mTextEdit, SIGNAL(editFinished()), this, SLOT(accepted()));
	OrkAssert(bOK);
	bOK = connect(&mTextEdit, SIGNAL(canceled()), this, SLOT(canceled()));
	OrkAssert(bOK);
//	mTextEdit.setTabChangesFocus( true );
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::accepted( ) {
	QDialog::done( QDialog::Accepted );
	mbChanged = true;
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::canceled( ) {
	QDialog::done( QDialog::Rejected );
	mbChanged = false;
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::textChanged( QString newtext ) {
	mResult = mTextEdit.text();
	printf("mResult<%s>\n", mResult.toStdString().c_str() );
	mbChanged = true;
}

///////////////////////////////////////////////////////////////////////////////

QString GedInputDialog::getResult() {
	return mResult;
}

///////////////////////////////////////////////////////////////////////////////

QString GedInputDialog::getText( const ork::ui::Event& ev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih ) {

	int isx = ork::lev2::logicalMousePos().x();
	int isy = ork::lev2::logicalMousePos().y();

	int ixb = (isx-ev.miRawX);
	int iyb = (isy-ev.miRawY);

    int ixa = ixb+pnode->GetX()+ix;
	int iya = iyb+pnode->GetY()+iy;
    if( ork::lev2::_HIDPI ){
      iw *= 2;
      ih *= 2;
      ixa *= 2;
      iya *= 2;
    }

	GedInputDialog dialog;
	dialog.setModal( true );

	dialog.setGeometry( ixa, iya, iw, ih );
	dialog.clear();
	dialog.mTextEdit.setGeometry( 0, 0, iw, ih );

	if( defstr )
		dialog.mTextEdit._setText( defstr );

	int iv = dialog.exec();

	QString res("");

	if( 0 == iv )
		res = dialog.getResult();

	return res;
}

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
