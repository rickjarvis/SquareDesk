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
#ifndef SD_REDO_STACK_INCLUDED
#define SD_REDO_STACK_INCLUDED 1
#include <QStringList>

class SDRedoStack {
    QList<QStringList> sd_redo_stack;
    bool doing_user_input;
    bool did_an_undo;
public:
    SDRedoStack();

    void initialize();
    void add_lines_to_row(int row);
    void add_command(int row, const QString &cmd);
    QStringList get_redo_commands(int row);
    void set_doing_user_input();
    void clear_doing_user_input();
    void set_did_an_undo();
};

#endif /* ifndef SD_REDO_STACK_INCLUDED */
