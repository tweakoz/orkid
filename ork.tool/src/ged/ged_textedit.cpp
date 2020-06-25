////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
#include <ork/reflect/properties/AbstractProperty.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/qtui/qtui.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

GedTextEdit::GedTextEdit(QWidget* parent)
    : QLineEdit(parent) {
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::focusOutEvent(QFocusEvent* pev) {
  emit canceled();
}

///////////////////////////////////////////////////////////////////////////////

void GedTextEdit::keyPressEvent(QKeyEvent* pev) {
  switch (pev->key()) {
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
      QLineEdit::keyPressEvent(pev);
      break;
    }
  }
}

void GedTextEdit::_setText(const char* ptext) {
  if (ptext) {
    setText(QString(ptext));
  }
}

///////////////////////////////////////////////////////////////////////////////

GedInputDialog::GedInputDialog()
    : QDialog(0, Qt::Dialog | Qt::FramelessWindowHint)
    , mTextEdit(this)
    , mbChanged(false)
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

void GedInputDialog::accepted() {
  QDialog::done(QDialog::Accepted);
  mbChanged = true;
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::canceled() {
  QDialog::done(QDialog::Rejected);
  mbChanged = false;
}

///////////////////////////////////////////////////////////////////////////////

void GedInputDialog::textChanged(QString newtext) {
  mResult = mTextEdit.text();
  printf("mResult<%s>\n", mResult.toStdString().c_str());
  mbChanged = true;
}

///////////////////////////////////////////////////////////////////////////////

QString GedInputDialog::getResult() {
  return mResult;
}

///////////////////////////////////////////////////////////////////////////////

QString
GedInputDialog::getText(ork::ui::event_constptr_t ev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih) {

  int isx = ork::lev2::logicalMousePos().x();
  int isy = ork::lev2::logicalMousePos().y();

  int ixb = (isx - ev->miRawX);
  int iyb = (isy - ev->miRawY);

  int ixa = ixb + pnode->GetX() + ix;
  int iya = iyb + pnode->GetY() + iy;

  bool hidpi    = ork::lev2::_HIDPI();
  bool mixeddpi = ork::lev2::_MIXEDDPI();
  if (hidpi) {
    iw *= 2;
    ih *= 2;
    ixa *= 2;
    iya *= 2;
  }

  printf(
      "getText hidpi(%d) mixeddpi<%d> ix<%d> iy<%d> isx<%d> isy<%d> ixa<%d> iya<%d> ixb<%d> iyb<%d>\n",
      int(hidpi),
      int(mixeddpi),
      ix,
      iy,
      isx,
      isy,
      ixa,
      iya,
      ixb,
      iyb);

  GedInputDialog dialog;
  dialog.setModal(true);

  dialog.setGeometry(ixa, iya, iw, ih);
  dialog.clear();
  dialog.mTextEdit.setGeometry(0, 0, iw, ih);

  if (defstr)
    dialog.mTextEdit._setText(defstr);

  if (mixeddpi and not hidpi) {
    // we probably figure out when this hack is actually necessary
    // It might depend on the window manager/desktop enviroment
    //   and the systemwide ScaleFactor settings
    //  but needless to say on linux
    //  they tend not to behave well in mixed dpu enviroments
    QFontInfo fontinfo = dialog.mTextEdit.fontInfo();
    printf("fontinfo pixelsize<%d> pointsize<%d>\n", fontinfo.pixelSize(), fontinfo.pointSize());
    QFont font  = dialog.mTextEdit.font();
    int pixsize = fontinfo.pixelSize();
    font.setPixelSize(pixsize / 2);
    dialog.mTextEdit.setFont(font);
  }

  int iv = dialog.exec();

  QString res("");

  if (0 == iv)
    res = dialog.getResult();

  return res;
}

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
