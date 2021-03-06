/****************************************************************************
**
** Copyright (C) 2016, 2017, 2018 Mike Pogue, Dan Lyke
** Contact: mpogue @ zenstarstudio.com
**
** This file is part of the SquareDesk application.
**
** $SQUAREDESK_BEGIN_LICENSE$
**
** Commercial License Usage
** For commercial licensing terms and conditions, contact the authors via the
** email address above.
**
** GNU General Public License Usage
** This file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appear in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file.
**
** $SQUAREDESK_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include <QApplication>

#include "startupwizard.h"

int main(int argc, char *argv[])
{
    // From: https://stackoverflow.com/questions/4954140/how-to-redirect-qdebug-qwarning-qcritical-etc-output

    QApplication a(argc, argv);
    a.setApplicationName("SquareDesk");
    a.setOrganizationName("Zenstar Software");
    a.setOrganizationDomain("zenstarstudio.com");

    MainWindow w;

    // put window back where it was last time (modulo the screen size, which
    //   is automatically taken care of.
    QSettings settings;
    w.restoreGeometry(settings.value("geometry").toByteArray());
    w.restoreState(settings.value("windowState").toByteArray());

    w.show();

    // http://stackoverflow.com/questions/6516299/qt-c-icons-not-showing-up-when-program-is-run-under-windows-o-s
    QString sDir = QCoreApplication::applicationDirPath();
    a.addLibraryPath(sDir + "/plugins");

    a.installEventFilter(new GlobalEventFilter(w.ui));

    Q_INIT_RESOURCE(startupwizard); // resources for the startup wizard

    // If running from QtCreator, use normal debugging -------------
    QByteArray envVar = qgetenv("QTDIR");       //  check if the app is ran in Qt Creator

    if (envVar.isEmpty()) {
        // if running as a standalone app, log to a file instead of the console
        qInstallMessageHandler(MainWindow::customMessageOutput); // custom message handler for debugging
    }

    return a.exec();
}
