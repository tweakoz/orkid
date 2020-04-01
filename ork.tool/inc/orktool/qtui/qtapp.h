////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <ork/kernel/opq.h>

namespace ork { namespace tool {

class OrkQtApp : public QApplication {
  Q_OBJECT

public:
  ///////////////////////////////////
  OrkQtApp(int& argc, char** argv);
  ///////////////////////////////////

public slots:
  void OnTimer();

  ///////////////////////////////////
public:
public:
  QTimer mIdleTimer;
  QMainWindow* mpMainWindow;
  opq::opq_ptr_t _mainq;
};

}} // namespace ork::tool
