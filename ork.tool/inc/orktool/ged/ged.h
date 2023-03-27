////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <orktool/orktool_pch.h>
#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/object/AutoConnector.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/any.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/ui/viewport.h>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialog>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
class GedTextEdit : public QLineEdit {
  Q_OBJECT

public:
  GedTextEdit(QWidget* parent);
  void focusOutEvent(QFocusEvent* pev) final; // virtual
  void keyPressEvent(QKeyEvent* pev) final;   // virtual
  void _setText(const char* ptext);

signals:
  void editFinished();
  void canceled();
};

class GedInputDialog : public QDialog {
  Q_OBJECT
public:
  GedInputDialog();

  static QString getText(ork::ui::event_constptr_t ev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih);
  bool wasChanged() const {
    return mbChanged;
  }
  QString getResult();
  void clear() {
    mTextEdit.clear();
  }
  GedTextEdit mTextEdit;
  QString mResult;
  bool mbChanged;

public slots:

  void canceled();
  void accepted();
  void textChanged(QString str);
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ged
} // namespace tool
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
