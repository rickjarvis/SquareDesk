/****************************************************************************
**
** Copyright (C) 2016, 2017 Mike Pogue, Dan Lyke
** Contact: mpogue @ zenstarstudio.com
**
** This file is part of the SquareDesk/SquareDeskPlayer application.
**
** $SQUAREDESK_BEGIN_LICENSE$
**
** Commercial License Usages
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

#include <QActionGroup>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QHostInfo>
#include <QMap>
#include <QMapIterator>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QThread>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QWidget>

#include <QPrinter>
#include <QPrintDialog>

#include "analogclock.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utility.h"
#include "tablenumberitem.h"
#include "prefsmanager.h"
#include "importdialog.h"
#include "exportdialog.h"
#include "calllistcheckbox.h"

#include "danceprograms.h"
#define CUSTOM_FILTER
#include "startupwizard.h"
#include "downloadmanager.h"

#if defined(Q_OS_MAC)
#include "src/communicator.h"
#endif

#if defined(Q_OS_MAC) | defined(Q_OS_WIN)
#include "JlCompress.h"
#endif

// experimental removal of silence at the beginning of the song
#define REMOVESILENCE 1

// BUG: Cmd-K highlights the next row, and hangs the app
// BUG: searching then clearing search will lose selection in songTable
// TODO: consider selecting the row in the songTable, if there is only one row valid as the result of a search
//   then, ENTER could select it, maybe?  Think about this.
// BUG: if you're playing a song on an external flash drive, and remove it, playback stops, but the song is still
//   in the currenttitlebar, and it tries to play (silently).  Should clear everything out at that point and unload the song.

// BUG: should allow Ctrl-U in the sd window, to clear the line (equiv to "erase that")

// TODO: include a license in the executable bundle for Mac (e.g. GPL2).  Include the same
//   license next to the Win32 executable (e.g. COPYING).

// REMINDER TO FUTURE SELF: (I forget how to do this every single time) -- to set a layout to fill a single tab:
//    In designer, you should first in form preview select requested tab,
//    than in tree-view click to PARENT QTabWidget and set the layout as for all tabs.
//    Really this layout appears as new properties for selected tab only. Every tab has own layout.
//    And, the tab must have at least one widget on it already.

#include <iostream>
#include <sstream>
#include <iomanip>
using namespace std;

// TAGLIB stuff is MAC OS X and WIN32 only for now...
#include <taglib/tlist.h>
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/id3v2header.h>

#include <taglib/unsynchronizedlyricsframe.h>
#include <string>

#include "typetracker.h"

using namespace TagLib;

// =================================================================================================
// SquareDeskPlayer Keyboard Shortcuts:
//
// function                 MAC                         PC                                SqView
// -------------------------------------------------------------------------------------------------
// FILE MENU
// Open Audio file          Cmd-O                       Ctrl-O, Alt-F-O
// Save                     Cmd-S                       Ctrl-S, Alt-F-S
// Save as                  Shft-Cmd-S                  Alt-F-A
// Quit                     Cmd-Q                       Ctrl-F4, Alt-F-E
//
// PLAYLIST MENU
// Load Playlist            Cmd-E
// Load Recent Playlist
// Save Playlist
// Add to Playlist TOP      Shft-Cmd-LeftArrow
// Add to Playlist BOT      Shft-Cmd-RightArrow
// Move Song UP             Shft-Cmd-UpArrow
// Move Song DOWN           Shft-Cmd-DownArrow
// Remove from Playlist     Shft-Cmd-DEL
// Next Song                K                            K                                K
// Prev Song
// Continuous Play
//
// MUSIC MENU
// play/pause               space                       space, Alt-M-P                    space
// rewind/stop              S, ESC, END, Cmd-.          S, ESC, END, Alt-M-S, Ctrl-.
// rewind/play (playing)    HOME, . (while playing)     HOME, .  (while playing)          .
// skip/back 5 sec          Cmd-RIGHT/LEFT,RIGHT/LEFT   Ctrl-RIGHT/LEFT, RIGHT/LEFT,
//                                                        Alt-M-A/Alt-M-B
// volume up/down           Cmd-UP/DOWN,UP/DOWN         Ctrl-UP/DOWN, UP/DOWN             N/B
// mute                     Cmd-M, M                    Ctrl-M, M
// pitch up                 Cmd-U, U                    Ctrl-U, U, Alt-M-U                F
// pitch down               Cmd-D, D                    Ctrl-D, D, Alt-M-D                D
// go faster                +,=                         +,=                               R
// go slower                -                           -                                 E
// Fade Out                 Cmd-Y, Y
// Loop                     Cmd-L, L
// force mono                                           Alt-M-F
// Autostart Playback
// Sound FX                 Cmd-1 thru 6
// clear search             ESC, Cmd-/                  Alt-M-S
//
// LYRICS MENU
// Auto-scroll cuesheet
//
// SD MENU
// Enable Input             Cmd-I
//
// VIEW MENU
// Zoom IN/OUT              Cmd-=, Cmd-Minus
// Reset Zoom               Cmd-0
//
// OTHER (Not in a menu)
// Toggle between Music/Lyrics  T

// GLOBALS:
bass_audio cBass;
static const char *music_file_extensions[] = { "mp3", "wav", "m4a" };     // NOTE: must use Qt::CaseInsensitive compares for these
static const char *cuesheet_file_extensions[] = { "htm", "html", "txt" }; // NOTE: must use Qt::CaseInsensitive compares for these

#include <QProxyStyle>

class MySliderClickToMoveStyle : public QProxyStyle
{
public:
//    using QProxyStyle::QProxyStyle;

    int styleHint(QStyle::StyleHint hint, const QStyleOption* option = 0, const QWidget* widget = 0, QStyleHintReturn* returnData = 0) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return (Qt::LeftButton | Qt::MidButton | Qt::RightButton);
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};


// ----------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    oldFocusWidget(NULL),
    lastCuesheetSavePath(),
    timerCountUp(NULL),
    timerCountDown(NULL),
    trapKeypresses(true),
    sd(NULL),
    firstTimeSongIsPlayed(false),
    loadingSong(false),
    cuesheetEditorReactingToCursorMovement(false),
    totalZoom(0),
    hotkeyMappings(KeyAction::defaultKeyToActionMappings()),
    sdLastLine(-1),
    sdWasNotDrawingPicture(true),
    sdLastLineWasResolve(false),
    sdOutputtingAvailableCalls(false),
    sdAvailableCalls(),
    sdLineEditSDInputLengthWhenAvailableCallsWasBuilt(-1)
{
    checkLockFile(); // warn, if some other copy of SquareDesk has database open

    for (size_t i = 0; i < sizeof(webview) / sizeof(*webview); ++i)
        webview[i] = 0;
    
    linesInCurrentPlaylist = 0;

    loadedCuesheetNameWithPath = "";
    justWentActive = false;

    for (int i=0; i<6; i++) {
        soundFXarray[i] = "";
    }

    maybeInstallSoundFX();

    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
//    qDebug() << "preferences recentFenceDateTime: " << prefsManager.GetrecentFenceDateTime();
    recentFenceDateTime = QDateTime::fromString(prefsManager.GetrecentFenceDateTime(),
                                                          "yyyy-MM-dd'T'hh:mm:ss'Z'");
    recentFenceDateTime.setTimeSpec(Qt::UTC);  // set timezone (all times are UTC)

    QHash<Qt::Key, KeyAction *> prefsHotkeyMappings = prefsManager.GetHotkeyMappings();
    if (!prefsHotkeyMappings.empty())
    {
        hotkeyMappings = prefsHotkeyMappings;
    }

//    if (prefsManager.GetenableAutoMicsOff()) {
//        currentInputVolume = getInputVolume();  // save current volume
//    }

    // Disable ScreenSaver while SquareDesk is running
#if defined(Q_OS_MAC)
    macUtils.disableScreensaver(); // NOTE: now needs to be called every N seconds
#elif defined(Q_OS_WIN)
#pragma comment(lib, "user32.lib")
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE , NULL, SPIF_SENDWININICHANGE);
#elif defined(Q_OS_LINUX)
    // TODO
#endif

    // Disable extra (Native Mac) tab bar
#if defined(Q_OS_MAC)
    macUtils.disableWindowTabbing();
#endif

    prefDialog = NULL;      // no preferences dialog created yet
    songLoaded = false;     // no song is loaded, so don't update the currentLocLabel

    ui->setupUi(this);
    ui->statusBar->showMessage("");

    setFontSizes();

    this->setWindowTitle(QString("SquareDesk Music Player/Sequence Editor"));

    ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));

    ui->previousSongButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    ui->nextSongButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));

    ui->playButton->setEnabled(false);
    ui->stopButton->setEnabled(false);

    ui->nextSongButton->setEnabled(false);
    ui->previousSongButton->setEnabled(false);

    // ============
    ui->menuFile->addSeparator();

    // ------------
    // NOTE: MAC OS X ONLY
#if defined(Q_OS_MAC)
    QAction *aboutAct = new QAction(QIcon(), tr("&About SquareDesk..."), this);
    aboutAct->setStatusTip(tr("SquareDesk Information"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutBox()));
    ui->menuFile->addAction(aboutAct);
#endif

    // ==============
    // HELP MENU IS WINDOWS ONLY
#if defined(Q_OS_WIN)
    QMenu *helpMenu = new QMenu("&Help");

    // ------------
    QAction *aboutAct2 = new QAction(QIcon(), tr("About &SquareDesk..."), this);
    aboutAct2->setStatusTip(tr("SquareDesk Information"));
    connect(aboutAct2, SIGNAL(triggered()), this, SLOT(aboutBox()));
    helpMenu->addAction(aboutAct2);
    menuBar()->addAction(helpMenu->menuAction());
#endif

#if defined(Q_OS_WIN)
    delete ui->mainToolBar; // remove toolbar on WINDOWS (toolbar is not present on Mac)
#endif

    // ------------
#if defined(Q_OS_WIN)
    // NOTE: WINDOWS ONLY
    closeAct = new QAction(QIcon(), tr("&Exit"), this);
    closeAct->setShortcuts(QKeySequence::Close);
    closeAct->setStatusTip(tr("Exit the program"));
    connect(closeAct, SIGNAL(triggered()), this, SLOT(close()));
    ui->menuFile->addAction(closeAct);
#endif

    currentState = kStopped;
    currentPitch = 0;
    tempoIsBPM = false;
    switchToLyricsOnPlay = false;

    Info_Seekbar(false);

    // setup playback timer
    UIUpdateTimer = new QTimer(this);
    connect(UIUpdateTimer, SIGNAL(timeout()), this, SLOT(on_UIUpdateTimerTick()));
    UIUpdateTimer->start(1000);           //adjust from GUI with timer->setInterval(newValue)

    closeEventHappened = false;

    ui->songTable->clearSelection();
    ui->songTable->clearFocus();

    //Create Bass audio system
    cBass.Init();

    //Set UI update
    cBass.SetVolume(100);
    currentVolume = 100;
    previousVolume = 100;
    Info_Volume();

    // VU Meter -----
    vuMeterTimer = new QTimer(this);
    connect(vuMeterTimer, SIGNAL(timeout()), this, SLOT(on_vuMeterTimerTick()));
    vuMeterTimer->start(50);           // adjust from GUI with timer->setInterval(newValue)

    vuMeter = new LevelMeter(this);
    ui->gridLayout_2->addWidget(vuMeter, 1,5);  // add it to the layout in the right spot
    vuMeter->setFixedHeight(20);

    vuMeter->reset();
    vuMeter->setEnabled(true);
    vuMeter->setVisible(true);

    // analog clock -----
    analogClock = new AnalogClock(this);
    ui->gridLayout_2->addWidget(analogClock, 2,6,4,1);  // add it to the layout in the right spot
    analogClock->setFixedSize(QSize(110,110));
    analogClock->setEnabled(true);
    analogClock->setVisible(true);

    // where is the root directory where all the music is stored?
    pathStack = new QList<QString>();

    musicRootPath = prefsManager.GetmusicPath();
    guestRootPath = ""; // initially, no guest music
    guestVolume = "";   // and no guest volume present
    guestMode = "main"; // and not guest mode

    switchToLyricsOnPlay = prefsManager.GetswitchToLyricsOnPlay();

    updateRecentPlaylistMenu();

    // ---------------------------------------
    // let's watch for changes in the musicDir
    QDirIterator it(musicRootPath, QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QRegExp ignoreTheseDirs("/(reference|choreography|notes|playlists|sd|soundfx|lyrics)");
    while (it.hasNext()) {
        QString aPath = it.next();
        if (ignoreTheseDirs.indexIn(aPath) == -1) {
            musicRootWatcher.addPath(aPath); // watch for add/deletes to musicDir and interesting subdirs
        }
    }

    musicRootWatcher.addPath(musicRootPath);  // let's not forget the musicDir itself
    QObject::connect(&musicRootWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(musicRootModified(QString)));

    // make sure that the "downloaded" directory exists, so that when we sync up with the Cloud,
    //   it will cause a rescan of the songTable and dropdown

    QDir dir(musicRootPath + "/lyrics/downloaded");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // do the same for lyrics (included the "downloaded" subdirectory) -------
    QDirIterator it2(musicRootPath + "/lyrics", QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it2.hasNext()) {
        QString aPath = it2.next();
        lyricsWatcher.addPath(aPath); // watch for add/deletes to musicDir and interesting subdirs
//        qDebug() << "adding lyrics path: " << aPath;
    }

    lyricsWatcher.addPath(musicRootPath + "/lyrics");  // add the root lyrics directory itself
    QObject::connect(&lyricsWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(maybeLyricsChanged()));
    // ---------------------------------------

#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
    // initial Guest Mode stuff works on Mac OS and WIN32 only
    //   should be straightforward to extend to Linux.
    lastKnownVolumeList = getCurrentVolumes();  // get initial list
    newVolumeList = lastKnownVolumeList;  // keep lists sorted, for easy comparisons
#endif

    // set initial colors for text in songTable, also used for shading the clock
    patterColorString = prefsManager.GetpatterColorString();
    singingColorString = prefsManager.GetsingingColorString();
    calledColorString = prefsManager.GetcalledColorString();
    extrasColorString = prefsManager.GetextrasColorString();

    // Tell the clock what colors to use for session segments
    analogClock->setColorForType(PATTER, QColor(patterColorString));
    analogClock->setColorForType(SINGING, QColor(singingColorString));
    analogClock->setColorForType(SINGING_CALLED, QColor(calledColorString));
    analogClock->setColorForType(XTRAS, QColor(extrasColorString));

    // ----------------------------------------------
    // Save the new settings for experimental break and patter timers --------
    tipLengthTimerEnabled = prefsManager.GettipLengthTimerEnabled();
    tipLength30secEnabled = prefsManager.GettipLength30secEnabled();
    int tipLengthTimerLength = prefsManager.GettipLengthTimerLength();
    tipLengthAlarmAction = prefsManager.GettipLengthAlarmAction();

    breakLengthTimerEnabled = prefsManager.GetbreakLengthTimerEnabled();
    breakLengthTimerLength = prefsManager.GetbreakLengthTimerLength();
    breakLengthAlarmAction = prefsManager.GetbreakLengthAlarmAction();

    analogClock->tipLengthTimerEnabled = tipLengthTimerEnabled;      // tell the clock whether the patter alarm is enabled
    analogClock->tipLength30secEnabled = tipLength30secEnabled;      // tell the clock whether the patter 30 sec warning is enabled
    analogClock->breakLengthTimerEnabled = breakLengthTimerEnabled;  // tell the clock whether the break alarm is enabled

    // ----------------------------------------------
    songFilenameFormat = static_cast<SongFilenameMatchingType>(prefsManager.GetSongFilenameFormat());

    // define type names (before reading in the music filenames!) ------------------
    QString value;
    value = prefsManager.GetMusicTypeSinging();
    songTypeNamesForSinging = value.toLower().split(";", QString::KeepEmptyParts);

    value = prefsManager.GetMusicTypePatter();
    songTypeNamesForPatter = value.toLower().split(";", QString::KeepEmptyParts);

    value = prefsManager.GetMusicTypeExtras();
    songTypeNamesForExtras = value.toLower().split(';', QString::KeepEmptyParts);

    value = prefsManager.GetMusicTypeCalled();
    songTypeNamesForCalled = value.toLower().split(';', QString::KeepEmptyParts);

    QAction *localSessionActions[] = {
        ui->actionPractice,
        ui->actionMonday,
        ui->actionTuesday,
        ui->actionWednesday,
        ui->actionThursday,
        ui->actionFriday,
        ui->actionSaturday,
        ui->actionSunday,
        NULL
    };
    sessionActions = new QAction*[sizeof(localSessionActions) / sizeof(*localSessionActions)];

    for (size_t i = 0;
         i < sizeof(localSessionActions) / sizeof(*localSessionActions);
         ++i)
    {
        sessionActions[i] = localSessionActions[i];
    }
    // -------------------------
    int sessionActionIndex = SessionDefaultPractice ==
        static_cast<SessionDefaultType>(prefsManager.GetSessionDefault()) ?
        0 : songSettings.currentDayOfWeek();
    sessionActions[sessionActionIndex]->setChecked(true);

    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled

    // used to store the file paths
    findMusic(musicRootPath,"","main", true);  // get the filenames from the user's directories
    loadMusicList(); // and filter them into the songTable

#ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
    ui->listWidgetChoreographySequences->setStyleSheet(
         "QListWidget::item { border-bottom: 1px solid black; }" );
#endif // ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
    loadChoreographyList();

    ui->toolButtonEditLyrics->setStyleSheet("QToolButton { border: 1px solid #575757; border-radius: 4px; background-color: palette(base); }");  // turn off border

    // ----------
    updateSongTableColumnView(); // update the actual view of Age/Pitch/Tempo in the songTable view

    // ----------
    clockColoringHidden = !prefsManager.GetexperimentalClockColoringEnabled();
    analogClock->setHidden(clockColoringHidden);

    // -----------
    ui->actionAutostart_playback->setChecked(prefsManager.Getautostartplayback());

    // -----------

    ui->checkBoxPlayOnEnd->setChecked(prefsManager.Getstartplaybackoncountdowntimer());
    ui->checkBoxStartOnPlay->setChecked(prefsManager.Getstartcountuptimeronplay());

    // -------
    on_monoButton_toggled(prefsManager.Getforcemono());

    on_actionRecent_toggled(prefsManager.GetshowRecentColumn());
    on_actionAge_toggled(prefsManager.GetshowAgeColumn());
    on_actionPitch_toggled(prefsManager.GetshowPitchColumn());
    on_actionTempo_toggled(prefsManager.GetshowTempoColumn());

// voice input is only available on MAC OS X and Win32 right now...
    on_actionEnable_voice_input_toggled(prefsManager.Getenablevoiceinput());
    voiceInputEnabled = prefsManager.Getenablevoiceinput();

    on_actionAuto_scroll_during_playback_toggled(prefsManager.Getenableautoscrolllyrics());
    autoScrollLyricsEnabled = prefsManager.Getenableautoscrolllyrics();

    // Volume, Pitch, and Mix can be set before loading a music file.  NOT tempo.
    ui->pitchSlider->setEnabled(true);
    ui->pitchSlider->setValue(0);
    ui->currentPitchLabel->setText("0 semitones");
    // FIX: initial focus is titleSearch, so the shortcuts for these menu items don't work
    //   at initial start.
    ui->actionPitch_Down->setEnabled(true);  // and the corresponding menu items
    ui->actionPitch_Up->setEnabled(true);

    ui->volumeSlider->setEnabled(true);
    ui->volumeSlider->setValue(100);
    ui->volumeSlider->SetOrigin(100);  // double click goes here
    ui->currentVolumeLabel->setText("Max");
    // FIX: initial focus is titleSearch, so the shortcuts for these menu items don't work
    //   at initial start.
    ui->actionVolume_Down->setEnabled(true);  // and the corresponding menu items
    ui->actionVolume_Up->setEnabled(true);
    ui->actionMute->setEnabled(true);

    ui->mixSlider->setEnabled(true);
    ui->mixSlider->setValue(0);
    ui->currentMixLabel->setText("100%L/100%R");

    // ...and the EQ sliders, too...
    ui->bassSlider->setEnabled(true);
    ui->midrangeSlider->setEnabled(true);
    ui->trebleSlider->setEnabled(true);

#ifndef Q_OS_MAC
    ui->seekBar->setStyle(new MySliderClickToMoveStyle());
    ui->tempoSlider->setStyle(new MySliderClickToMoveStyle());
    ui->pitchSlider->setStyle(new MySliderClickToMoveStyle());
    ui->volumeSlider->setStyle(new MySliderClickToMoveStyle());
    ui->mixSlider->setStyle(new MySliderClickToMoveStyle());
    ui->bassSlider->setStyle(new MySliderClickToMoveStyle());
    ui->midrangeSlider->setStyle(new MySliderClickToMoveStyle());
    ui->trebleSlider->setStyle(new MySliderClickToMoveStyle());
    ui->seekBarCuesheet->setStyle(new MySliderClickToMoveStyle());
#endif /* ifndef Q_OS_MAC */

    // in the Designer, these have values, making it easy to visualize there
    //   must clear those out, because a song is not loaded yet.
    ui->currentLocLabel->setText("");
    ui->songLengthLabel->setText("");

    inPreferencesDialog = false;

    // save info about the experimental timers tab
    // experimental timers tab is tab #1 (second tab)
    // experimental lyrics tab is tab #2 (third tab)
    tabmap.insert(1, QPair<QWidget *,QString>(ui->tabWidget->widget(1), ui->tabWidget->tabText(1)));
    tabmap.insert(2, QPair<QWidget *,QString>(ui->tabWidget->widget(2), ui->tabWidget->tabText(2)));

    bool timersEnabled = prefsManager.GetexperimentalTimersEnabled();
    // ----------
    showTimersTab = true;
    if (!timersEnabled) {
        ui->tabWidget->removeTab(1);  // it's remembered, don't worry!
        showTimersTab = false;
    }

    // ----------
    bool lyricsEnabled = true;
    showLyricsTab = true;
    lyricsTabNumber = (showTimersTab ? 2 : 1);
    if (!lyricsEnabled) {
        ui->tabWidget->removeTab(timersEnabled ? 2 : 1);  // it's remembered, don't worry!
        showLyricsTab = false;
        lyricsTabNumber = -1;
    }
    ui->tabWidget->setCurrentIndex(0); // Music Player tab is primary, regardless of last setting in Qt Designer
    on_tabWidget_currentChanged(0);     // update the menu item names

    ui->tabWidget->setTabText(lyricsTabNumber, "Lyrics");  // c.f. Preferences

    // ----------
    connect(ui->songTable->horizontalHeader(),&QHeaderView::sectionResized,
            this, &MainWindow::columnHeaderResized);
    connect(ui->songTable->horizontalHeader(),&QHeaderView::sortIndicatorChanged,
            this, &MainWindow::columnHeaderSorted);

    resize(QDesktopWidget().availableGeometry(this).size() * 0.7);  // initial size is 70% of screen

    setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            size(),
            qApp->desktop()->availableGeometry()
        )
    );

    ui->titleSearch->setFocus();  // this should be the intial focus

#ifdef DEBUGCLOCK
    analogClock->tipLengthAlarmMinutes = 10;  // FIX FIX FIX
    analogClock->breakLengthAlarmMinutes = 10;
#endif
    analogClock->tipLengthAlarmMinutes = tipLengthTimerLength;
    analogClock->breakLengthAlarmMinutes = breakLengthTimerLength;

    ui->warningLabel->setText("");
    ui->warningLabel->setStyleSheet("QLabel { color : red; }");
    ui->warningLabelCuesheet->setText("");
    ui->warningLabelCuesheet->setStyleSheet("QLabel { color : red; }");

    // LYRICS TAB ------------
    ui->pushButtonSetIntroTime->setEnabled(false);  // initially not singing call, buttons will be greyed out on Lyrics tab
    ui->pushButtonSetOutroTime->setEnabled(false);
    ui->dateTimeEditIntroTime->setEnabled(false);  // initially not singing call, buttons will be greyed out on Lyrics tab
    ui->dateTimeEditOutroTime->setEnabled(false);

    ui->pushButtonTestLoop->setHidden(true);
    ui->pushButtonTestLoop->setEnabled(false);

    analogClock->setTimerLabel(ui->warningLabel, ui->warningLabelCuesheet);  // tell the clock which label to use for the patter timer

    // read list of calls (in application bundle on Mac OS X)
    // TODO: make this work on other platforms, but first we have to figure out where to put the allcalls.csv
    //   file on those platforms.  It's convenient to stick it in the bundle on Mac OS X.  Maybe parallel with
    //   the executable on Windows and Linux?

    // restore the Flash Calls menu checkboxes state -----
    on_flashcallbasic_toggled(prefsManager.Getflashcallbasic());
    on_flashcallmainstream_toggled(prefsManager.Getflashcallmainstream());
    on_flashcallplus_toggled(prefsManager.Getflashcallplus());
    on_flashcalla1_toggled(prefsManager.Getflashcalla1());
    on_flashcalla2_toggled(prefsManager.Getflashcalla2());
    on_flashcallc1_toggled(prefsManager.Getflashcallc1());
    on_flashcallc2_toggled(prefsManager.Getflashcallc2());
    on_flashcallc3a_toggled(prefsManager.Getflashcallc3a());
    on_flashcallc3b_toggled(prefsManager.Getflashcallc3b());

    readFlashCallsList();

    currentSongType = "";
    currentSongTitle = "";

    // -----------------------------
    sessionActionGroup = new QActionGroup(this);
    sessionActionGroup->setExclusive(true);
    sessionActionGroup->addAction(ui->actionPractice);
    sessionActionGroup->addAction(ui->actionMonday);
    sessionActionGroup->addAction(ui->actionTuesday);
    sessionActionGroup->addAction(ui->actionWednesday);
    sessionActionGroup->addAction(ui->actionThursday);
    sessionActionGroup->addAction(ui->actionFriday);
    sessionActionGroup->addAction(ui->actionSaturday);
    sessionActionGroup->addAction(ui->actionSunday);

    // -----------------------
    // Make SD menu items for checker style mutually exclusive
    QList<QAction*> actions = ui->menuSequence->actions();
    //    qDebug() << "ACTIONS 1:" << actions;

    sdActionGroup1 = new QActionGroup(this);  // checker styles
    sdActionGroup1->setExclusive(true);

    sdActionGroup2 = new QActionGroup(this);  // SD GUI levels
    sdActionGroup2->setExclusive(true);

//    // WARNING: fragile.  If you add menu items above these, the numbers must be changed manually.
//    //   Is there a better way to do this?
//#define NORMALNUM (3)
//    sdActionGroup1->addAction(actions[NORMALNUM]);      // NORMAL
//    sdActionGroup1->addAction(actions[NORMALNUM+1]);    // Color only
//    sdActionGroup1->addAction(actions[NORMALNUM+2]);    // Mental image
//    sdActionGroup1->addAction(actions[NORMALNUM+3]);    // Sight

    connect(sdActionGroup1, SIGNAL(triggered(QAction*)), this, SLOT(sdActionTriggered(QAction*)));
    connect(sdActionGroup2, SIGNAL(triggered(QAction*)), this, SLOT(sdAction2Triggered(QAction*)));

    // let's look through the items in the SD menu
    QStringList ag1, ag2;
    ag1 << "Normal" << "Color only" << "Mental image" << "Sight";
    ag2 << "Mainstream" << "Plus" << "A1" << "A2" << "C1" << "C2" << "C3a" << "C3" << "C3x" << "C4a" << "C4" << "C4x";

    foreach (QAction *action, ui->menuSequence->actions()) {
        if (action->isSeparator()) {
//            qDebug() << "separator";
        } else if (action->menu()) {
//            qDebug() << "item with submenu: " << action->text();
            // iterating just one level down
            foreach (QAction *action2, action->menu()->actions()) {
                if (action2->isSeparator()) {
//                    qDebug() << "     separator";
                } else if (action2->menu()) {
//                    qDebug() << "     item with submenu: " << action2->text();
                } else {
//                    qDebug() << "     item: " << action2->text();
                    if (ag2.contains(action2->text()) ) {
                        sdActionGroup2->addAction(action2); // ag2 are all mutually exclusive, and are all one level down
                        action2->setCheckable(true); // all these items are checkable
                        if (action2->text() == "Plus") {
                            action2->setChecked(true);   // initially only PLUS is checked
                        }
                    }
                }
            }
        } else {
//            qDebug() << "item: " << action->text();
            if (ag1.contains(action->text()) ) {
                sdActionGroup1->addAction(action); // ag1 are all mutually exclusive, and are all at top level
            }
        }
    }

    // -----------------------
//    // Make SD menu items for GUI level mutually exclusive
//    QList<QAction*> actions2 = ui->menuSequence->actions();
//    qDebug() << "ACTIONS 2:" << actions2;

//    sdActionGroup2 = new QActionGroup(this);
//    sdActionGroup2->setExclusive(true);

//    // WARNING: fragile.  If you add menu items above these, the numbers must be changed manually.
//    //   Is there a better way to do this?
//#define BASICNUM (0)
//    sdActionGroup2->addAction(actions2[BASICNUM]);      // Basic
//    sdActionGroup2->addAction(actions2[BASICNUM+1]);    // Mainstream
//    sdActionGroup2->addAction(actions2[BASICNUM+2]);    // Plus
//    sdActionGroup2->addAction(actions2[BASICNUM+3]);    // A1
//    sdActionGroup2->addAction(actions2[BASICNUM+4]);    // A2
//    sdActionGroup2->addAction(actions2[BASICNUM+5]);    // C1
//    sdActionGroup2->addAction(actions2[BASICNUM+6]);    // C2
//    sdActionGroup2->addAction(actions2[BASICNUM+7]);    // C3a

//    connect(sdActionGroup2, SIGNAL(triggered(QAction*)), this, SLOT(sdAction2Triggered(QAction*)));

    QSettings settings;

    {
#if defined(Q_OS_MAC) | defined(Q_OS_WIN)
        ui->tableWidgetCallList->setColumnWidth(kCallListOrderCol,67);
        ui->tableWidgetCallList->setColumnWidth(kCallListCheckedCol, 34);
        ui->tableWidgetCallList->setColumnWidth(kCallListWhenCheckedCol, 100);
#elif defined(Q_OS_LINUX)
        ui->tableWidgetCallList->setColumnWidth(kCallListOrderCol,40);
        ui->tableWidgetCallList->setColumnWidth(kCallListCheckedCol, 24);
        ui->tableWidgetCallList->setColumnWidth(kCallListWhenCheckedCol, 100);
#endif
        ui->tableWidgetCallList->verticalHeader()->setVisible(false);  // turn off row numbers (we already have the Teach order, which is #'s)

        // #define kCallListNameCol        2
        QHeaderView *headerView = ui->tableWidgetCallList->horizontalHeader();
        headerView->setSectionResizeMode(kCallListOrderCol, QHeaderView::Fixed);
        headerView->setSectionResizeMode(kCallListCheckedCol, QHeaderView::Fixed);
        headerView->setSectionResizeMode(kCallListNameCol, QHeaderView::Stretch);
        headerView->setSectionResizeMode(kCallListWhenCheckedCol, QHeaderView::Fixed);
        headerView->setStretchLastSection(false);
        QString lastDanceProgram(settings.value("lastCallListDanceProgram").toString());
        loadDanceProgramList(lastDanceProgram);
    }

    lastCuesheetSavePath = settings.value("lastCuesheetSavePath").toString();

    initialize_internal_sd_tab();

    if (prefsManager.GetenableAutoAirplaneMode()) {
        airplaneMode(true);
    }

    connect(QApplication::instance(), SIGNAL(applicationStateChanged(Qt::ApplicationState)),
            this, SLOT(changeApplicationState(Qt::ApplicationState)));
    connect(QApplication::instance(), SIGNAL(focusChanged(QWidget*,QWidget*)),
            this, SLOT(focusChanged(QWidget*,QWidget*)));

    int songCount = 0;
    QString firstBadSongLine;
    QString CurrentPlaylistFileName = musicRootPath + "/.squaredesk/current.m3u";
    firstBadSongLine = loadPlaylistFromFile(CurrentPlaylistFileName, songCount);  // load "current.csv" (if doesn't exist, do nothing)

    ui->songTable->setColumnWidth(kNumberCol,40);  // NOTE: This must remain a fixed width, due to a bug in Qt's tracking of its width.
    ui->songTable->setColumnWidth(kTypeCol,96);
    ui->songTable->setColumnWidth(kLabelCol,80);
//  kTitleCol is always expandable, so don't set width here
    ui->songTable->setColumnWidth(kRecentCol, 70);
    ui->songTable->setColumnWidth(kAgeCol, 36);
    ui->songTable->setColumnWidth(kPitchCol,60);
    ui->songTable->setColumnWidth(kTempoCol,60);

    usePersistentFontSize(); // sets the font of the songTable, which is used by adjustFontSizes to scale other font sizes

    adjustFontSizes();  // now adjust to match contents ONCE
    //on_actionReset_triggered();  // set initial layout
    ui->songTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);  // auto set height of rows

    QPalette* palette1 = new QPalette();
    palette1->setColor(QPalette::ButtonText, Qt::red);
    ui->pushButtonCueSheetEditHeader->setPalette(*palette1);

    QPalette* palette2 = new QPalette();
    palette2->setColor(QPalette::ButtonText, QColor("#0000FF"));
    ui->pushButtonCueSheetEditArtist->setPalette(*palette2);

    QPalette* palette3 = new QPalette();
    palette3->setColor(QPalette::ButtonText, QColor("#60C060"));
    ui->pushButtonCueSheetEditLabel->setPalette(*palette3);

//    QPalette* palette4 = new QPalette();
//    palette4->setColor(QPalette::Background, QColor("#FFC0CB"));
//    ui->pushButtonCueSheetEditLyrics->setPalette(*palette4);

    ui->pushButtonCueSheetEditLyrics->setAutoFillBackground(true);
    ui->pushButtonCueSheetEditLyrics->setStyleSheet(
                "QPushButton {background-color: #FFC0CB; color: #000000; border-radius:4px; padding:1px 8px; border:0.5px solid #CF9090;}"
                "QPushButton:checked { background-color: qlineargradient(x1: 0, y1: 1, x2: 0, y2: 0, stop: 0 #1E72FE, stop: 1 #3E8AFC); color: #FFFFFF; border:0.5px solid #0D60E3;}"
                "QPushButton:pressed { background-color: qlineargradient(x1: 0, y1: 1, x2: 0, y2: 0, stop: 0 #1E72FE, stop: 1 #3E8AFC); color: #FFFFFF; border:0.5px solid #0D60E3;}"
                "QPushButton:disabled {background-color: #F1F1F1; color: #7F7F7F; border-radius:4px; padding:1px 8px; border:0.5px solid #D0D0D0;}"
                );

    maybeLoadCSSfileIntoTextBrowser();

    QTimer::singleShot(0,ui->titleSearch,SLOT(setFocus()));

    initReftab();
    currentSDVUILevel      = "plus"; // one of sd's names: {basic, mainstream, plus, a1, a2, c1, c2, c3a}
    currentSDKeyboardLevel = "plus"; // one of sd's names: {basic, mainstream, plus, a1, a2, c1, c2, c3a}

    // Initializers for these should probably be up in the constructor
    sdthread = new SDThread(this);
    sdthread->start();
    sdthread->unlock();
}

void MainWindow::musicRootModified(QString s)
{
    Q_UNUSED(s)
    // reload the musicTable.  Note that it will switch to default sort order.
    //   TODO: At some point, this probably should save the sort order, and then restore it.
    findMusic(musicRootPath,"","main", true);  // get the filenames from the user's directories
    loadMusicList(); // and filter them into the songTable
}

void MainWindow::changeApplicationState(Qt::ApplicationState state)
{
    currentApplicationState = state;
    microphoneStatusUpdate();  // this will disable the mics, if not Active state

    if (state == Qt::ApplicationActive) {
        justWentActive = true;
    }
}

void MainWindow::focusChanged(QWidget *old1, QWidget *new1)
{
    // all this mess, just to restore NO FOCUS, after ApplicationActivate, if there was NO FOCUS
    //   when going into Inactive state
    if (!justWentActive && new1 == 0) {
        oldFocusWidget = old1;  // GOING INACTIVE, RESTORE THIS ONE
    } else if (justWentActive) {
        if (oldFocusWidget == 0) {
            if (QApplication::focusWidget() != 0) {
                QApplication::focusWidget()->clearFocus();  // BOGUS
            }
        } else {
//            oldFocusWidget->setFocus(); // RESTORE HAPPENS HERE;  RESTORE DISABLED because it crashes
                                          //  if leave app with # open for edit and come back in
        }
        justWentActive = false;  // this was a one-shot deal.
    } else {
        oldFocusWidget = new1;  // clicked on something, this is the one to restore
    }

}

void MainWindow::setCurrentSessionId(int id)
{
    songSettings.setCurrentSession(id);
}

QString MainWindow::ageToIntString(QString ageString) {
    if (ageString == "") {
        return(QString(""));
    }
    int ageAsInt = (int)(ageString.toFloat());
    QString ageAsIntString(QString("%1").arg(ageAsInt, 3).trimmed());
//    qDebug() << "ageString: " << ageString << ", ageAsInt: " << ageAsInt << ", ageAsIntString: " << ageAsIntString;
    return(ageAsIntString);
}

QString MainWindow::ageToRecent(QString ageInDaysFloatString) {
    QDateTime now = QDateTime::currentDateTimeUtc();

    QString recentString = "";
    if (ageInDaysFloatString != "") {
        qint64 ageInSecs = (qint64)(60.0*60.0*24.0*ageInDaysFloatString.toFloat());
        bool newerThanFence = now.addSecs(-ageInSecs) > recentFenceDateTime;
        if (newerThanFence) {
//            qDebug() << "recent fence: " << recentFenceDateTime << ", now: " << now << ", ageInSecs: " << ageInSecs;
            recentString = "🔺";  // this is a nice compromise between clearly visible and too annoying
        } // else it will be ""
    }
    return(recentString);
}

void MainWindow::reloadSongAges(bool show_all_ages)  // also reloads Recent columns entries
{
    QHash<QString,QString> ages;
    songSettings.getSongAges(ages, show_all_ages);

//    QHash<QString,QString>::const_iterator i;
//    for (i = ages.constBegin(); i != ages.constEnd(); ++i) {
//        qDebug() << "key: " << i.key() << ", val: " << i.value();
//    }

    ui->songTable->setSortingEnabled(false);
    ui->songTable->hide();

    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QString origPath = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();
        QString path = songSettings.removeRootDirs(origPath);
        QHash<QString,QString>::const_iterator age = ages.constFind(path);

        ui->songTable->item(i,kAgeCol)->setText(age == ages.constEnd() ? "" : ageToIntString(age.value()));
        ui->songTable->item(i,kAgeCol)->setTextAlignment(Qt::AlignCenter);

        ui->songTable->item(i,kRecentCol)->setText(age == ages.constEnd() ? "" : ageToRecent(age.value()));
        ui->songTable->item(i,kRecentCol)->setTextAlignment(Qt::AlignCenter);
    }
    ui->songTable->show();
    ui->songTable->setSortingEnabled(true);
}

void MainWindow::setCurrentSessionIdReloadSongAges(int id)
{
    setCurrentSessionId(id);
    reloadSongAges(ui->actionShow_All_Ages->isChecked());
    on_comboBoxCallListProgram_currentIndexChanged(ui->comboBoxCallListProgram->currentIndex());
}

static CallListCheckBox * AddItemToCallList(QTableWidget *tableWidget,
                              const QString &number, const QString &name,
                              const QString &taughtOn)
{
    int initialRowCount = tableWidget->rowCount();
    tableWidget->setRowCount(initialRowCount + 1);
    int row = initialRowCount;

    QTableWidgetItem *numberItem = new QTableWidgetItem(number);
    numberItem->setTextAlignment(Qt::AlignCenter);  // center the #'s in the Teach column
    QTableWidgetItem *nameItem = new QTableWidgetItem(name);

    numberItem->setFlags(numberItem->flags() & ~Qt::ItemIsEditable);
    nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);

    tableWidget->setItem(row, kCallListOrderCol, numberItem);
    tableWidget->setItem(row, kCallListNameCol, nameItem);

    QTableWidgetItem *dateItem = new QTableWidgetItem(taughtOn);
    dateItem->setFlags(dateItem->flags() | Qt::ItemIsEditable);
    dateItem->setTextAlignment(Qt::AlignCenter);  // center the dates
    tableWidget->setItem(row, kCallListWhenCheckedCol, dateItem);

    CallListCheckBox *checkbox = new CallListCheckBox();
    //   checkbox->setStyleSheet("margin-left:50%; margin-right:50%;");
    QHBoxLayout *layout = new QHBoxLayout();
    QWidget *widget = new QWidget();
    layout->setAlignment( Qt::AlignCenter );
    layout->addWidget( checkbox );
    layout->setContentsMargins(0,0,0,0);
    widget->setLayout( layout);
    checkbox->setCheckState((taughtOn.isNull() || taughtOn.isEmpty()) ? Qt::Unchecked : Qt::Checked);
    tableWidget->setCellWidget(row, kCallListCheckedCol, widget );;
    checkbox->setRow(row);
    return checkbox;
}

void MainWindow::loadCallList(SongSettings &songSettings, QTableWidget *tableWidget, const QString &danceProgram, const QString &filename)
{
    static QRegularExpression regex_numberCommaName(QRegularExpression("^((\\s*\\d+)(\\.\\w+)?)\\,?\\s+(.*)$"));

    tableWidget->setRowCount(0);

    QFile inputFile(filename);
    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);
        int line_number = 0;
        while (!in.atEnd())
        {
            line_number++;
            QString line = in.readLine();

            QString number(QString("%1").arg(line_number, 2));
            QString name(line);

            QRegularExpressionMatch match = regex_numberCommaName.match(line);
            if (match.hasMatch())
            {
                QString prefix("");
                if (match.captured(2).length() < 2)
                {
                    prefix = " ";
                }
                number = prefix + match.captured(1);
                name = match.captured(4);
            }
            QString taughtOn = songSettings.getCallTaughtOn(danceProgram, name);
            CallListCheckBox *checkbox = AddItemToCallList(tableWidget, number, name, taughtOn);
            checkbox->setMainWindow(this);

        }
        inputFile.close();
    }
}

void breakDanceProgramIntoParts(const QString &filename,
                                QString &name,
                                QString &program)
{
    QFileInfo fi(filename);
    name = fi.completeBaseName();
    program = name;
    int j = name.indexOf('.');
    if (-1 != j)
    {
        bool isSortOrder = true;
        for (int i = 0; i < j; ++i)
        {
            if (!name[i].isDigit())
            {
                isSortOrder = false;
            }
        }
        if (!isSortOrder)
        {
            program = name.left(j);
        }
        name.remove(0, j + 1);
        j = name.indexOf('.');
        if (-1 != j && isSortOrder)
        {
            program = name.left(j);
            name.remove(0, j + 1);
        }
        else
        {
            program = name;
        }
    }

}

// =============================================================================
// =============================================================================
// =============================================================================
// START LYRICS EDITOR STUFF

// get a resource file, and return as string or "" if not found
QString MainWindow::getResourceFile(QString s) {
#if defined(Q_OS_MAC)
    QString appPath = QApplication::applicationFilePath();
    QString patterTemplatePath = appPath + "/Contents/Resources/" + s;
    patterTemplatePath.replace("Contents/MacOS/SquareDeskPlayer/","");
#elif defined(Q_OS_WIN32)
    // TODO: There has to be a better way to do this.
    QString appPath = QApplication::applicationFilePath();
    QString patterTemplatePath = appPath + "/" + s;
    patterTemplatePath.replace("SquareDeskPlayer.exe/","");
#else
    Q_UNUSED(s);
    // Linux
    return("");
#endif

    QString fileContents;

#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
    QFile file(patterTemplatePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open '" + s + "' file.";
        qDebug() << "looked here:" << patterTemplatePath;
        return("");  // NOTE: early return, couldn't find template file
    } else {
        fileContents = file.readAll();
        file.close();
    }
#else
    // LINUX
    // never gets here....
#endif

    return(fileContents);
}


void MainWindow::showHTML(QString fromWhere) {
    Q_UNUSED(fromWhere)
    qDebug() << "***** showHTML(" << fromWhere << "):\n";

    QString editedCuesheet = ui->textBrowserCueSheet->toHtml();
    QString tEditedCuesheet = tidyHTML(editedCuesheet);
    QString pEditedCuesheet = postProcessHTMLtoSemanticHTML(tEditedCuesheet);
    qDebug().noquote() << "***** Post-processed HTML will be:\n" << pEditedCuesheet;
}

void MainWindow::on_toolButtonEditLyrics_toggled(bool checkState)
{
//    qDebug() << "on_toolButtonEditLyrics_toggled" << checkState;
    bool checked = (checkState != Qt::Unchecked);

    ui->pushButtonCueSheetEditTitle->setEnabled(checked);
    ui->pushButtonCueSheetEditLabel->setEnabled(checked);
    ui->pushButtonCueSheetEditArtist->setEnabled(checked);
    ui->pushButtonCueSheetEditHeader->setEnabled(checked);
    ui->pushButtonCueSheetEditLyrics->setEnabled(checked);

    ui->pushButtonCueSheetEditBold->setEnabled(checked);
    ui->pushButtonCueSheetEditItalic->setEnabled(checked);

    ui->pushButtonCueSheetClearFormatting->setEnabled(checked);

    if (checked) {
        // unlocked now, so must set up button state, too
//        qDebug() << "setting up button state using lastKnownTextCharFormat...";
        on_textBrowserCueSheet_currentCharFormatChanged(lastKnownTextCharFormat);
    }
}

int MainWindow::currentSelectionContains() {
    int result = 0;
    QTextCursor cursor = ui->textBrowserCueSheet->textCursor();
    QString theHTML = cursor.selection().toHtml();

//    qDebug() << "\n*****selection (rich text):\n " << theHTML << "\n";

    // NOTE: This code depends on using a special cuesheet2.css file...
    if (theHTML.contains("color:#010101")) {
        result |= titleBit;
    }
    if (theHTML.contains("color:#60c060")) {
        result |= labelBit;
    }
    if (theHTML.contains("color:#0000ff")) {
        result |= artistBit;
    }
    if (theHTML.contains("color:#ff0002")) {
        result |= headerBit;
    }
    if (theHTML.contains("color:#030303")) {
        result |= lyricsBit;
    }
    if (theHTML.contains("color:#000000")) {
        result |= noneBit;
    }

    return (result);
}

void MainWindow::on_textBrowserCueSheet_selectionChanged()
{
    if (ui->toolButtonEditLyrics->isChecked()) {
        // if editing is enabled:
        if (ui->textBrowserCueSheet->textCursor().hasSelection()) {
            // if it has a non-empty selection, then set the buttons based on what the selection contains
            int selContains = currentSelectionContains();
//            qDebug() << "currentSelectionContains: " << selContains;

            ui->pushButtonCueSheetEditTitle->setChecked(selContains & titleBit);
            ui->pushButtonCueSheetEditLabel->setChecked(selContains & labelBit);
            ui->pushButtonCueSheetEditArtist->setChecked(selContains & artistBit);
            ui->pushButtonCueSheetEditHeader->setChecked(selContains & headerBit);
            ui->pushButtonCueSheetEditLyrics->setChecked(selContains & lyricsBit);
        } else {
            // else, base the button state on the charformat at the current cursor position
            QTextCharFormat tcf = ui->textBrowserCueSheet->textCursor().charFormat();

            //        qDebug() << "tell me about: " << tcf.font();
            //        qDebug() << "foreground: " << tcf.foreground();
            //        qDebug() << "background: " << tcf.background();
            //        qDebug() << "family: " << tcf.font().family();
            //        qDebug() << "point size: " << tcf.font().pointSize();
            //        qDebug() << "pixel size: " << tcf.font().pixelSize();

            charsType c = FG_BG_to_type(tcf.foreground().color(), tcf.background().color());
//            qDebug() << "empty selection, charsType: " << c;

            ui->pushButtonCueSheetEditTitle->setChecked(c == TitleChars);
            ui->pushButtonCueSheetEditLabel->setChecked(c == LabelChars);
            ui->pushButtonCueSheetEditArtist->setChecked(c == ArtistChars);
            ui->pushButtonCueSheetEditHeader->setChecked(c == HeaderChars);
            ui->pushButtonCueSheetEditLyrics->setChecked(c == LyricsChars);
        }
    } else {
        // if editing is disabled, all the buttons are disabled.
//        qDebug() << "selection, but editing is disabled.";
        ui->pushButtonCueSheetEditTitle->setChecked(false);
        ui->pushButtonCueSheetEditLabel->setChecked(false);
        ui->pushButtonCueSheetEditArtist->setChecked(false);
        ui->pushButtonCueSheetEditHeader->setChecked(false);
        ui->pushButtonCueSheetEditLyrics->setChecked(false);
    }
}

// TODO: can't make a doc from scratch yet.

void MainWindow::on_pushButtonCueSheetClearFormatting_clicked()
{
//    qDebug() << "on_pushButtonCueSheetClearFormatting_clicked";
        QTextCursor cursor = ui->textBrowserCueSheet->textCursor();

        // now look at it as HTML
        QString selected = cursor.selection().toHtml();
//        qDebug() << "\n***** initial selection (HTML): " << selected;

        // Qt gives us a whole HTML doc here.  Strip off all the parts we don't want.
        QRegExp startSpan("<span.*>");
        startSpan.setMinimal(true);  // don't be greedy!

        selected.replace(QRegExp("<.*<!--StartFragment-->"),"")
                .replace(QRegExp("<!--EndFragment-->.*</html>"),"")
                .replace(startSpan,"")
                .replace("</span>","")
                ;
//        qDebug() << "current replacement: " << selected;

        // WARNING: this has a dependency on internal cuesheet2.css's definition of BODY text.
        QString HTMLreplacement =
                "<span style=\" font-family:'Verdana'; font-size:large; color:#000000;\">" +
                selected +
                "</span>";

//        qDebug() << "\n***** HTMLreplacement: " << HTMLreplacement;

        cursor.beginEditBlock(); // start of grouping for UNDO purposes
        cursor.removeSelectedText();  // remove the rich text...
//        cursor.insertText(selected);  // ...and put back in the stripped-down text
        cursor.insertHtml(HTMLreplacement);  // ...and put back in the stripped-down text
        cursor.endEditBlock(); // end of grouping for UNDO purposes
}

// Knowing what the FG and BG colors are (from internal cuesheet2.css) allows us to determine the character type
// The parsing code below looks at FG and BG in a very specific order.  Be careful if making changes.
// The internal cuesheet2.css file is also now fixed.  If you change colors there, you'll break editing.
//

MainWindow::charsType MainWindow::FG_BG_to_type(QColor fg, QColor bg) {
    Q_UNUSED(bg)
//    qDebug() << "fg: " << fg.blue() << ", bg: " << bg.blue();
    return((charsType)fg.blue());  // the blue channel encodes the type
}

void MainWindow::on_textBrowserCueSheet_currentCharFormatChanged(const QTextCharFormat & f)
{
//    qDebug() << "on_textBrowserCueSheet_currentCharFormatChanged" << f;

    RecursionGuard guard(cuesheetEditorReactingToCursorMovement);

    ////    ui->pushButtonCueSheetEditHeader->setChecked(f.fontPointSize() == 14);

    ui->pushButtonCueSheetEditItalic->setChecked(f.fontItalic());
    ui->pushButtonCueSheetEditBold->setChecked(f.fontWeight() == QFont::Bold);

//    if (f.isCharFormat()) {
//        QTextCharFormat tcf = (QTextCharFormat)f;
////        qDebug() << "tell me about: " << tcf.font();
////        qDebug() << "foreground: " << tcf.foreground();
////        qDebug() << "background: " << tcf.background();
////        qDebug() << "family: " << tcf.font().family();
////        qDebug() << "point size: " << tcf.font().pointSize();
////        qDebug() << "pixel size: " << tcf.font().pixelSize();

//        charsType c = FG_BG_to_type(tcf.foreground().color(), tcf.background().color());
////        qDebug() << "charsType: " << c;

//        if (ui->toolButtonEditLyrics->isChecked()) {
//            ui->pushButtonCueSheetEditTitle->setChecked(c == TitleChars);
//            ui->pushButtonCueSheetEditLabel->setChecked(c == LabelChars);
//            ui->pushButtonCueSheetEditArtist->setChecked(c == ArtistChars);
//            ui->pushButtonCueSheetEditHeader->setChecked(c == HeaderChars);
//            ui->pushButtonCueSheetEditLyrics->setChecked(c == LyricsChars);
//        } else {
//            ui->pushButtonCueSheetEditTitle->setChecked(false);
//            ui->pushButtonCueSheetEditLabel->setChecked(false);
//            ui->pushButtonCueSheetEditArtist->setChecked(false);
//            ui->pushButtonCueSheetEditHeader->setChecked(false);
//            ui->pushButtonCueSheetEditLyrics->setChecked(false);
//        }
//    }

    lastKnownTextCharFormat = f;  // save it away for when we unlock editing
}

static void setSelectedTextToClass(QTextEdit *editor, QString blockClass)
{
//    qDebug() << "setSelectedTextToClass: " << blockClass;
    QTextCursor cursor = editor->textCursor();

    if (!cursor.hasComplexSelection())
    {
        // TODO: remove <SPAN class="title"></SPAN> from entire rest of the document (title is a singleton)
        QString selectedText = cursor.selectedText();
        if (selectedText.isEmpty())
        {
            // if cursor is not selecting any text, make the change apply to the entire line (vs block)
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            selectedText = cursor.selectedText();
//            qDebug() << "setSelectedTestToClass: " << selectedText;
        }

        if (!selectedText.isEmpty())  // this is not redundant.
        {
            cursor.beginEditBlock(); // start of grouping for UNDO purposes
            cursor.removeSelectedText();
            QString newText = "<SPAN class=\"" + blockClass + "\">" + selectedText.toHtmlEscaped() + "</SPAN>";
//            qDebug() << "newText before: " << newText;
            newText = newText.replace(QChar(0x2028),"</SPAN><BR/><SPAN class=\"" + blockClass + "\">");  // 0x2028 = Unicode Line Separator
//            qDebug() << "newText after: " << newText;
            cursor.insertHtml(newText);
            cursor.endEditBlock(); // end of grouping for UNDO purposes
        }


    } else {
        // this should only happen for tables, which we really don't support yet.
        qDebug() << "Sorry, on_pushButtonCueSheetEdit...: " + blockClass + ": Title_toggled has complex selection...";
    }
}

void MainWindow::on_pushButtonCueSheetEditHeader_clicked(bool /* checked */)
{
    if (!cuesheetEditorReactingToCursorMovement)
    {
        setSelectedTextToClass(ui->textBrowserCueSheet, "hdr");
    }
//    showHTML(__FUNCTION__);
}

void MainWindow::on_pushButtonCueSheetEditItalic_toggled(bool checked)
{
    if (!cuesheetEditorReactingToCursorMovement)
    {
        ui->textBrowserCueSheet->setFontItalic(checked);
    }
//    showHTML(__FUNCTION__);
}

void MainWindow::on_pushButtonCueSheetEditBold_toggled(bool checked)
{
    if (!cuesheetEditorReactingToCursorMovement)
    {
        ui->textBrowserCueSheet->setFontWeight(checked ? QFont::Bold : QFont::Normal);
    }
//    showHTML(__FUNCTION__);
}


void MainWindow::on_pushButtonCueSheetEditTitle_clicked(bool checked)
{
    Q_UNUSED(checked)
    if (!cuesheetEditorReactingToCursorMovement)
    {
        setSelectedTextToClass(ui->textBrowserCueSheet, "title");
    }
//    showHTML(__FUNCTION__);
}

void MainWindow::on_pushButtonCueSheetEditArtist_clicked(bool checked)
{
    Q_UNUSED(checked)
    if (!cuesheetEditorReactingToCursorMovement)
    {
        setSelectedTextToClass(ui->textBrowserCueSheet, "artist");
    }
//    showHTML(__FUNCTION__);
}

void MainWindow::on_pushButtonCueSheetEditLabel_clicked(bool checked)
{
    Q_UNUSED(checked)
    if (!cuesheetEditorReactingToCursorMovement)
    {
        setSelectedTextToClass(ui->textBrowserCueSheet, "label");
    }
//    showHTML(__FUNCTION__);
}

void MainWindow::on_pushButtonCueSheetEditLyrics_clicked(bool checked)
{
    Q_UNUSED(checked)
    if (!cuesheetEditorReactingToCursorMovement)
    {
        setSelectedTextToClass(ui->textBrowserCueSheet, "lyrics");
    }
//    showHTML(__FUNCTION__);
}

static bool isFileInPathStack(QList<QString> *pathStack, const QString &checkFilename)
{
    QListIterator<QString> iter(*pathStack);
    while (iter.hasNext()) {
        QString s = iter.next();
        QStringList sl1 = s.split("#!#");
        QString type = sl1[0];  // the type (of original pathname, before following aliases)
        QString filename = sl1[1];  // everything else
        if (filename == checkFilename)
            return true;
    }
    return false;
}

// This function is called to write out the tidied/semantically-processed HTML to a file.
// If SAVE or SAVE AS..., then the file is read in again from disk, in case there are round-trip problems.
//
void MainWindow::writeCuesheet(QString filename)
{
//    qDebug() << "writeCuesheet: " << filename;
    bool needs_extension = true;
    for (size_t i = 0; i < (sizeof(cuesheet_file_extensions) / sizeof(*cuesheet_file_extensions)); ++i)
    {
        QString ext(".");
        ext.append(cuesheet_file_extensions[i]);
        if (filename.endsWith(ext, Qt::CaseInsensitive))
        {
            needs_extension = false;
            break;
        }
    }
    if (needs_extension)
    {
        filename += ".html";
    }

    QFile file(filename);
    QDir d = QFileInfo(file).absoluteDir();
    QString directoryName = d.absolutePath();  // directory of the saved filename

    lastCuesheetSavePath = directoryName;

#define WRITETHEMODIFIEDLYRICSFILE
#ifdef WRITETHEMODIFIEDLYRICSFILE
    if ( file.open(QIODevice::WriteOnly) )
    {
        // Make sure the destructor gets called before we try to load this file...
        {
            QTextStream stream( &file );
            QString editedCuesheet = ui->textBrowserCueSheet->toHtml();
            QString tEditedCuesheet = tidyHTML(editedCuesheet);
            QString postProcessedCuesheet = postProcessHTMLtoSemanticHTML(tEditedCuesheet);
            stream << postProcessedCuesheet;
            stream.flush();
        }

        if (!isFileInPathStack(pathStack, filename))
        {
            QFileInfo fi(filename);
            QStringList section = fi.path().split("/");
            QString type = section[section.length()-1];  // must be the last item in the path
//            qDebug() << "writeCuesheet() adding " + type + "#!#" + filename + " to pathStack";
            pathStack->append(type + "#!#" + filename);
        }
    }
#else
                qDebug() << "************** SAVE FILE ***************";
                showHTML(__FUNCTION__);

                QString editedCuesheet = ui->textBrowserCueSheet->toHtml();
    //                qDebug().noquote() << "***** editedCuesheet to write:\n" << editedCuesheet;

                QString tEditedCuesheet = tidyHTML(editedCuesheet);
    //                qDebug().noquote() << "***** tidied editedCuesheet to write:\n" << tEditedCuesheet;

                QString postProcessedCuesheet = postProcessHTMLtoSemanticHTML(tEditedCuesheet);
                qDebug().noquote() << "***** I AM THINKING ABOUT WRITING TO FILE postProcessed:\n" << postProcessedCuesheet;
#endif
}

void MainWindow::on_pushButtonCueSheetEditSave_clicked()
{
//    qDebug() << "on_pushButtonCueSheetEditSave_clicked";
    saveLyricsAs();
}

QString MainWindow::tidyHTML(QString cuesheet) {
//    qDebug() << "tidyHTML";
//    qDebug().noquote() << "************\ncuesheet in:" << cuesheet;

    // first get rid of <L> and </L>.  Those are ILLEGAL.
    cuesheet.replace("<L>","",Qt::CaseInsensitive).replace("</L>","",Qt::CaseInsensitive);

    // then get rid of <NOBR> and </NOBR>, NOT SUPPORTED BY W3C.
    cuesheet.replace("<NOBR>","",Qt::CaseInsensitive).replace("</NOBR>","",Qt::CaseInsensitive);

    // and &nbsp; too...let the layout engine do its thing.
    cuesheet.replace("&nbsp;"," ");

    // convert to a c_string, for HTML-TIDY
    char* tidyInput;
    string csheet = cuesheet.toStdString();
    tidyInput = new char [csheet.size()+1];
    strcpy( tidyInput, csheet.c_str() );

////    qDebug().noquote() << "\n***** TidyInput:\n" << QString((char *)tidyInput);

    TidyBuffer output;// = {0};
    TidyBuffer errbuf;// = {0};
    tidyBufInit(&output);
    tidyBufInit(&errbuf);
    int rc = -1;
    Bool ok;

    // TODO: error handling here...using GOTO!

    TidyDoc tdoc = tidyCreate();
    ok = tidyOptSetBool( tdoc, TidyHtmlOut, yes );  // Convert to XHTML
    if (ok) {
        ok = tidyOptSetBool( tdoc, TidyUpperCaseTags, yes );  // span -> SPAN
    }
//    if (ok) {
//        ok = tidyOptSetInt( tdoc, TidyUpperCaseAttrs, TidyUppercaseYes );  // href -> HREF
//    }
    if (ok) {
        ok = tidyOptSetBool( tdoc, TidyDropEmptyElems, yes );  // Discard empty elements
    }
    if (ok) {
        ok = tidyOptSetBool( tdoc, TidyDropEmptyParas, yes );  // Discard empty p elements
    }
    if (ok) {
        ok = tidyOptSetInt( tdoc, TidyIndentContent, TidyYesState );  // text/block level content indentation
    }
    if (ok) {
        ok = tidyOptSetInt( tdoc, TidyWrapLen, 150 );  // text/block level content indentation
    }
    if (ok) {
        ok = tidyOptSetBool( tdoc, TidyMark, no);  // do not add meta element indicating tidied doc
    }
    if (ok) {
        ok = tidyOptSetBool( tdoc, TidyLowerLiterals, yes);  // Folds known attribute values to lower case
    }
    if (ok) {
        ok = tidyOptSetInt( tdoc, TidySortAttributes, TidySortAttrAlpha);  // Sort attributes
    }
    if ( ok )
        rc = tidySetErrorBuffer( tdoc, &errbuf );      // Capture diagnostics
    if ( rc >= 0 )
        rc = tidyParseString( tdoc, tidyInput );           // Parse the input
    if ( rc >= 0 )
        rc = tidyCleanAndRepair( tdoc );               // Tidy it up!
    if ( rc >= 0 )
        rc = tidyRunDiagnostics( tdoc );               // Kvetch
    if ( rc > 1 )                                    // If error, force output.
        rc = ( tidyOptSetBool(tdoc, TidyForceOutput, yes) ? rc : -1 );
    if ( rc >= 0 )
        rc = tidySaveBuffer( tdoc, &output );          // Pretty Print

    QString cuesheet_tidied;
    if ( rc >= 0 )
    {
        if ( rc > 0 ) {
//            qDebug().noquote() << "\n***** Diagnostics:" << QString((char*)errbuf.bp);
//            qDebug().noquote() << "\n***** TidyOutput:\n" << QString((char*)output.bp);
        }
        cuesheet_tidied = QString((char*)output.bp);
    }
    else {
        qDebug() << "***** Severe error:" << rc;
    }

    tidyBufFree( &output );
    tidyBufFree( &errbuf );
    tidyRelease( tdoc );

//    // get rid of TIDY cruft
////    cuesheet_tidied.replace("<META NAME=\"generator\" CONTENT=\"HTML Tidy for HTML5 for Mac OS X version 5.5.31\">","");

//    qDebug().noquote() << "************\ncuesheet out:" << cuesheet_tidied;

    return(cuesheet_tidied);
}

// ------------------------
QString MainWindow::postProcessHTMLtoSemanticHTML(QString cuesheet) {
//    qDebug() << "postProcessHTMLtoSemanticHTML";

    // margin-top:12px;
    // margin-bottom:12px;
    // margin-left:0px;
    // margin-right:0px;
    // -qt-block-indent:0;
    // text-indent:0px;
    // line-height:100%;
    // KEEP: background-color:#ffffe0;
    cuesheet
            .replace(QRegExp("margin-top:[0-9]+px;"), "")
            .replace(QRegExp("margin-bottom:[0-9]+px;"), "")
            .replace(QRegExp("margin-left:[0-9]+px;"), "")
            .replace(QRegExp("margin-right:[0-9]+px;"), "")
            .replace(QRegExp("text-indent:[0-9]+px;"), "")
            .replace(QRegExp("line-height:[0-9]+%;"), "")
            .replace(QRegExp("-qt-block-indent:[0-9]+;"), "")
            ;

    // get rid of unwanted QTextEdit tags
    QRegExp styleRegExp("(<STYLE.*</STYLE>)|(<META.*>)");
    styleRegExp.setMinimal(true);
    styleRegExp.setCaseSensitivity(Qt::CaseInsensitive);
    cuesheet.replace(styleRegExp,"");  // don't be greedy

//    qDebug().noquote() << "***** postProcess 1: " << cuesheet;
    QString cuesheet3 = tidyHTML(cuesheet);

    // now the semantic replacement.
    // assumes that QTextEdit spits out spans in a consistent way
    // TODO: allow embedded NL (due to line wrapping)
    // NOTE: background color is optional here, because I got rid of the the spec for it in BODY
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"font-family:'Verdana'; font-size:x-large; color:#ff0000;[\\s\n]*(background-color:#ffffe0;)*\">"),
                             "<SPAN class=\"hdr\">");
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"font-family:'Verdana'; font-size:large; color:#000000; background-color:#ffc0cb;\">"),  // background-color required for lyrics
                             "<SPAN class=\"lyrics\">");
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"font-family:'Verdana'; font-size:medium; color:#60c060;[\\s\n]*(background-color:#ffffe0;)*\">"),
                             "<SPAN class=\"label\">");
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"font-family:'Verdana'; font-size:medium; color:#0000ff;[\\s\n]*(background-color:#ffffe0;)*\">"),
                             "<SPAN class=\"artist\">");
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"font-family:'Verdana'; font-size:x-large;[\\s\n]*(font-weight:600;)*[\\s\n]*color:#000000;[\\s\n]*(background-color:#ffffe0;)*\">"),
                             "<SPAN class=\"title\">");
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"font-family:'Verdana'; font-size:large;[\\s\n]*font-weight:600;[\\s\n]*color:#000000;[\\s\n]*(background-color:#ffffe0;)*\">"),
                                     "<SPAN style=\"font-weight: Bold;\">");

    cuesheet3.replace("<P style=\"\">","<P>");
    cuesheet3.replace("<P style=\"background-color:#ffffe0;\">","<P>");  // background color already defaults via the BODY statement
    cuesheet3.replace("<BODY bgcolor=\"#FFFFE0\" style=\"font-family:'.SF NS Text'; font-size:13pt; font-weight:400; font-style:normal;\">","<BODY>");  // must go back to USER'S choices in cuesheet2.css

    // multi-step replacement
//    qDebug().noquote() << "***** REALLY BEFORE:\n" << cuesheet3;
    //      <SPAN style="font-family:'Verdana'; font-size:large; color:#000000; background-color:#ffffe0;">
    cuesheet3.replace(QRegExp("\"font-family:'Verdana'; font-size:large; color:#000000;( background-color:#ffffe0;)*\""),"\"XXXXX\""); // must go back to USER'S choices in cuesheet2.css
//    qDebug().noquote() << "***** BEFORE:\n" << cuesheet3;
    cuesheet3.replace(QRegExp("<SPAN style=[\\s\n]*\"XXXXX\">"),"<SPAN>");
//    qDebug().noquote() << "***** AFTER:\n" << cuesheet3;

    // now replace null SPAN tags
    QRegExp nullStyleRegExp("<SPAN>(.*)</SPAN>");
    nullStyleRegExp.setMinimal(true);
    nullStyleRegExp.setCaseSensitivity(Qt::CaseInsensitive);
    cuesheet3.replace(nullStyleRegExp,"\\1");  // don't be greedy, and replace <SPAN>foo</SPAN> with foo

    // TODO: bold -- <SPAN style="font-family:'Verdana'; font-size:large; font-weight:600; color:#000000;">
    // TODO: italic -- TBD
    // TODO: get rid of style="background-color:#ffffe0;, yellowish, put at top once
    // TODO: get rid of these, use body: <SPAN style="font-family:'Verdana'; font-size:large; color:#000000;">

    // put the <link rel="STYLESHEET" type="text/css" href="cuesheet2.css"> back in
    if (!cuesheet3.contains("<link",Qt::CaseInsensitive)) {
//        qDebug() << "Putting the <LINK> back in...";
        cuesheet3.replace("</TITLE>","</TITLE><LINK rel=\"STYLESHEET\" type=\"text/css\" href=\"cuesheet2.css\">");
    }
    // tidy it one final time before writing it to a file (gets rid of blank SPAN's, among other things)
    QString cuesheet4 = tidyHTML(cuesheet3);
//    qDebug().noquote() << "***** postProcess 2: " << cuesheet4;
    return(cuesheet4);
}

void MainWindow::maybeLoadCSSfileIntoTextBrowser() {
//    qDebug() << "maybeLoadCSSfileIntoTextBrowser";
    // makes the /lyrics directory, if it doesn't exist already
    // also copies cuesheet2.css to /lyrics, if not already present

    // read the CSS file (if any)
    PreferencesManager prefsManager;
    QString musicDirPath = prefsManager.GetmusicPath();
    QString lyricsDir = musicDirPath + "/lyrics";

    // if the lyrics directory doesn't exist, create it
    QDir dir(lyricsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // ------------------------------------------------------------------
    // get internal cuesheet2.css file, if it exists
    QString cuesheet2 = getResourceFile("cuesheet2.css");
//    qDebug() << "cuesheet2: " << cuesheet2;
    if (cuesheet2.isEmpty()) {
        qDebug() << "SOMETHING WENT WRONG with fetching the internal cuesheet2.css";
    } else {
        ui->textBrowserCueSheet->document()->setDefaultStyleSheet(cuesheet2);
    }
}

void MainWindow::loadCuesheet(const QString &cuesheetFilename)
{
    loadedCuesheetNameWithPath = ""; // nothing loaded yet

    QUrl cuesheetUrl(QUrl::fromLocalFile(cuesheetFilename));  // NOTE: can contain HTML that references a customer's cuesheet2.css
    if (cuesheetFilename.endsWith(".txt", Qt::CaseInsensitive)) {
        // text files are read in, converted to HTML, and sent to the Lyrics tab
        QFile f1(cuesheetFilename);
        f1.open(QIODevice::ReadOnly | QIODevice::Text);
        QTextStream in(&f1);
        QString html = txtToHTMLlyrics(in.readAll(), cuesheetFilename);
        ui->textBrowserCueSheet->setText(html);
        loadedCuesheetNameWithPath = cuesheetFilename;
        f1.close();
    }
    else if (cuesheetFilename.endsWith(".mp3", Qt::CaseInsensitive)) {
//        qDebug() << "loadCuesheet():";
        QString embeddedID3Lyrics = loadLyrics(cuesheetFilename);
//        qDebug() << "embLyrics:" << embeddedID3Lyrics;
        if (embeddedID3Lyrics != "") {
            QString HTMLlyrics = txtToHTMLlyrics(embeddedID3Lyrics, cuesheetFilename);
//            qDebug() << "HTML:" << HTMLlyrics;
            QString html(HTMLlyrics);  // embed CSS, if found, since USLT is plain text
            ui->textBrowserCueSheet->setHtml(html);
            loadedCuesheetNameWithPath = cuesheetFilename;
        }
    } else {

        // read in the HTML for the cuesheet

        QFile f1(cuesheetFilename);
        QString cuesheet;
        if ( f1.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f1);
            cuesheet = in.readAll();  // read the entire CSS file, if it exists

            if (cuesheet.contains("charset=windows-1252") || cuesheetFilename.contains("GP 956")) {  // WARNING: HACK HERE
                // this is very likely to be an HTML file converted from MS WORD,
                //   and it still uses windows-1252 encoding.

                f1.seek(0);  // go back to the beginning of the file

                QByteArray win1252bytes(f1.readAll());  // and read it again (as bytes this time)
                QTextCodec *codec = QTextCodec::codecForName("windows-1252");  // FROM win-1252 bytes
                cuesheet = codec->toUnicode(win1252bytes);                     // TO Unicode QString
            }

//            qDebug() << "Cuesheet: " << cuesheet;
            cuesheet.replace("\xB4","'");  // replace wacky apostrophe, which doesn't display well in QEditText
            // NOTE: o-umlaut is already translated (incorrectly) here to \xB4, too.  There's not much we
            //   can do with non UTF-8 HTML files that aren't otherwise marked as to encoding.

//#if defined(Q_OS_MAC) || defined(Q_OS_LINUX) || defined(Q_OS_WIN)
            // HTML-TIDY IT ON INPUT *********
            QString cuesheet_tidied = tidyHTML(cuesheet);
//#else
//            QString cuesheet_tidied = cuesheet;  // LINUX, WINDOWS
//#endif

            // ----------------------
            // set the HTML for the cuesheet itself (must set CSS first)
//            ui->textBrowserCueSheet->setHtml(cuesheet);
//            qDebug() << "tidied: " << cuesheet_tidied;
            ui->textBrowserCueSheet->setHtml(cuesheet_tidied);
            loadedCuesheetNameWithPath = cuesheetFilename;
            f1.close();
//            showHTML(__FUNCTION__);  // DEBUG DEBUG DEBUG
        }

    }
    ui->textBrowserCueSheet->document()->setModified(false);

    // -----------
    QTextCursor cursor = ui->textBrowserCueSheet->textCursor();     // cursor for this document
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor); // select entire document

    QTextBlockFormat fmt;       // = cursor.blockFormat(); // get format of current block
    fmt.setTopMargin(0.0);
    fmt.setBottomMargin(0.0);   // modify it

    cursor.mergeBlockFormat(fmt); // set margins to zero for all blocks

    cursor.movePosition(QTextCursor::Start);  // move cursor back to the start of the document
}

// END LYRICS EDITOR STUFF
// =============================================================================
// =============================================================================
// =============================================================================

void MainWindow::tableWidgetCallList_checkboxStateChanged(int row, int state)
{
    int currentIndex = ui->comboBoxCallListProgram->currentIndex();
    QString programFilename(ui->comboBoxCallListProgram->itemData(currentIndex).toString());
    QString displayName;
    QString danceProgram;
    breakDanceProgramIntoParts(programFilename, displayName, danceProgram);

    QString callName = ui->tableWidgetCallList->item(row,kCallListNameCol)->text();

    if (state == Qt::Checked)
    {
        songSettings.setCallTaught(danceProgram, callName);
    }
    else
    {
        songSettings.deleteCallTaught(danceProgram, callName);
    }

    QTableWidgetItem *dateItem(new QTableWidgetItem(songSettings.getCallTaughtOn(danceProgram, callName)));
    dateItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetCallList->setItem(row, kCallListWhenCheckedCol, dateItem);
}

void MainWindow::on_comboBoxCallListProgram_currentIndexChanged(int currentIndex)
{
    ui->tableWidgetCallList->setRowCount(0);
    ui->tableWidgetCallList->setSortingEnabled(false);
    QString programFilename(ui->comboBoxCallListProgram->itemData(currentIndex).toString());
    if (!programFilename.isNull() && !programFilename.isEmpty())
    {
        QString name;
        QString program;
        breakDanceProgramIntoParts(programFilename, name, program);

        loadCallList(songSettings, ui->tableWidgetCallList, program, programFilename);
        QSettings settings;
        settings.setValue("lastCallListDanceProgram",program);
    }
    ui->tableWidgetCallList->setSortingEnabled(true);
}


void MainWindow::on_comboBoxCuesheetSelector_currentIndexChanged(int currentIndex)
{
    if (currentIndex != -1 && !cuesheetEditorReactingToCursorMovement) {
        QString cuesheetFilename = ui->comboBoxCuesheetSelector->itemData(currentIndex).toString();
        loadCuesheet(cuesheetFilename);
    }
}

void MainWindow::on_menuLyrics_aboutToShow()
{
    // only allow Save if it's not a template, and the doc was modified
    ui->actionSave->setEnabled(ui->textBrowserCueSheet->document()->isModified() && !loadedCuesheetNameWithPath.contains(".template.html"));
    ui->actionLyricsCueSheetRevert_Edits->setEnabled(ui->textBrowserCueSheet->document()->isModified());
}

void MainWindow::on_actionLyricsCueSheetRevert_Edits_triggered(bool /*checked*/)
{
    on_comboBoxCuesheetSelector_currentIndexChanged(ui->comboBoxCuesheetSelector->currentIndex());
}

void MainWindow::on_actionIn_Out_Loop_points_to_default_triggered(bool /* checked */)
{
    // MUST scan here (once), because the user asked us to, and SetDefaultIntroOutroPositions() (below) needs it
    cBass.songStartDetector(qPrintable(currentMP3filenameWithPath), &startOfSong_sec, &endOfSong_sec);

    ui->seekBarCuesheet->SetDefaultIntroOutroPositions(tempoIsBPM, cBass.Stream_BPM, startOfSong_sec, endOfSong_sec, cBass.FileLength);
    ui->seekBar->SetDefaultIntroOutroPositions(tempoIsBPM, cBass.Stream_BPM, startOfSong_sec, endOfSong_sec, cBass.FileLength);
    double length = cBass.FileLength;
    double intro = ui->seekBarCuesheet->GetIntro();
    double outro = ui->seekBarCuesheet->GetOutro();

    ui->dateTimeEditIntroTime->setTime(QTime(0,0,0,0).addMSecs((int)(1000.0*intro*length+0.5)));  // milliseconds
    ui->dateTimeEditOutroTime->setTime(QTime(0,0,0,0).addMSecs((int)(1000.0*outro*length+0.5)));  // milliseconds
}

void MainWindow::on_actionCompact_triggered(bool checked)
{
    bool visible = !checked;
    setCueSheetAdditionalControlsVisible(visible);
    ui->actionCompact->setChecked(!visible);

    for (int col = 0; col < ui->gridLayout_2->columnCount(); ++col)
    {
        for (int row = 2; row < ui->gridLayout_2->rowCount(); ++row)
        {
            QLayoutItem *layout_item = ui->gridLayout_2->itemAtPosition(row,col);
            if (layout_item)
            {
                QWidget *widget = layout_item->widget();
                if (widget)
                {
                    if (visible)
                    {
                        widget->show();
                    }
                    else
                    {
                        widget->hide();
                    }
                }
            }
        }
    }

    ui->pushButtonTestLoop->setHidden(!songTypeNamesForPatter.contains(currentSongType)); // this button is PATTER ONLY
    return;
}


void MainWindow::on_actionShow_All_Ages_triggered(bool checked)
{
    reloadSongAges(checked);
}

void MainWindow::on_actionPractice_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(1);
}

void MainWindow::on_actionMonday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(2);
}

void MainWindow::on_actionTuesday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(3);
}

void MainWindow::on_actionWednesday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(4);
}

void MainWindow::on_actionThursday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(5);
}

void MainWindow::on_actionFriday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(6);
}

void MainWindow::on_actionSaturday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(7);
}

void MainWindow::on_actionSunday_triggered(bool /* checked */)
{
    setCurrentSessionIdReloadSongAges(8);
}


// ----------------------------------------------------------------------
MainWindow::~MainWindow()
{
    // Just before the app quits, save the current playlist state in "current.m3u", and it will be reloaded
    //   when the app starts up again.
    // Save the current playlist state to ".squaredesk/current.m3u".  Tempo/pitch are NOT saved here.
    QString PlaylistFileName = musicRootPath + "/.squaredesk/current.m3u";
    saveCurrentPlaylistToFile(PlaylistFileName);

    PreferencesManager prefsManager; // Will be using application information for correct location of your settings

    // bug workaround: https://bugreports.qt.io/browse/QTBUG-56448
    QColorDialog colorDlg(0);
    colorDlg.setOption(QColorDialog::NoButtons);
    colorDlg.setCurrentColor(Qt::white);

    delete ui;

    // REENABLE SCREENSAVER, RELEASE THE KRAKEN
#if defined(Q_OS_MAC)
    macUtils.reenableScreensaver();
#elif defined(Q_OS_WIN32)
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE , NULL, SPIF_SENDWININICHANGE);
#endif
    // REENABLE SCREENSAVER, RELEASE THE KRAKEN
#if defined(Q_OS_MAC)
    macUtils.reenableScreensaver();
#elif defined(Q_OS_WIN32)
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, TRUE , NULL, SPIF_SENDWININICHANGE);
#endif
    if (sd) {
        sd->kill();
    }
    if (sdthread)
    {
        sdthread->finishAndShutdownSD();
    }
    if (ps) {
        ps->kill();
    }

    if (prefsManager.GetenableAutoAirplaneMode()) {
        airplaneMode(false);
    }

    delete[] sessionActions;
    delete[] danceProgramActions;
    delete sessionActionGroup;
    delete sdActionGroupDanceProgram;

    clearLockFile(); // release the lock that we took (other locks were thrown away)
}

// ----------------------------------------------------------------------
void MainWindow::updateSongTableColumnView()
{
    PreferencesManager prefsManager;

    ui->songTable->setColumnHidden(kRecentCol,!prefsManager.GetshowRecentColumn());
    ui->songTable->setColumnHidden(kAgeCol,!prefsManager.GetshowAgeColumn());
    ui->songTable->setColumnHidden(kPitchCol,!prefsManager.GetshowPitchColumn());
    ui->songTable->setColumnHidden(kTempoCol,!prefsManager.GetshowTempoColumn());

    // http://www.qtcentre.org/threads/3417-QTableWidget-stretch-a-column-other-than-the-last-one
    QHeaderView *headerView = ui->songTable->horizontalHeader();
    headerView->setSectionResizeMode(kNumberCol, QHeaderView::Interactive);
    headerView->setSectionResizeMode(kTypeCol, QHeaderView::Interactive);
    headerView->setSectionResizeMode(kLabelCol, QHeaderView::Interactive);
    headerView->setSectionResizeMode(kTitleCol, QHeaderView::Stretch);

    headerView->setSectionResizeMode(kRecentCol, QHeaderView::Fixed);
    headerView->setSectionResizeMode(kAgeCol, QHeaderView::Fixed);
    headerView->setSectionResizeMode(kPitchCol, QHeaderView::Fixed);
    headerView->setSectionResizeMode(kTempoCol, QHeaderView::Fixed);
    headerView->setStretchLastSection(false);
}


// ----------------------------------------------------------------------
void MainWindow::on_loopButton_toggled(bool checked)
{
    if (checked) {
        ui->actionLoop->setChecked(true);

        ui->seekBar->SetLoop(true);
        ui->seekBarCuesheet->SetLoop(true);

        double songLength = cBass.FileLength;
//        qDebug() << "songLength: " << songLength << ", Intro: " << ui->seekBar->GetIntro();

//        cBass.SetLoop(songLength * 0.9, songLength * 0.1); // FIX: use parameters in the MP3 file
        cBass.SetLoop(songLength * ui->seekBar->GetOutro(),
                      songLength * ui->seekBar->GetIntro());
    }
    else {
        ui->actionLoop->setChecked(false);

        ui->seekBar->SetLoop(false);
        ui->seekBarCuesheet->SetLoop(false);

        cBass.ClearLoop();
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_monoButton_toggled(bool checked)
{
    if (checked) {
        ui->actionForce_Mono_Aahz_mode->setChecked(true);
        cBass.SetMono(true);
    }
    else {
        ui->actionForce_Mono_Aahz_mode->setChecked(false);
        cBass.SetMono(false);
    }

    // the Force Mono (Aahz Mode) setting is persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setforcemono(ui->actionForce_Mono_Aahz_mode->isChecked());
}

// ----------------------------------------------------------------------
void MainWindow::on_stopButton_clicked()
{
// TODO: instead of removing focus on STOP, better we should restore focus to the previous focused widget on STOP
//    if (QApplication::focusWidget() != NULL) {
//        QApplication::focusWidget()->clearFocus();  // we don't want to continue editing the search fields after a STOP
//                                                    //  or it will eat our keyboard shortcuts
//    }

    ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  // change PAUSE to PLAY
    ui->actionPlay->setText("Play");  // now stopped, press Cmd-P to Play
    currentState = kStopped;

    cBass.Stop();  // Stop playback, rewind to the beginning

    ui->nowPlayingLabel->setText(currentSongTitle);  // restore the song title, if we were Flash Call mucking with it

#ifdef REMOVESILENCE
    // last thing we do is move the stream position to 1 sec before start of music
    // this will move BOTH seekBar's to the right spot
    cBass.StreamSetPosition((double)startOfSong_sec);
    Info_Seekbar(false);  // update just the text
#else
    ui->seekBar->setValue(0);
    ui->seekBarCuesheet->setValue(0);
    Info_Seekbar(false);  // update just the text
#endif
}

// ----------------------------------------------------------------------
void MainWindow::randomizeFlashCall() {
    int numCalls = flashCalls.length();
    if (!numCalls)
        return;

    int newRandCallIndex;
    do {
        newRandCallIndex = qrand() % numCalls;
    } while (newRandCallIndex == randCallIndex);
    randCallIndex = newRandCallIndex;
}

// ----------------------------------------------------------------------
void MainWindow::on_playButton_clicked()
{
    if (!songLoaded) {
        return;  // if there is no song loaded, no point in toggling anything.
    }

    cBass.Play();  // currently paused, so start playing
    if (currentState == kStopped || currentState == kPaused) {
        // randomize the Flash Call, if PLAY (but not PAUSE) is pressed
        randomizeFlashCall();

        if (firstTimeSongIsPlayed)
        {
            firstTimeSongIsPlayed = false;
            saveCurrentSongSettings();
            songSettings.markSongPlayed(currentMP3filename, currentMP3filenameWithPath);
            QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
            QModelIndexList selected = selectionModel->selectedRows();

            ui->songTable->setSortingEnabled(false);
            int row = getSelectionRowForFilename(currentMP3filenameWithPath);
            if (row != -1)
            {
                ui->songTable->item(row, kAgeCol)->setText("0");
                ui->songTable->item(row, kAgeCol)->setTextAlignment(Qt::AlignCenter);

                ui->songTable->item(row, kRecentCol)->setText(ageToRecent("0"));
                ui->songTable->item(row, kRecentCol)->setTextAlignment(Qt::AlignCenter);
            }
            ui->songTable->setSortingEnabled(true);

            if (switchToLyricsOnPlay &&
                    (songTypeNamesForSinging.contains(currentSongType) || songTypeNamesForCalled.contains(currentSongType)))
            {
                // switch to Lyrics tab ONLY for singing calls or vocals
                for (int i = 0; i < ui->tabWidget->count(); ++i)
                {
                    if (ui->tabWidget->tabText(i).endsWith("*Lyrics"))  // do not switch if *Patter or Patter (using Lyrics tab for written Patter)
                    {
                        ui->tabWidget->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
        // If we just started playing, clear focus from all widgets
        if (QApplication::focusWidget() != NULL) {
            QApplication::focusWidget()->clearFocus();  // we don't want to continue editing the search fields after a STOP
                                                        //  or it will eat our keyboard shortcuts
        }
        ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));  // change PLAY to PAUSE
        ui->actionPlay->setText("Pause");
        currentState = kPlaying;
    }
    else {
        // TODO: we might want to restore focus here....
        ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  // change PAUSE to PLAY
        ui->actionPlay->setText("Play");
        currentState = kPaused;
        ui->nowPlayingLabel->setText(currentSongTitle);  // restore the song title, if we were Flash Call mucking with it
    }
    if (ui->checkBoxStartOnPlay->isChecked()) {
        on_pushButtonCountUpTimerStartStop_clicked();
    }
}

// ----------------------------------------------------------------------
bool MainWindow::timerStopStartClick(QTimer *&timer, QPushButton *button)
{
    if (timer) {
        button->setText("Start");
        timer->stop();
        delete timer;
        timer = NULL;
    }
    else {
        button->setText("Stop");
        timer = new QTimer(this);
        timer->start(1000);
    }
    return NULL != timer;
}

// ----------------------------------------------------------------------
int MainWindow::updateTimer(qint64 timeZeroEpochMs, QLabel *label)
{
    QDateTime now(QDateTime::currentDateTime());
    qint64 timeNowEpochMs = now.currentMSecsSinceEpoch();
    int signedSeconds = (int)((timeNowEpochMs - timeZeroEpochMs) / 1000);
    int seconds = signedSeconds;
    char sign = ' ';

    if (seconds < 0) {
        sign = '-';
        seconds = -seconds;
    }

    stringstream ss;
    int hours = seconds / (60*60);
    int minutes = (seconds / 60) % 60;

    ss << sign;
    if (hours) {
        ss << hours << ":" << setw(2);
    }
    ss << setfill('0') << minutes << ":" << setw(2) << setfill('0') << (seconds % 60);
    string s(ss.str());
    label->setText(s.c_str());
    return signedSeconds;
}

// ----------------------------------------------------------------------
void MainWindow::on_pushButtonCountDownTimerStartStop_clicked()
{
    if (timerStopStartClick(timerCountDown,
                            ui->pushButtonCountDownTimerStartStop)) {
        on_pushButtonCountDownTimerReset_clicked();
        connect(timerCountDown, SIGNAL(timeout()), this, SLOT(timerCountDown_update()));
    }
}

// ----------------------------------------------------------------------

const qint64 timerJitter = 50;

void MainWindow::on_pushButtonCountDownTimerReset_clicked()
{
    QString offset(ui->lineEditCountDownTimer->text());

    int seconds = 0;
    int minutes = 0;
    bool found_colon = false;

    for (int i = 0; i < offset.length(); ++i) {
        int ch = offset[i].unicode();

        if (ch >= '0' && ch <= '9') {
            if (found_colon) {
                seconds *= 10;
                seconds += ch - '0';
            }
            else {
                minutes *= 10;
                minutes += ch - '0';
            }
        }
        else if (ch == ':') {
            found_colon = true;
        }
    }
    timeCountDownZeroMs = QDateTime::currentDateTime().currentMSecsSinceEpoch();
    timeCountDownZeroMs += (qint64)(minutes * 60 + seconds) * (qint64)(1000) + timerJitter;
    updateTimer(timeCountDownZeroMs, ui->labelCountDownTimer);
}

// ----------------------------------------------------------------------
void MainWindow::on_pushButtonCountUpTimerStartStop_clicked()
{
    if (timerStopStartClick(timerCountUp,
                            ui->pushButtonCountUpTimerStartStop)) {
        on_pushButtonCountUpTimerReset_clicked();
        connect(timerCountUp, SIGNAL(timeout()), this, SLOT(timerCountUp_update()));
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_pushButtonCountUpTimerReset_clicked()
{
    timeCountUpZeroMs = QDateTime::currentDateTime().currentMSecsSinceEpoch() + timerJitter;
    updateTimer(timeCountUpZeroMs, ui->labelCountUpTimer);
}

// ----------------------------------------------------------------------
void MainWindow::timerCountUp_update()
{
    updateTimer(timeCountUpZeroMs, ui->labelCountUpTimer);
}

// ----------------------------------------------------------------------
void MainWindow::timerCountDown_update()
{
    if (updateTimer(timeCountDownZeroMs, ui->labelCountDownTimer) >= 0
            && ui->checkBoxPlayOnEnd->isChecked()
            && currentState == kStopped) {
        on_playButton_clicked();
    }
}

int MainWindow::getSelectionRowForFilename(const QString &filePath)
{
    for (int i=0; i < ui->songTable->rowCount(); i++) {
        QString origPath = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();
        if (filePath == origPath)
            return i;
    }
    return -1;
}

// ----------------------------------------------------------------------

void MainWindow::on_pitchSlider_valueChanged(int value)
{
    cBass.SetPitch(value);
    currentPitch = value;
    QString plural;
    if (currentPitch == 1 || currentPitch == -1) {
        plural = "";
    }
    else {
        plural = "s";
    }
    QString sign = "";
    if (currentPitch > 0) {
        sign = "+";
    }
    ui->currentPitchLabel->setText(sign + QString::number(currentPitch) +" semitone" + plural);

    saveCurrentSongSettings();
    // update the hidden pitch column
    ui->songTable->setSortingEnabled(false);
    int row = getSelectionRowForFilename(currentMP3filenameWithPath);
    if (row != -1)
    {
        ui->songTable->item(row, kPitchCol)->setText(QString::number(currentPitch)); // already trimmed()
    }
    ui->songTable->setSortingEnabled(true);
}

// ----------------------------------------------------------------------
void MainWindow::Info_Volume(void)
{
    int volSliderPos = ui->volumeSlider->value();
    if (volSliderPos == 0) {
        ui->currentVolumeLabel->setText("Mute");
    }
    else if (volSliderPos == 100) {
        ui->currentVolumeLabel->setText("MAX");
    }
    else {
        ui->currentVolumeLabel->setText(QString::number(volSliderPos)+"%");
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_volumeSlider_valueChanged(int value)
{
    int voltageLevelToSet = 100.0*pow(10.0,((((float)value*0.8)+20)/2.0 - 50)/20.0);
    if (value == 0) {
        voltageLevelToSet = 0;  // special case for slider all the way to the left (MUTE)
    }
    cBass.SetVolume(voltageLevelToSet);     // now logarithmic, 0 --> 0.01, 50 --> 0.1, 100 --> 1.0 (values * 100 for libbass)
    currentVolume = value;                  // this will be saved with the song (0-100)

    Info_Volume();  // update the slider text

    if (value == 0) {
        ui->actionMute->setText("Un&mute");
    }
    else {
        ui->actionMute->setText("&Mute");
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_actionMute_triggered()
{
    if (ui->volumeSlider->value() != 0) {
        previousVolume = ui->volumeSlider->value();
        ui->volumeSlider->setValue(0);
        ui->actionMute->setText("Un&mute");
    }
    else {
        ui->volumeSlider->setValue(previousVolume);
        ui->actionMute->setText("&Mute");
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_tempoSlider_valueChanged(int value)
{
    if (tempoIsBPM) {
        float desiredBPM = (float)value;            // desired BPM
        int newBASStempo = (int)(round(100.0*desiredBPM/baseBPM));
        cBass.SetTempo(newBASStempo);
        ui->currentTempoLabel->setText(QString::number(value) + " BPM (" + QString::number(newBASStempo) + "%)");
    }
    else {
        float basePercent = 100.0;                      // original detected percent
        float desiredPercent = (float)value;            // desired percent
        int newBASStempo = (int)(round(100.0*desiredPercent/basePercent));
        cBass.SetTempo(newBASStempo);
        ui->currentTempoLabel->setText(QString::number(value) + "%");
    }

    saveCurrentSongSettings();
    // update the hidden tempo column
    ui->songTable->setSortingEnabled(false);
    int row = getSelectionRowForFilename(currentMP3filenameWithPath);
    if (row != -1)
    {
        if (tempoIsBPM) {
            ui->songTable->item(row, kTempoCol)->setText(QString::number(value));
        }
        else {
            ui->songTable->item(row, kTempoCol)->setText(QString::number(value) + "%");
        }
    }
    ui->songTable->setSortingEnabled(true);

}

// ----------------------------------------------------------------------
void MainWindow::on_mixSlider_valueChanged(int value)
{
    int Lpercent, Rpercent;

    // NOTE: we're misleading the user a bit here.  It SOUNDS like it's doing the right thing,
    //   but under-the-covers we're implementing Constant Power, so the overall volume is (correctly)
    //   held constant.  From the user's perspective, the use of Constant Power means sin/cos(), which is
    //   not intuitive when converted to percent.  So, let's tell the user that it's all nicely linear
    //   (which will agree with the user's ear), and let's do the Right Thing Anyway internally.

    if (value < 0) {
        Lpercent = 100;
        Rpercent = 100 + value;
    } else {
        Rpercent = 100;
        Lpercent = 100 - value;
    }

    QString s = QString::number(Lpercent) + "%L/" + QString::number(Rpercent) + "%R ";

    ui->currentMixLabel->setText(s);
    cBass.SetPan(value/100.0);
}

// ----------------------------------------------------------------------
QString MainWindow::position2String(int position, bool pad = false)
{
    int songMin = position/60;
    int songSec = position - 60*songMin;
    QString songSecString = QString("%1").arg(songSec, 2, 10, QChar('0')); // pad with zeros
    QString s(QString::number(songMin) + ":" + songSecString);

    // pad on the left with zeros, if needed to prevent numbers shifting left and right
    if (pad) {
        // NOTE: don't use more than 7 chars total, or Possum Sop (long) will cause weird
        //   shift left/right effects when the slider moves.
        switch (s.length()) {
            case 4:
                s = "   " + s; // 4 + 3 = 7 chars
                break;
            case 5:
                s = "  " + s;  // 5 + 2 = 7 chars
                break;
            default:
                break;
        }
    }

    return s;
}

void InitializeSeekBar(MySlider *seekBar)
{
    seekBar->setMinimum(0);
    seekBar->setMaximum((int)cBass.FileLength-1); // NOTE: TRICKY, counts on == below
    seekBar->setTickInterval(10);  // 10 seconds per tick
}
void SetSeekBarPosition(MySlider *seekBar, int currentPos_i)
{
    seekBar->blockSignals(true); // setValue should NOT initiate a valueChanged()
    seekBar->setValue(currentPos_i);
    seekBar->blockSignals(false);
}
void SetSeekBarNoSongLoaded(MySlider *seekBar)
{
    seekBar->setMinimum(0);
    seekBar->setValue(0);
}

// ----------------------------------------------------------------------
void MainWindow::Info_Seekbar(bool forceSlider)
{
    static bool in_Info_Seekbar = false;
    if (in_Info_Seekbar) {
        return;
    }
    RecursionGuard recursion_guard(in_Info_Seekbar);

    if (songLoaded) {  // FIX: this needs to pay attention to the bool
        // FIX: this code doesn't need to be executed so many times.
        InitializeSeekBar(ui->seekBar);
        InitializeSeekBar(ui->seekBarCuesheet);

        cBass.StreamGetPosition();  // update cBass.Current_Position

        int currentPos_i = (int)cBass.Current_Position;
        if (forceSlider) {
            SetSeekBarPosition(ui->seekBar, currentPos_i);
            SetSeekBarPosition(ui->seekBarCuesheet, currentPos_i);

            int minScroll = ui->textBrowserCueSheet->verticalScrollBar()->minimum();
            int maxScroll = ui->textBrowserCueSheet->verticalScrollBar()->maximum();
            int maxSeekbar = ui->seekBar->maximum();  // NOTE: minSeekbar is always 0
            float fracSeekbar = (float)currentPos_i/(float)maxSeekbar;
            float targetScroll = 1.08 * fracSeekbar * (maxScroll - minScroll) + minScroll;  // FIX: this is heuristic and not right yet

            // NOTE: only auto-scroll when the lyrics are LOCKED (if not locked, you're probably editing)
            if (autoScrollLyricsEnabled &&
                    !ui->toolButtonEditLyrics->isChecked()) {
                // lyrics scrolling at the same time as the InfoBar
                ui->textBrowserCueSheet->verticalScrollBar()->setValue((int)targetScroll);
            }
        }
        int fileLen_i = (int)cBass.FileLength;

        if (currentPos_i == fileLen_i) {  // NOTE: TRICKY, counts on -1 above
            // avoids the problem of manual seek to max slider value causing auto-STOP
            if (!ui->actionContinuous_Play->isChecked()) {
                on_stopButton_clicked(); // pretend we pressed the STOP button when EOS is reached
            }
            else {
                // figure out which row is currently selected
                QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
                QModelIndexList selected = selectionModel->selectedRows();
                int row = -1;
                if (selected.count() == 1) {
                    // exactly 1 row was selected (good)
                    QModelIndex index = selected.at(0);
                    row = index.row();
                }
                else {
                    // more than 1 row or no rows at all selected (BAD)
                    return;
                }

                int maxRow = ui->songTable->rowCount() - 1;

                if (row != maxRow) {
                    on_nextSongButton_clicked(); // pretend we pressed the NEXT SONG button when EOS is reached, then:
                    on_playButton_clicked();     // pretend we pressed the PLAY button
                }
                else {
                    on_stopButton_clicked();     // pretend we pressed the STOP button when End of Playlist is reached
                }
            }
            return;
        }

        PreferencesManager prefsManager; // Will be using application information for correct location of your settings
        if (prefsManager.GetuseTimeRemaining()) {
            // time remaining in song
            ui->currentLocLabel->setText(position2String(fileLen_i - currentPos_i, true));  // pad on the left
        } else {
            // current position in song
            ui->currentLocLabel->setText(position2String(currentPos_i, true));              // pad on the left
        }
        ui->songLengthLabel->setText("/ " + position2String(fileLen_i));    // no padding

        // singing call sections
        if (songTypeNamesForSinging.contains(currentSongType) || songTypeNamesForCalled.contains(currentSongType)) {
            double introLength = ui->seekBar->GetIntro() * cBass.FileLength; // seconds
            double outroTime = ui->seekBar->GetOutro() * cBass.FileLength; // seconds
//            qDebug() << "InfoSeekbar()::introLength: " << introLength << ", " << outroTime;
            double outroLength = fileLen_i-outroTime;

            int section;
            if (currentPos_i < introLength) {
                section = 0; // intro
            } else if (currentPos_i > outroTime) {
                section = 8;  // tag
            } else {
                section = 1.0 + 7.0*((currentPos_i - introLength)/(fileLen_i-(introLength+outroLength)));
                if (section > 8 || section < 0) {
                    section = 0; // needed for the time before fields are fully initialized
                }
            }

            QStringList sectionName;
            sectionName << "Intro" << "Opener" << "Figure 1" << "Figure 2"
                        << "Break" << "Figure 3" << "Figure 4" << "Closer" << "Tag";

            if (cBass.Stream_State == BASS_ACTIVE_PLAYING &&
                    (songTypeNamesForSinging.contains(currentSongType) || songTypeNamesForCalled.contains(currentSongType))) {
                // if singing call OR called, then tell the clock to show the section type
                analogClock->setSingingCallSection(sectionName[section]);
            } else {
                // else tell the clock that there isn't a section type
                analogClock->setSingingCallSection("");
            }

//            qDebug() << "currentPos:" << currentPos_i << ", fileLen: " << fileLen_i
//                     << "outroTime:" << outroTime
//                     << "introLength:" << introLength
//                     << "outroLength:" << outroLength
//                     << "section: " << section
//                     << "sectionName[section]: " << sectionName[section];
        }

#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
        // FLASH CALL FEATURE ======================================
        // TODO: do this only if patter? -------------------
        // TODO: right now this is hard-coded for Plus calls.  Need to add a preference to specify other levels (not
        //   mutually exclusive, either).
        int flashCallEverySeconds = 10;
        if (currentPos_i % flashCallEverySeconds == 0 && currentPos_i != 0) {
            // Now pick a new random number, but don't pick the same one twice in a row.
            // TODO: should really do a permutation over all the allowed calls, with no repeats
            //   but this should be good enough for now, if the number of calls is sufficiently
            //   large.
            randomizeFlashCall();
        }

        if (flashCalls.length() != 0) {
            // if there are flash calls on the list, then Flash Calls are enabled.
             if (cBass.Stream_State == BASS_ACTIVE_PLAYING && songTypeNamesForPatter.contains(currentSongType)) {
                 // if playing, and Patter type
                 // TODO: don't show any random calls until at least the end of the first N seconds
                 ui->nowPlayingLabel->setStyleSheet("QLabel { color : red; font-style: italic; }");
                 ui->nowPlayingLabel->setText(flashCalls[randCallIndex]);
             } else {
                 ui->nowPlayingLabel->setStyleSheet("QLabel { color : black; font-style: normal; }");
                 ui->nowPlayingLabel->setText(currentSongTitle);
             }
        }
#endif
    }
    else {
        SetSeekBarNoSongLoaded(ui->seekBar);
        SetSeekBarNoSongLoaded(ui->seekBarCuesheet);
    }
}

// --------------------------------1--------------------------------------

// blame https://stackoverflow.com/questions/4065378/qt-get-children-from-layout
bool isChildWidgetOfAnyLayout(QLayout *layout, QWidget *widget)
{
   if (layout == NULL || widget == NULL)
      return false;

   if (layout->indexOf(widget) >= 0)
      return true;

   foreach(QObject *o, layout->children())
   {
      if (isChildWidgetOfAnyLayout((QLayout*)o,widget))
         return true;
   }

   return false;
}

void setVisibleWidgetsInLayout(QLayout *layout, bool visible)
{
   if (layout == NULL)
      return;

   QWidget *pw = layout->parentWidget();
   if (pw == NULL)
      return;

   foreach(QWidget *w, pw->findChildren<QWidget*>())
   {
      if (isChildWidgetOfAnyLayout(layout,w))
          w->setVisible(visible);
   }
}

bool isVisibleWidgetsInLayout(QLayout *layout)
{
   if (layout == NULL)
      return false;

   QWidget *pw = layout->parentWidget();
   if (pw == NULL)
      return false;

   foreach(QWidget *w, pw->findChildren<QWidget*>())
   {
      if (isChildWidgetOfAnyLayout(layout,w))
          return w->isVisible();
   }
   return false;
}


void MainWindow::setCueSheetAdditionalControlsVisible(bool visible)
{
    setVisibleWidgetsInLayout(ui->verticalLayoutCueSheetAdditional, visible);
}

bool MainWindow::cueSheetAdditionalControlsVisible()
{
    return isVisibleWidgetsInLayout(ui->verticalLayoutCueSheetAdditional);
}

// --------------------------------1--------------------------------------


double timeToDouble(const QString &str, bool *ok)
{
    double t = -1;
    static QRegularExpression regex = QRegularExpression("((\\d+)\\:)?(\\d+(\\.\\d+)?)");
    QRegularExpressionMatch match = regex.match(str);
    if (match.hasMatch())
    {
        t = match.captured(3).toDouble(ok);
        if (*ok)
        {
            if (match.captured(2).length())
            {
                t += 60 * match.captured(2).toDouble(ok);
            }
        }
    }
//    qDebug() << "timeToDouble: " << str << ", out: " << t;
    return t;
}


void MainWindow::on_pushButtonClearTaughtCalls_clicked()
{
    QString danceProgram(ui->comboBoxCallListProgram->currentText());
    QMessageBox::StandardButton reply;

    reply = QMessageBox::question(this, "Clear Taught Calls",
                                  "Do you really want to clear all taught calls for the " +
                                   danceProgram +
                                  " dance program for the current session?",
                                  QMessageBox::Yes|QMessageBox::No);
  if (reply == QMessageBox::Yes) {
      songSettings.clearTaughtCalls(danceProgram);
      on_comboBoxCallListProgram_currentIndexChanged(ui->comboBoxCallListProgram->currentIndex());
  } else {
  }
}

// --------------------------------1--------------------------------------
void MainWindow::getCurrentPointInStream(double *pos, double *len) {
    double position, length;

    if (cBass.Stream_State == BASS_ACTIVE_PLAYING) {
        // if we're playing, this is accurate to sub-second.
        cBass.StreamGetPosition(); // snapshot the current position
        position = cBass.Current_Position;
        length = cBass.FileLength;  // always use the value with maximum precision

    } else {
        // if we're NOT playing, this is accurate to the second.  (This should be fixed!)
        position = (double)(ui->seekBarCuesheet->value());
        length = ui->seekBarCuesheet->maximum();
    }

    // return values
    *pos = position;
    *len = length;
}

// --------------------------------1--------------------------------------
void MainWindow::on_pushButtonSetIntroTime_clicked()
{
    double position, length;
    getCurrentPointInStream(&position, &length);

    QTime currentOutroTime = ui->dateTimeEditOutroTime->time();
    double currentOutroTimeSec = 60.0*currentOutroTime.minute() + currentOutroTime.second() + currentOutroTime.msec()/1000.0;
    position = fmax(0.0, fmin(position, (int)currentOutroTimeSec) );

    // set in ms
    ui->dateTimeEditIntroTime->setTime(QTime(0,0,0,0).addMSecs((int)(1000.0*position+0.5))); // milliseconds

    // set in fractional form
    float frac = position/length;
    ui->seekBarCuesheet->SetIntro(frac);  // after the events are done, do this.
    ui->seekBar->SetIntro(frac);

    on_loopButton_toggled(ui->actionLoop->isChecked()); // then finally do this, so that cBass is told what the loop points are (or they are cleared)
}

// --------------------------------1--------------------------------------
void MainWindow::on_pushButtonSetOutroTime_clicked()
{
    double position, length;
    getCurrentPointInStream(&position, &length);

    QTime currentIntroTime = ui->dateTimeEditIntroTime->time();
    double currentIntroTimeSec = 60.0*currentIntroTime.minute() + currentIntroTime.second() + currentIntroTime.msec()/1000.0;
    position = fmin(length, fmax(position, (int)currentIntroTimeSec) );

    // set in ms
    ui->dateTimeEditOutroTime->setTime(QTime(0,0,0,0).addMSecs((int)(1000.0*position+0.5))); // milliseconds

    // set in fractional form
    float frac = position/length;
    ui->seekBarCuesheet->SetOutro(frac);  // after the events are done, do this.
    ui->seekBar->SetOutro(frac);

    on_loopButton_toggled(ui->actionLoop->isChecked()); // then finally do this, so that cBass is told what the loop points are (or they are cleared)
}

// --------------------------------1--------------------------------------
void MainWindow::on_seekBarCuesheet_valueChanged(int value)
{
    on_seekBar_valueChanged(value);
}

// ----------------------------------------------------------------------
void MainWindow::on_seekBar_valueChanged(int value)
{
    // These must happen in this order.
    cBass.StreamSetPosition(value);
    Info_Seekbar(false);
}

// ----------------------------------------------------------------------
void MainWindow::on_clearSearchButton_clicked()
{
    // FIX: bug when clearSearch is pressed, the order in the songTable can change.

    // figure out which row is currently selected
    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    int row = -1;
    if (selected.count() == 1) {
        // exactly 1 row was selected (good)
        QModelIndex index = selected.at(0);
        row = index.row();
    }
    else {
        // more than 1 row or no rows at all selected (BAD)
    }

    ui->labelSearch->setText("");
    ui->typeSearch->setText("");
    ui->titleSearch->setText("");

    if (row != -1) {
        // if a row was selected, restore it after a clear search
        // FIX: this works much of the time, but it doesn't handle the case where search field is typed, then cleared.  In this case,
        //   the row isn't highlighted again.
        ui->songTable->selectRow(row);
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_actionLoop_triggered()
{
    on_loopButton_toggled(ui->actionLoop->isChecked());
}

// ----------------------------------------------------------------------
void MainWindow::on_UIUpdateTimerTick(void)
{

#if defined(Q_OS_MAC)
    if (screensaverSeconds++ % 55 == 0) {  // 55 because lowest screensaver setting is 60 seconds of idle time
        macUtils.disableScreensaver(); // NOTE: now needs to be called every N seconds
    }
#endif

    Info_Seekbar(true);

    // update the session coloring analog clock
    QTime time = QTime::currentTime();
    int theType = NONE;
//    qDebug() << "Stream_State:" << cBass.Stream_State; //FIX
    if (cBass.Stream_State == BASS_ACTIVE_PLAYING) {
        // if it's currently playing (checked once per second), then color this segment
        //   with the current segment type
        if (songTypeNamesForPatter.contains(currentSongType)) {
            theType = PATTER;
        }
        else if (songTypeNamesForSinging.contains(currentSongType)) {
            theType = SINGING;
        }
        else if (songTypeNamesForCalled.contains(currentSongType)) {
            theType = SINGING_CALLED;
        }
        else if (songTypeNamesForExtras.contains(currentSongType)) {
            theType = XTRAS;
        }
        else {
            theType = NONE;
        }

        analogClock->breakLengthAlarm = false;  // if playing, then we can't be in break
    } else if (cBass.Stream_State == BASS_ACTIVE_PAUSED) {
        // if we paused due to FADE, for example...
        // FIX: this could be factored out, it's used twice.
        ui->playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  // change PAUSE to PLAY
        ui->actionPlay->setText("Play");
        currentState = kPaused;
        ui->nowPlayingLabel->setText(currentSongTitle);  // restore the song title, if we were Flash Call mucking with it
    }

#ifndef DEBUGCLOCK
    analogClock->setSegment(time.hour(), time.minute(), time.second(), theType);  // always called once per second
#else
//    analogClock->setSegment(0, time.minute(), time.second(), theType);  // DEBUG DEBUG DEBUG
    analogClock->setSegment(time.minute(), time.second(), 0, theType);  // DEBUG DEBUG DEBUG
#endif

    // ------------------------
    // Sounds associated with Tip and Break Timers (one-shots)
    newTimerState = analogClock->currentTimerState;

    if ((newTimerState & THIRTYSECWARNING)&&!(oldTimerState & THIRTYSECWARNING)) {
        // one-shot transition to 30 second warning
        if (tipLength30secEnabled) {
            playSFX("thirty_second_warning");  // play this file (once), if warning enabled and we cross T-30 boundary
        }
    }

    if ((newTimerState & LONGTIPTIMEREXPIRED)&&!(oldTimerState & LONGTIPTIMEREXPIRED)) {
        // one-shot transitioned to Long Tip
        switch (tipLengthAlarmAction) {
        case 1: playSFX("long_tip"); break;
        default:
            if (tipLengthAlarmAction < 6) {  // unsigned, so always >= 0
                playSFX(QString::number(tipLengthAlarmAction-1));
            }
            break;
        }
    }

    if ((newTimerState & BREAKTIMEREXPIRED)&&!(oldTimerState & BREAKTIMEREXPIRED)) {
        // one-shot transitioned to End of Break
        switch (breakLengthAlarmAction) {
        case 1: playSFX("break_over"); break;
        default:
            if (breakLengthAlarmAction < 6) {  // unsigned, so always >= 0
                playSFX(QString::number(breakLengthAlarmAction-1));
            }
            break;
        }
    }
    oldTimerState = newTimerState;

    // -----------------------
    // about once a second, poll for volume changes
    newVolumeList = getCurrentVolumes();  // get the current state of the world
    qSort(newVolumeList);  // sort to allow direct comparison
    if(lastKnownVolumeList == newVolumeList){
        // This space intentionally blank.
    } else {
        on_newVolumeMounted();
    }
    lastKnownVolumeList = newVolumeList;
}

// ----------------------------------------------------------------------
void MainWindow::on_vuMeterTimerTick(void)
{
    float currentVolumeSlider = ui->volumeSlider->value();
    int level = cBass.StreamGetVuMeter();
    float levelF = (currentVolumeSlider/100.0)*((float)level)/32768.0;
    // TODO: iff music is playing.
    vuMeter->levelChanged(levelF/2.0,levelF,256);  // 10X/sec, update the vuMeter
}

// --------------
void MainWindow::closeEvent(QCloseEvent *event)
{
    // Work around bug: https://codereview.qt-project.org/#/c/125589/
    if (closeEventHappened) {
        event->accept();
        return;
    }
    closeEventHappened = true;
    if (true) {
        on_actionAutostart_playback_triggered();  // write AUTOPLAY setting back
        event->accept();  // OK to close, if user said "OK" or "SAVE"
        saveCurrentSongSettings();

        // as per http://doc.qt.io/qt-5.7/restoring-geometry.html
        QSettings settings;
        settings.setValue("lastCuesheetSavePath", lastCuesheetSavePath);
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState());
        QMainWindow::closeEvent(event);
    }
    else {
        event->ignore();  // do not close, if used said "CANCEL"
        closeEventHappened = false;
    }
}

// ------------------------------------------------------------------------------------------
void MainWindow::aboutBox()
{
    QMessageBox msgBox;
    msgBox.setText(QString("<p><h2>SquareDesk Player, V") + QString(VERSIONSTRING) + QString("</h2>") +
                   QString("<p>See our website at <a href=\"http://squaredesk.net\">squaredesk.net</a></p>") +
                   QString("Uses: <a href=\"http://www.un4seen.com/bass.html\">libbass</a>, ") +
                   QString("<a href=\"http://www.jobnik.org/?mnu=bass_fx\">libbass_fx</a>, ") +
                   QString("<a href=\"http://www.lynette.org/sd\">sd</a>, ") +
                   QString("<a href=\"http://cmusphinx.sourceforge.net\">PocketSphinx</a>, ") +
                   QString("<a href=\"https://github.com/yshurik/qpdfjs\">qpdfjs</a>, ") +
                   QString("<a href=\"http://tidy.sourceforge.net\">tidy-html5</a>, and ") +
                   QString("<a href=\"http://quazip.sourceforge.net\">QuaZIP</a>.") +
                   QString("<p>Thanks to: <a href=\"http://all8.com\">all8.com</a>"));
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

bool MainWindow::someWebViewHasFocus() {
//    qDebug() << "numWebviews: " << numWebviews;
    bool hasFocus = false;
    for (unsigned int i=0; i<numWebviews && !hasFocus; i++) {
//        qDebug() << "     checking: " << i;
        hasFocus = hasFocus || ((numWebviews > i) && (webview[i] != nullptr) && (webview[i]->hasFocus()));
    }
//    qDebug() << "     returning: " << hasFocus;
    return(hasFocus);
}


// ------------------------------------------------------------------------
// http://www.codeprogress.com/cpp/libraries/qt/showQtExample.php?key=QApplicationInstallEventFilter&index=188
bool GlobalEventFilter::eventFilter(QObject *Object, QEvent *Event)
{
    if (Event->type() == QEvent::KeyPress) {
        QKeyEvent *KeyEvent = (QKeyEvent *)Event;

        MainWindow *maybeMainWindow = dynamic_cast<MainWindow *>(((QApplication *)Object)->activeWindow());
        if (maybeMainWindow == 0) {
            // if the PreferencesDialog is open, for example, do not dereference the NULL pointer (duh!).
            return QObject::eventFilter(Object,Event);
        }

        // if any of these widgets has focus, let them process the key
        //  otherwise, we'll process the key
        // UNLESS it's one of the search/timer edit fields and the ESC key is pressed (we must still allow
        //   stopping of music when editing a text field).  Sorry, can't use the SPACE BAR
        //   when editing a search field, because " " is a valid character to search for.
        //   If you want to do this, hit ESC to get out of edit search field mode, then SPACE.
        if (ui->lineEditSDInput->hasFocus()
            && KeyEvent->key() == Qt::Key_Tab)
        {
            maybeMainWindow->do_sd_tab_completion();
            return true;
        }
        else if ( !(ui->labelSearch->hasFocus() ||
               ui->typeSearch->hasFocus() ||
               ui->titleSearch->hasFocus() ||
               (ui->textBrowserCueSheet->hasFocus() && ui->toolButtonEditLyrics->isChecked()) ||
               ui->dateTimeEditIntroTime->hasFocus() ||
               ui->dateTimeEditOutroTime->hasFocus() ||
               ui->lineEditSDInput->hasFocus() ||
#ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
               ui->lineEditCountDownTimer->hasFocus() ||
               ui->lineEditChoreographySearch->hasFocus() ||
#endif // ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
               ui->songTable->isEditing() ||
//               maybeMainWindow->webview[0]->hasFocus() ||      // EXPERIMENTAL FIX FIX FIX, will crash if webview[n] not exist yet
               maybeMainWindow->someWebViewHasFocus() ) ||           // safe now (won't crash, if there are no webviews!)

             ( (ui->labelSearch->hasFocus() ||
                ui->typeSearch->hasFocus() ||
                ui->titleSearch->hasFocus() ||
                ui->dateTimeEditIntroTime->hasFocus() ||
                ui->dateTimeEditOutroTime->hasFocus() ||

                ui->lineEditCountDownTimer->hasFocus() ||
                ui->lineEditSDInput->hasFocus() || 
                ui->textBrowserCueSheet->hasFocus()) &&
                (KeyEvent->key() == Qt::Key_Escape) )  ||

             ( (ui->labelSearch->hasFocus() || ui->typeSearch->hasFocus() || ui->titleSearch->hasFocus()) &&
               (KeyEvent->key() == Qt::Key_Return || KeyEvent->key() == Qt::Key_Up || KeyEvent->key() == Qt::Key_Down)
             )
           ) {
            // call handleKeypress on the Applications's active window ONLY if this is a MainWindow
//            qDebug() << "eventFilter YES:" << ui << currentWindowName << maybeMainWindow;
            return (maybeMainWindow->handleKeypress(KeyEvent->key(), KeyEvent->text()));
        }

    }
    return QObject::eventFilter(Object,Event);
}

void MainWindow::actionTempoPlus()
{
    ui->tempoSlider->setValue(ui->tempoSlider->value() + 1);
    on_tempoSlider_valueChanged(ui->tempoSlider->value());
}
void MainWindow::actionTempoMinus()
{
    ui->tempoSlider->setValue(ui->tempoSlider->value() - 1);
    on_tempoSlider_valueChanged(ui->tempoSlider->value());
}
void MainWindow::actionFadeOutAndPause()
{
    cBass.FadeOutAndPause();
}
void MainWindow::actionNextTab()
{
    int currentTab = ui->tabWidget->currentIndex();
    if (currentTab == 0) {
        // if Music tab active, go to Lyrics tab
        ui->tabWidget->setCurrentIndex(lyricsTabNumber);
    } else if (currentTab == lyricsTabNumber) {
        // if Lyrics tab active, go to Music tab
        ui->tabWidget->setCurrentIndex(0);
    } else {
        // if currently some other tab, just go to the Music tab
        ui->tabWidget->setCurrentIndex(0);
    }
}

// ----------------------------------------------------------------------
bool MainWindow::handleKeypress(int key, QString text)
{
    Q_UNUSED(text)
    QString tabTitle;

    if (inPreferencesDialog || !trapKeypresses || (prefDialog != NULL)) {
        return false;
    }

    switch (key) {

        case Qt::Key_Escape:
            // ESC is special:  it always gets you out of editing a search field or timer field, and it can
            //   also STOP the music (second press, worst case)

            oldFocusWidget = 0;  // indicates that we want NO FOCUS on restore
            if (QApplication::focusWidget() != NULL) {
                QApplication::focusWidget()->clearFocus();  // clears the focus from ALL widgets
            }
            oldFocusWidget = 0;  // indicates that we want NO FOCUS on restore, yes both of these are needed.

            // FIX: should we also stop editing of the songTable on ESC?

            ui->textBrowserCueSheet->clearFocus();  // ESC should always get us out of editing lyrics/patter

            if (ui->labelSearch->text() != "" || ui->typeSearch->text() != "" || ui->titleSearch->text() != "") {
                // clear the search fields, if there was something in them.  (First press of ESCAPE).
                ui->labelSearch->setText("");
                ui->typeSearch->setText("");
                ui->titleSearch->setText("");
            } else {
                // if the search fields were already clear, then this is the second press of ESCAPE (or the first press
                //   of ESCAPE when the search function was not being used).  So, ONLY NOW do we Stop Playback.
                // So, GET ME OUT OF HERE is now "ESC ESC", or "Hit ESC a couple of times".
                //    and, CLEAR SEARCH is just ESC (or click on the Clear Search button).
                if (currentState == kPlaying) {
                    on_playButton_clicked();  // we were playing, so PAUSE now.
                }
            }

            cBass.StopAllSoundEffects();  // and, it also stops ALL sound effects
            break;

            // TO BE REMOVED ONCE WE SETTLE ON HOTKEY EDITING
#if 0
        case Qt::Key_End:  // FIX: should END go to the end of the song? or stop playback?
        case Qt::Key_S:
            on_stopButton_clicked();
            break;

        case Qt::Key_P:
        case Qt::Key_Space:  // for SqView compatibility ("play/pause")
            // if Stopped, PLAY;  if Playing, Pause.  If Paused, Resume.
            on_playButton_clicked();
            break;

        case Qt::Key_Home:
        case Qt::Key_Period:  // for SqView compatibility ("restart")
            on_stopButton_clicked();
            on_playButton_clicked();
            on_warningLabel_clicked();  // also reset the Patter Timer to zero
            break;

        case Qt::Key_Right:
            on_actionSkip_Ahead_15_sec_triggered();
            break;
        case Qt::Key_Left:
            on_actionSkip_Back_15_sec_triggered();
            break;

        case Qt::Key_Backspace:  // either one will delete a row
        case Qt::Key_Delete:
            break;

        case Qt::Key_Down:
            ui->volumeSlider->setValue(ui->volumeSlider->value() - 5);
            break;
        case Qt::Key_Up:
            ui->volumeSlider->setValue(ui->volumeSlider->value() + 5);
            break;

        case Qt::Key_Plus:
        case Qt::Key_Equal:
            actionTempoPlus();
            break;
        case Qt::Key_Minus:
            actionTempoMinus();
            break;

        case Qt::Key_K:
            on_actionNext_Playlist_Item_triggered();  // compatible with SqView!
            break;

        case Qt::Key_M:
            on_actionMute_triggered();
            break;

        case Qt::Key_U:
            on_actionPitch_Up_triggered();
            break;

        case Qt::Key_D:
            on_actionPitch_Down_triggered();
            break;

        case Qt::Key_Y:
            actionFadeOutAndPause();
            break;

        case Qt::Key_L:
            on_loopButton_toggled(!ui->actionLoop->isChecked());  // toggle it
            break;

        case Qt::Key_T:
            actionNextTab();
            break;
#endif // #if 0 - temporarily leaving in 'til we agree on hotkey editing

        case Qt::Key_PageDown:
            // only move the scrolled Lyrics area, if the Lyrics tab is currently showing, and lyrics are loaded
            //   or if Patter is currently showing and patter is loaded
            tabTitle = ui->tabWidget->tabText(ui->tabWidget->currentIndex());
            if (tabTitle.endsWith("*Lyrics") || tabTitle.endsWith("*Patter")) {
                ui->textBrowserCueSheet->verticalScrollBar()->setValue(ui->textBrowserCueSheet->verticalScrollBar()->value() + 200);
            }
            break;

        case Qt::Key_PageUp:
            // only move the scrolled Lyrics area, if the Lyrics tab is currently showing, and lyrics are loaded
            //   or if Patter is currently showing and patter is loaded
            tabTitle = ui->tabWidget->tabText(ui->tabWidget->currentIndex());
            if (tabTitle.endsWith("*Lyrics") || tabTitle.endsWith("*Patter")) {
                ui->textBrowserCueSheet->verticalScrollBar()->setValue(ui->textBrowserCueSheet->verticalScrollBar()->value() - 200);
            }
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
//            qDebug() << "Key RETURN/ENTER detected.";
            if (ui->typeSearch->hasFocus() || ui->labelSearch->hasFocus() || ui->titleSearch->hasFocus()) {
//                qDebug() << "   and search has focus.";

                // figure out which row is currently selected
                QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
                QModelIndexList selected = selectionModel->selectedRows();
                int row = -1;
                if (selected.count() == 1) {
                    // exactly 1 row was selected (good)
                    QModelIndex index = selected.at(0);
                    row = index.row();
                }
                else {
                    // more than 1 row or no rows at all selected (BAD)
                    return true;
                }

                on_songTable_itemDoubleClicked(ui->songTable->item(row,1));
                ui->typeSearch->clearFocus();
                ui->labelSearch->clearFocus();
                ui->titleSearch->clearFocus();
            }
            break;

        case Qt::Key_Down:
        case Qt::Key_Up:
//            qDebug() << "Key up/down detected.";
            if (ui->typeSearch->hasFocus() || ui->labelSearch->hasFocus() || ui->titleSearch->hasFocus()) {
//                qDebug() << "   and search has focus.";
                if (key == Qt::Key_Up) {
                    // TODO: this same code appears FOUR times.  FACTOR IT
                    // on_actionPrevious_Playlist_Item_triggered();
                    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
                    QModelIndexList selected = selectionModel->selectedRows();
                    int row = -1;
                    if (selected.count() == 1) {
                        // exactly 1 row was selected (good)
                        QModelIndex index = selected.at(0);
                        row = index.row();
                    }
                    else {
                        // more than 1 row or no rows at all selected (BAD)
                        return true;
                    }

                    // which is the next VISIBLE row?
                    int lastVisibleRow = row;
                    row = (row-1 < 0 ? 0 : row-1); // bump backwards by 1

                    while (ui->songTable->isRowHidden(row) && row > 0) {
                        // keep bumping backwards, until the previous VISIBLE row is found, or we're at the BEGINNING
                        row = (row-1 < 0 ? 0 : row-1); // bump backwards by 1
                    }
                    if (ui->songTable->isRowHidden(row)) {
                        // if we try to go past the beginning of the VISIBLE rows, stick at the first visible row (which
                        //   was the last one we were on.  Well, that's not always true, but this is a quick and dirty
                        //   solution.  If I go to a row, select it, and then filter all rows out, and hit one of the >>| buttons,
                        //   hilarity will ensue.
                        row = lastVisibleRow;
                    }

                    ui->songTable->selectRow(row); // select new row!

                } else {
                    // TODO: this same code appears FOUR times.  FACTOR IT
                    // on_actionNext_Playlist_Item_triggered();
                    // figure out which row is currently selected
                    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
                    QModelIndexList selected = selectionModel->selectedRows();
                    int row = -1;
                    if (selected.count() == 1) {
                        // exactly 1 row was selected (good)
                        QModelIndex index = selected.at(0);
                        row = index.row();
                    }
                    else {
                        // more than 1 row or no rows at all selected (BAD)
                        return true;
                    }

                    int maxRow = ui->songTable->rowCount() - 1;

                    // which is the next VISIBLE row?
                    int lastVisibleRow = row;
                    row = (maxRow < row+1 ? maxRow : row+1); // bump up by 1
                    while (ui->songTable->isRowHidden(row) && row < maxRow) {
                        // keep bumping, until the next VISIBLE row is found, or we're at the END
                        row = (maxRow < row+1 ? maxRow : row+1); // bump up by 1
                    }
                    if (ui->songTable->isRowHidden(row)) {
                        // if we try to go past the end of the VISIBLE rows, stick at the last visible row (which
                        //   was the last one we were on.  Well, that's not always true, but this is a quick and dirty
                        //   solution.  If I go to a row, select it, and then filter all rows out, and hit one of the >>| buttons,
                        //   hilarity will ensue.
                        row = lastVisibleRow;
                    }
                    ui->songTable->selectRow(row); // select new row!

                }
            }
            break;

        default:
            auto keyMapping = hotkeyMappings.find((Qt::Key)(key));
            if (keyMapping != hotkeyMappings.end())
            {
                keyMapping.value()->doAction(this);
            }
//            qDebug() << "unhandled key:" << key;
            break;
    }

    Info_Seekbar(true);
    return true;
}

// ------------------------------------------------------------------------
void MainWindow::on_actionSpeed_Up_triggered()
{
    ui->tempoSlider->setValue(ui->tempoSlider->value() + 1);
    on_tempoSlider_valueChanged(ui->tempoSlider->value());
}

void MainWindow::on_actionSlow_Down_triggered()
{
    ui->tempoSlider->setValue(ui->tempoSlider->value() - 1);
    on_tempoSlider_valueChanged(ui->tempoSlider->value());
}

// ------------------------------------------------------------------------
void MainWindow::on_actionSkip_Ahead_15_sec_triggered()
{
    cBass.StreamGetPosition();  // update the position
    // set the position to one second before the end, so that RIGHT ARROW works as expected
    cBass.StreamSetPosition((int)fmin(cBass.Current_Position + 15.0, cBass.FileLength-1.0));
    Info_Seekbar(true);
}

void MainWindow::on_actionSkip_Back_15_sec_triggered()
{
    Info_Seekbar(true);
    cBass.StreamGetPosition();  // update the position
    cBass.StreamSetPosition((int)fmax(cBass.Current_Position - 15.0, 0.0));
}

// ------------------------------------------------------------------------
void MainWindow::on_actionVolume_Up_triggered()
{
    ui->volumeSlider->setValue(ui->volumeSlider->value() + 5);
}

void MainWindow::on_actionVolume_Down_triggered()
{
    ui->volumeSlider->setValue(ui->volumeSlider->value() - 5);
}

// ------------------------------------------------------------------------
void MainWindow::on_actionPlay_triggered()
{
    on_playButton_clicked();
}

void MainWindow::on_actionStop_triggered()
{
    on_stopButton_clicked();
}

// ------------------------------------------------------------------------
void MainWindow::on_actionForce_Mono_Aahz_mode_triggered()
{
    on_monoButton_toggled(ui->actionForce_Mono_Aahz_mode->isChecked());
}


// ------------------------------------------------------------------------
void MainWindow::on_bassSlider_valueChanged(int value)
{
    cBass.SetEq(0, (float)value);
}

void MainWindow::on_midrangeSlider_valueChanged(int value)
{
    cBass.SetEq(1, (float)value);
}

void MainWindow::on_trebleSlider_valueChanged(int value)
{
    cBass.SetEq(2, (float)value);
}


struct FilenameMatchers {
    QRegularExpression regex;
    int title_match;
    int label_match;
    int number_match;
    int additional_label_match;
    int additional_title_match;
};

struct FilenameMatchers *getFilenameMatchersForType(enum SongFilenameMatchingType songFilenameFormat)
{
    static struct FilenameMatchers best_guess_matches[] = {
        { QRegularExpression("^(.*) - ([A-Z]+[\\- ]\\d+)( *-?[VMA-C]|\\-\\d+)?$"), 1, 2, -1, 3, -1 },
        { QRegularExpression("^([A-Z]+[\\- ]\\d+)(-?[VvMA-C]?) - (.*)$"), 3, 1, -1, 2, -1 },
        { QRegularExpression("^([A-Z]+ ?\\d+)([MV]?)[ -]+(.*)$/"), 3, 1, -1, 2, -1 },
        { QRegularExpression("^([A-Z]?[0-9][A-Z]+[\\- ]?\\d+)([MV]?)[ -]+(.*)$"), 3, 1, -1, 2, -1 },
        { QRegularExpression("^(.*) - ([A-Z]{1,5}+)[\\- ](\\d+)( .*)?$"), 1, 2, 3, -1, 4 },
        { QRegularExpression("^([A-Z]+ ?\\d+)([ab])?[ -]+(.*)$/"), 3, 1, -1, 2, -1 },
        { QRegularExpression("^([A-Z]+\\-\\d+)\\-(.*)/"), 2, 1, -1, -1, -1 },
//    { QRegularExpression("^(\\d+) - (.*)$"), 2, -1, -1, -1, -1 },         // first -1 prematurely ended the search (typo?)
//    { QRegularExpression("^(\\d+\\.)(.*)$"), 2, -1, -1, -1, -1 },         // first -1 prematurely ended the search (typo?)
        { QRegularExpression("^(\\d+)\\s*-\\s*(.*)$"), 2, 1, -1, -1, -1 },  // e.g. "123 - Chicken Plucker"
        { QRegularExpression("^(\\d+\\.)(.*)$"), 2, 1, -1, -1, -1 },            // e.g. "123.Chicken Plucker"
//        { QRegularExpression("^(.*?) - (.*)$"), 2, 1, -1, -1, -1 },           // ? is a non-greedy match (So that "A - B - C", first group only matches "A")
        { QRegularExpression("^([A-Z]{1,5}+[\\- ]*\\d+[A-Z]*)\\s*-\\s*(.*)$"), 2, 1, -1, -1, -1 }, // e.g. "ABC 123-Chicken Plucker"
        { QRegularExpression("^([A-Z0-9]{1,5}+)\\s*(\\d+)([a-zA-Z]{1,2})?\\s*-\\s*(.*?)\\s*(\\(.*\\))?$"), 4, 1, 2, 3, 5 }, // SIR 705b - Papa Was A Rollin Stone (Instrumental).mp3
        { QRegularExpression("^([A-Z0-9]{1,5}+)\\s*-\\s*(.*)$"), 2, 1, -1, -1, -1 },    // e.g. "POP - Chicken Plucker" (if it has a dash but fails all other tests,
        { QRegularExpression("^(.*?)\\s*\\-\\s*([A-Z]{1,5})(\\d{1,5})\\s*(\\(.*\\))?$"), 1, 2, 3, -1, 4 },    // e.g. "A Summer Song - CHIC3002 (female vocals)
        { QRegularExpression("^(.*?)\\s*\\-\\s*([A-Za-z]{1,7})-(\\d{1,5})(\\-?([AB]))?$"), 1, 2, 3, 5, -1 },    // e.g. "Paper Doll - Windsor-4936B"

        { QRegularExpression(), -1, -1, -1, -1, -1 }
    };
    static struct FilenameMatchers label_first_matches[] = {
        { QRegularExpression("^(.*)\\s*-\\s*(.*)$"), 2, 1, -1, -1, -1 },    // e.g. "ABC123X - Chicken Plucker"
        { QRegularExpression(), -1, -1, -1, -1, -1 }
    };
    static struct FilenameMatchers filename_first_matches[] = {
        { QRegularExpression("^(.*)\\s*-\\s*(.*)$"), 1, 2, -1, -1, -1 },    // e.g. "Chicken Plucker - ABC123X"
        { QRegularExpression(), -1, -1, -1, -1, -1 }
    };

    switch (songFilenameFormat) {
        default:
        case SongFilenameLabelDashName :
            return label_first_matches;
        case SongFilenameNameDashLabel :
            return filename_first_matches;
        case SongFilenameBestGuess :
            return best_guess_matches;
    }
}


bool MainWindow::breakFilenameIntoParts(const QString &s,
                                        QString &label, QString &labelnum,
                                        QString &labelnum_extra,
                                        QString &title, QString &shortTitle )
{
    bool foundParts = true;
    int match_num = 0;
    struct FilenameMatchers *matches = getFilenameMatchersForType(songFilenameFormat);

    for (match_num = 0;
         matches[match_num].label_match >= 0
             && matches[match_num].title_match >= 0;
         ++match_num) {
        QRegularExpressionMatch match = matches[match_num].regex.match(s);
        if (match.hasMatch()) {
            if (matches[match_num].label_match >= 0) {
                label = match.captured(matches[match_num].label_match);
            }
            if (matches[match_num].title_match >= 0) {
                title = match.captured(matches[match_num].title_match);
                shortTitle = title;
            }
            if (matches[match_num].number_match >= 0) {
                labelnum = match.captured(matches[match_num].number_match);
            }
            if (matches[match_num].additional_label_match >= 0) {
                labelnum_extra = match.captured(matches[match_num].additional_label_match);
            }
            if (matches[match_num].additional_title_match >= 0
                && !match.captured(matches[match_num].additional_title_match).isEmpty()) {
                title += " " + match.captured(matches[match_num].additional_title_match);
            }
            break;
        } else {
//                qDebug() << s << "didn't match" << matches[match_num].regex;
        }
    }
    if (!(matches[match_num].label_match >= 0
          && matches[match_num].title_match >= 0)) {
        label = "";
        title = s;
        foundParts = false;
    }

    label = label.simplified();

    if (labelnum.length() == 0)
    {
        static QRegularExpression regexLabelPlusNum = QRegularExpression("^(\\w+)[\\- ](\\d+)(\\w?)$");
        QRegularExpressionMatch match = regexLabelPlusNum.match(label);
        if (match.hasMatch())
        {
            label = match.captured(1);
            labelnum = match.captured(2);
            if (labelnum_extra.length() == 0)
            {
                labelnum_extra = match.captured(3);
            }
            else
            {
                labelnum = labelnum + match.captured(3);
            }
        }
    }
    labelnum = labelnum.simplified();
    title = title.simplified();
    shortTitle = shortTitle.simplified();

    return foundParts;
}

class CuesheetWithRanking {
public:
    QString filename;
    QString name;
    int score;
};

static bool CompareCuesheetWithRanking(CuesheetWithRanking *a, CuesheetWithRanking *b)
{
    if (a->score == b->score) {
        return a->name < b->name;  // if scores are equal, sort names lexicographically (note: should be QCollator numericMode() for natural sort order)
    }
    // else:
    return a->score > b->score;
}

// -----------------------------------------------------------------
QStringList splitIntoWords(const QString &str)
{
    static QRegExp regexNotAlnum(QRegExp("\\W+"));

    QStringList words = str.split(regexNotAlnum);

    static QRegularExpression LetterNumber("[A-Z][0-9]|[0-9][A-Z]"); // do we need to split?  Most of the time, no.
    QRegularExpressionMatch quickmatch(LetterNumber.match(str));

    if (quickmatch.hasMatch()) {
        static QRegularExpression regexLettersAndNumbers("^([A-Z]+)([0-9].*)$");
        static QRegularExpression regexNumbersAndLetters("^([0-9]+)([A-Z].*)$");
//        qDebug() << "quickmatch!";
        // we gotta split it one word at a time
//        words = str.split(regexNotAlnum);
        for (int i = 0; i < words.length(); ++i)
        {
            bool splitFurther = true;

            while (splitFurther)
            {
                splitFurther = false;
                QRegularExpressionMatch match(regexLettersAndNumbers.match(words[i]));
                if (match.hasMatch())
                {
                    words.append(match.captured(1));
                    words[i] = match.captured(2);
                    splitFurther = true;
                }
                match = regexNumbersAndLetters.match(words[i]);
                if (match.hasMatch())
                {
                    splitFurther = true;
                    words.append(match.captured(1));
                    words[i] = match.captured(2);
                }
            }
        }
    }
    // else no splitting needed (e.g. it's already split, as is the case for most cuesheets)
    //   so we skip the per-word splitting, and go right to sorting
    words.sort(Qt::CaseInsensitive);
    return words;
}

int compareSortedWordListsForRelevance(const QStringList &l1, const QStringList l2)
{
    int i1 = 0, i2 = 0;
    int score = 0;

    while (i1 < l1.length() &&  i2 < l2.length())
    {
        int comp = l1[i1].compare(l2[i2], Qt::CaseInsensitive);
        if (comp == 0)
        {
            ++score;
            ++i1;
            ++i2;
        }
        else if (comp < 0)
        {
            ++i1;
        }
        else
        {
            ++i2;
        }
    }

//    qDebug() << "Score" << score << " / " << l1.length() << "/" << l2.length() << " s1: " << l1.join("-") << " s2: " << l2.join("-");

    if (l1.length() >= 2 && l2.length() >= 2 &&
        (
            (score > ((l1.length() + l2.length()) / 4))
            || (score >= l1.length())                       // all of l1 words matched something in l2
            || (score >= l2.length())                       // all of l2 words matched something in l1
            )
        )
    {
        QString s1 = l1.join("-");
        QString s2 = l2.join("-");
        return score * 500 - 100 * (abs(l1.length()) - l2.length());
    }
    else
        return 0;
}

// TODO: the match needs to be a little fuzzier, since RR103B - Rocky Top.mp3 needs to match RR103 - Rocky Top.html
void MainWindow::findPossibleCuesheets(const QString &MP3Filename, QStringList &possibleCuesheets)
{
    QString fileType = filepath2SongType(MP3Filename);
    bool fileTypeIsPatter = (fileType == "patter");

    QFileInfo mp3FileInfo(MP3Filename);
    QString mp3CanonicalPath = mp3FileInfo.canonicalPath();
    QString mp3CompleteBaseName = mp3FileInfo.completeBaseName();
    QString mp3Label = "";
    QString mp3Labelnum = "";
    QString mp3Labelnum_short = "";
    QString mp3Labelnum_extra = "";
    QString mp3Title = "";
    QString mp3ShortTitle = "";
    breakFilenameIntoParts(mp3CompleteBaseName, mp3Label, mp3Labelnum, mp3Labelnum_extra, mp3Title, mp3ShortTitle);
    QList<CuesheetWithRanking *> possibleRankings;

    QStringList mp3Words = splitIntoWords(mp3CompleteBaseName);
    mp3Labelnum_short = mp3Labelnum;
    while (mp3Labelnum_short.length() > 0 && mp3Labelnum_short[0] == '0')
    {
        mp3Labelnum_short.remove(0,1);
    }

//    qDebug() << "load 2.1.1.0A: " << t2.elapsed() << "ms";

//    QList<QString> extensions;
//    QString dot(".");
//    for (size_t i = 0; i < sizeof(cuesheet_file_extensions) / sizeof(*cuesheet_file_extensions); ++i)
//    {
//        extensions.append(dot + cuesheet_file_extensions[i]);
//    }

    QListIterator<QString> iter(*pathStack);
    while (iter.hasNext()) {

        QString s = iter.next();
//        qDebug() << "load 2.1.1.0A.1: " << t2.nsecsElapsed() << "ms, s:" << s;
//        int extensionIndex = 0;

//        // Is this a file extension we recognize as a cuesheet file?
//        QListIterator<QString> extensionIterator(extensions);
//        bool foundExtension = false;
//        while (extensionIterator.hasNext())
//        {
//            extensionIndex++;
//            QString extension(extensionIterator.next());
//            if (s.endsWith(extension))
//            {
//                foundExtension = true;
//                break;
//            }
//        }

//        bool foundExtension0 = s.endsWith("htm");
//        bool foundExtension1 = s.endsWith("html");
//        bool foundExtension2 = s.endsWith("txt");

        int extensionIndex = 0;

        if (s.endsWith("htm", Qt::CaseInsensitive)) {
            // nothing
        } else if (s.endsWith("html", Qt::CaseInsensitive)) {
            extensionIndex = 1;
        } else if (s.endsWith("txt", Qt::CaseInsensitive)) {
            extensionIndex = 2;
        } else {
            continue;
        }

//        qDebug() << "load 2.1.1.0A.2: " << t2.nsecsElapsed() << "ms";

        QStringList sl1 = s.split("#!#");
        QString type = sl1[0];  // the type (of original pathname, before following aliases)
        QString filename = sl1[1];  // everything else

//        qDebug() << "possibleCuesheets(): " << fileTypeIsPatter << filename << filepath2SongType(filename) << type;
        if (fileTypeIsPatter && (type=="lyrics")) {
            // if it's a patter MP3, then do NOT match it against anything in the lyrics folder
            continue;
        }

//        if (type=="choreography" || type == "sd" || type=="reference") {
//            // if it's a dance program .txt file, or an sd sequence file, or a reference .txt file, don't bother trying to match
//            continue;
//        }

        QFileInfo fi(filename);
//        QString fi2 = fi.canonicalPath();

//        if (fi2 == musicRootPath && type.right(1) != "*") {
//            // e.g. "/Users/mpogue/__squareDanceMusic/C 117 - Bad Puppy (Patter).mp3" --> NO TYPE PRESENT and NOT a guest song
//            type = "";
//        }

//        qDebug() << "load 2.1.1.0A.3: " << t2.nsecsElapsed() << "ms";

        QString label = "";
        QString labelnum = "";
        QString title = "";
        QString labelnum_extra;
        QString shortTitle = "";


        QString completeBaseName = fi.completeBaseName(); // e.g. "/Users/mpogue/__squareDanceMusic/patter/RIV 307 - Going to Ceili (Patter).mp3" --> "RIV 307 - Going to Ceili (Patter)"
//        qDebug() << "   load 2.1.1.0A.3a: " << t2.nsecsElapsed() << "ms";
        breakFilenameIntoParts(completeBaseName, label, labelnum, labelnum_extra, title, shortTitle);
//        qDebug() << "   load 2.1.1.0A.3b: " << t2.nsecsElapsed() << "ms";
        QStringList words = splitIntoWords(completeBaseName);
//        qDebug() << "   load 2.1.1.0A.3c: " << t2.nsecsElapsed() << "ms, words:" << words;
        QString labelnum_short = labelnum;
        while (labelnum_short.length() > 0 && labelnum_short[0] == '0')
        {
            labelnum_short.remove(0,1);
        }

//        qDebug() << "load 2.1.1.0A.4: " << t2.nsecsElapsed() << "ms";

//        qDebug() << "Comparing: " << completeBaseName << " to " << mp3CompleteBaseName;
//        qDebug() << "           " << title << " to " << mp3Title;
//        qDebug() << "           " << shortTitle << " to " << mp3ShortTitle;
//        qDebug() << "    label: " << label << " to " << mp3Label <<
//            " and num " << labelnum << " to " << mp3Labelnum <<
//            " short num " << labelnum_short << " to " << mp3Labelnum_short;
//        qDebug() << "    title: " << mp3Title << " to " << QString(label + "-" + labelnum);
        int score = 0;
        // Minimum criteria:
        if (completeBaseName.compare(mp3CompleteBaseName, Qt::CaseInsensitive) == 0
            || title.compare(mp3Title, Qt::CaseInsensitive) == 0
            || (shortTitle.length() > 0
                && shortTitle.compare(mp3ShortTitle, Qt::CaseInsensitive) == 0)
            || (labelnum_short.length() > 0 && label.length() > 0
                &&  labelnum_short.compare(mp3Labelnum_short, Qt::CaseInsensitive) == 0
                && label.compare(mp3Label, Qt::CaseInsensitive) == 0
                )
            || (labelnum.length() > 0 && label.length() > 0
                && mp3Title.length() > 0
                && mp3Title.compare(label + "-" + labelnum, Qt::CaseInsensitive) == 0)
            )
        {

            score = extensionIndex
                + (mp3CanonicalPath.compare(fi.canonicalPath(), Qt::CaseInsensitive) == 0 ? 10000 : 0)
                + (mp3CompleteBaseName.compare(fi.completeBaseName(), Qt::CaseInsensitive) == 0 ? 1000 : 0)
                + (title.compare(mp3Title, Qt::CaseInsensitive) == 0 ? 100 : 0)
                + (shortTitle.compare(mp3ShortTitle, Qt::CaseInsensitive) == 0 ? 50 : 0)
                + (labelnum.compare(mp3Labelnum, Qt::CaseInsensitive) == 0 ? 10 : 0)
                + (labelnum_short.compare(mp3Labelnum_short, Qt::CaseInsensitive) == 0 ? 7 : 0)
                + (mp3Label.compare(mp3Label, Qt::CaseInsensitive) == 0 ? 5 : 0);

            CuesheetWithRanking *cswr = new CuesheetWithRanking();
            cswr->filename = filename;
            cswr->name = completeBaseName;
            cswr->score = score;
            possibleRankings.append(cswr);
//            qDebug() << "load 2.1.1.0A.5a: " << t2.elapsed() << "ms";
        } /* end of if we minimally included this cuesheet */
        else if ((score = compareSortedWordListsForRelevance(mp3Words, words)) > 0)
        {
            CuesheetWithRanking *cswr = new CuesheetWithRanking();
            cswr->filename = filename;
            cswr->name = completeBaseName;
            cswr->score = score;
            possibleRankings.append(cswr);
//            qDebug() << "load 2.1.1.0A.5b: " << t2.elapsed() << "ms";
        }
    } /* end of looping through all files we know about */

//    qDebug() << "load 2.1.1.0B: " << t2.elapsed() << "ms";

//    qDebug() << "findPossibleCuesheets():";
    QString mp3Lyrics = loadLyrics(MP3Filename);
//    qDebug() << "mp3Lyrics:" << mp3Lyrics;
    if (mp3Lyrics.length())
    {
        possibleCuesheets.append(MP3Filename);
    }

//    qDebug() << "load 2.1.1.0C: " << t2.elapsed() << "ms";

    qSort(possibleRankings.begin(), possibleRankings.end(), CompareCuesheetWithRanking);
    QListIterator<CuesheetWithRanking *> iterRanked(possibleRankings);
    while (iterRanked.hasNext())
    {
        CuesheetWithRanking *cswr = iterRanked.next();
        possibleCuesheets.append(cswr->filename);
        delete cswr;
    }

//    qDebug() << "load 2.1.1.0D: " << t2.elapsed() << "ms";
}

void MainWindow::loadCuesheets(const QString &MP3FileName, const QString preferredCuesheet)
{
    hasLyrics = false;

    QString HTML;

    QStringList possibleCuesheets;
//    qDebug() << "load 2.1.1.0: " << t2.elapsed() << "ms";

    findPossibleCuesheets(MP3FileName, possibleCuesheets);


//    qDebug() << "possibleCuesheets:" << possibleCuesheets;

    int defaultCuesheetIndex = 0;
    loadedCuesheetNameWithPath = ""; // nothing loaded yet

    QString firstCuesheet(preferredCuesheet);
    ui->comboBoxCuesheetSelector->clear();

    foreach (const QString &cuesheet, possibleCuesheets)
    {
        RecursionGuard guard(cuesheetEditorReactingToCursorMovement);
        if ((!preferredCuesheet.isNull()) && preferredCuesheet.length() >= 0
            && cuesheet == preferredCuesheet)
        {
            defaultCuesheetIndex = ui->comboBoxCuesheetSelector->count();
        }

        QString displayName = cuesheet;
        if (displayName.startsWith(musicRootPath))
            displayName.remove(0, musicRootPath.length());

        ui->comboBoxCuesheetSelector->addItem(displayName,
                                              cuesheet);
    }

//    qDebug() << "load 2.1.2: " << t2.elapsed() << "ms";

    if (ui->comboBoxCuesheetSelector->count() > 0)
    {
        ui->comboBoxCuesheetSelector->setCurrentIndex(defaultCuesheetIndex);
        // if it was zero, we didn't load it because the index didn't change,
        // and we skipped loading it above. Sooo...
        if (0 == defaultCuesheetIndex)
            on_comboBoxCuesheetSelector_currentIndexChanged(0);
        hasLyrics = true;
    }

    // be careful here.  The Lyrics tab can now be the Patter tab.
    bool isPatter = songTypeNamesForPatter.contains(currentSongType);

//    qDebug() << "loadCuesheets: " << currentSongType << isPatter;

    if (isPatter) {
        // ----- PATTER -----
        ui->menuLyrics->setTitle("Patter");
//        ui->actionFilePrint->setText("Print Patter...");
//        ui->actionSave_Lyrics->setText("Save Patter");
//        ui->actionSave_Lyrics_As->setText("Save Patter As...");
        ui->actionAuto_scroll_during_playback->setText("Auto-scroll Patter");

        if (hasLyrics && lyricsTabNumber != -1) {
//            qDebug() << "loadCuesheets 2: " << "setting to *";
            ui->tabWidget->setTabText(lyricsTabNumber, "*Patter");
        } else {
//            qDebug() << "loadCuesheets 2: " << "setting to NOT *";
            ui->tabWidget->setTabText(lyricsTabNumber, "Patter");

            // ------------------------------------------------------------------
            // get pre-made patter.template.html file, if it exists
            QString patterTemplate = getResourceFile("patter.template.html");
//            qDebug() << "patterTemplate: " << patterTemplate;
            if (patterTemplate.isEmpty()) {
                ui->textBrowserCueSheet->setHtml("No patter found for this song.");
                loadedCuesheetNameWithPath = "";
            } else {
                ui->textBrowserCueSheet->setHtml(patterTemplate);
                loadedCuesheetNameWithPath = "patter.template.html";  // as a special case, this is allowed to not be the full path
            }

        } // else (sequence could not be found)
    } else {
        // ----- SINGING CALL -----
        ui->menuLyrics->setTitle("Lyrics");
        ui->actionAuto_scroll_during_playback->setText("Auto-scroll Cuesheet");

        if (hasLyrics && lyricsTabNumber != -1) {
            ui->tabWidget->setTabText(lyricsTabNumber, "*Lyrics");
        } else {
            ui->tabWidget->setTabText(lyricsTabNumber, "Lyrics");

            // ------------------------------------------------------------------
            // get pre-made lyrics.template.html file, if it exists
            QString lyricsTemplate = getResourceFile("lyrics.template.html");
            qDebug() << "lyricsTemplate: " << lyricsTemplate;
            if (lyricsTemplate.isEmpty()) {
                ui->textBrowserCueSheet->setHtml("No lyrics found for this song.");
                loadedCuesheetNameWithPath = "";
            } else {
                ui->textBrowserCueSheet->setHtml(lyricsTemplate);
                loadedCuesheetNameWithPath = "lyrics.template.html";  // as a special case, this is allowed to not be the full path
            }

        } // else (lyrics could not be found)
    } // isPatter

//    qDebug() << "load 2.1.3: " << t2.elapsed() << "ms";
}


float MainWindow::getID3BPM(QString MP3FileName) {
    MPEG::File *mp3file;
    ID3v2::Tag *id3v2tag;  // NULL if it doesn't have a tag, otherwise the address of the tag

    mp3file = new MPEG::File(MP3FileName.toStdString().c_str()); // FIX: this leaks on read of another file
    id3v2tag = mp3file->ID3v2Tag(true);  // if it doesn't have one, create one

    float theBPM = 0.0;

    ID3v2::FrameList::ConstIterator it = id3v2tag->frameList().begin();
    for (; it != id3v2tag->frameList().end(); it++)
    {
        if ((*it)->frameID() == "TBPM")  // This is an Apple standard, which means it's everybody's standard now.
        {
            QString BPM((*it)->toString().toCString());
            theBPM = BPM.toFloat();
        }

    }

    return(theBPM);
}

void MainWindow::reloadCurrentMP3File() {
    // if there is a song loaded, reload it (to pick up, e.g. new cuesheets)
    if ((currentMP3filenameWithPath != "")&&(currentSongTitle != "")&&(currentSongType != "")) {
//        qDebug() << "reloading song: " << currentMP3filename;
        loadMP3File(currentMP3filenameWithPath, currentSongTitle, currentSongType);
    }
}

void MainWindow::loadMP3File(QString MP3FileName, QString songTitle, QString songType)
{
    RecursionGuard recursion_guard(loadingSong);
    firstTimeSongIsPlayed = true;

    currentMP3filenameWithPath = MP3FileName;

    // resolve aliases at load time, rather than findFilesRecursively time, because it's MUCH faster
    QFileInfo fi(MP3FileName);
    QString resolvedFilePath = fi.symLinkTarget(); // path with the symbolic links followed/removed
    if (resolvedFilePath != "") {
        MP3FileName = resolvedFilePath;
    }

    currentSongType = songType;  // save it for session coloring on the analog clock later...

    ui->toolButtonEditLyrics->setChecked(false); // lyrics/cuesheets of new songs when loaded default to NOT editable

//    qDebug() << "load 2.1: " << t2.elapsed() << "ms";

    loadCuesheets(MP3FileName);

//    qDebug() << "load 2.2: " << t2.elapsed() << "ms";

    QStringList pieces = MP3FileName.split( "/" );
    QString filebase = pieces.value(pieces.length()-1);
    QStringList pieces2 = filebase.split(".");

    currentMP3filename = pieces2.value(pieces2.length()-2);


    if (songTitle != "") {
        ui->nowPlayingLabel->setText(songTitle);
    }
    else {
        ui->nowPlayingLabel->setText(currentMP3filename);  // FIX?  convert to short version?
    }
    currentSongTitle = ui->nowPlayingLabel->text();  // save, in case we are Flash Calling

    QDir md(MP3FileName);
    QString canonicalFN = md.canonicalPath();

//    qDebug() << "load 2.3.1: " << t2.elapsed() << "ms";

    // let's do a quick preview (takes <1ms), to see if the intro/outro are already set.
    SongSetting settings1;
    double intro1 = 0.0;
    double outro1 = 0.0;
    if (songSettings.loadSettings(currentMP3filenameWithPath, settings1)) {
        if (settings1.isSetIntroPos()) {
            intro1 = settings1.getIntroPos();
//            qDebug() << "intro was set to: " << intro1;
        }
        if (settings1.isSetOutroPos()) {
            outro1 = settings1.getOutroPos();
//            qDebug() << "outro was set to: " << outro1;
        }
    }

//    qDebug() << "load 2.3.1.2: " << t2.elapsed() << "ms";

    cBass.StreamCreate(MP3FileName.toStdString().c_str(), &startOfSong_sec, &endOfSong_sec, intro1, outro1);  // load song, and figure out where the song actually starts and ends

    // OK, by this time we always have an introOutro
    //   if DB had one, we didn't scan, and just used that one
    //   if DB did not have one, we scanned

    //    qDebug() << "song starts: " << startOfSong_sec << ", ends: " << endOfSong_sec;

#ifdef REMOVESILENCE
        startOfSong_sec = (startOfSong_sec > 1.0 ? startOfSong_sec - 1.0 : 0.0);  // start 1 sec before non-silence
#endif

//    qDebug() << "load 2.3.2: " << t2.elapsed() << "ms";

    QStringList ss = MP3FileName.split('/');
    QString fn = ss.at(ss.size()-1);
    this->setWindowTitle(fn + QString(" - SquareDesk MP3 Player/Editor"));

    int length_sec = cBass.FileLength;
    int songBPM = round(cBass.Stream_BPM);  // libbass's idea of the BPM

    // If the MP3 file has an embedded TBPM frame in the ID3 tag, then it overrides the libbass auto-detect of BPM
    float songBPM_ID3 = getID3BPM(MP3FileName);  // returns 0.0, if not found or not understandable

    if (songBPM_ID3 != 0.0) {
        songBPM = (int)songBPM_ID3;
    }

    baseBPM = songBPM;  // remember the base-level BPM of this song, for when the Tempo slider changes later

//    qDebug() << "load 2.4: " << t2.elapsed() << "ms";

    // Intentionally compare against a narrower range here than BPM detection, because BPM detection
    //   returns a number at the limits, when it's actually out of range.
    // Also, turn off BPM for xtras (they are all over the place, including round dance cues, which have no BPM at all).
    //
    // TODO: make the types for turning off BPM detection a preference
    if ((songBPM>=125-15) && (songBPM<=125+15) && songType != "xtras") {
        tempoIsBPM = true;
        ui->currentTempoLabel->setText(QString::number(songBPM) + " BPM (100%)"); // initial load always at 100%

        ui->tempoSlider->setMinimum(songBPM-15);
        ui->tempoSlider->setMaximum(songBPM+15);

        PreferencesManager prefsManager;
        bool tryToSetInitialBPM = prefsManager.GettryToSetInitialBPM();
        int initialBPM = prefsManager.GetinitialBPM();
        if (tryToSetInitialBPM) {
            // if the user wants us to try to hit a particular BPM target, use that value
            ui->tempoSlider->setValue(initialBPM);
            ui->tempoSlider->valueChanged(initialBPM);  // fixes bug where second song with same BPM doesn't update songtable::tempo
        } else {
            // otherwise, if the user wants us to start with the slider at the regular detected BPM
            //   NOTE: this can be overridden by the "saveSongPreferencesInConfig" preference, in which case
            //     all saved tempo preferences will always win.
            ui->tempoSlider->setValue(songBPM);
            ui->tempoSlider->valueChanged(songBPM);  // fixes bug where second song with same BPM doesn't update songtable::tempo
        }

        ui->tempoSlider->SetOrigin(songBPM);    // when double-clicked, goes here
        ui->tempoSlider->setEnabled(true);
        statusBar()->showMessage(QString("Song length: ") + position2String(length_sec) +
                                 ", base tempo: " + QString::number(songBPM) + " BPM");
    }
    else {
        tempoIsBPM = false;
        // if we can't figure out a BPM, then use percent as a fallback (centered: 100%, range: +/-20%)
        ui->currentTempoLabel->setText("100%");
        ui->tempoSlider->setMinimum(100-20);        // allow +/-20%
        ui->tempoSlider->setMaximum(100+20);
        ui->tempoSlider->setValue(100);
        ui->tempoSlider->valueChanged(100);  // fixes bug where second song with same 100% doesn't update songtable::tempo
        ui->tempoSlider->SetOrigin(100);  // when double-clicked, goes here
        ui->tempoSlider->setEnabled(true);
        statusBar()->showMessage(QString("Song length: ") + position2String(length_sec) +
                                 ", base tempo: 100%");
    }
//    qDebug() << "load 2.5: " << t2.elapsed() << "ms";

    fileModified = false;

    ui->playButton->setEnabled(true);
    ui->stopButton->setEnabled(true);

    ui->actionPlay->setEnabled(true);
    ui->actionStop->setEnabled(true);
    ui->actionSkip_Ahead_15_sec->setEnabled(true);
    ui->actionSkip_Back_15_sec->setEnabled(true);

    ui->seekBar->setEnabled(true);
    ui->seekBarCuesheet->setEnabled(true);

    // when we add Pitch to the songTable as a hidden column, we do NOT need to force pitch anymore, because it
    //   will be set by the loader to the correct value (which is zero, if the MP3 file wasn't on the current playlist).
//    ui->pitchSlider->valueChanged(ui->pitchSlider->value()); // force pitch change, if pitch slider preset before load
    ui->volumeSlider->valueChanged(ui->volumeSlider->value()); // force vol change, if vol slider preset before load
    ui->mixSlider->valueChanged(ui->mixSlider->value()); // force mix change, if mix slider preset before load

    ui->actionMute->setEnabled(true);
    ui->actionLoop->setEnabled(true);
    ui->actionTest_Loop->setEnabled(true);
    ui->actionIn_Out_Loop_points_to_default->setEnabled(true);

    ui->actionVolume_Down->setEnabled(true);
    ui->actionVolume_Up->setEnabled(true);
    ui->actionSpeed_Up->setEnabled(true);
    ui->actionSlow_Down->setEnabled(true);
    ui->actionForce_Mono_Aahz_mode->setEnabled(true);
    ui->actionPitch_Down->setEnabled(true);
    ui->actionPitch_Up->setEnabled(true);

    ui->bassSlider->valueChanged(ui->bassSlider->value()); // force bass change, if bass slider preset before load
    ui->midrangeSlider->valueChanged(
        ui->midrangeSlider->value()); // force midrange change, if midrange slider preset before load
    ui->trebleSlider->valueChanged(ui->trebleSlider->value()); // force treble change, if treble slider preset before load

    cBass.Stop();

//    qDebug() << "load 2.6: " << t2.elapsed() << "ms";

    songLoaded = true;
    Info_Seekbar(true);

//    qDebug() << "load 2.7: " << t2.elapsed() << "ms";

    bool isSingingCall = songTypeNamesForSinging.contains(songType) ||
                         songTypeNamesForCalled.contains(songType);

    bool isPatter = songTypeNamesForPatter.contains(songType);

    ui->dateTimeEditIntroTime->setTime(QTime(0,0,0,0));
    ui->dateTimeEditOutroTime->setTime(QTime(0,0,0,0));

    // NOTE: no need to scan for intro/outro here, because we are guaranteed that it was set by StreamCreate() above
    ui->seekBarCuesheet->SetDefaultIntroOutroPositions(tempoIsBPM, cBass.Stream_BPM, startOfSong_sec, endOfSong_sec, cBass.FileLength);
    ui->seekBar->SetDefaultIntroOutroPositions(tempoIsBPM, cBass.Stream_BPM, startOfSong_sec, endOfSong_sec, cBass.FileLength);

    ui->dateTimeEditIntroTime->setEnabled(true);
    ui->dateTimeEditOutroTime->setEnabled(true);

    ui->pushButtonSetIntroTime->setEnabled(true);  // always enabled now, because anything CAN be looped now OR it has an intro/outro
    ui->pushButtonSetOutroTime->setEnabled(true);
    ui->pushButtonTestLoop->setEnabled(true);

    ui->dateTimeEditIntroTime->setTimeRange(QTime(0,0,0,0), QTime(0,0,0,0).addMSecs((int)(1000.0*length_sec+0.5)));
    ui->dateTimeEditOutroTime->setTimeRange(QTime(0,0,0,0), QTime(0,0,0,0).addMSecs((int)(1000.0*length_sec+0.5)));

    ui->seekBar->SetSingingCall(isSingingCall); // if singing call, color the seek bar
    ui->seekBarCuesheet->SetSingingCall(isSingingCall); // if singing call, color the seek bar

    cBass.SetVolume(100);
    currentVolume = 100;
    previousVolume = 100;
    Info_Volume();

//    qDebug() << "load 2.8: " << t2.elapsed() << "ms";

    if (isPatter) {
        on_loopButton_toggled(true); // default is to loop, if type is patter
//        ui->tabWidget->setTabText(lyricsTabNumber, "Patter");  // Lyrics tab does double duty as Patter tab
        ui->pushButtonSetIntroTime->setText("Start Loop");
        ui->pushButtonSetOutroTime->setText("End Loop");
        ui->pushButtonTestLoop->setHidden(false);
    } else {
        // singing call or vocals or xtras, so Loop mode defaults to OFF
        on_loopButton_toggled(false); // default is to loop, if type is patter
//        ui->tabWidget->setTabText(lyricsTabNumber, "Lyrics");  // Lyrics tab is named "Lyrics"
        ui->pushButtonSetIntroTime->setText("In");
        ui->pushButtonSetOutroTime->setText("Out");
        ui->pushButtonTestLoop->setHidden(true);
    }

//    qDebug() << "load 2.9.1: " << t2.elapsed() << "ms";
    loadSettingsForSong(songTitle);

//    qDebug() << "load 2.9.2: " << t2.elapsed() << "ms";

//    qDebug() << "setting stream position to: " << startOfSong_sec;
    cBass.StreamSetPosition((double)startOfSong_sec);  // last thing we do is move the stream position to 1 sec before start of music
}

void MainWindow::on_actionOpen_MP3_file_triggered()
{
    on_stopButton_clicked();  // if we're loading a new MP3 file, stop current playback

    saveCurrentSongSettings();

    // http://stackoverflow.com/questions/3597900/qsettings-file-chooser-should-remember-the-last-directory
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings

    QString startingDirectory = prefsManager.Getdefault_dir();

    QString MP3FileName =
        QFileDialog::getOpenFileName(this,
                                     tr("Import Audio File"),
                                     startingDirectory,
                                     tr("Audio Files (*.mp3 *.m4a *.wav)"));
    if (MP3FileName.isNull()) {
        return;  // user cancelled...so don't do anything, just return
    }

    // not null, so save it in Settings (File Dialog will open in same dir next time)
    QDir CurrentDir;
    prefsManager.Setdefault_dir(CurrentDir.absoluteFilePath(MP3FileName));

    ui->songTable->clearSelection();  // if loaded via menu, then clear previous selection (if present)
    ui->nextSongButton->setEnabled(false);  // and, next/previous song buttons are disabled
    ui->previousSongButton->setEnabled(false);

    // --------
    loadMP3File(MP3FileName, QString(""), QString(""));  // "" means use title from the filename
}


// this function stores the absolute paths of each file in a QVector
void findFilesRecursively(QDir rootDir, QList<QString> *pathStack, QString suffix, Ui::MainWindow *ui, QString soundFXarray[6], QString soundFXname[6])
{
    QDirIterator it(rootDir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext()) {
        QString s1 = it.next();
        QString resolvedFilePath=s1;

        QFileInfo fi(s1);

        // Option A: first sub-folder is the type of the song.
        // Examples:
        //    <musicDir>/foo/*.mp3 is of type "foo"
        //    <musicDir>/foo/bar/music.mp3 is of type "foo"
        //    <musicDir>/foo/bar/ ... /baz/music.mp3 is of type "foo"
        //    <musicDir>/music.mp3 is of type ""

        QString newType = (fi.path().replace(rootDir.path() + "/","").split("/"))[0];
        QStringList section = fi.path().split("/");

//        QString type = section[section.length()-1] + suffix;  // must be the last item in the path
//                                                              // of where the alias is, not where the file is, and append "*" or not
//        qDebug() << "FFR: " << fi.path() << rootDir.path() << type << newType;
        if (section[section.length()-1] != "soundfx") {
//            qDebug() << "findFilesRecursively() adding " + type + "#!#" + resolvedFilePath + " to pathStack";
//            pathStack->append(type + "#!#" + resolvedFilePath);

            // add to the pathStack iff it's not a sound FX .mp3 file (those are internal)
            pathStack->append(newType + "#!#" + resolvedFilePath);
        } else {
            if (suffix != "*") {
                // if it IS a sound FX file (and not GUEST MODE), then let's squirrel away the paths so we can play them later
                QString path1 = resolvedFilePath;
                QString baseName = resolvedFilePath.replace(rootDir.absolutePath(),"").replace("/soundfx/","");
                QStringList sections = baseName.split(".");
                if (sections.length() == 3 && sections[0].toInt() != 0 && sections[2].compare("mp3",Qt::CaseInsensitive)==0) {
//                    if (sections.length() == 3 && sections[0].toInt() != 0 && sections[2] == "mp3") {
                    soundFXname[sections[0].toInt()-1] = sections[1];  // save for populating Preferences dropdown later
                    switch (sections[0].toInt()) {
                        case 1: ui->action_1->setText(sections[1]); break;
                        case 2: ui->action_2->setText(sections[1]); break;
                        case 3: ui->action_3->setText(sections[1]); break;
                        case 4: ui->action_4->setText(sections[1]); break;
                        case 5: ui->action_5->setText(sections[1]); break;
                        case 6: ui->action_6->setText(sections[1]); break;
                        default: break;
                    }
                soundFXarray[sections[0].toInt()-1] = path1;  // remember the path for playing it later
                } // if
            } // if
        } // else
    }
}

void MainWindow::checkLockFile() {
//    qDebug() << "checkLockFile()";

    PreferencesManager prefsManager;
    QString musicRootPath = prefsManager.GetmusicPath();

    QString databaseDir(musicRootPath + "/.squaredesk");

    QFileInfo checkFile(databaseDir + "/lock.txt");
    if (checkFile.exists()) {

        // get the hostname of who is using it
        QFile file(databaseDir + "/lock.txt");
        file.open(QIODevice::ReadOnly | QIODevice::Text);
        QTextStream lockfile(&file);
        QString hostname = lockfile.readLine();
        file.close();

        QString myHostName = QHostInfo::localHostName();

        if (hostname != myHostName) {
            // probably another instance of SquareDesk somewhere
            QMessageBox msgBox(QMessageBox::Warning,
                               "TITLE",
                               QString("The SquareDesk database is already being used by '") + hostname + QString("'.")
                               );
            msgBox.setInformativeText("If you continue, any changes might be lost.");
            msgBox.addButton(tr("&Continue anyway"), QMessageBox::AcceptRole);
            msgBox.addButton(tr("&Quit"), QMessageBox::RejectRole);
            if (msgBox.exec() != QMessageBox::AcceptRole) {
                exit(-1);
            }
        } else {
            // probably a recent crash of SquareDesk on THIS device
            // so we're already locked.  Just return, since we already have the lock.
            return;
        }
    }

    // Lock file does NOT exist yet: create a new lock file with our hostname inside
    QFile file2(databaseDir + "/lock.txt");
    file2.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream outfile(&file2);
//    qDebug() << "localHostName: " << QHostInfo::localHostName();
    outfile << QHostInfo::localHostName();
    file2.close();
}

void MainWindow::clearLockFile() {
//    qDebug() << "clearLockFile()";

    PreferencesManager prefsManager;
    QString musicRootPath = prefsManager.GetmusicPath();

    QString databaseDir(musicRootPath + "/.squaredesk");
    QFileInfo checkFile(databaseDir + "/lock.txt");
    if (checkFile.exists()) {
        QFile file(databaseDir + "/lock.txt");
        file.remove();
    }
}


void MainWindow::findMusic(QString mainRootDir, QString guestRootDir, QString mode, bool refreshDatabase)
{
    QString databaseDir(mainRootDir + "/.squaredesk");

    if (refreshDatabase)
    {
        songSettings.openDatabase(databaseDir, mainRootDir, guestRootDir, false);
    }
    // always gets rid of the old pathstack...
    if (pathStack) {
        delete pathStack;
    }

    // make a new one
    pathStack = new QList<QString>();

    // mode == "main": look only in the main directory (e.g. there isn't a guest directory)
    // mode == "guest": look only in the guest directory (e.g. guest overrides main)
    // mode == "both": look in both places for MP3 files (e.g. merge guest into main)
    if (mode == "main" || mode == "both") {
        // looks for files in the mainRootDir --------
        QDir rootDir1(mainRootDir);
        rootDir1.setFilter(QDir::Files | QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

        QStringList qsl;
        QString starDot("*.");
        for (size_t i = 0; i < sizeof(music_file_extensions) / sizeof(*music_file_extensions); ++i)
        {
            qsl.append(starDot + music_file_extensions[i]);
        }
        for (size_t i = 0; i < sizeof(cuesheet_file_extensions) / sizeof(*cuesheet_file_extensions); ++i)
        {
            qsl.append(starDot + cuesheet_file_extensions[i]);
        }

        rootDir1.setNameFilters(qsl);

        findFilesRecursively(rootDir1, pathStack, "", ui, soundFXarray, soundFXname);  // appends to the pathstack
    }

    if (guestRootDir != "" && (mode == "guest" || mode == "both")) {
        // looks for files in the guestRootDir --------
        QDir rootDir2(guestRootDir);
        rootDir2.setFilter(QDir::Files | QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

        QStringList qsl;
        qsl.append("*.mp3");                // I only want MP3 files
        qsl.append("*.m4a");                //          or M4A files
        qsl.append("*.wav");                //          or WAV files
        rootDir2.setNameFilters(qsl);

        findFilesRecursively(rootDir2, pathStack, "*", ui, soundFXarray, soundFXname);  // appends to the pathstack, "*" for "Guest"
    }
}

void addStringToLastRowOfSongTable(QColor &textCol, MyTableWidget *songTable,
                                   QString str, int column)
{
    QTableWidgetItem *newTableItem;
    if (column == kNumberCol || column == kAgeCol || column == kPitchCol || column == kTempoCol) {
        newTableItem = new TableNumberItem( str.trimmed() );  // does sorting correctly for numbers
    } else {
        newTableItem = new QTableWidgetItem( str.trimmed() );
    }
    newTableItem->setFlags(newTableItem->flags() & ~Qt::ItemIsEditable);      // not editable
    newTableItem->setTextColor(textCol);
    if (column == kRecentCol || column == kAgeCol || column == kPitchCol || column == kTempoCol) {
        newTableItem->setTextAlignment(Qt::AlignCenter);
    }
    songTable->setItem(songTable->rowCount()-1, column, newTableItem);
}



// --------------------------------------------------------------------------------
void MainWindow::filterMusic()
{
#ifdef CUSTOM_FILTER
    QString label = ui->labelSearch->text();
    QString type = ui->typeSearch->text();
    QString title = ui->titleSearch->text();

    ui->songTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);  // DO NOT SET height of rows (for now)

    ui->songTable->setSortingEnabled(false);

    int initialRowCount = ui->songTable->rowCount();
    int rowsVisible = initialRowCount;
    int firstVisibleRow = -1;
    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QString songTitle = ui->songTable->item(i,kTitleCol)->text();
        QString songType = ui->songTable->item(i,kTypeCol)->text();
        QString songLabel = ui->songTable->item(i,kLabelCol)->text();

        bool show = true;

        if (!(label.isEmpty()
              || songLabel.contains(label, Qt::CaseInsensitive)))
        {
            show = false;
        }
        if (!(type.isEmpty()
              || songType.contains(type, Qt::CaseInsensitive)))
        {
            show = false;
        }
        if (!(title.isEmpty()
              || songTitle.contains(title, Qt::CaseInsensitive)))
        {
            show = false;
        }
        ui->songTable->setRowHidden(i, !show);
        rowsVisible -= (show ? 0 : 1); // decrement row count, if hidden
        if (show && firstVisibleRow == -1) {
            firstVisibleRow = i;
        }
    }
    ui->songTable->setSortingEnabled(true);

//    qDebug() << "rowsVisible: " << rowsVisible << ", initialRowCount: " << initialRowCount << ", firstVisibleRow: " << firstVisibleRow;
    if (rowsVisible > 0 && rowsVisible != initialRowCount && firstVisibleRow != -1) {
        ui->songTable->selectRow(firstVisibleRow);
    } else {
        ui->songTable->clearSelection();
    }

#else /* ifdef CUSTOM_FILTER */
    loadMusicList();
#endif /* else ifdef CUSTOM_FILTER */

    ui->songTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);  // auto set height of rows
}

// --------------------------------------------------------------------------------
void MainWindow::loadMusicList()
{
    startLongSongTableOperation("loadMusicList");  // for performance, hide and sorting off

    // Need to remember the PL# mapping here, and reapply it after the filter
    // left = path, right = number string
    QMap<QString, QString> path2playlistNum;

    // Iterate over the songTable, saving the mapping in "path2playlistNum"
    // TODO: optimization: save this once, rather than recreating each time.
    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        QString playlistIndex = theItem->text();  // this is the playlist #
        QString pathToMP3 = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();  // this is the full pathname
        if (playlistIndex != " " && playlistIndex != "") {
            // item HAS an index (that is, it is on the list, and has a place in the ordering)
            // TODO: reconcile int here with float elsewhere on insertion
            path2playlistNum[pathToMP3] = playlistIndex;
        }
    }

    // clear out the table
    ui->songTable->setRowCount(0);

    QStringList m_TableHeader;
    m_TableHeader << "#" << "Type" << "Label" << "Title";
    ui->songTable->setHorizontalHeaderLabels(m_TableHeader);
    ui->songTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->songTable->horizontalHeader()->setVisible(true);

    QListIterator<QString> iter(*pathStack);
    QColor textCol = QColor::fromRgbF(0.0/255.0, 0.0/255.0, 0.0/255.0);  // defaults to Black
    QList<QString> extensions;
    QString dot(".");
    for (size_t i = 0; i < sizeof(music_file_extensions) / sizeof(*music_file_extensions); ++i)
    {
        extensions.append(dot + music_file_extensions[i]);
    }
    bool show_all_ages = ui->actionShow_All_Ages->isChecked();

    while (iter.hasNext()) {
        QString s = iter.next();

        QListIterator<QString> extensionIterator(extensions);
        bool foundExtension = false;
        while (extensionIterator.hasNext())
        {
            QString extension(extensionIterator.next());
            if (s.endsWith(extension, Qt::CaseInsensitive))
            {
                foundExtension = true;
            }
        }
        if (!foundExtension)
            continue;

        QStringList sl1 = s.split("#!#");
        QString type = sl1[0];  // the type (of original pathname, before following aliases)
        s = sl1[1];  // everything else
        QString origPath = s;  // for when we double click it later on...

        QFileInfo fi(s);

        if (fi.canonicalPath() == musicRootPath && type.right(1) != "*") {
            // e.g. "/Users/mpogue/__squareDanceMusic/C 117 - Bad Puppy (Patter).mp3" --> NO TYPE PRESENT and NOT a guest song
            type = "";
        }

        QStringList section = fi.canonicalPath().split("/");
        QString label = "";
        QString labelnum = "";
        QString labelnum_extra = "";
        QString title = "";
        QString shortTitle = "";

        s = fi.completeBaseName(); // e.g. "/Users/mpogue/__squareDanceMusic/patter/RIV 307 - Going to Ceili (Patter).mp3" --> "RIV 307 - Going to Ceili (Patter)"
        breakFilenameIntoParts(s, label, labelnum, labelnum_extra, title, shortTitle);
        labelnum += labelnum_extra;

        ui->songTable->setRowCount(ui->songTable->rowCount()+1);  // make one more row for this line

        QString cType = type;  // type for Color purposes
        if (cType.right(1)=="*") {
            cType.chop(1);  // remove the "*" for the purposes of coloring
        }

        if (songTypeNamesForExtras.contains(cType)) {
            textCol = QColor(extrasColorString);
        }
        else if (songTypeNamesForPatter.contains(cType)) {
            textCol = QColor(patterColorString);
        }
        else if (songTypeNamesForSinging.contains(cType)) {
            textCol = QColor(singingColorString);
        }
        else if (songTypeNamesForCalled.contains(cType)) {
            textCol = QColor(calledColorString);
        } else {
            textCol = QColor(Qt::black);  // if not a recognized type, color it black.
        }

        // look up origPath in the path2playlistNum map, and reset the s2 text to the user's playlist # setting (if any)
        QString s2("");
        if (path2playlistNum.contains(origPath)) {
            s2 = path2playlistNum[origPath];
        }
        TableNumberItem *newTableItem4 = new TableNumberItem(s2);

        newTableItem4->setTextAlignment(Qt::AlignCenter);                           // editable by default
        newTableItem4->setTextColor(textCol);
        ui->songTable->setItem(ui->songTable->rowCount()-1, kNumberCol, newTableItem4);      // add it to column 0

        addStringToLastRowOfSongTable(textCol, ui->songTable, type, kTypeCol);
        addStringToLastRowOfSongTable(textCol, ui->songTable, label + " " + labelnum, kLabelCol );
        addStringToLastRowOfSongTable(textCol, ui->songTable, title, kTitleCol);
        QString ageString = songSettings.getSongAge(fi.completeBaseName(), origPath,
                                                    show_all_ages);

        QString ageAsIntString = ageToIntString(ageString);

        addStringToLastRowOfSongTable(textCol, ui->songTable, ageAsIntString, kAgeCol);
        QString recentString = ageToRecent(ageString);  // passed as float string
        addStringToLastRowOfSongTable(textCol, ui->songTable, recentString, kRecentCol);

        int pitch = 0;
        int tempo = 0;
        bool loadedTempoIsPercent(false);
        SongSetting settings;
        songSettings.loadSettings(origPath,
                                  settings);

        if (settings.isSetPitch()) { pitch = settings.getPitch(); }
        if (settings.isSetTempo()) { tempo = settings.getTempo(); }
        if (settings.isSetTempoIsPercent()) { loadedTempoIsPercent = settings.getTempoIsPercent(); }

        addStringToLastRowOfSongTable(textCol, ui->songTable,
                                      QString("%1").arg(pitch),
                                      kPitchCol);

        QString tempoStr = QString("%1").arg(tempo);
        if (loadedTempoIsPercent) tempoStr += "%";
        addStringToLastRowOfSongTable(textCol, ui->songTable,
                                      tempoStr,
                                      kTempoCol);
        // keep the path around, for loading in when we double click on it
        ui->songTable->item(ui->songTable->rowCount()-1, kPathCol)->setData(Qt::UserRole,
                QVariant(origPath)); // path set on cell in col 0

        // Filter out (hide) rows that we're not interested in, based on the search fields...
        //   4 if statements is clearer than a gigantic single if....
        QString labelPlusNumber = label + " " + labelnum;
#ifndef CUSTOM_FILTER
        if (ui->labelSearch->text() != "" &&
                !labelPlusNumber.contains(QString(ui->labelSearch->text()),Qt::CaseInsensitive)) {
            ui->songTable->setRowHidden(ui->songTable->rowCount()-1,true);
        }

        if (ui->typeSearch->text() != "" && !type.contains(QString(ui->typeSearch->text()),Qt::CaseInsensitive)) {
            ui->songTable->setRowHidden(ui->songTable->rowCount()-1,true);
        }

        if (ui->titleSearch->text() != "" &&
                !title.contains(QString(ui->titleSearch->text()),Qt::CaseInsensitive)) {
            ui->songTable->setRowHidden(ui->songTable->rowCount()-1,true);
        }
#endif /* ifndef CUSTOM_FILTER */
    }

#ifdef CUSTOM_FILTER
    filterMusic();
#endif /* ifdef CUSTOM_FILTER */

    sortByDefaultSortOrder();
    stopLongSongTableOperation("loadMusicList");  // for performance, sorting on again and show

    QString msg1;
    if (guestMode == "main") {
        msg1 = QString::number(ui->songTable->rowCount()) + QString(" audio files found.");
    } else if (guestMode == "guest") {
        msg1 = QString::number(ui->songTable->rowCount()) + QString(" guest audio files found.");
    } else if (guestMode == "both") {
        msg1 = QString::number(ui->songTable->rowCount()) + QString(" total audio files found.");
    }
    ui->statusBar->showMessage(msg1);
}

QString processSequence(QString sequence,
                        const QStringList &include,
                        const QStringList &exclude)
{
    static QRegularExpression regexEmpty("^[\\s\\n]*$");
    QRegularExpressionMatch match = regexEmpty.match(sequence);
    if (match.hasMatch())
    {
        return QString();
    }

    for (int i = 0; i < exclude.length(); ++i)
    {
        if (sequence.contains(exclude[i], Qt::CaseInsensitive))
        {
            return QString();
        }
    }
    for (int i = 0; i < include.length(); ++i)
    {
        if (!sequence.contains(include[i], Qt::CaseInsensitive))
        {
            return QString();
        }
    }

    return sequence.trimmed();

//    QRegExp regexpAmp("&");
//    QRegExp regexpLt("<");
//    QRegExp regexpGt(">");
//    QRegExp regexpApos("'");
//    QRegExp regexpNewline("\n");
//
//    sequence = sequence.replace(regexpAmp, "&amp;");
//    sequence = sequence.replace(regexpLt, "&lt;");
//    sequence = sequence.replace(regexpGt, "&gt;");
//    sequence = sequence.replace(regexpApos, "&apos;");
//    sequence = sequence.replace(regexpNewline, "<br/>\n");
//
//    return "<h1>" + title + "</h1>\n<p>" + sequence + "</p>\n";

}

void extractSequencesFromFile(QStringList &sequences,
                                 const QString &filename,
                                 const QString &program,
                                 const QStringList &include,
                                 const QStringList &exclude)
{
    QFile file(filename);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QTextStream in(&file);
    bool isSDFile(false);
    bool firstSDLine(false);
    QString thisProgram = "";
    QString title(program);

    if (filename.contains(program, Qt::CaseInsensitive))
    {
        thisProgram = program;
    }

    // Sun Jan 10 17:03:38 2016     Sd38.58:db38.58     Plus
    static QRegularExpression regexIsSDFile("^(Mon|Tue|Wed|Thur|Fri|Sat|Sun)\\s+" // Sun
                                           "(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\\s+" // Jan
                                           "\\d+\\s+\\d+\\:\\d+\\:\\d+\\s+\\d\\d\\d\\d\\s+" // 10 17:03:38 2016
                                           "Sd\\d+\\.\\d+\\:db\\d+\\.\\d+\\s+" //Sd38.58:db38.58
                                           "(\\w+)\\s*$"); // Plus

    QString sequence;

    while (!in.atEnd())
    {
        QString line(in.readLine());

        QRegularExpressionMatch match = regexIsSDFile.match(line);

        if (match.hasMatch())
        {
            if (0 == thisProgram.compare(program, Qt::CaseInsensitive))
            {
                sequences << processSequence(sequence, include, exclude);
            }
            isSDFile = true;
            firstSDLine = true;
            thisProgram = match.captured(3);
            sequence.clear();
            title.clear();
        }
        else if (!isSDFile)
        {
            QString line_simplified = line.simplified();
            if (line_simplified.startsWith("Basic", Qt::CaseInsensitive))
            {
                if (0 == thisProgram.compare(title, program, Qt::CaseInsensitive))
                {
                    sequences << processSequence(sequence, include, exclude);
                }
                thisProgram = "Basic";
                sequence.clear();
            }
            else if (line_simplified.startsWith("+", Qt::CaseInsensitive)
                || line_simplified.startsWith("Plus", Qt::CaseInsensitive))
            {
                if (0 == thisProgram.compare(title, program, Qt::CaseInsensitive))
                {
                    sequences << processSequence(sequence, include, exclude);
                }
                thisProgram = "Plus";
                sequence.clear();
            }
            else if (line_simplified.length() == 0)
            {
                if (0 == thisProgram.compare(title, program, Qt::CaseInsensitive))
                {
                    sequences << processSequence(sequence, include, exclude);
                }
                sequence.clear();
            }
            else
            {
                sequence += line + "\n";
            }

        }
        else // is SD file
        {
            QString line_simplified = line.simplified();

            if (firstSDLine)
            {
                if (line_simplified.length() == 0)
                {
                    firstSDLine = false;
                }
                else
                {
                    title += line;
                }
            }
            else
            {
                if (!(line_simplified.length() == 0))
                {
                    sequence += line + "\n";
                }
            }
        }
    }
    sequences << processSequence(sequence, include, exclude);
}




QStringList MainWindow::getUncheckedItemsFromCurrentCallList()
{
    QStringList uncheckedItems;
    for (int row = 0; row < ui->tableWidgetCallList->rowCount(); ++row)
    {
        if (ui->tableWidgetCallList->item(row, kCallListCheckedCol)->checkState() == Qt::Unchecked)
        {
            uncheckedItems.append(ui->tableWidgetCallList->item(row, kCallListNameCol)->data(0).toString());
        }
    }
    return uncheckedItems;
}

void MainWindow::filterChoreography()
{
    QStringList exclude(getUncheckedItemsFromCurrentCallList());
    QString program = ui->comboBoxCallListProgram->currentText();
#ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
    QStringList include = ui->lineEditChoreographySearch->text().split(",");
    for (int i = 0; i < include.length(); ++i)
    {
        include[i] = include[i].simplified();
    }

    if (ui->comboBoxChoreographySearchType->currentIndex() == 0)
    {
        exclude.clear();
    }

    QStringList sequences;

    for (int i = 0; i < ui->listWidgetChoreographyFiles->count()
             && sequences.length() < 128000; ++i)
    {
        QListWidgetItem *item = ui->listWidgetChoreographyFiles->item(i);
        if (item->checkState() == Qt::Checked)
        {
            QString filename = item->data(1).toString();
            extractSequencesFromFile(sequences, filename, program,
                                     include, exclude);
        }
    }

    ui->listWidgetChoreographySequences->clear();
    for (auto sequence : sequences)
    {
        if (!sequence.isEmpty())
        {
            QListWidgetItem *item = new QListWidgetItem(sequence);
            ui->listWidgetChoreographySequences->addItem(item);
        }
    }
#endif // ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
}

#ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
void MainWindow::on_listWidgetChoreographySequences_itemDoubleClicked(QListWidgetItem * /* item */)
{
    QListWidgetItem *choreoItem = new QListWidgetItem(item->text());
    ui->listWidgetChoreography->addItem(choreoItem);
}

void MainWindow::on_listWidgetChoreography_itemDoubleClicked(QListWidgetItem * /* item */)
{
    ui->listWidgetChoreography->takeItem(ui->listWidgetChoreography->row(item));
}


void MainWindow::on_lineEditChoreographySearch_textChanged()
{
    filterChoreography();
}

void MainWindow::on_listWidgetChoreographyFiles_itemChanged(QListWidgetItem * /* item */)
{
    filterChoreography();
}
#endif // ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT

void MainWindow::loadChoreographyList()
{
#ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
    ui->listWidgetChoreographyFiles->clear();

    QListIterator<QString> iter(*pathStack);

    while (iter.hasNext()) {
        QString s = iter.next();

        if (s.endsWith(".txt", Qt::CaseInsensitive)
            && (s.contains("sequence", Qt::CaseInsensitive)
                || s.contains("singer", Qt::CaseInsensitive)))
        {
            QStringList sl1 = s.split("#!#");
            QString type = sl1[0];  // the type (of original pathname, before following aliases)
            QString origPath = sl1[1];  // everything else

            QFileInfo fi(origPath);
//            QStringList section = fi.canonicalPath().split("/");
            QString name = fi.completeBaseName();
            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(1,origPath);
            item->setCheckState(Qt::Unchecked);
            ui->listWidgetChoreographyFiles->addItem(item);
        }
    }
#endif // ifdef EXPERIMENTAL_CHOREOGRAPHY_MANAGEMENT
}


static void addToProgramsAndWriteTextFile(QStringList &programs, QDir outputDir,
                                   const char *filename,
                                   const char *fileLines[])
{
    QString outputFile = outputDir.canonicalPath() + "/" + filename;
    QFile file(outputFile);
    if (file.open(QIODevice::ReadWrite)) {
        QTextStream stream(&file);
        for (int i = 0; fileLines[i]; ++i)
        {
            stream << fileLines[i] << endl;
        }
        programs << outputFile;
    }
}




void MainWindow::loadDanceProgramList(QString lastDanceProgram)
{
    ui->comboBoxCallListProgram->clear();
    QListIterator<QString> iter(*pathStack);
    QStringList programs;

    // FIX: This should be changed to look only in <rootDir>/reference, rather than looking
    //   at all pathnames in the <rootDir>.  It will be much faster.
    while (iter.hasNext()) {
        QString s = iter.next();

//        if (s.endsWith(".txt", Qt::CaseInsensitive))
        if (QRegExp("reference/[a-zA-Z0-9]+\\.[a-zA-Z0-9' ]+\\.txt$", Qt::CaseInsensitive).indexIn(s) != -1)  // matches the Dance Program files in /reference
        {
            //qDebug() << "Dance Program Match:" << s;
            QStringList sl1 = s.split("#!#");
            QString type = sl1[0];  // the type (of original pathname, before following aliases)
            QString origPath = sl1[1];  // everything else
            QFileInfo fi(origPath);
            if (fi.dir().canonicalPath().endsWith("/reference", Qt::CaseInsensitive))
            {
                programs << origPath;
            }
        }
    }

    if (programs.length() == 0)
    {
        QString referencePath = musicRootPath + "/reference";
        QDir outputDir(referencePath);
        if (!outputDir.exists())
        {
            outputDir.mkpath(".");
        }

        addToProgramsAndWriteTextFile(programs, outputDir, "010.basic1.txt", danceprogram_basic1);
        addToProgramsAndWriteTextFile(programs, outputDir, "020.basic2.txt", danceprogram_basic2);
        addToProgramsAndWriteTextFile(programs, outputDir, "030.mainstream.txt", danceprogram_mainstream);
        addToProgramsAndWriteTextFile(programs, outputDir, "040.plus.txt", danceprogram_plus);
        addToProgramsAndWriteTextFile(programs, outputDir, "050.a1.txt", danceprogram_a1);
        addToProgramsAndWriteTextFile(programs, outputDir, "060.a2.txt", danceprogram_a2);

    }
    programs.sort(Qt::CaseInsensitive);
    QListIterator<QString> program(programs);
    while (program.hasNext())
    {
        QString origPath = program.next();
        QString name;
        QString program;
        breakDanceProgramIntoParts(origPath, name, program);
        ui->comboBoxCallListProgram->addItem(name, origPath);
    }

    if (ui->comboBoxCallListProgram->maxCount() == 0)
    {
        ui->comboBoxCallListProgram->addItem("<no dance programs found>", "");
    }

    for (int i = 0; i < ui->comboBoxCallListProgram->count(); ++i)
    {
        if (ui->comboBoxCallListProgram->itemText(i) == lastDanceProgram)
        {
            ui->comboBoxCallListProgram->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::on_labelSearch_textChanged()
{
    filterMusic();
}

void MainWindow::on_typeSearch_textChanged()
{
    filterMusic();
}

void MainWindow::on_titleSearch_textChanged()
{
    filterMusic();
}

void MainWindow::on_songTable_itemDoubleClicked(QTableWidgetItem *item)
{
//    qDebug() << "Timer starting...";
//    t2.start(); // DEBUG

    on_stopButton_clicked();  // if we're loading a new MP3 file, stop current playback
    saveCurrentSongSettings();

//    qDebug() << "load 1: " << t2.elapsed() << "ms";

    int row = item->row();
    QString pathToMP3 = ui->songTable->item(row,kPathCol)->data(Qt::UserRole).toString();

    QString songTitle = ui->songTable->item(row,kTitleCol)->text();
    // FIX:  This should grab the title from the MP3 metadata in the file itself instead.

    QString songType = ui->songTable->item(row,kTypeCol)->text().toLower();

    // these must be up here to get the correct values...
    QString pitch = ui->songTable->item(row,kPitchCol)->text();
    QString tempo = ui->songTable->item(row,kTempoCol)->text();

//    qDebug() << "load 2: " << t2.elapsed() << "ms";

    loadMP3File(pathToMP3, songTitle, songType);

//    qDebug() << "load 3: " << t2.elapsed() << "ms";

    // these must be down here, to set the correct values...
    int pitchInt = pitch.toInt();
    ui->pitchSlider->setValue(pitchInt);

    on_pitchSlider_valueChanged(pitchInt); // manually call this, in case the setValue() line doesn't call valueChanged() when the value set is
                                           //   exactly the same as the previous value.  This will ensure that cBass.setPitch() gets called (right now) on the new stream.

    if (tempo != "0" && tempo != "0%") {
        // iff tempo is known, then update the table
        QString tempo2 = tempo.replace("%",""); // if percentage (not BPM) just get rid of the "%" (setValue knows what to do)
        int tempoInt = tempo2.toInt();
        if (tempoInt !=0)
        {
            ui->tempoSlider->setValue(tempoInt);
        }
    }
    if (ui->actionAutostart_playback->isChecked()) {
        on_playButton_clicked();
    }

//    qDebug() << "load 4: " << t2.elapsed() << "ms";
}

void MainWindow::on_actionClear_Search_triggered()
{
    on_clearSearchButton_clicked();
}

void MainWindow::on_actionPitch_Up_triggered()
{
    ui->pitchSlider->setValue(ui->pitchSlider->value() + 1);
}

void MainWindow::on_actionPitch_Down_triggered()
{
    ui->pitchSlider->setValue(ui->pitchSlider->value() - 1);
}

void MainWindow::on_actionAutostart_playback_triggered()
{
    // the Autostart on Playback mode setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.Setautostartplayback(ui->actionAutostart_playback->isChecked());
}

void MainWindow::on_checkBoxPlayOnEnd_clicked()
{
    PreferencesManager prefsManager;
    prefsManager.Setstartplaybackoncountdowntimer(ui->checkBoxPlayOnEnd->isChecked());
}

void MainWindow::on_checkBoxStartOnPlay_clicked()
{
    PreferencesManager prefsManager;
    prefsManager.Setstartcountuptimeronplay(ui->checkBoxStartOnPlay->isChecked());
}


void MainWindow::on_actionImport_triggered()
{
    RecursionGuard dialog_guard(inPreferencesDialog);

    ImportDialog *importDialog = new ImportDialog();
    int dialogCode = importDialog->exec();
    RecursionGuard keypress_guard(trapKeypresses);
    if (dialogCode == QDialog::Accepted)
    {
        importDialog->importSongs(songSettings, pathStack);
        loadMusicList();
    }
    delete importDialog;
    importDialog = NULL;
}

void MainWindow::on_actionExport_triggered()
{
    RecursionGuard dialog_guard(inPreferencesDialog);

    if (true)
    {
        QString filename =
            QFileDialog::getSaveFileName(this, tr("Select Export File"),
                                         QDir::homePath(),
                                         tr("Tab Separated (*.tsv);;Comma Separated (*.csv)"));
        if (!filename.isNull())
        {
            QFile file( filename );
            if ( file.open(QIODevice::WriteOnly) )
            {
                QTextStream stream( &file );

                enum ColumnExportData outputFields[7];
                int outputFieldCount = sizeof(outputFields) / sizeof(*outputFields);
                char separator = filename.endsWith(".csv", Qt::CaseInsensitive) ? ',' :
                    '\t';

                outputFields[0] = ExportDataFileName;
                outputFields[1] = ExportDataPitch;
                outputFields[2] = ExportDataTempo;
                outputFields[3] = ExportDataIntro;
                outputFields[4] = ExportDataOutro;
                outputFields[5] = ExportDataVolume;
                outputFields[6] = ExportDataCuesheetPath;

                exportSongList(stream, songSettings, pathStack,
                               outputFieldCount, outputFields,
                               separator,
                               true, false);
            }
        }
    }
    else
    {
        ExportDialog *exportDialog = new ExportDialog();
        int dialogCode = exportDialog->exec();
        RecursionGuard keypress_guard(trapKeypresses);
        if (dialogCode == QDialog::Accepted)
        {
            exportDialog->exportSongs(songSettings, pathStack);
        }
        delete exportDialog;
        exportDialog = NULL;
    }
}

// --------------------------------------------------------
void MainWindow::on_actionPreferences_triggered()
{
    RecursionGuard dialog_guard(inPreferencesDialog);
    trapKeypresses = false;
//    on_stopButton_clicked();  // stop music, if it was playing...
    PreferencesManager prefsManager;

    prefDialog = new PreferencesDialog(soundFXname);
    prefsManager.SetHotkeyMappings(hotkeyMappings);
    prefsManager.populatePreferencesDialog(prefDialog);
    prefDialog->songTableReloadNeeded = false;  // nothing has changed...yet.
    SessionDefaultType previousSessionDefaultType =
        static_cast<SessionDefaultType>(prefsManager.GetSessionDefault());

    // modal dialog
    int dialogCode = prefDialog->exec();
    trapKeypresses = true;

    // act on dialog return code
    if(dialogCode == QDialog::Accepted) {
        // OK clicked
        // Save the new value for musicPath --------
        prefsManager.extractValuesFromPreferencesDialog(prefDialog);
        hotkeyMappings = prefsManager.GetHotkeyMappings();

        // USER SAID "OK", SO HANDLE THE UPDATED PREFS ---------------
        musicRootPath = prefsManager.GetmusicPath();

        if (previousSessionDefaultType !=
            static_cast<SessionDefaultType>(prefsManager.GetSessionDefault()))
        {
            int sessionActionIndex = SessionDefaultPractice ==
                static_cast<SessionDefaultType>(prefsManager.GetSessionDefault()) ?
                0 : songSettings.currentDayOfWeek();
            sessionActions[sessionActionIndex]->setChecked(true);
        }

        findMusic(musicRootPath, "", "main", true); // always refresh the songTable after the Prefs dialog returns with OK
        switchToLyricsOnPlay = prefsManager.GetswitchToLyricsOnPlay();

        // Save the new value for music type colors --------
        patterColorString = prefsManager.GetpatterColorString();
        singingColorString = prefsManager.GetsingingColorString();
        calledColorString = prefsManager.GetcalledColorString();
        extrasColorString = prefsManager.GetextrasColorString();

        // ----------------------------------------------------------------
        // Show the Timers tab, if it is enabled now
        if (prefsManager.GetexperimentalTimersEnabled()) {
            if (!showTimersTab) {
                // iff the tab was NOT showing, make it show up now
                ui->tabWidget->insertTab(1, tabmap.value(1).first, tabmap.value(1).second);  // bring it back now!
            }
            showTimersTab = true;
        }
        else {
            if (showTimersTab) {
                // iff timers tab was showing, remove it
                ui->tabWidget->removeTab(1);  // hidden, but we can bring it back later
            }
            showTimersTab = false;
        }

        // ----------------------------------------------------------------
        // Show the Lyrics tab, if it is enabled now
        lyricsTabNumber = (showTimersTab ? 2 : 1);

        bool isPatter = songTypeNamesForPatter.contains(currentSongType);
//        qDebug() << "actionPreferences_triggered: " << currentSongType << isPatter;

        if (isPatter) {
            if (hasLyrics && lyricsTabNumber != -1) {
                ui->tabWidget->setTabText(lyricsTabNumber, "*Patter");
            } else {
                ui->tabWidget->setTabText(lyricsTabNumber, "Patter");
            }
        } else {
            if (hasLyrics && lyricsTabNumber != -1) {
                ui->tabWidget->setTabText(lyricsTabNumber, "*Lyrics");
            } else {
                ui->tabWidget->setTabText(lyricsTabNumber, "Lyrics");
            }
        }

        // -----------------------------------------------------------------------
        // Save the new settings for experimental break and patter timers --------
        tipLengthTimerEnabled = prefsManager.GettipLengthTimerEnabled();  // save new settings in MainWindow, too
        tipLength30secEnabled = prefsManager.GettipLength30secEnabled();
        tipLengthTimerLength = prefsManager.GettipLengthTimerLength();
        tipLengthAlarmAction = prefsManager.GettipLengthAlarmAction();

        breakLengthTimerEnabled = prefsManager.GetbreakLengthTimerEnabled();
        breakLengthTimerLength = prefsManager.GetbreakLengthTimerLength();
        breakLengthAlarmAction = prefsManager.GetbreakLengthAlarmAction();

        // and tell the clock, too.
        analogClock->tipLengthTimerEnabled = tipLengthTimerEnabled;
        analogClock->tipLength30secEnabled = tipLength30secEnabled;
        analogClock->tipLengthAlarmMinutes = tipLengthTimerLength;

        analogClock->breakLengthTimerEnabled = breakLengthTimerEnabled;
        analogClock->breakLengthAlarmMinutes = breakLengthTimerLength;

        // ----------------------------------------------------------------
        // Save the new value for experimentalClockColoringEnabled --------
        clockColoringHidden = !prefsManager.GetexperimentalClockColoringEnabled();
        analogClock->setHidden(clockColoringHidden);

        {
            QString value;
            value = prefsManager.GetMusicTypeSinging();
            songTypeNamesForSinging = value.toLower().split(";", QString::KeepEmptyParts);

            value = prefsManager.GetMusicTypePatter();
            songTypeNamesForPatter = value.toLower().split(";", QString::KeepEmptyParts);

            value = prefsManager.GetMusicTypeExtras();
            songTypeNamesForExtras = value.toLower().split(";", QString::KeepEmptyParts);

            value = prefsManager.GetMusicTypeCalled();
            songTypeNamesForCalled = value.split(";", QString::KeepEmptyParts);
        }
        songFilenameFormat = static_cast<enum SongFilenameMatchingType>(prefsManager.GetSongFilenameFormat());

        if (prefDialog->songTableReloadNeeded) {
            loadMusicList();
        }

        if (prefsManager.GetenableAutoAirplaneMode()) {
            // if the user JUST set the preference, turn Airplane Mode on RIGHT NOW (radios OFF).
            airplaneMode(true);
        } else {
            // if the user JUST set the preference, turn Airplane Mode OFF RIGHT NOW (radios ON).
            airplaneMode(false);
        }

//        if (prefsManager.GetenableAutoMicsOff()) {
//            microphoneStatusUpdate();
//        }
    }

    delete prefDialog;
    prefDialog = NULL;
}

QString MainWindow::removePrefix(QString prefix, QString s)
{
    QString s2 = s.remove( prefix );
    return s2;
}

// Adapted from: https://github.com/hnaohiro/qt-csv/blob/master/csv.cpp
QStringList MainWindow::parseCSV(const QString &string)
{
    enum State {Normal, Quote} state = Normal;
    QStringList fields;
    QString value;

    for (int i = 0; i < string.size(); i++)
    {
        QChar current = string.at(i);

        // Normal state
        if (state == Normal)
        {
            // Comma
            if (current == ',')
            {
                // Save field
                fields.append(value);
                value.clear();
            }

            // Double-quote
            else if (current == '"')
                state = Quote;

            // Other character
            else
                value += current;
        }

        // In-quote state
        else if (state == Quote)
        {
            // Another double-quote
            if (current == '"')
            {
                if (i+1 < string.size())
                {
                    QChar next = string.at(i+1);

                    // A double double-quote?
                    if (next == '"')
                    {
                        value += '"';
                        i++;
                    }
                    else
                        state = Normal;
                }
            }

            // Other character
            else
                value += current;
        }
    }
    if (!value.isEmpty())
        fields.append(value);

    return fields;
}

// returns first song error, and also updates the songCount as it goes (2 return values)
QString MainWindow::loadPlaylistFromFile(QString PlaylistFileName, int &songCount) {

//    qDebug() << "loadPlaylist: " << PlaylistFileName;
    addFilenameToRecentPlaylist(PlaylistFileName);  // remember it in the Recent list

    // --------
    QString firstBadSongLine = "";
    QFile inputFile(PlaylistFileName);
    if (inputFile.open(QIODevice::ReadOnly)) { // defaults to Text mode

        // first, clear all the playlist numbers that are there now.
        for (int i = 0; i < ui->songTable->rowCount(); i++) {
            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
            theItem->setText("");
        }

        int lineCount = 1;
        linesInCurrentPlaylist = 0;

        QTextStream in(&inputFile);

        if (PlaylistFileName.endsWith(".csv", Qt::CaseInsensitive)) {
            // CSV FILE =================================

            QString header = in.readLine();  // read header (and throw away for now), should be "abspath,pitch,tempo"

            while (!in.atEnd()) {
                QString line = in.readLine();

                if (line == "abspath") {
                    // V1 of the CSV file format has exactly one field, an absolute pathname in quotes
                }
                else if (line == "") {
                    // ignore, it's a blank line
                }
                else {
                    songCount++;  // it's a real song path

                    QStringList list1 = parseCSV(line);  // This is more robust than split(). Handles commas inside double quotes, double double quotes, etc.

                    bool match = false;
                    // exit the loop early, if we find a match
                    for (int i = 0; (i < ui->songTable->rowCount())&&(!match); i++) {

                        QString pathToMP3 = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();

                        if (list1[0] == pathToMP3) { // FIX: this is fragile, if songs are moved around, since absolute paths are used.

                            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
                            theItem->setText(QString::number(songCount));

                            QTableWidgetItem *theItem2 = ui->songTable->item(i,kPitchCol);
                            theItem2->setText(list1[1].trimmed());

                            QTableWidgetItem *theItem3 = ui->songTable->item(i,kTempoCol);
                            theItem3->setText(list1[2].trimmed());

                            match = true;
                        }
                    }
                    // if we had no match, remember the first non-matching song path
                    if (!match && firstBadSongLine == "") {
                        firstBadSongLine = line;
                    }

                }

                lineCount++;
            } // while
        }
        else {
            // M3U FILE =================================
            // to workaround old-style Mac files that have a bare "\r" (which readLine() can't handle)
            //   in particular, iTunes exported playlists use this old format.
            QStringList theWholeFile = in.readAll().replace("\r\n","\n").replace("\r","\n").split("\n");

            foreach (const QString &line, theWholeFile) {
//                qDebug() << "line:" << line;
                if (line == "#EXTM3U") {
                    // ignore, it's the first line of the M3U file
                }
                else if (line == "") {
                    // ignore, it's a blank line
                }
                else if (line.at( 0 ) == '#' ) {
                    // it's a comment line
                    if (line.mid(0,7) == "#EXTINF") {
                        // it's information about the next line, ignore for now.
                    }
                }
                else {
                    songCount++;  // it's a real song path

                    bool match = false;
                    // exit the loop early, if we find a match
                    for (int i = 0; (i < ui->songTable->rowCount())&&(!match); i++) {
                        QString pathToMP3 = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();
//                        qDebug() << "pathToMP3:" << pathToMP3;
                        if (line == pathToMP3) { // FIX: this is fragile, if songs are moved around
                            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
                            theItem->setText(QString::number(songCount));

                            match = true;
                        }
                    }
                    // if we had no match, remember the first non-matching song path
                    if (!match && firstBadSongLine == "") {
                        firstBadSongLine = line;
                    }

                }

                lineCount++;

            } // for each line in the M3U file

        } // else M3U file

        inputFile.close();
        linesInCurrentPlaylist += songCount; // when non-zero, this enables saving of the current playlist
//        qDebug() << "linesInCurrentPlaylist:" << linesInCurrentPlaylist;

//        qDebug() << "FBS:" << firstBadSongLine << ", linesInCurrentPL:" << linesInCurrentPlaylist;
        if (firstBadSongLine=="" && linesInCurrentPlaylist != 0) {
            // a playlist is now loaded, NOTE: side effect of loading a playlist is enabling Save/SaveAs...
            ui->actionSave->setEnabled(false);  // save playlist (TODO: doesn't remember current playlist name)
            ui->actionSave_As->setEnabled(true);  // save playlist as...
        }
    }
    else {
        // file didn't open...
        return("");
    }

    return(firstBadSongLine);  // return error song (if any)
}


// PLAYLIST MANAGEMENT ===============================================
void MainWindow::finishLoadingPlaylist(QString PlaylistFileName) {

    startLongSongTableOperation("finishLoadingPlaylist"); // for performance measurements, hide and sorting off

    // --------
    QString firstBadSongLine = "";
    int songCount = 0;

    firstBadSongLine = loadPlaylistFromFile(PlaylistFileName, songCount);

    sortByDefaultSortOrder();
    ui->songTable->sortItems(kNumberCol);  // sort by playlist # as primary (must be LAST)

    // select the very first row, and trigger a GO TO PREVIOUS, which will load row 0 (and start it, if autoplay is ON).
    // only do this, if there were no errors in loading the playlist numbers.
    if (firstBadSongLine == "") {
        ui->songTable->selectRow(0); // select first row of newly loaded and sorted playlist!
        on_actionPrevious_Playlist_Item_triggered();
    }

    stopLongSongTableOperation("finishLoadingPlaylist"); // for performance measurements, sorting on again and show

    QString msg1 = QString("Loaded playlist with ") + QString::number(songCount) + QString(" items.");
    if (firstBadSongLine != "") {
        // if there was a non-matching path, tell the user what the first one of those was
        msg1 = QString("ERROR: could not find '") + firstBadSongLine + QString("'");
        ui->songTable->clearSelection(); // select nothing, if error
    }
    ui->statusBar->showMessage(msg1);
}

void MainWindow::on_actionLoad_Playlist_triggered()
{
    on_stopButton_clicked();  // if we're loading a new PLAYLIST file, stop current playback

    // http://stackoverflow.com/questions/3597900/qsettings-file-chooser-should-remember-the-last-directory
    const QString DEFAULT_PLAYLIST_DIR_KEY("default_playlist_dir");
    PreferencesManager prefsManager;
    QString musicRootPath = prefsManager.GetmusicPath();
    QString startingPlaylistDirectory = prefsManager.Getdefault_playlist_dir();

    trapKeypresses = false;
    QString PlaylistFileName =
        QFileDialog::getOpenFileName(this,
                                     tr("Load Playlist"),
                                     startingPlaylistDirectory,
                                     tr("Playlist Files (*.m3u *.csv)"));
    trapKeypresses = true;
    if (PlaylistFileName.isNull()) {
        return;  // user cancelled...so don't do anything, just return
    }

    // not null, so save it in Settings (File Dialog will open in same dir next time)
    QDir CurrentDir;
    QFileInfo fInfo(PlaylistFileName);
    prefsManager.Setdefault_playlist_dir(fInfo.absolutePath());

    finishLoadingPlaylist(PlaylistFileName);
}

struct PlaylistExportRecord
{
    int index;
    QString title;
    QString pitch;
    QString tempo;
};

static bool comparePlaylistExportRecord(const PlaylistExportRecord &a, const PlaylistExportRecord &b)
{
    return a.index < b.index;
}

// SAVE CURRENT PLAYLIST TO FILE
void MainWindow::saveCurrentPlaylistToFile(QString PlaylistFileName) {
    // --------
    QList<PlaylistExportRecord> exports;

    // Iterate over the songTable
    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        QString playlistIndex = theItem->text();
        QString pathToMP3 = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();
        QString songTitle = ui->songTable->item(i,kTitleCol)->text();
        QString pitch = ui->songTable->item(i,kPitchCol)->text();
        QString tempo = ui->songTable->item(i,kTempoCol)->text();

        if (playlistIndex != "") {
            // item HAS an index (that is, it is on the list, and has a place in the ordering)
            // TODO: reconcile int here with float elsewhere on insertion
            PlaylistExportRecord rec;
            rec.index = playlistIndex.toInt();
//            rec.title = songTitle;
            rec.title = pathToMP3;  // NOTE: this is an absolute path that does not survive moving musicDir
            rec.pitch = pitch;
            rec.tempo = tempo;
            exports.append(rec);
        }
    }

    qSort(exports.begin(), exports.end(), comparePlaylistExportRecord);
    // TODO: strip the initial part of the path off the Paths, e.g.
    //   /Users/mpogue/__squareDanceMusic/patter/C 117 - Restless Romp (Patter).mp3
    //   becomes
    //   patter/C 117 - Restless Romp (Patter).mp3
    //
    //   So, the remaining path is relative to the root music directory.
    //   When loading, first look at the patter and the rest
    //     if no match, try looking at the rest only
    //     if no match, then error (dialog?)
    //   Then on Save Playlist, write out the NEW patter and the rest

    QFile file(PlaylistFileName);
    if (PlaylistFileName.endsWith(".m3u", Qt::CaseInsensitive)) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {  // delete, if it exists already
            QTextStream stream(&file);
            stream << "#EXTM3U" << endl << endl;

            // list is auto-sorted here
            foreach (const PlaylistExportRecord &rec, exports)
            {
                stream << "#EXTINF:-1," << endl;  // nothing after the comma = no special name
                stream << rec.title << endl;
            }
            file.close();
            addFilenameToRecentPlaylist(PlaylistFileName);  // add to the MRU list
        }
        else {
            ui->statusBar->showMessage(QString("ERROR: could not open M3U file."));
        }
    }
    else if (PlaylistFileName.endsWith(".csv", Qt::CaseInsensitive)) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream stream(&file);
            stream << "abspath,pitch,tempo" << endl;

            foreach (const PlaylistExportRecord &rec, exports)
            {
                stream << "\"" << rec.title << "\"," <<
                    rec.pitch << "," <<
                    rec.tempo << endl; // quoted absolute path, integer pitch (no quotes), integer tempo (opt % or 0)
            }
            file.close();
            addFilenameToRecentPlaylist(PlaylistFileName);  // add to the MRU list
        }
        else {
            ui->statusBar->showMessage(QString("ERROR: could not open CSV file."));
        }
    }
}


// TODO: strip off the root directory before saving...
void MainWindow::on_actionSave_Playlist_triggered()
{
    on_stopButton_clicked();  // if we're saving a new PLAYLIST file, stop current playback

    // http://stackoverflow.com/questions/3597900/qsettings-file-chooser-should-remember-the-last-directory
    const QString DEFAULT_PLAYLIST_DIR_KEY("default_playlist_dir");
    QSettings MySettings; // Will be using application informations for correct location of your settings

    QString startingPlaylistDirectory = MySettings.value(DEFAULT_PLAYLIST_DIR_KEY).toString();
    if (startingPlaylistDirectory.isNull()) {
        // first time through, start at HOME
        startingPlaylistDirectory = QDir::homePath();
    }

    QString preferred("CSV files (*.csv)");
    trapKeypresses = false;
    QString PlaylistFileName =
        QFileDialog::getSaveFileName(this,
                                     tr("Save Playlist"),
                                     startingPlaylistDirectory + "/playlist.csv",
                                     tr("M3U playlists (*.m3u);;CSV files (*.csv)"),
                                     &preferred);  // preferred is CSV
    trapKeypresses = true;
    if (PlaylistFileName.isNull()) {
        return;  // user cancelled...so don't do anything, just return
    }

    // not null, so save it in Settings (File Dialog will open in same dir next time)
    QFileInfo fInfo(PlaylistFileName);
    PreferencesManager prefsManager;
    prefsManager.Setdefault_playlist_dir(fInfo.absolutePath());

    saveCurrentPlaylistToFile(PlaylistFileName);  // SAVE IT

    // TODO: if there are no songs specified in the playlist (yet, because not edited, or yet, because
    //   no playlist was loaded), Save Playlist... should be greyed out.

    if (PlaylistFileName.endsWith(".csv", Qt::CaseInsensitive)) {
        ui->statusBar->showMessage(QString("Playlist items saved as CSV file."));
    }
    else if (PlaylistFileName.endsWith(".m3u", Qt::CaseInsensitive)) {
        ui->statusBar->showMessage(QString("Playlist items saved as M3U file."));
    }
    else {
        ui->statusBar->showMessage(QString("ERROR: Can't save to that format."));
    }
}

void MainWindow::on_actionNext_Playlist_Item_triggered()
{
    // This code is similar to the row double clicked code...
    on_stopButton_clicked();  // if we're going to the next file in the playlist, stop current playback
    saveCurrentSongSettings();

    // figure out which row is currently selected
    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    int row = -1;
    if (selected.count() == 1) {
        // exactly 1 row was selected (good)
        QModelIndex index = selected.at(0);
        row = index.row();
    }
    else {
        // more than 1 row or no rows at all selected (BAD)
        return;
    }

    int maxRow = ui->songTable->rowCount() - 1;

    // which is the next VISIBLE row?
    int lastVisibleRow = row;
    row = (maxRow < row+1 ? maxRow : row+1); // bump up by 1
    while (ui->songTable->isRowHidden(row) && row < maxRow) {
        // keep bumping, until the next VISIBLE row is found, or we're at the END
        row = (maxRow < row+1 ? maxRow : row+1); // bump up by 1
    }
    if (ui->songTable->isRowHidden(row)) {
        // if we try to go past the end of the VISIBLE rows, stick at the last visible row (which
        //   was the last one we were on.  Well, that's not always true, but this is a quick and dirty
        //   solution.  If I go to a row, select it, and then filter all rows out, and hit one of the >>| buttons,
        //   hilarity will ensue.
        row = lastVisibleRow;
    }
    ui->songTable->selectRow(row); // select new row!

    // load all the UI fields, as if we double-clicked on the new row
    QString pathToMP3 = ui->songTable->item(row,kPathCol)->data(Qt::UserRole).toString();
    QString songTitle = ui->songTable->item(row,kTitleCol)->text();
    // FIX:  This should grab the title from the MP3 metadata in the file itself instead.

    QString songType = ui->songTable->item(row,kTypeCol)->text();

    // must be up here...
    QString pitch = ui->songTable->item(row,kPitchCol)->text();
    QString tempo = ui->songTable->item(row,kTempoCol)->text();

    loadMP3File(pathToMP3, songTitle, songType);

    // must be down here...
    int pitchInt = pitch.toInt();
    ui->pitchSlider->setValue(pitchInt);

    if (tempo != "0") {
        QString tempo2 = tempo.replace("%",""); // get rid of optional "%", slider->setValue will do the right thing
        int tempoInt = tempo2.toInt();
        ui->tempoSlider->setValue(tempoInt);
    }

    if (ui->actionAutostart_playback->isChecked()) {
        on_playButton_clicked();
    }

}

void MainWindow::on_actionPrevious_Playlist_Item_triggered()
{
    // This code is similar to the row double clicked code...
    on_stopButton_clicked();  // if we're going to the next file in the playlist, stop current playback
    saveCurrentSongSettings();

    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    int row = -1;
    if (selected.count() == 1) {
        // exactly 1 row was selected (good)
        QModelIndex index = selected.at(0);
        row = index.row();
    }
    else {
        // more than 1 row or no rows at all selected (BAD)
        return;
    }

    // which is the next VISIBLE row?
    int lastVisibleRow = row;
    row = (row-1 < 0 ? 0 : row-1); // bump backwards by 1

    while (ui->songTable->isRowHidden(row) && row > 0) {
        // keep bumping backwards, until the previous VISIBLE row is found, or we're at the BEGINNING
        row = (row-1 < 0 ? 0 : row-1); // bump backwards by 1
    }
    if (ui->songTable->isRowHidden(row)) {
        // if we try to go past the beginning of the VISIBLE rows, stick at the first visible row (which
        //   was the last one we were on.  Well, that's not always true, but this is a quick and dirty
        //   solution.  If I go to a row, select it, and then filter all rows out, and hit one of the >>| buttons,
        //   hilarity will ensue.
        row = lastVisibleRow;
    }

    ui->songTable->selectRow(row); // select new row!

    // load all the UI fields, as if we double-clicked on the new row
    QString pathToMP3 = ui->songTable->item(row,kPathCol)->data(Qt::UserRole).toString();
    QString songTitle = ui->songTable->item(row,kTitleCol)->text();
    // FIX:  This should grab the title from the MP3 metadata in the file itself instead.

    QString songType = ui->songTable->item(row,kTypeCol)->text();

    // must be up here...
    QString pitch = ui->songTable->item(row,kPitchCol)->text();
    QString tempo = ui->songTable->item(row,kTempoCol)->text();

    loadMP3File(pathToMP3, songTitle, songType);

    // must be down here...
    int pitchInt = pitch.toInt();
    ui->pitchSlider->setValue(pitchInt);

    if (tempo != "0") {
        QString tempo2 = tempo.replace("%",""); // get rid of optional "%", setValue will take care of it
        int tempoInt = tempo.toInt();
        ui->tempoSlider->setValue(tempoInt);
    }

    if (ui->actionAutostart_playback->isChecked()) {
        on_playButton_clicked();
    }
}

void MainWindow::on_previousSongButton_clicked()
{
    on_actionPrevious_Playlist_Item_triggered();
}

void MainWindow::on_nextSongButton_clicked()
{
    on_actionNext_Playlist_Item_triggered();
}

void MainWindow::on_songTable_itemSelectionChanged()
{
    // When item selection is changed, enable Next/Previous song buttons,
    //   if at least one item in the table is selected.
    //
    // figure out which row is currently selected
    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if (selected.count() == 1) {
        ui->nextSongButton->setEnabled(true);
        ui->previousSongButton->setEnabled(true);
    }
    else {
        ui->nextSongButton->setEnabled(false);
        ui->previousSongButton->setEnabled(false);
    }

    // ----------------
    int selectedRow = selectedSongRow();  // get current row or -1

    // turn them all OFF
    ui->actionAt_TOP->setEnabled(false);
    ui->actionAt_BOTTOM->setEnabled(false);
    ui->actionUP_in_Playlist->setEnabled(false);
    ui->actionDOWN_in_Playlist->setEnabled(false);
    ui->actionRemove_from_Playlist->setEnabled(false);

    if (selectedRow != -1) {
        // if a single row was selected
        QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
        int currentNumberInt = currentNumberText.toInt();
        int playlistItemCount = PlaylistItemCount();

        // this function is always called when playlist items are added or deleted, so
        // figure out whether save/save as are enabled here
        linesInCurrentPlaylist = playlistItemCount;
//        qDebug() << "songTableItemSelectionChanged:" << playlistItemCount;
        if (playlistItemCount > 0) {
            ui->actionSave->setEnabled(true);
            ui->actionSave_As->setEnabled(true);
        }

        if (currentNumberText == "") {
            // if not in a Playlist then we can add it at Top or Bottom, that's it.
            ui->actionAt_TOP->setEnabled(true);
            ui->actionAt_BOTTOM->setEnabled(true);
        } else {
            ui->actionRemove_from_Playlist->setEnabled(true);  // can remove it

            // current item is on the Playlist already
            if (playlistItemCount > 1) {
                // more than one item on the list
                if (currentNumberInt == 1) {
//                     it's the first item, and there's more than one item on the list, so moves make sense
                    ui->actionAt_BOTTOM->setEnabled(true);
                    ui->actionDOWN_in_Playlist->setEnabled(true);
                } else if (currentNumberInt == playlistItemCount) {
                    // it's the last item, and there's more than one item on the list, so moves make sense
                    ui->actionAt_TOP->setEnabled(true);
                    ui->actionUP_in_Playlist->setEnabled(true);
                } else {
                    // it's somewhere in the middle, and there's more than one item on the list, so moves make sense
                    ui->actionAt_TOP->setEnabled(true);
                    ui->actionAt_BOTTOM->setEnabled(true);
                    ui->actionUP_in_Playlist->setEnabled(true);
                    ui->actionDOWN_in_Playlist->setEnabled(true);
                }
            } else {
                // One item on the playlist, and this is it.
                // Can't move up/down or to top/bottom.
                // Can remove it, though.
            }
        }
    }
}

void MainWindow::on_actionClear_Playlist_triggered()
{
    startLongSongTableOperation("on_actionClear_Playlist_triggered");  // for performance, hide and sorting off

    // Iterate over the songTable
    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        theItem->setText(""); // clear out the current list

        // let's intentionally NOT clear the pitches.  They are persistent within a session.
        // let's intentionally NOT clear the tempos.  They are persistent within a session.
    }

    linesInCurrentPlaylist = 0;
    ui->actionSave->setDisabled(true);
    ui->actionSave_As->setDisabled(true);

    sortByDefaultSortOrder();

    stopLongSongTableOperation("on_actionClear_Playlist_triggered");  // for performance, sorting on again and show

    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled
}

// ---------------------------------------------------------
void MainWindow::showInFinderOrExplorer(QString filePath)
{
// From: http://lynxline.com/show-in-finder-show-in-explorer/
#if defined(Q_OS_MAC)
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \""+filePath+"\"";
    args << "-e";
    args << "end tell";
    QProcess::startDetached("osascript", args);
#endif

#if defined(Q_OS_WIN)
    QStringList args;
    args << "/select," << QDir::toNativeSeparators(filePath);
    QProcess::startDetached("explorer", args);
#endif

#ifdef Q_OS_LINUX
    QStringList args;
    args << QFileInfo(filePath).absoluteDir().canonicalPath();
    QProcess::startDetached("xdg-open", args);
#endif // ifdef Q_OS_LINUX
}

// ---------------------------------------------------------
//int MainWindow::getInputVolume()
//{
//#if defined(Q_OS_MAC)
//    QProcess getVolumeProcess;
//    QStringList args;
//    args << "-e";
//    args << "set ivol to input volume of (get volume settings)";

//    getVolumeProcess.start("osascript", args);
//    getVolumeProcess.waitForFinished(); // sets current thread to sleep and waits for pingProcess end
//    QString output(getVolumeProcess.readAllStandardOutput());

//    int vol = output.trimmed().toInt();

//    return(vol);
//#endif

//#if defined(Q_OS_WIN)
//    return(-1);
//#endif

//#ifdef Q_OS_LINUX
//    return(-1);
//#endif
//}

//void MainWindow::setInputVolume(int newVolume)
//{
//#if defined(Q_OS_MAC)
//    if (newVolume != -1) {
//        QProcess getVolumeProcess;
//        QStringList args;
//        args << "-e";
//        args << "set volume input volume " + QString::number(newVolume);

//        getVolumeProcess.start("osascript", args);
//        getVolumeProcess.waitForFinished();
//        QString output(getVolumeProcess.readAllStandardOutput());
//    }
//#else
//    Q_UNUSED(newVolume)
//#endif

//#if defined(Q_OS_WIN)
//#endif

//#ifdef Q_OS_LINUX
//#endif
//}

//void MainWindow::muteInputVolume()
//{
//    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
//    if (!prefsManager.GetenableAutoMicsOff()) {
//        return;
//    }

//    int vol = getInputVolume();
//    if (vol > 0) {
//        // if not already muted, save the current volume (for later restore)
//        currentInputVolume = vol;
//        setInputVolume(0);
//    }
//}

//void MainWindow::unmuteInputVolume()
//{
//    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
//    if (!prefsManager.GetenableAutoMicsOff()) {
//        return;
//    }

//    int vol = getInputVolume();
//    if (vol > 0) {
//        // the user has changed it, so don't muck with it!
//    } else {
//        setInputVolume(currentInputVolume);     // restore input from the mics
//    }
//}

// ----------------------------------------------------------------------
int MainWindow::selectedSongRow() {
    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    int row = -1;
    if (selected.count() == 1) {
        // exactly 1 row was selected (good)
        QModelIndex index = selected.at(0);
        row = index.row();
    } // else more than 1 row or no rows, just return -1
    return row;
}

// ----------------------------------------------------------------------
int MainWindow::PlaylistItemCount() {
    int playlistItemCount = 0;

    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        QString playlistIndex = theItem->text();  // this is the playlist #
        if (playlistIndex != "") {
            playlistItemCount++;
        }
    }

    return (playlistItemCount);
}

// ----------------------------------------------------------------------
void MainWindow::PlaylistItemToTop() {

    int selectedRow = selectedSongRow();  // get current row or -1

    if (selectedRow == -1) {
        return;
    }

    QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
    int currentNumberInt = currentNumberText.toInt();

    if (currentNumberText == "") {
        // add to list, and make it the #1

        // Iterate over the entire songTable, incrementing every item
        // TODO: turn off sorting
        for (int i=0; i<ui->songTable->rowCount(); i++) {
            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
            QString playlistIndex = theItem->text();  // this is the playlist #
            if (playlistIndex != "") {
                // if a # was set, increment it
                QString newIndex = QString::number(playlistIndex.toInt()+1);
                ui->songTable->item(i,kNumberCol)->setText(newIndex);
            }
        }

        ui->songTable->item(selectedRow, kNumberCol)->setText("1");  // this one is the new #1
        // TODO: turn on sorting again

    } else {
        // already on the list
        // Iterate over the entire songTable, incrementing items BELOW this item
        // TODO: turn off sorting
        for (int i=0; i<ui->songTable->rowCount(); i++) {
            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
            QString playlistIndexText = theItem->text();  // this is the playlist #
            if (playlistIndexText != "") {
                int playlistIndexInt = playlistIndexText.toInt();
                if (playlistIndexInt < currentNumberInt) {
                    // if a # was set and less, increment it
                    QString newIndex = QString::number(playlistIndexInt+1);
//                    qDebug() << "old, new:" << playlistIndexText << newIndex;
                    ui->songTable->item(i,kNumberCol)->setText(newIndex);
                }
            }
        }
        // and then set this one to #1
        ui->songTable->item(selectedRow, kNumberCol)->setText("1");  // this one is the new #1
    }

    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled
}

// --------------------------------------------------------------------
void MainWindow::PlaylistItemToBottom() {
    int selectedRow = selectedSongRow();  // get current row or -1

    if (selectedRow == -1) {
        return;
    }

    QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
    int currentNumberInt = currentNumberText.toInt();

    int playlistItemCount = PlaylistItemCount();  // how many items in the playlist right now?

    if (currentNumberText == "") {
        // add to list, and make it the bottom

        // Iterate over the entire songTable, not touching every item
        // TODO: turn off sorting
        ui->songTable->item(selectedRow, kNumberCol)->setText(QString::number(playlistItemCount+1));  // this one is the new #LAST
        // TODO: turn on sorting again

    } else {
        // already on the list
        // Iterate over the entire songTable, decrementing items ABOVE this item
        // TODO: turn off sorting
        for (int i=0; i<ui->songTable->rowCount(); i++) {
            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
            QString playlistIndexText = theItem->text();  // this is the playlist #
            if (playlistIndexText != "") {
                int playlistIndexInt = playlistIndexText.toInt();
                if (playlistIndexInt > currentNumberInt) {
                    // if a # was set and more, decrement it
                    QString newIndex = QString::number(playlistIndexInt-1);
                    ui->songTable->item(i,kNumberCol)->setText(newIndex);
                }
            }
        }
        // and then set this one to #LAST
        ui->songTable->item(selectedRow, kNumberCol)->setText(QString::number(playlistItemCount));  // this one is the new #1
    }
    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled
}

// --------------------------------------------------------------------
void MainWindow::PlaylistItemMoveUp() {
    int selectedRow = selectedSongRow();  // get current row or -1

    if (selectedRow == -1) {
        return;
    }

    QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
    int currentNumberInt = currentNumberText.toInt();

    // Iterate over the entire songTable, find the item just above this one, and move IT down (only)
    // TODO: turn off sorting

    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        QString playlistIndex = theItem->text();  // this is the playlist #
        if (playlistIndex != "") {
            int playlistIndexInt = playlistIndex.toInt();
            if (playlistIndexInt == currentNumberInt - 1) {
                QString newIndex = QString::number(playlistIndex.toInt()+1);
                ui->songTable->item(i,kNumberCol)->setText(newIndex);
            }
        }
    }

    ui->songTable->item(selectedRow, kNumberCol)->setText(QString::number(currentNumberInt-1));  // this one moves UP
    // TODO: turn on sorting again
    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled
}

// --------------------------------------------------------------------
void MainWindow::PlaylistItemMoveDown() {
    int selectedRow = selectedSongRow();  // get current row or -1

    if (selectedRow == -1) {
        return;
    }

    QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
    int currentNumberInt = currentNumberText.toInt();

    // add to list, and make it the bottom

    // Iterate over the entire songTable, find the item just BELOW this one, and move it UP (only)
    // TODO: turn off sorting

    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        QString playlistIndex = theItem->text();  // this is the playlist #
        if (playlistIndex != "") {
            int playlistIndexInt = playlistIndex.toInt();
            if (playlistIndexInt == currentNumberInt + 1) {
                QString newIndex = QString::number(playlistIndex.toInt()-1);
                ui->songTable->item(i,kNumberCol)->setText(newIndex);
            }
        }
    }

    ui->songTable->item(selectedRow, kNumberCol)->setText(QString::number(currentNumberInt+1));  // this one moves UP
    // TODO: turn on sorting again
    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled
}

// --------------------------------------------------------------------
void MainWindow::PlaylistItemRemove() {
    int selectedRow = selectedSongRow();  // get current row or -1

    if (selectedRow == -1) {
        return;
    }

    QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
    int currentNumberInt = currentNumberText.toInt();

    // already on the list
    // Iterate over the entire songTable, decrementing items BELOW this item
    // TODO: turn off sorting
    for (int i=0; i<ui->songTable->rowCount(); i++) {
        QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
        QString playlistIndexText = theItem->text();  // this is the playlist #
        if (playlistIndexText != "") {
            int playlistIndexInt = playlistIndexText.toInt();
            if (playlistIndexInt > currentNumberInt) {
                // if a # was set and more, decrement it
                QString newIndex = QString::number(playlistIndexInt-1);
                ui->songTable->item(i,kNumberCol)->setText(newIndex);
            }
        }
    }
    // and then set this one to #LAST
    ui->songTable->item(selectedRow, kNumberCol)->setText("");  // this one is off the list
    on_songTable_itemSelectionChanged();  // reevaluate which menu items are enabled
}


void MainWindow::on_songTable_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);

    if (ui->songTable->selectionModel()->hasSelection()) {
        QMenu menu(this);

        int selectedRow = selectedSongRow();  // get current row or -1

        if (selectedRow != -1) {
            // if a single row was selected
            QString currentNumberText = ui->songTable->item(selectedRow, kNumberCol)->text();  // get current number
            int currentNumberInt = currentNumberText.toInt();
            int playlistItemCount = PlaylistItemCount();

            if (currentNumberText == "") {
                if (playlistItemCount == 0) {
                    menu.addAction ( "Add to playlist" , this , SLOT (PlaylistItemToTop()) );
                } else {
                    menu.addAction ( "Add to TOP of playlist" , this , SLOT (PlaylistItemToTop()) );
                    menu.addAction ( "Add to BOTTOM of playlist" , this , SLOT (PlaylistItemToBottom()) );
                }
            } else {
                // currently on the playlist
                if (playlistItemCount > 1) {
                    // more than one item
                    if (currentNumberInt == 1) {
                        // already the first item, and there's more than one item on the list, so moves make sense
                        menu.addAction ( "Move DOWN in playlist" , this , SLOT (PlaylistItemMoveDown()) );
                        menu.addAction ( "Move to BOTTOM of playlist" , this , SLOT (PlaylistItemToBottom()) );
                    } else if (currentNumberInt == playlistItemCount) {
                        // already the last item, and there's more than one item on the list, so moves make sense
                        menu.addAction ( "Move to TOP of playlist" , this , SLOT (PlaylistItemToTop()) );
                        menu.addAction ( "Move UP in playlist" , this , SLOT (PlaylistItemMoveUp()) );
                    } else {
                        // somewhere in the middle, and there's more than one item on the list, so moves make sense
                        menu.addAction ( "Move to TOP of playlist" , this , SLOT (PlaylistItemToTop()) );
                        menu.addAction ( "Move UP in playlist" , this , SLOT (PlaylistItemMoveUp()) );
                        menu.addAction ( "Move DOWN in playlist" , this , SLOT (PlaylistItemMoveDown()) );
                        menu.addAction ( "Move to BOTTOM of playlist" , this , SLOT (PlaylistItemToBottom()) );
                    }
                } else {
                    // exactly one item, and this is it.
                }
                // this item is on the playlist, so it can be removed.
                menu.addSeparator();
                menu.addAction ( "Remove from playlist" , this , SLOT (PlaylistItemRemove()) );
            }
        }
        menu.addSeparator();

#if defined(Q_OS_MAC)
        menu.addAction ( "Reveal in Finder" , this , SLOT (revealInFinder()) );
#endif

#if defined(Q_OS_WIN)
        menu.addAction ( "Show in Explorer" , this , SLOT (revealInFinder()) );
#endif

#if defined(Q_OS_LINUX)
        menu.addAction ( "Open containing folder" , this , SLOT (revealInFinder()) );
#endif

        menu.popup(QCursor::pos());
        menu.exec();
    }
}

void MainWindow::revealInFinder()
{
    QItemSelectionModel *selectionModel = ui->songTable->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    int row = -1;
    if (selected.count() == 1) {
        // exactly 1 row was selected (good)
        QModelIndex index = selected.at(0);
        row = index.row();

        QString pathToMP3 = ui->songTable->item(row,kPathCol)->data(Qt::UserRole).toString();
        showInFinderOrExplorer(pathToMP3);
    }
    else {
        // more than 1 row or no rows at all selected (BAD)
    }
}

void MainWindow::columnHeaderSorted(int logicalIndex, Qt::SortOrder order)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(order)
    adjustFontSizes();  // when sort triangle is added, columns need to adjust width a bit...
}


void MainWindow::columnHeaderResized(int logicalIndex, int /* oldSize */, int newSize)
{
    int x1,y1,w1,h1;
    int x2,y2,w2,h2;
    int x3,y3,w3,h3;

    switch (logicalIndex) {
        case 0: // #
            // FIX: there's a bug here if the # column width is changed.  Qt doesn't seem to keep track of
            //  the correct size of the # column thereafter.  This is particularly visible on Win10, but it's
            //  also present on Mac OS X (Sierra).

            // # column width is not tracked by Qt (BUG), so we have to do it manually
            col0_width = newSize;

            x1 = newSize + 14;
            y1 = ui->typeSearch->y();
            w1 = ui->songTable->columnWidth(1) - 5;
            h1 = ui->typeSearch->height();
            ui->typeSearch->setGeometry(x1,y1,w1,h1);

            x2 = x1 + w1 + 6 - 1;
            y2 = ui->labelSearch->y();
            w2 = ui->songTable->columnWidth(2) - 5;
            h2 = ui->labelSearch->height();
            ui->labelSearch->setGeometry(x2,y2,w2,h2);

            x3 = x2 + w2 + 6;
            y3 = ui->titleSearch->y();
            w3 = ui->songTable->width() - ui->clearSearchButton->width() - x3;
            h3 = ui->titleSearch->height();
            ui->titleSearch->setGeometry(x3,y3,w3,h3);

            break;

        case 1: // Type
            x1 = col0_width + 35;
            y1 = ui->typeSearch->y();
            w1 = newSize - 4;
            h1 = ui->typeSearch->height();
            ui->typeSearch->setGeometry(x1,y1,w1,h1);
            ui->typeSearch->setFixedWidth(w1);

            x2 = x1 + w1 + 6;
            y2 = ui->labelSearch->y();
            w2 = ui->songTable->columnWidth(2) - 6;
            h2 = ui->labelSearch->height();
            ui->labelSearch->setGeometry(x2,y2,w2,h2);
            ui->labelSearch->setFixedWidth(w2);

            x3 = x2 + w2 + 6;
            y3 = ui->titleSearch->y();
            w3 = ui->songTable->width() - ui->clearSearchButton->width() - x3 + 17;
            h3 = ui->titleSearch->height();
            ui->titleSearch->setGeometry(x3,y3,w3,h3);
            break;

        case 2: // Label
            x1 = ui->typeSearch->x();
            y1 = ui->typeSearch->y();
            w1 = ui->typeSearch->width();
            h1 = ui->typeSearch->height();

            x2 = x1 + w1 + 6;
            y2 = ui->labelSearch->y();
            w2 = newSize - 6;
            h2 = ui->labelSearch->height();
            ui->labelSearch->setGeometry(x2,y2,w2,h2);
            ui->labelSearch->setFixedWidth(w2);

            x3 = x2 + w2 + 6;
            y3 = ui->titleSearch->y();
            w3 = ui->songTable->width() - ui->clearSearchButton->width() - x3 + 17;
            h3 = ui->titleSearch->height();
            ui->titleSearch->setGeometry(x3,y3,w3,h3);
            break;

        case 3: // Title
            break;

        default:
            break;
    }

}

// ----------------------------------------------------------------------
void MainWindow::saveCurrentSongSettings()
{
    if (loadingSong)
        return;

    QString currentSong = ui->nowPlayingLabel->text();

    if (!currentSong.isEmpty()) {
        int pitch = ui->pitchSlider->value();
        int tempo = ui->tempoSlider->value();
        int cuesheetIndex = ui->comboBoxCuesheetSelector->currentIndex();
        QString cuesheetFilename = cuesheetIndex >= 0 ?
            ui->comboBoxCuesheetSelector->itemData(cuesheetIndex).toString()
            : "";

        SongSetting setting;
        setting.setFilename(currentMP3filename);
        setting.setFilenameWithPath(currentMP3filenameWithPath);
        setting.setSongname(currentSong);
        setting.setVolume(currentVolume);
        setting.setPitch(pitch);
        setting.setTempo(tempo);
        setting.setTempoIsPercent(!tempoIsBPM);
        setting.setIntroPos(ui->seekBarCuesheet->GetIntro());
        setting.setOutroPos(ui->seekBarCuesheet->GetOutro());
        setting.setIntroOutroIsTimeBased(false);
        setting.setCuesheetName(cuesheetFilename);
        setting.setSongLength((double)(ui->seekBarCuesheet->maximum()));

        setting.setTreble( ui->trebleSlider->value() );
        setting.setBass( ui->bassSlider->value() );
        setting.setMidrange( ui->midrangeSlider->value() );
        setting.setMix( ui->mixSlider->value() );

        if (ui->actionLoop->isChecked()) {
            setting.setLoop( 1 );
        } else {
            setting.setLoop( -1 );
        }

        songSettings.saveSettings(currentMP3filenameWithPath,
                                  setting);

        if (ui->checkBoxAutoSaveLyrics->isChecked())
        {
            writeCuesheet(cuesheetFilename);
        }
    }


}

void MainWindow::loadSettingsForSong(QString songTitle)
{
    int pitch = ui->pitchSlider->value();
    int tempo = ui->tempoSlider->value();
    int volume = ui->volumeSlider->value();
    double intro = ui->seekBarCuesheet->GetIntro();
    double outro = ui->seekBarCuesheet->GetOutro();
    QString cuesheetName = "";

    SongSetting settings;
    settings.setFilename(currentMP3filename);
    settings.setFilenameWithPath(currentMP3filenameWithPath);
    settings.setSongname(songTitle);
    settings.setVolume(volume);
    settings.setPitch(pitch);
    settings.setTempo(tempo);
    settings.setIntroPos(intro);
    settings.setOutroPos(outro);

    if (songSettings.loadSettings(currentMP3filenameWithPath,
                                  settings))
    {
        if (settings.isSetPitch()) { pitch = settings.getPitch(); }
        if (settings.isSetTempo()) { tempo = settings.getTempo(); }
        if (settings.isSetVolume()) { volume = settings.getVolume(); }
        if (settings.isSetIntroPos()) { intro = settings.getIntroPos(); }
        if (settings.isSetOutroPos()) { outro = settings.getOutroPos(); }
        if (settings.isSetCuesheetName()) { cuesheetName = settings.getCuesheetName(); } // ADDED *****

        // double length = (double)(ui->seekBarCuesheet->maximum());  // This is not correct, results in non-round-tripping
        double length = cBass.FileLength;  // This seems to work better, and round-tripping looks like it is working now.
        if (settings.isSetIntroOutroIsTimeBased() && settings.getIntroOutroIsTimeBased())
        {
            intro = intro / length;
            outro = outro / length;
        }

        ui->pitchSlider->setValue(pitch);
        ui->tempoSlider->setValue(tempo);
        ui->volumeSlider->setValue(volume);
        ui->seekBarCuesheet->SetIntro(intro);
        ui->seekBarCuesheet->SetOutro(outro);

        QTime iTime = QTime(0,0,0,0).addMSecs((int)(1000.0*intro*length+0.5));
        QTime oTime = QTime(0,0,0,0).addMSecs((int)(1000.0*outro*length+0.5));
//        qDebug() << "loadSettingsForSong: " << iTime << ", " << oTime;
        ui->dateTimeEditIntroTime->setTime(iTime); // milliseconds
        ui->dateTimeEditOutroTime->setTime(oTime);

        if (cuesheetName.length() > 0)
        {
            for (int i = 0; i < ui->comboBoxCuesheetSelector->count(); ++i)
            {
                QString itemName = ui->comboBoxCuesheetSelector->itemData(i).toString();
                if (itemName == cuesheetName)
                {
                    ui->comboBoxCuesheetSelector->setCurrentIndex(i);
                    break;
                }
            }
        }
        if (settings.isSetTreble())
        {
            ui->trebleSlider->setValue(settings.getTreble() );
        }
        else
        {
            ui->trebleSlider->setValue(0) ;
        }
        if (settings.isSetBass())
        {
            ui->bassSlider->setValue( settings.getBass() );
        }
        else
        {
            ui->bassSlider->setValue(0);
        }
        if (settings.isSetMidrange())
        {
            ui->midrangeSlider->setValue( settings.getMidrange() );
        }
        else
        {
            ui->midrangeSlider->setValue(0);
        }
        if (settings.isSetMix())
        {
            ui->mixSlider->setValue( settings.getMix() );
        }
        else
        {
            ui->mixSlider->setValue(0);
        }

        // Looping is similar to Mix, but it's a bit more complicated:
        //   If the DB says +1, turn on loop.
        //   If the DB says -1, turn looping off.
        //   otherwise, the DB says "NULL", so use the default that we currently have (patter = loop).
        if (settings.isSetLoop())
        {
            if (settings.getLoop() == -1) {
                on_loopButton_toggled(false);
            } else if (settings.getLoop() == 1) {
                on_loopButton_toggled(true);
            } else {
                // DO NOTHING
            }
        }

    }
    else
    {
        ui->trebleSlider->setValue(0);
        ui->bassSlider->setValue(0);
        ui->midrangeSlider->setValue(0);
        ui->mixSlider->setValue(0);
    }
}

// ------------------------------------------------------------------------------------------
QString MainWindow::loadLyrics(QString MP3FileName)
{
    QString USLTlyrics;

    MPEG::File mp3file(MP3FileName.toStdString().c_str());
    ID3v2::Tag *id3v2tag = mp3file.ID3v2Tag(true);

    ID3v2::FrameList::ConstIterator it = id3v2tag->frameList().begin();
    for (; it != id3v2tag->frameList().end(); it++)
    {
        if ((*it)->frameID() == "SYLT")
        {
//            qDebug() << "LOAD LYRICS -- found an SYLT frame!";
        }

        if ((*it)->frameID() == "USLT")
        {
//            qDebug() << "LOAD LYRICS -- found a USLT frame!";

            ID3v2::UnsynchronizedLyricsFrame* usltFrame = (ID3v2::UnsynchronizedLyricsFrame*)(*it);
            USLTlyrics = usltFrame->text().toCString();
        }
    }
//    qDebug() << "Got lyrics:" << USLTlyrics;
    return (USLTlyrics);
}

// ------------------------------------------------------------------------
QString MainWindow::txtToHTMLlyrics(QString text, QString filePathname) {
    Q_UNUSED(filePathname)

//    QStringList pieces = filePathname.split( "/" );
//    pieces.pop_back(); // get rid of actual filename, keep the path
//    QString filedir = pieces.join("/"); // FIX: MAC SPECIFIC?

//    QString css("");
//    bool fileIsOpen = false;
//    QFile f1(filedir + "/cuesheet2.css");  // This is the SqView convention for a CSS file
//    if ( f1.open(QIODevice::ReadOnly | QIODevice::Text)) {
//        // if there's a "cuesheet2.css" file in the same directory as the .txt file,
//        //   then we're going to embed it into the HTML representation of the .txt file,
//        //   so that the font preferences therein apply.
//        fileIsOpen = true;
//        QTextStream in(&f1);
//        css = in.readAll();  // read the entire CSS file, if it exists
//    }

    // get internal CSS file (we no longer let users change it in individual folders)
    QString css = getResourceFile("cuesheet2.css");

    text = text.toHtmlEscaped();  // convert ">" to "&gt;" etc
    text = text.replace(QRegExp("[\r|\n]"),"<br/>\n");

    QString HTML;
    HTML += "<HTML>\n";
    HTML += "<HEAD><STYLE>" + css + "</STYLE></HEAD>\n";
    HTML += "<BODY>\n" + text + "</BODY>\n";
    HTML += "</HTML>\n";

//    if (fileIsOpen) {
//        f1.close();
//    }
    return(HTML);
}

// ----------------------------------------------------------------------------
QStringList MainWindow::getCurrentVolumes() {

    QStringList volumeDirList;

#if defined(Q_OS_MAC)
    foreach (const QStorageInfo &storageVolume, QStorageInfo::mountedVolumes()) {
            if (storageVolume.isValid() && storageVolume.isReady()) {
            volumeDirList.append(storageVolume.name());}
    }
#endif

#if defined(Q_OS_WIN32)
    foreach (const QFileInfo &fileinfo, QDir::drives()) {
        volumeDirList.append(fileinfo.absoluteFilePath());
    }
#endif

    qSort(volumeDirList);     // always return sorted, for convenient comparisons later

    return(volumeDirList);
}

// ----------------------------------------------------------------------------
void MainWindow::on_newVolumeMounted() {

    QSet<QString> newVolumes, goneVolumes;
    QString newVolume, goneVolume;

    guestMode = "both"; // <-- replace, don't merge (use "both" for merge, "guest" for just guest's music)

    if (lastKnownVolumeList.length() < newVolumeList.length()) {
        // ONE OR MORE VOLUMES APPEARED
        //   ONLY LOOK AT THE LAST ONE IN THE LIST THAT'S NEW (if more than 1)
        newVolumes = newVolumeList.toSet().subtract(lastKnownVolumeList.toSet());
        foreach (const QString &item, newVolumes) {
            newVolume = item;  // first item is the volume added
        }

#if defined(Q_OS_MAC)
        guestVolume = newVolume;  // e.g. MIKEPOGUE
        guestRootPath = "/Volumes/" + guestVolume + "/";  // this is where to search for a Music Directory
        QApplication::beep();  // beep only on MAC OS X (Win32 already beeps by itself)
#endif

#if defined(Q_OS_WIN32)
        guestVolume = newVolume;            // e.g. E:
        guestRootPath = newVolume + "\\";   // this is where to search for a Music Directory
#endif

        // We do it this way, so that the name of the root directory is case insensitive (squareDanceMusic, squaredancemusic, etc.)
        QDir newVolumeRootDir(guestRootPath);
        newVolumeRootDir.setFilter(QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

        QDirIterator it(newVolumeRootDir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        QString d;
        bool foundSquareDeskMusicDir = false;
        QString foundDirName;
        while(it.hasNext()) {
            d = it.next();
            // If alias, try to follow it.
            QString resolvedFilePath = it.fileInfo().symLinkTarget(); // path with the symbolic links followed/removed
            if (resolvedFilePath == "") {
                // If NOT alias, then use the original fileName
                resolvedFilePath = d;
            }

            QFileInfo fi(d);
            QStringList section = fi.canonicalFilePath().split("/");  // expand
            if (section.length() >= 1) {
                QString dirName = section[section.length()-1];

                if (dirName.compare("squaredancemusic",Qt::CaseInsensitive) == 0) { // exact match, but case-insensitive
                    foundSquareDeskMusicDir = true;
                    foundDirName = dirName;
                    break;  // break out of the for loop when we find first directory that matches "squaredancemusic"
                }
            } else {
                continue;
            }
        }

        if (!foundSquareDeskMusicDir) {
            return;  // if we didn't find anything, just return
        }

        guestRootPath += foundDirName;  // e.g. /Volumes/MIKE/squareDanceMusic, or E:\SquareDanceMUSIC

        ui->statusBar->showMessage("SCANNING GUEST VOLUME: " + newVolume);
        QThread::sleep(1);  // FIX: not sure this is needed, but it sometimes hangs if not used, on first mount of a flash drive.

        findMusic(musicRootPath, guestRootPath, guestMode, false);  // get the filenames from the guest's directories
    } else if (lastKnownVolumeList.length() > newVolumeList.length()) {
        // ONE OR MORE VOLUMES WENT AWAY
        //   ONLY LOOK AT THE LAST ONE IN THE LIST THAT'S GONE
        goneVolumes = lastKnownVolumeList.toSet().subtract(newVolumeList.toSet());
        foreach (const QString &item, goneVolumes) {
            goneVolume = item;
        }

        ui->statusBar->showMessage("REMOVING GUEST VOLUME: " + goneVolume);
        QApplication::beep();  // beep on MAC OS X and Win32
        QThread::sleep(1);  // FIX: not sure this is needed.

        guestMode = "main";
        guestRootPath = "";
        findMusic(musicRootPath, "", guestMode, false);  // get the filenames from the user's directories
    } else {
//        qDebug() << "No volume added/lost by the time we got here. I give up. :-(";
        return;
    }

    lastKnownVolumeList = newVolumeList;

    loadMusicList(); // and filter them into the songTable
}

void MainWindow::on_warningLabel_clicked() {
    analogClock->resetPatter();
}

void MainWindow::on_warningLabelCuesheet_clicked() {
    // this one is clickable, too!
    on_warningLabel_clicked();
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    if (ui->tabWidget->tabText(index) == "SD"
        || ui->tabWidget->tabText(index) == "SD 2") {
        // SD Tab ---------------

        ui->lineEditSDInput->setFocus();
//        ui->actionSave_Lyrics->setDisabled(true);
//        ui->actionSave_Lyrics_As->setDisabled(true);
//        ui->actionPrint_Lyrics->setDisabled(true);

        ui->actionFilePrint->setEnabled(true); // FIX: when sequences can be printed
        ui->actionFilePrint->setText("Print SD Sequence...");

        ui->actionSave->setDisabled(true);      // sequences can't be saved (no default filename to save to)
        ui->actionSave->setText("Save SD Sequence");        // greyed out
        ui->actionSave_As->setDisabled(false);  // sequences can be saved
        ui->actionSave_As->setText("Save SD Sequence As...");
    } else if (ui->tabWidget->tabText(index) == "Lyrics" || ui->tabWidget->tabText(index) == "*Lyrics" ||
               ui->tabWidget->tabText(index) == "Patter" || ui->tabWidget->tabText(index) == "*Patter") {
        // Lyrics Tab ---------------
//        ui->actionPrint_Lyrics->setDisabled(false);
//        ui->actionSave_Lyrics->setDisabled(false);      // TODO: enable only when we've made changes
//        ui->actionSave_Lyrics_As->setDisabled(false);   // always enabled, because we can always save as a different name
        ui->actionFilePrint->setDisabled(false);

        ui->actionSave->setEnabled(hasLyrics);      // lyrics/patter can be saved when there are lyrics to save
        ui->actionSave_As->setEnabled(hasLyrics);   // lyrics/patter can be saved as when there are lyrics to save

        if (ui->tabWidget->tabText(index) == "Lyrics" || ui->tabWidget->tabText(index) == "*Lyrics") {
            ui->actionSave->setText("Save Lyrics"); // but greyed out, until modified
            ui->actionSave_As->setText("Save Lyrics As...");  // greyed out until modified

            ui->actionFilePrint->setText("Print Lyrics...");
        } else {
            ui->actionSave->setText("Save Patter"); // but greyed out, until modified
            ui->actionSave_As->setText("Save Patter As...");  // greyed out until modified

            ui->actionFilePrint->setText("Print Patter...");
        }
    } else if (ui->tabWidget->tabText(index) == "Music Player") {
        ui->actionSave->setEnabled(linesInCurrentPlaylist != 0);      // playlist can be saved if there are >0 lines
        ui->actionSave->setText("Save Playlist"); // but greyed out, until there is a playlist
        ui->actionSave_As->setEnabled(linesInCurrentPlaylist != 0);  // playlist can be saved as if there are >0 lines
        ui->actionSave_As->setText("Save Playlist As...");  // greyed out until modified

        ui->actionFilePrint->setEnabled(true);
        ui->actionFilePrint->setText("Print Playlist...");
    } else  {
        // dance programs, reference
//        ui->actionPrint_Lyrics->setDisabled(true);
//        ui->actionSave_Lyrics->setDisabled(true);
//        ui->actionSave_Lyrics_As->setDisabled(true);

        ui->actionSave->setDisabled(true);      // dance programs etc can't be saved
        ui->actionSave->setText("Save"); // but greyed out
        ui->actionSave_As->setDisabled(true);  // patter can be saved as
        ui->actionSave_As->setText("Save As...");  // greyed out

        ui->actionFilePrint->setDisabled(true);
    }

    microphoneStatusUpdate();
}

void MainWindow::microphoneStatusUpdate() {
    bool killVoiceInput(true);

    int index = ui->tabWidget->currentIndex();

    if (ui->tabWidget->tabText(index) == "SD"
        || ui->tabWidget->tabText(index) == "SD 2") {
        if (voiceInputEnabled &&
            currentApplicationState == Qt::ApplicationActive) {
            if (!ps)
                initSDtab();
        }
        if (voiceInputEnabled && ps &&
            currentApplicationState == Qt::ApplicationActive) {


            ui->statusBar->setStyleSheet("color: red");
//            ui->statusBar->showMessage("Microphone enabled for voice input (Voice level: " + currentSDVUILevel.toUpper() + ", Keyboard level: " + currentSDKeyboardLevel.toUpper() + ")");
            ui->statusBar->showMessage("Microphone enabled for voice input (Voice level: " + currentSDVUILevel.toUpper() + ", Keyboard level: " + currentSDKeyboardLevel.toUpper() + ")");
            killVoiceInput = false;
        } else {
            ui->statusBar->setStyleSheet("color: black");
            ui->statusBar->showMessage("Microphone disabled (Voice level: " + currentSDVUILevel.toUpper() + ", Keyboard level: " + currentSDKeyboardLevel.toUpper() + ")");
        }
    } else {
        if (voiceInputEnabled && currentApplicationState == Qt::ApplicationActive) {
            ui->statusBar->setStyleSheet("color: black");
            ui->statusBar->showMessage("Microphone will be enabled for voice input in SD tab");
//            muteInputVolume();                      // disable all input from the mics
        } else {
            ui->statusBar->setStyleSheet("color: black");
            ui->statusBar->showMessage("Microphone disabled");
//            muteInputVolume();                      // disable all input from the mics
        }
    }
    if (killVoiceInput && ps)
    {
        ps->kill();
        delete ps;
        ps = NULL;
    }
}


void MainWindow::readPSStdErr()
{
    QByteArray s(ps->readAllStandardError());
    QString str = QString::fromUtf8(s.data());
}

void MainWindow::readPSData()
{
    // ASR --------------------------------------------
    // pocketsphinx has a valid string, send it to sd
    QByteArray s = ps->readAllStandardOutput();
    QString str = QString::fromUtf8(s.data());
    if (str.startsWith("INFO: "))
        return;

//    qDebug() << "PS str: " << str;

    int index = ui->tabWidget->currentIndex();
    if (!voiceInputEnabled || (currentApplicationState != Qt::ApplicationActive) ||
        ((ui->tabWidget->tabText(index) != "SD" && ui->tabWidget->tabText(index) != "SD 2"))) {
        // if voiceInput is explicitly disabled, or the app is not Active, we're not on the sd tab, then voiceInput is disabled,
        //  and we're going to read the data from PS and just throw it away.
        // This is a cheesy way to do it.  We really should disable the mics somehow.
        return;
    }

    // NLU --------------------------------------------
    // This section does the impedance match between what you can say and the exact wording that sd understands.
    // TODO: put this stuff into an external text file, read in at runtime?
    //
    QString s2 = str.toLower();
    s2.replace("\r\n","\n");  // for Windows PS only, harmless to Mac/Linux
    s2.replace(QRegExp("allocating .* buffers of .* samples each\\n"),"");  // garbage from windows PS only, harmless to Mac/Linux
    s2 = s2.simplified();

    if (s2 == "erase" || s2 == "erase that") {
        ui->lineEditSDInput->clear();
    }
    else
    {
        ui->lineEditSDInput->setText(s2);
        on_lineEditSDInput_returnPressed();
    }
}

void MainWindow::pocketSphinx_errorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error);
}
void MainWindow::pocketSphinx_started()
{
}

void MainWindow::initReftab() {
    numWebviews = 0;
    
    documentsTab = new QTabWidget();

    QString referencePath = musicRootPath + "/reference";
    QDirIterator it(referencePath, QDir::NoDot | QDir::NoDotDot | QDir::Dirs | QDir::Files);
    while (it.hasNext()) {
        QString filename = it.next();
//        qDebug() << filename;

        QFileInfo info1(filename);
        QString tabname;
        bool HTMLfolderExists = false;
        QString whichHTM = "";
        if (info1.isDir()) {
            QFileInfo info2(filename + "/index.html");
            if (info2.exists()) {
//                qDebug() << "    FOUND INDEX.HTML";
                tabname = filename.split("/").last();
                HTMLfolderExists = true;
                whichHTM = "/index.html";
            } else {
                QFileInfo info3(filename + "/index.htm");
                if (info3.exists()) {
//                    qDebug() << "    FOUND INDEX.HTM";
                    tabname = filename.split("/").last();
                    HTMLfolderExists = true;
                    whichHTM = "/index.htm";
                }
            }
            if (HTMLfolderExists) {
#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
                webview[numWebviews] = new QWebEngineView();
#else
                webview[numWebviews] = new QWebView();
#endif                
                QString indexFileURL = "file://" + filename + whichHTM;
//                qDebug() << "    indexFileURL:" << indexFileURL;
                webview[numWebviews]->setUrl(QUrl(indexFileURL));
                documentsTab->addTab(webview[numWebviews], tabname);
                numWebviews++;
            }
        } else if (filename.endsWith(".txt") &&   // ends in .txt, AND
                   QRegExp("reference/[a-zA-Z0-9]+\\.[a-zA-Z0-9' ]+\\.txt$", Qt::CaseInsensitive).indexIn(filename) == -1) {  // is not a Dance Program file in /reference
//                qDebug() << "    FOUND TXT FILE";
                tabname = filename.split("/").last().remove(QRegExp(".txt$"));
//                qDebug() << "    tabname:" << tabname;

#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
                webview[numWebviews] = new QWebEngineView();
#else
                webview[numWebviews] = new QWebView();
#endif                
                QString indexFileURL = "file://" + filename;
//                qDebug() << "    indexFileURL:" << indexFileURL;
                webview[numWebviews]->setUrl(QUrl(indexFileURL));
                documentsTab->addTab(webview[numWebviews], tabname);
                numWebviews++;
        } else if (filename.endsWith(".pdf")) {
//                qDebug() << "PDF FILE DETECTED:" << filename;

                QString app_path = qApp->applicationDirPath();
                auto url = QUrl::fromLocalFile(app_path+"/minified/web/viewer.html");  // point at the viewer
//                qDebug() << "    Viewer URL:" << url;

                QDir dir(app_path+"/minified/web/");
                QString pdf_path = dir.relativeFilePath(filename);  // point at the file to be viewed (relative!)
//                qDebug() << "    pdf_path: " << pdf_path;

#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
                Communicator *m_communicator = new Communicator(this);
                m_communicator->setUrl(pdf_path);

                webview[numWebviews] = new QWebEngineView();
                QWebChannel * channel = new QWebChannel(this);
                channel->registerObject(QStringLiteral("communicator"), m_communicator);
                webview[numWebviews]->page()->setWebChannel(channel);

                webview[numWebviews]->load(url);
#else
                webview[numWebviews] = new QWebView();
                webview[numWebviews]->setUrl(url);
#endif                


//                QString indexFileURL = "file://" + filename;
//                qDebug() << "    indexFileURL:" << indexFileURL;

//                webview[numWebviews]->setUrl(QUrl(pdf_path));
                QFileInfo fInfo(filename); 
                documentsTab->addTab(webview[numWebviews], fInfo.baseName());
                numWebviews++;
        }

    } // while iterating through <musicRoot>/reference

    ui->refGridLayout->addWidget(documentsTab, 0,1);
}

void MainWindow::initSDtab() {
//    console->setFixedHeight(150);

    // POCKET_SPHINX -------------------------------------------
    //    WHICH=5365
    //    pocketsphinx_continuous -dict $WHICH.dic -lm $WHICH.lm -inmic yes
    // MAIN CMU DICT: /usr/local/Cellar/cmu-pocketsphinx/HEAD-584be6e/share/pocketsphinx/model/en-us
    // TEST DIR: /Users/mpogue/Documents/QtProjects/SquareDeskPlayer/build-SquareDesk-Desktop_Qt_5_7_0_clang_64bit-Debug/test123/SquareDeskPlayer.app/Contents/MacOS
    // TEST PS MANUALLY: pocketsphinx_continuous -dict 5365a.dic -jsgf plus.jsgf -inmic yes -hmm ../models/en-us
    //   also try: -remove_noise yes, as per http://stackoverflow.com/questions/25641154/noise-reduction-before-pocketsphinx-reduces-recognition-accuracy
    // TEST SD MANUALLY: ./sd
    unsigned int whichModel = 5365;
#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
    QString appDir = QCoreApplication::applicationDirPath() + "/";  // this is where the actual ps executable is
    QString pathToPS = appDir + "pocketsphinx_continuous";
#if defined(Q_OS_WIN32)
    pathToPS += ".exe";   // executable has a different name on Win32
#endif

#else /* must be (Q_OS_LINUX) */
    QString pathToPS = "pocketsphinx_continuous";
#endif
    // NOTE: <whichmodel>a.dic and <VUIdanceLevel>.jsgf MUST be in the same directory.
    QString pathToDict = QString::number(whichModel) + "a.dic";
    QString pathToJSGF = currentSDVUILevel + ".jsgf";

#if defined(Q_OS_MAC)
    // The acoustic models are one level up in the models subdirectory on MAC
    QString pathToHMM  = "../models/en-us";
#endif
#if defined(Q_OS_WIN32)
    // The acoustic models are at the same level, but in the models subdirectory on MAC
    QString pathToHMM  = "models/en-us";
#endif
#if defined(Q_OS_LINUX)
    QString pathToHMM = "../pocketsphinx/binaries/win32/models/en-us/";
#endif

    QStringList PSargs;
    PSargs << "-dict" << pathToDict     // pronunciation dictionary
           << "-jsgf" << pathToJSGF     // language model
           << "-inmic" << "yes"         // use the built-in microphone
           << "-remove_noise" << "yes"  // try turning on PS noise reduction
           << "-hmm" << pathToHMM;      // the US English acoustic model (a bunch of files) is in ../models/en-us

    ps = new QProcess(Q_NULLPTR);

//    qDebug() << "PS start: " << pathToPS << PSargs;

    ps->setWorkingDirectory(QCoreApplication::applicationDirPath()); // NOTE: nothing will be written here
//    ps->setProcessChannelMode(QProcess::MergedChannels);
//    ps->setReadChannel(QProcess::StandardOutput);
    connect(ps, SIGNAL(readyReadStandardOutput()),
            this, SLOT(readPSData()));                 // output data from ps
    connect(ps, SIGNAL(readyReadStandardError()),
            this, SLOT(readPSStdErr()));                 // output data from ps
    connect(ps, SIGNAL(errorOccurred(QProcess::ProcessError)),
            this, SLOT(pocketSphinx_errorOccurred(QProcess::ProcessError)));
    connect(ps, SIGNAL(started()),
            this, SLOT(pocketSphinx_started()));
    ps->start(pathToPS, PSargs);

    bool startedStatus = ps->waitForStarted();
    if (!startedStatus)
    {
        delete ps;
        ps = NULL;
    }

    // SD -------------------------------------------
    copyrightShown = false;  // haven't shown it once yet
}

void MainWindow::on_actionEnable_voice_input_toggled(bool checked)
{
    if (checked) {
        ui->actionEnable_voice_input->setChecked(true);
        voiceInputEnabled = true;
    }
    else {
        ui->actionEnable_voice_input->setChecked(false);
        voiceInputEnabled = false;
    }

    microphoneStatusUpdate();

    // the Enable Voice Input setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.Setenablevoiceinput(ui->actionEnable_voice_input->isChecked());
}

void MainWindow::on_actionAuto_scroll_during_playback_toggled(bool checked)
{
    if (checked) {
        ui->actionAuto_scroll_during_playback->setChecked(true);
        autoScrollLyricsEnabled = true;
    }
    else {
        ui->actionAuto_scroll_during_playback->setChecked(false);
        autoScrollLyricsEnabled = false;
    }

    // the Enable Auto-scroll during playback setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.Setenableautoscrolllyrics(ui->actionAuto_scroll_during_playback->isChecked());
}

void MainWindow::on_actionAt_TOP_triggered()    // Add > at TOP
{
    PlaylistItemToTop();
}

void MainWindow::on_actionAt_BOTTOM_triggered()  // Add > at BOTTOM
{
    PlaylistItemToBottom();
}

void MainWindow::on_actionRemove_from_Playlist_triggered()
{
    PlaylistItemRemove();
}

void MainWindow::on_actionUP_in_Playlist_triggered()
{
    PlaylistItemMoveUp();
}

void MainWindow::on_actionDOWN_in_Playlist_triggered()
{
    PlaylistItemMoveDown();
}

void MainWindow::on_actionStartup_Wizard_triggered()
{
    StartupWizard wizard;
    int dialogCode = wizard.exec();

    if(dialogCode == QDialog::Accepted) {
        // must setup internal variables, from updated Preferences..
        PreferencesManager prefsManager; // Will be using application information for correct location of your settings

        musicRootPath = prefsManager.GetmusicPath();

        QString value;
        value = prefsManager.GetMusicTypeSinging();
        songTypeNamesForSinging = value.toLower().split(";", QString::KeepEmptyParts);

        value = prefsManager.GetMusicTypePatter();
        songTypeNamesForPatter = value.toLower().split(";", QString::KeepEmptyParts);

        value = prefsManager.GetMusicTypeExtras();
        songTypeNamesForExtras = value.toLower().split(';', QString::KeepEmptyParts);

        value = prefsManager.GetMusicTypeCalled();
        songTypeNamesForCalled = value.toLower().split(';', QString::KeepEmptyParts);

        // used to store the file paths
        findMusic(musicRootPath,"","main", true);  // get the filenames from the user's directories
        filterMusic(); // and filter them into the songTable
        loadMusicList();

        // install soundFX if not already present
        maybeInstallSoundFX();

        // FIX: When SD directory is changed, we need to kill and restart SD, or SD output will go to the old directory.
        // initSDtab();  // sd directory has changed, so startup everything again.
    }
}

void MainWindow::sortByDefaultSortOrder()
{
    // these must be in "backwards" order to get the right order, which
    //   is that Type is primary, Title is secondary, Label is tertiary
    ui->songTable->sortItems(kLabelCol);  // sort last by label/label #
    ui->songTable->sortItems(kTitleCol);  // sort second by title in alphabetical order
    ui->songTable->sortItems(kTypeCol);   // sort first by type (singing vs patter)
}

void MainWindow::sdActionTriggered(QAction * action) {
//    qDebug() << "***** sdActionTriggered()" << action << action->isChecked();
    action->setChecked(true);  // check the new one
    renderArea->setCoupleColoringScheme(action->text());
    setSDCoupleColoringScheme(action->text());
}

void MainWindow::sdAction2Triggered(QAction * action) {
//    qDebug() << "***** sdAction2Triggered()" << action << action->isChecked();
    action->setChecked(true);  // check the new one
    currentSDKeyboardLevel = action->text().toLower();   // convert to all lower case for SD
    microphoneStatusUpdate();  // update status message, based on new keyboard SD level
}

void MainWindow::airplaneMode(bool turnItOn) {
#if defined(Q_OS_MAC)
    char cmd[100];
    if (turnItOn) {
        sprintf(cmd, "osascript -e 'do shell script \"networksetup -setairportpower en0 off\"'\n");
    } else {
        sprintf(cmd, "osascript -e 'do shell script \"networksetup -setairportpower en0 on\"'\n");
    }
    system(cmd);
#else
    Q_UNUSED(turnItOn)
#endif
}

void MainWindow::on_action_1_triggered()
{
    playSFX("1");
}

void MainWindow::on_action_2_triggered()
{
    playSFX("2");
}

void MainWindow::on_action_3_triggered()
{
    playSFX("3");
}

void MainWindow::on_action_4_triggered()
{
    playSFX("4");
}

void MainWindow::on_action_5_triggered()
{
    playSFX("5");
}

void MainWindow::on_action_6_triggered()
{
    playSFX("6");
}

void MainWindow::playSFX(QString which) {
    QString soundEffectFile;

    if (which.toInt() > 0) {
        soundEffectFile = soundFXarray[which.toInt()-1];
    } else {
        // conversion failed, this is break_end or long_tip.mp3
        soundEffectFile = musicRootPath + "/soundfx/" + which + ".mp3";
    }

    if(QFileInfo(soundEffectFile).exists()) {
        // play sound FX only if file exists...
        cBass.PlayOrStopSoundEffect(which.toInt(),
                                    soundEffectFile.toLocal8Bit().constData());  // convert to C string; defaults to volume 100%
    }
}

void MainWindow::on_actionClear_Recent_List_triggered()
{
    QSettings settings;
    QStringList recentFilePaths;  // empty list

    settings.setValue("recentFiles", recentFilePaths);  // remember the new list
    updateRecentPlaylistMenu();
}

void MainWindow::loadRecentPlaylist(int i) {

    on_stopButton_clicked();  // if we're loading a new PLAYLIST file, stop current playback

    QSettings settings;
    QStringList recentFilePaths = settings.value("recentFiles").toStringList();

    if (i < recentFilePaths.size()) {
        // then we can do it
        QString filename = recentFilePaths.at(i);
        finishLoadingPlaylist(filename);

        addFilenameToRecentPlaylist(filename);
    }
}

void MainWindow::updateRecentPlaylistMenu() {
    QSettings settings;
    QStringList recentFilePaths = settings.value("recentFiles").toStringList();

    int numRecentPlaylists = recentFilePaths.length();
    ui->actionRecent1->setVisible(numRecentPlaylists >=1);
    ui->actionRecent2->setVisible(numRecentPlaylists >=2);
    ui->actionRecent3->setVisible(numRecentPlaylists >=3);
    ui->actionRecent4->setVisible(numRecentPlaylists >=4);

    QString playlistsPath = musicRootPath + "/playlists/";

    switch(numRecentPlaylists) {
        case 4: ui->actionRecent4->setText(QString(recentFilePaths.at(3)).replace(playlistsPath,""));  // intentional fall-thru
        case 3: ui->actionRecent3->setText(QString(recentFilePaths.at(2)).replace(playlistsPath,""));  // intentional fall-thru
        case 2: ui->actionRecent2->setText(QString(recentFilePaths.at(1)).replace(playlistsPath,""));  // intentional fall-thru
        case 1: ui->actionRecent1->setText(QString(recentFilePaths.at(0)).replace(playlistsPath,""));  // intentional fall-thru
        default: break;
    }

    ui->actionClear_Recent_List->setEnabled(numRecentPlaylists > 0);
}

void MainWindow::addFilenameToRecentPlaylist(QString filename) {
    if (!filename.endsWith(".squaredesk/current.m3u")) {  // do not remember the initial persistent playlist
        QSettings settings;
        QStringList recentFilePaths = settings.value("recentFiles").toStringList();

        recentFilePaths.removeAll(filename);  // remove if it exists already
        recentFilePaths.prepend(filename);    // push it onto the front
        while (recentFilePaths.size() > 4) {  // get rid of those that fell off the end
            recentFilePaths.removeLast();
        }

        settings.setValue("recentFiles", recentFilePaths);  // remember the new list
        updateRecentPlaylistMenu();
    }
}

void MainWindow::on_actionRecent1_triggered()
{
    loadRecentPlaylist(0);
}

void MainWindow::on_actionRecent2_triggered()
{
    loadRecentPlaylist(1);
}

void MainWindow::on_actionRecent3_triggered()
{
    loadRecentPlaylist(2);
}

void MainWindow::on_actionRecent4_triggered()
{
    loadRecentPlaylist(3);
}

void MainWindow::on_actionCheck_for_Updates_triggered()
{
    QString latestVersionURL = "https://raw.githubusercontent.com/mpogue2/SquareDesk/master/latest";

    QNetworkAccessManager* manager = new QNetworkAccessManager();

    QUrl murl = latestVersionURL;
    QNetworkReply *reply = manager->get(QNetworkRequest(murl));

    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit())); // just in case
    timer.start(5000);

    loop.exec();

    QByteArray result = reply->readAll();

    if ( reply->error() != QNetworkReply::NoError ) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText("<B>ERROR</B><P>Sorry, the update server is not reachable right now.<P>" + reply->errorString());
        msgBox.exec();
        return;
    }

    // "latest" is of the form "X.Y.Z\n", so trim off the NL
    QString latestVersionNumber = QString(result).trimmed();

    if (latestVersionNumber == VERSIONSTRING) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("<B>You are running the latest version of SquareDesk.</B>");
        msgBox.exec();
    } else {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("<H2>Newer version available</H2>\nYour version of SquareDesk: " + QString(VERSIONSTRING) +
                       "<P>Latest version of SquareDesk: " + latestVersionNumber +
                       "<P><a href=\"https://github.com/mpogue2/SquareDesk/releases\">Download new version</a>");
        msgBox.exec();
    }

}

void MainWindow::maybeInstallSoundFX() {

#if defined(Q_OS_MAC)
    QString pathFromAppDirPathToStarterSet = "/../soundfx";
#elif defined(Q_OS_WIN32)
    // WARNING: untested
    QString pathFromAppDirPathToStarterSet = "/soundfx";
#elif defined(Q_OS_LINUX)
    // WARNING: untested
    QString pathFromAppDirPathToStarterSet = "/soundfx";
#endif

    // Let's make a "soundfx" directory in the Music Directory, if it doesn't exist already
    PreferencesManager prefsManager;
    QString musicDirPath = prefsManager.GetmusicPath();
    QString soundfxDir = musicDirPath + "/soundfx";

    // if the soundfx directory doesn't exist, create it (always, automatically)
    QDir dir(soundfxDir);
    if (!dir.exists()) {
        dir.mkpath(".");  // make it
    }

    // and populate it with the starter set, if it didn't exist already
    // check for break_over.mp3 and copy it in, if it doesn't exist already
    QFile breakOverSound(soundfxDir + "/break_over.mp3");
    if (!breakOverSound.exists()) {
        QString source = QCoreApplication::applicationDirPath() + pathFromAppDirPathToStarterSet + "/break_over.mp3";
        QString destination = soundfxDir + "/break_over.mp3";
        //            qDebug() << "COPY from: " << source << " to " << destination;
        QFile::copy(source, destination);
    }

    // check for long_tip.mp3 and copy it in, if it doesn't exist already
    QFile longTipSound(soundfxDir + "/long_tip.mp3");
    if (!longTipSound.exists()) {
        QString source = QCoreApplication::applicationDirPath() + pathFromAppDirPathToStarterSet + "/long_tip.mp3";
        QString destination = soundfxDir + "/long_tip.mp3";
        QFile::copy(source, destination);
    }

    // check for thirty_second_warning.mp3 and copy it in, if it doesn't exist already
    QFile thirtySecSound(soundfxDir + "/thirty_second_warning.mp3");
    if (!thirtySecSound.exists()) {
        QString source = QCoreApplication::applicationDirPath() + pathFromAppDirPathToStarterSet + "/thirty_second_warning.mp3";
        QString destination = soundfxDir + "/thirty_second_warning.mp3";
        QFile::copy(source, destination);
    }

    // check for individual soundFX files, and copy in a starter-set sound, if one doesn't exist,
    // which is checked when the App starts, and when the Startup Wizard finishes setup.  In which case, we
    //   only want to copy files into the soundfx directory if there aren't already files there.
    //   It might seem like overkill right now (and it is, right now).
    bool foundSoundFXfile[6];
    for (int i=0; i<6; i++) {
        foundSoundFXfile[i] = false;
    }

    QDirIterator it(soundfxDir);
    while(it.hasNext()) {
        QString s1 = it.next();

        QString baseName = s1.replace(soundfxDir,"");
        QStringList sections = baseName.split(".");

        if (sections.length() == 3 && sections[0].toInt() != 0 && sections[2].compare("mp3",Qt::CaseInsensitive)==0) {
//            if (sections.length() == 3 && sections[0].toInt() != 0 && sections[2] == "mp3") {
            foundSoundFXfile[sections[0].toInt() - 1] = true;  // found file of the form <N>.<something>.mp3
        }
    } // while

    QString starterSet[6] = {
        "1.whistle.mp3",
        "2.clown_honk.mp3",
        "3.submarine.mp3",
        "4.applause.mp3",
        "5.fanfare.mp3",
        "6.fade.mp3"
    };
    for (int i=0; i<6; i++) {
        if (!foundSoundFXfile[i]) {
            QString destination = soundfxDir + "/" + starterSet[i];
            QFile dest(destination);
            if (!dest.exists()) {
                QString source = QCoreApplication::applicationDirPath() + pathFromAppDirPathToStarterSet + "/" + starterSet[i];
                QFile::copy(source, destination);
            }
        }
    } // for

}

void MainWindow::on_actionStop_Sound_FX_triggered()
{
    // whatever SFX are playing, stop them.
    cBass.StopAllSoundEffects();
}

// FONT SIZE STUFF ========================================
unsigned int MainWindow::pointSizeToIndex(unsigned int pointSize) {
    // converts our old-style range: 11,13,15,17,19,21,23,25
    //   to an index:                 0, 1, 2, 3, 4, 5, 6, 7
    // returns -1 if not in range, or if even number
    if (pointSize < 11 || pointSize > 25 || pointSize % 2 != 1) {
        return(-1);
    }
    return((pointSize-11)/2);
}

unsigned int MainWindow::indexToPointSize(unsigned int index) {
#if defined(Q_OS_MAC)
    return (2*index + 11);
#elif defined(Q_OS_WIN)
    return (int)((8.0/13.0)*((float)(2*index + 11)));
#elif defined(Q_OS_LINUX)
    return (2*index + 11);
#endif
}

void MainWindow::setFontSizes()
{
    // initial font sizes (with no zoom in/out)

#if defined(Q_OS_MAC)
    preferredVerySmallFontSize = 13;
    preferredSmallFontSize = 13;
    preferredWarningLabelFontSize = 20;
    preferredNowPlayingFontSize = 27;
#elif defined(Q_OS_WIN32)
    preferredVerySmallFontSize = 8;
    preferredSmallFontSize = 8;
    preferredWarningLabelFontSize = 12;
    preferredNowPlayingFontSize = 16;
#elif defined(Q_OS_LINUX)
    preferredVerySmallFontSize = 8;
    preferredSmallFontSize = 13;
    preferredWarningLabelFontSize = 20;
    preferredNowPlayingFontSize = 27;
#endif

    QFont font = ui->currentTempoLabel->font();  // system font for most everything

    // preferred very small text
    font.setPointSize(preferredVerySmallFontSize);
    ui->typeSearch->setFont(font);
    ui->labelSearch->setFont(font);
    ui->titleSearch->setFont(font);
    ui->clearSearchButton->setFont(font);
    ui->songTable->setFont(font);

    // preferred normal small text
    font.setPointSize(preferredSmallFontSize);
    ui->tabWidget->setFont(font);  // most everything inherits from this one
    ui->statusBar->setFont(font);
    ui->currentLocLabel->setFont(font);
    ui->songLengthLabel->setFont(font);

    ui->bassLabel->setFont(font);
    ui->midrangeLabel->setFont(font);
    ui->trebleLabel->setFont(font);
    ui->EQgroup->setFont(font);

    ui->pushButtonCueSheetEditTitle->setFont(font);
    ui->pushButtonCueSheetEditLabel->setFont(font);
    ui->pushButtonCueSheetEditArtist->setFont(font);
    ui->pushButtonCueSheetEditHeader->setFont(font);
    ui->pushButtonCueSheetEditBold->setFont(font);
    ui->pushButtonCueSheetEditItalic->setFont(font);

    // preferred Warning Label (medium sized)
    font.setPointSize(preferredWarningLabelFontSize);
    ui->warningLabel->setFont(font);
    ui->warningLabelCuesheet->setFont(font);

    // preferred Now Playing (large sized)
    font.setPointSize(preferredNowPlayingFontSize);
    ui->nowPlayingLabel->setFont(font);
}

void MainWindow::adjustFontSizes()
{
    // ui->songTable->resizeColumnToContents(kNumberCol);  // nope
    ui->songTable->resizeColumnToContents(kTypeCol);
    ui->songTable->resizeColumnToContents(kLabelCol);
    // kTitleCol = nope

    QFont currentFont = ui->songTable->font();
    int currentFontPointSize = currentFont.pointSize();  // platform-specific point size

    unsigned int index = pointSizeToIndex(currentMacPointSize);  // current index

    // give a little extra space when sorted...
    int sortedSection = ui->songTable->horizontalHeader()->sortIndicatorSection();

    // pixel perfection for each platform
#if defined(Q_OS_MAC)
    float extraColWidth[8] = {0.25, 0.0, 0.0, 0.0,  0.0, 0.0, 0.0, 0.0};

    float recentBase = 4.5;
    float ageBase = 3.5;
    float pitchBase = 4.0;
    float tempoBase = 4.5;

    float recentFactor = 0.9;
    float ageFactor = 0.5;
    float pitchFactor = 0.5;
    float tempoFactor = 0.9;

    unsigned int searchBoxesHeight[8] = {22, 26, 30, 34,  38, 42, 46, 50};
    float scaleWidth1 = 7.75;
    float scaleWidth2 = 3.25;
    float scaleWidth3 = 8.5;

    // lyrics buttons
    unsigned int TitleButtonWidth[8] = {55,60,65,70, 80,90,95,105};

    float maxEQsize = 16;
    float scaleIcons = 24.0/13.0;

    unsigned int warningLabelSize[8] = {16,20,23,26, 29,32,35,38};  // basically 20/13 * pointSize
    unsigned int warningLabelWidth[8] = {93,110,126,143, 160,177,194,211};  // basically 20/13 * pointSize * 5.5

    unsigned int nowPlayingSize[8] = {22,27,31,35, 39,43,47,51};  // basically 27/13 * pointSize

    float nowPlayingHeightFactor = 1.5;

    float buttonSizeH = 1.875;
    float buttonSizeV = 1.125;
#elif defined(Q_OS_WIN32)
    float extraColWidth[8] = {0.25f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f};

    float recentBase = 7.5f;
    float ageBase = 5.5f;
    float pitchBase = 6.0f;
    float tempoBase = 7.5f;

    float recentFactor = 0.0f;
    float ageFactor = 0.0f;
    float pitchFactor = 0.0f;
    float tempoFactor = 0.0f;

    unsigned int searchBoxesHeight[8] = {22, 26, 30, 34,  38, 42, 46, 50};
    float scaleWidth1 = 12.75f;
    float scaleWidth2 = 5.25f;
    float scaleWidth3 = 10.5f;

    // lyrics buttons
    unsigned int TitleButtonWidth[8] = {55,60,65,70, 80,90,95,105};

    float maxEQsize = 16;
    float scaleIcons = (float)(1.5*24.0/13.0);
    unsigned int warningLabelSize[8] = {9,12,14,16, 18,20,22,24};  // basically 20/13 * pointSize
    unsigned int warningLabelWidth[8] = {84,100,115,130, 146, 161, 176, 192};  // basically 20/13 * pointSize * 5

//    unsigned int nowPlayingSize[8] = {22,27,31,35, 39,43,47,51};  // basically 27/13 * pointSize
    unsigned int nowPlayingSize[8] = {16,20,23,26, 29,32,35,38};  // basically 27/13 * pointSize

    float nowPlayingHeightFactor = 1.5f;

    float buttonSizeH = 1.5f*1.875f;
    float buttonSizeV = 1.5f*1.125f;
#elif defined(Q_OS_LINUX)
    float extraColWidth[8] = {0.25, 0.0, 0.0, 0.0,  0.0, 0.0, 0.0, 0.0};

    float recentBase = 4.5;
    float ageBase = 3.5;
    float pitchBase = 4.0;
    float tempoBase = 4.5;

    float recentFactor = 0.9;
    float ageFactor = 0.5;
    float pitchFactor = 0.5;
    float tempoFactor = 0.9;

    unsigned int searchBoxesHeight[8] = {22, 26, 30, 34,  38, 42, 46, 50};
    float scaleWidth1 = 7.75;
    float scaleWidth2 = 3.25;
    float scaleWidth3 = 8.5;

    // lyrics buttons
    unsigned int TitleButtonWidth[8] = {55,60,65,70, 80,90,95,105};

    float maxEQsize = 16;
    float scaleIcons = 24.0/13.0;
    unsigned int warningLabelSize[8] = {16,20,23,26, 29,32,35,38};  // basically 20/13 * pointSize
    unsigned int warningLabelWidth[8] = {93,110,126,143, 160,177,194,211};  // basically 20/13 * pointSize * 5.5

    unsigned int nowPlayingSize[8] = {22,27,31,35, 39,43,47,51};  // basically 27/13 * pointSize

    float nowPlayingHeightFactor = 1.5;

    float buttonSizeH = 1.875;
    float buttonSizeV = 1.125;
#endif

    // also a little extra space for the smallest zoom size
    ui->songTable->setColumnWidth(kRecentCol, (recentBase+(sortedSection==kRecentCol?recentFactor:0.0)+extraColWidth[index])*currentFontPointSize);
    ui->songTable->setColumnWidth(kAgeCol, (ageBase+(sortedSection==kAgeCol?ageFactor:0.0)+extraColWidth[index])*currentFontPointSize);
    ui->songTable->setColumnWidth(kPitchCol, (pitchBase+(sortedSection==kPitchCol?pitchFactor:0.0)+extraColWidth[index])*currentFontPointSize);
    ui->songTable->setColumnWidth(kTempoCol, (tempoBase+(sortedSection==kTempoCol?tempoFactor:0.0)+extraColWidth[index])*currentFontPointSize);

    ui->typeSearch->setFixedHeight(searchBoxesHeight[index]);
    ui->labelSearch->setFixedHeight(searchBoxesHeight[index]);
    ui->titleSearch->setFixedHeight(searchBoxesHeight[index]);

    // set all the related fonts to the same size
    ui->typeSearch->setFont(currentFont);
    ui->labelSearch->setFont(currentFont);
    ui->titleSearch->setFont(currentFont);

    ui->tempoLabel->setFont(currentFont);
    ui->pitchLabel->setFont(currentFont);
    ui->volumeLabel->setFont(currentFont);
    ui->mixLabel->setFont(currentFont);

    ui->currentTempoLabel->setFont(currentFont);
    ui->currentPitchLabel->setFont(currentFont);
    ui->currentVolumeLabel->setFont(currentFont);
    ui->currentMixLabel->setFont(currentFont);

    int newCurrentWidth = scaleWidth1 * currentFontPointSize;
    ui->currentTempoLabel->setFixedWidth(newCurrentWidth);
    ui->currentPitchLabel->setFixedWidth(newCurrentWidth);
    ui->currentVolumeLabel->setFixedWidth(newCurrentWidth);
    ui->currentMixLabel->setFixedWidth(newCurrentWidth);

    ui->statusBar->setFont(currentFont);

    ui->currentLocLabel->setFont(currentFont);
    ui->songLengthLabel->setFont(currentFont);

    ui->currentLocLabel->setFixedWidth(scaleWidth2 * currentFontPointSize);
    ui->songLengthLabel->setFixedWidth(scaleWidth2 * currentFontPointSize);

    ui->clearSearchButton->setFont(currentFont);
    ui->clearSearchButton->setFixedWidth(scaleWidth3 * currentFontPointSize);

    ui->tabWidget->setFont(currentFont);  // most everything inherits from this one

    ui->pushButtonCueSheetEditTitle->setFont(currentFont);
    ui->pushButtonCueSheetEditLabel->setFont(currentFont);
    ui->pushButtonCueSheetEditArtist->setFont(currentFont);
    ui->pushButtonCueSheetEditHeader->setFont(currentFont);
    ui->pushButtonCueSheetEditLyrics->setFont(currentFont);

    ui->pushButtonCueSheetEditBold->setFont(currentFont);
    ui->pushButtonCueSheetEditItalic->setFont(currentFont);

    ui->pushButtonCueSheetEditTitle->setFixedWidth(TitleButtonWidth[index]);
    ui->pushButtonCueSheetEditLabel->setFixedWidth(TitleButtonWidth[index]);
    ui->pushButtonCueSheetEditArtist->setFixedWidth(TitleButtonWidth[index]);
    ui->pushButtonCueSheetEditHeader->setFixedWidth(TitleButtonWidth[index] * 1.5);
    ui->pushButtonCueSheetEditLyrics->setFixedWidth(TitleButtonWidth[index]);

    ui->pushButtonCueSheetClearFormatting->setFixedWidth(TitleButtonWidth[index] * 2.25);

    ui->tableWidgetCallList->horizontalHeader()->setFont(currentFont);

    ui->tableWidgetCallList->setColumnWidth(kCallListOrderCol,67*(currentMacPointSize/13.0));
    ui->tableWidgetCallList->setColumnWidth(kCallListCheckedCol, 34*(currentMacPointSize/13.0));
    ui->tableWidgetCallList->setColumnWidth(kCallListWhenCheckedCol, 100*(currentMacPointSize/13.0));

    // these are special -- don't want them to get too big, even if user requests huge fonts
    currentFont.setPointSize(currentFontPointSize > maxEQsize ? maxEQsize : currentFontPointSize);  // no bigger than 20pt
    ui->bassLabel->setFont(currentFont);
    ui->midrangeLabel->setFont(currentFont);
    ui->trebleLabel->setFont(currentFont);
    ui->EQgroup->setFont(currentFont);

    // resize the icons for the buttons
    int newIconDimension = (int)((float)currentFontPointSize * scaleIcons);
    QSize newIconSize(newIconDimension, newIconDimension);
    ui->stopButton->setIconSize(newIconSize);
    ui->playButton->setIconSize(newIconSize);
    ui->previousSongButton->setIconSize(newIconSize);
    ui->nextSongButton->setIconSize(newIconSize);

    ui->toolButtonEditLyrics->setIconSize(newIconSize);

    // these are special MEDIUM
    int warningLabelFontSize = warningLabelSize[index]; // keep ratio constant
    currentFont.setPointSize(warningLabelFontSize);
    ui->warningLabel->setFont(currentFont);
    ui->warningLabelCuesheet->setFont(currentFont);

    ui->warningLabel->setFixedWidth(warningLabelWidth[index]);
    ui->warningLabelCuesheet->setFixedWidth(warningLabelWidth[index]);

    // these are special BIG
    int nowPlayingLabelFontSize = (nowPlayingSize[index]); // keep ratio constant
    currentFont.setPointSize(nowPlayingLabelFontSize);
    ui->nowPlayingLabel->setFont(currentFont);
    ui->nowPlayingLabel->setFixedHeight(nowPlayingHeightFactor * nowPlayingLabelFontSize);

    // BUTTON SIZES ---------
    ui->stopButton->setFixedSize(buttonSizeH*nowPlayingLabelFontSize,buttonSizeV*nowPlayingLabelFontSize);
    ui->playButton->setFixedSize(buttonSizeH*nowPlayingLabelFontSize,buttonSizeV*nowPlayingLabelFontSize);
    ui->previousSongButton->setFixedSize(buttonSizeH*nowPlayingLabelFontSize,buttonSizeV*nowPlayingLabelFontSize);
    ui->nextSongButton->setFixedSize(buttonSizeH*nowPlayingLabelFontSize,buttonSizeV*nowPlayingLabelFontSize);
}

void MainWindow::usePersistentFontSize() {
    PreferencesManager prefsManager;

    int newPointSize = prefsManager.GetsongTableFontSize(); // gets the persisted value
    if (newPointSize == 0) {
        newPointSize = 13;  // default backstop, if not set properly
    }

    // ensure it is a reasonable size...
    newPointSize = (newPointSize > BIGGESTZOOM ? BIGGESTZOOM : newPointSize);
    newPointSize = (newPointSize < SMALLESTZOOM ? SMALLESTZOOM : newPointSize);

    //qDebug() << "usePersistentFontSize: " << newPointSize;

    QFont currentFont = ui->songTable->font();  // set the font size in the songTable
//    qDebug() << "index: " << pointSizeToIndex(newPointSize);
    unsigned int platformPS = indexToPointSize(pointSizeToIndex(newPointSize));  // convert to PLATFORM pointsize
//    qDebug() << "platformPS: " << platformPS;
    currentFont.setPointSize(platformPS);
    ui->songTable->setFont(currentFont);
    currentMacPointSize = newPointSize;

    adjustFontSizes();  // use that font size to scale everything else (relative)
}


void MainWindow::persistNewFontSize(int currentMacPointSize) {
    PreferencesManager prefsManager;

//    qDebug() << "NOT PERSISTING: " << currentMacPointSize;
//    return;
    prefsManager.SetsongTableFontSize(currentMacPointSize);  // persist this
//    qDebug() << "persisting new Mac font size: " << currentMacPointSize;
}

void MainWindow::on_actionZoom_In_triggered()
{
    QFont currentFont = ui->songTable->font();
    unsigned int newPointSize = currentMacPointSize + ZOOMINCREMENT;
    newPointSize = (newPointSize > BIGGESTZOOM ? BIGGESTZOOM : newPointSize);
    newPointSize = (newPointSize < SMALLESTZOOM ? SMALLESTZOOM : newPointSize);

    if (newPointSize > currentMacPointSize) {
        ui->textBrowserCueSheet->zoomIn(2*ZOOMINCREMENT);
        totalZoom += 2*ZOOMINCREMENT;
    }

    unsigned int platformPS = indexToPointSize(pointSizeToIndex(newPointSize));  // convert to PLATFORM pointsize
    currentFont.setPointSize(platformPS);
    ui->songTable->setFont(currentFont);
    currentMacPointSize = newPointSize;

    persistNewFontSize(currentMacPointSize);

    adjustFontSizes();
//    qDebug() << "currentMacPointSize:" << newPointSize << ", totalZoom:" << totalZoom;
}

void MainWindow::on_actionZoom_Out_triggered()
{
    QFont currentFont = ui->songTable->font();
    unsigned int newPointSize = currentMacPointSize - ZOOMINCREMENT;
    newPointSize = (newPointSize > BIGGESTZOOM ? BIGGESTZOOM : newPointSize);
    newPointSize = (newPointSize < SMALLESTZOOM ? SMALLESTZOOM : newPointSize);

    if (newPointSize < currentMacPointSize) {
        ui->textBrowserCueSheet->zoomOut(2*ZOOMINCREMENT);
        totalZoom -= 2*ZOOMINCREMENT;
    }

    unsigned int platformPS = indexToPointSize(pointSizeToIndex(newPointSize));  // convert to PLATFORM pointsize
//    qDebug() << "newIndex: " << pointSizeToIndex(newPointSize);
    currentFont.setPointSize(platformPS);
    ui->songTable->setFont(currentFont);
    currentMacPointSize = newPointSize;

    persistNewFontSize(currentMacPointSize);

    adjustFontSizes();

//    qDebug() << "currentMacPointSize:" << newPointSize << ", totalZoom:" << totalZoom;
}

void MainWindow::on_actionReset_triggered()
{
    QFont currentFont;  // system font, and system default point size

    currentMacPointSize = 13; // by definition
    unsigned int platformPS = indexToPointSize(pointSizeToIndex(currentMacPointSize));  // convert to PLATFORM pointsize

    currentFont.setPointSize(platformPS);
    ui->songTable->setFont(currentFont);

    ui->textBrowserCueSheet->zoomOut(totalZoom);  // undo all zooming in the lyrics pane
    totalZoom = 0;

    persistNewFontSize(currentMacPointSize);
    adjustFontSizes();
//    qDebug() << "currentMacPointSize:" << currentMacPointSize << ", totalZoom:" << totalZoom;
}

// ---------------------------------------------------------------------------------------------------------------------
void MainWindow::on_actionRecent_toggled(bool checked)
{
    ui->actionRecent->setChecked(checked);  // when this function is called at constructor time, preferences sets the checkmark

    // the showRecentColumn setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.SetshowRecentColumn(checked);

    updateSongTableColumnView();
}

void MainWindow::on_actionAge_toggled(bool checked)
{
    ui->actionAge->setChecked(checked);  // when this function is called at constructor time, preferences sets the checkmark

    // the showAgeColumn setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.SetshowAgeColumn(checked);

    updateSongTableColumnView();
}

void MainWindow::on_actionPitch_toggled(bool checked)
{
    ui->actionPitch->setChecked(checked);  // when this function is called at constructor time, preferences sets the checkmark

    // the showAgeColumn setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.SetshowPitchColumn(checked);

    updateSongTableColumnView();
}

void MainWindow::on_actionTempo_toggled(bool checked)
{
    ui->actionTempo->setChecked(checked);  // when this function is called at constructor time, preferences sets the checkmark

    // the showAgeColumn setting is persistent across restarts of the application
    PreferencesManager prefsManager;
    prefsManager.SetshowTempoColumn(checked);

    updateSongTableColumnView();
}

void MainWindow::on_actionFade_Out_triggered()
{
    cBass.FadeOutAndPause();
}

// For improving as well as measuring performance of long songTable operations
// The hide() really gets most of the benefit:
//
//                                          no hide()   with hide()
// loadMusicList                             129ms      112ms
// finishLoadingPlaylist (50 items list)    3227ms     1154ms
// clear playlist                           2119ms      147ms
//
void MainWindow::startLongSongTableOperation(QString s) {
    Q_UNUSED(s)
//    t1.start();  // DEBUG

    ui->songTable->hide();
    ui->songTable->setSortingEnabled(false);
}

void MainWindow::stopLongSongTableOperation(QString s) {
    Q_UNUSED(s)

    ui->songTable->setSortingEnabled(true);
    ui->songTable->show();

//    qDebug() << s << ": " << t1.elapsed() << "ms.";  // DEBUG
}


void MainWindow::on_actionDownload_Cuesheets_triggered()
{
    fetchListOfCuesheetsFromCloud();
//  cuesheetListDownloadEnd();  // <-- will be called, when the fetch from the Cloud is complete

//#if defined(Q_OS_MAC) | defined(Q_OS_WIN)

//    PreferencesManager prefsManager;
//    QString musicDirPath = prefsManager.GetmusicPath();
//    QString lyricsDirPath = musicDirPath + "/lyrics";

//    QDir lyricsDir(lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME);

//    if (lyricsDir.exists()) {
////        qDebug() << "You already have the latest cuesheets downloaded.  Are you sure you want to download them again (erasing any edits you made)?";

//        QMessageBox msgBox;
//        msgBox.setIcon(QMessageBox::Warning);
//        msgBox.setText(QString("You already have the latest lyrics files: '") + CURRENTSQVIEWLYRICSNAME + "'");
//        msgBox.setInformativeText("Are you sure?  This will overwrite any lyrics that you have edited in that folder.");
//        QPushButton *downloadButton = msgBox.addButton(tr("Download Anyway"), QMessageBox::AcceptRole);
//        QPushButton *abortButton = msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
//        msgBox.exec();

//        if (msgBox.clickedButton() == downloadButton) {
//            // Download
////            qDebug() << "DOWNLOAD WILL PROCEED NORMALLY.";
//        } else if (msgBox.clickedButton() == abortButton) {
//            // Abort
////            qDebug() << "ABORTING DOWNLOAD.";
//            return;
//        }
//    }

//    Downloader *d = new Downloader(this);

//    QUrl lyricsZipFileURL(QString("https://raw.githubusercontent.com/mpogue2/SquareDesk/master/") + CURRENTSQVIEWLYRICSNAME + QString(".zip"));  // FIX: hard-coded for now
////    qDebug() << "url to download:" << lyricsZipFileURL.toDisplayString();

//    QString lyricsZipFileName = lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME + ".zip";

//    d->doDownload(lyricsZipFileURL, lyricsZipFileName);  // download URL and put it into lyricsZipFileName

//    QObject::connect(d,SIGNAL(downloadFinished()), this, SLOT(lyricsDownloadEnd()));
//    QObject::connect(d,SIGNAL(downloadFinished()), d, SLOT(deleteLater()));

//    // PROGRESS BAR ---------------------
//    // assume ~10MB
//    progressDialog = new QProgressDialog("Downloading lyrics...", "Cancel Download", 0, 100, this);
//    connect(progressDialog, SIGNAL(canceled()), this, SLOT(cancelProgress()));
//    progressTimer = new QTimer(this);
//    connect(progressTimer, SIGNAL(timeout()), this, SLOT(makeProgress()));

//    progressOffset = 0.0;
//    progressTotal = 0.0;
//    progressTimer->start(500);  // twice per second
//#endif
}

void MainWindow::makeProgress() {
#if defined(Q_OS_MAC) | defined(Q_OS_WIN)
    if (progressTotal < 70) {
        progressTotal += 7.0;
    } else if (progressTotal < 90) {
        progressTotal += 2.0;
    } else if (progressTotal < 98) {
        progressTotal += 0.5;
    } // else no progress for you.

//    qDebug() << "making progress..." << progressOffset << "," << progressTotal;

    progressDialog->setValue((unsigned int)(progressOffset + 33.0*(progressTotal/100.0)));
#endif
}

void MainWindow::cancelProgress() {
#if defined(Q_OS_MAC) | defined(Q_OS_WIN)
//    qDebug() << "cancelling progress...";
    progressTimer->stop();
    progressTotal = 0;
    progressOffset = 0;
    progressCancelled = true;
#endif
}


void MainWindow::lyricsDownloadEnd() {
//#if defined(Q_OS_MAC) | defined(Q_OS_WIN)
////    qDebug() << "MainWindow::lyricsDownloadEnd() -- Download done:";

////    qDebug() << "UNPACKING ZIP FILE INTO LYRICS DIRECTORY...";
//    PreferencesManager prefsManager;
//    QString musicDirPath = prefsManager.GetmusicPath();
//    QString lyricsDirPath = musicDirPath + "/lyrics";
//    QString lyricsZipFileName = lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME + ".zip";

//    QString destinationDir = lyricsDirPath;

//    // extract the ZIP file
//    QStringList extracted = JlCompress::extractDir(lyricsZipFileName, destinationDir); // extracts /root/lyrics/SqView_xxxxxx.zip to /root/lyrics/Text

//    if (extracted.empty()) {
////        qDebug() << "There was a problem extracting the files.  No files extracted.";
//        progressDialog->setValue(100);  // kill the progress bar
//        progressTimer->stop();
//        progressOffset = 0;
//        progressTotal = 0;
//        progressCancelled = false;

//        QMessageBox msgBox;
//        msgBox.setText("The lyrics have been downloaded to <musicDirectory>/lyrics, but you need to manually unpack them.\nWindows: Right click, Extract All on: " +
//                       lyricsZipFileName);
//        msgBox.exec();
//        return;  // and don't delete the ZIP file, for debugging
//    }

////    qDebug() << "DELETING ZIP FILE...";
//    QFile file(lyricsZipFileName);
//    file.remove();

//    QDir currentLyricsDir(lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME);
//    if (currentLyricsDir.exists()) {
////        qDebug() << "Refused to overwrite existing cuesheets, renamed to: " << lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME + "_backup";
//        QFile::rename(lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME, lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME + "_backup");
//    }

////    qDebug() << "RENAMING Text/ TO SqViewCueSheets_2017.03.14/ ...";
//    QFile::rename(lyricsDirPath + "/Text", lyricsDirPath + "/" + CURRENTSQVIEWLYRICSNAME);

//    // RESCAN THE ENTIRE MUSIC DIRECTORY FOR LYRICS ------------
//    findMusic(musicRootPath,"","main", true);  // get the filenames from the user's directories
//    loadMusicList(); // and filter them into the songTable

////    qDebug() << "DONE DOWNLOADING LATEST LYRICS: " << CURRENTSQVIEWLYRICSNAME << "\n";

//    progressDialog->setValue(100);  // kill the progress bar
//    progressTimer->stop();
//    progressOffset = 0;
//    progressTotal = 0;

//#endif
}

QString MainWindow::filepath2SongType(QString MP3Filename)
{
    // returns the type (as a string).  patter, hoedown -> "patter", as per user prefs

    MP3Filename.replace(QRegExp("^" + musicRootPath),"");  // delete the <path to musicDir> from the front of the pathname
    QStringList parts = MP3Filename.split("/");

    if (parts.length() <= 1) {
        return("unknown");
    } else {
        QString folderTypename = parts[1];
        if (songTypeNamesForPatter.contains(folderTypename)) {
            return("patter");
        } else if (songTypeNamesForSinging.contains(folderTypename)) {
            return("singing");
        } else if (songTypeNamesForCalled.contains(folderTypename)) {
            return("called");
        } else if (songTypeNamesForExtras.contains(folderTypename)) {
            return("extras");
        } else {
            return(folderTypename);
        }
    }
}

void MainWindow::on_actionFilePrint_triggered()
{
    // this is the only tab that has printable content right now
    // this is only a double-check (the menu item should be disabled when not on Lyrics/Patter
    QPrinter printer;
    QPrintDialog printDialog(&printer, this);

    int i = ui->tabWidget->currentIndex();
    if (ui->tabWidget->tabText(i).endsWith("Lyrics") || ui->tabWidget->tabText(i).endsWith("Patter")) {
        if (ui->tabWidget->tabText(i).endsWith("Lyrics")) {
            printDialog.setWindowTitle("Print Cuesheet");
        } else {
            printDialog.setWindowTitle("Print Patter");
        }

        if (printDialog.exec() == QDialog::Rejected) {
            return;
        }

        ui->textBrowserCueSheet->print(&printer);
    } else if (ui->tabWidget->tabText(i).endsWith("SD")) {
        QPrinter printer;
        QPrintDialog printDialog(&printer, this);
        printDialog.setWindowTitle("Print SD Sequence");

        if (printDialog.exec() == QDialog::Rejected) {
            return;
        }

        QTextDocument doc;
        QSizeF paperSize;
        paperSize.setWidth(printer.width());
        paperSize.setHeight(printer.height());
        doc.setPageSize(paperSize);

        QString contents("<html><head><title>Square Dance Sequence</title>\n"
                         "<body><h1>Square Dance Sequence</h1>\n");
        for (int row = 0; row < ui->tableWidgetCurrentSequence->rowCount();
             ++row)
        {
            QTableWidgetItem *item = ui->tableWidgetCurrentSequence->item(row,0);
            contents += item->text() + "<br>\n";
        }
        contents += "</body></html>\n";
        doc.setHtml(contents);
        doc.print(&printer);
    } else if (ui->tabWidget->tabText(i).endsWith("Music Player")) {
        QPrinter printer;
        QPrintDialog printDialog(&printer, this);
        printDialog.setWindowTitle("Print Playlist");

        if (printDialog.exec() == QDialog::Rejected) {
            return;
        }

        QPainter painter;

        painter.begin(&printer);

        QFont font = painter.font();
        font.setPixelSize(14);
        painter.setFont(font);

        // --------
        QList<PlaylistExportRecord> exports;

        // Iterate over the songTable to get all the info about the playlist
        for (int i=0; i<ui->songTable->rowCount(); i++) {
            QTableWidgetItem *theItem = ui->songTable->item(i,kNumberCol);
            QString playlistIndex = theItem->text();
            QString pathToMP3 = ui->songTable->item(i,kPathCol)->data(Qt::UserRole).toString();
            QString songTitle = ui->songTable->item(i,kTitleCol)->text();
            QString pitch = ui->songTable->item(i,kPitchCol)->text();
            QString tempo = ui->songTable->item(i,kTempoCol)->text();

            if (playlistIndex != "") {
                // item HAS an index (that is, it is on the list, and has a place in the ordering)
                // TODO: reconcile int here with float elsewhere on insertion
                PlaylistExportRecord rec;
                rec.index = playlistIndex.toInt();
    //            rec.title = songTitle;
                rec.title = pathToMP3;  // NOTE: this is an absolute path that does not survive moving musicDir
                rec.pitch = pitch;
                rec.tempo = tempo;
                exports.append(rec);
            }
        }

        qSort(exports.begin(), exports.end(), comparePlaylistExportRecord);  // they are now IN INDEX ORDER

        QString toBePrinted = "PLAYLIST\n\n";

        char buf[128];
        // list is sorted here, in INDEX order
        foreach (const PlaylistExportRecord &rec, exports)
        {
            QString baseName = rec.title;
            baseName.replace(QRegExp("^" + musicRootPath),"");  // delete musicRootPath at beginning of string

            sprintf(buf, "%02d: %s\n", rec.index, baseName.toLatin1().data());
            toBePrinted += buf;
        }

        //        painter.drawText(20, 20, 500, 500, Qt::AlignLeft|Qt::AlignTop, "Hello world!\nMore lines\n");
        painter.drawText(20,20,500,500, Qt::AlignLeft|Qt::AlignTop, toBePrinted);

        painter.end();
    }
}

void MainWindow::saveLyrics()
{
    // Save cuesheet to the current cuesheet filename...
    RecursionGuard dialog_guard(inPreferencesDialog);

    QString cuesheetFilename = ui->comboBoxCuesheetSelector->itemData(ui->comboBoxCuesheetSelector->currentIndex()).toString();
    if (!cuesheetFilename.isNull())
    {
        writeCuesheet(cuesheetFilename);
        loadCuesheets(currentMP3filenameWithPath, cuesheetFilename);
        saveCurrentSongSettings();
    }

}


void MainWindow::saveLyricsAs()
{
    // Ask me where to save it...
    RecursionGuard dialog_guard(inPreferencesDialog);
    QFileInfo fi(currentMP3filenameWithPath);

    if (lastCuesheetSavePath.isEmpty()) {
        lastCuesheetSavePath = musicRootPath + "/lyrics";
    }

    loadedCuesheetNameWithPath = lastCuesheetSavePath + "/" + fi.baseName() + ".html";

    QString maybeFilename = loadedCuesheetNameWithPath;
    QFileInfo fi2(loadedCuesheetNameWithPath);
    if (fi2.exists()) {
        // choose the next name in the series (this won't be done, if we came from a template)
        QString cuesheetExt = loadedCuesheetNameWithPath.split(".").last();
        QString cuesheetBase = loadedCuesheetNameWithPath
                .replace(QRegExp(cuesheetExt + "$"),"")  // remove extension, e.g. ".html"
                .replace(QRegExp("[0-9]+\\.$"),"");      // remove .<number>, e.g. ".2"

        // find an appropriate not-already-used filename to save to
        bool done = false;
        int which = 2;  // I suppose we could be smarter than this at some point.
        while (!done) {
            maybeFilename = cuesheetBase + QString::number(which) + "." + cuesheetExt;
            QFileInfo maybeFile(maybeFilename);
            done = !maybeFile.exists();  // keep going until a proposed filename does not exist (don't worry -- it won't spin forever)
            which++;
        }
    }

    QString filename = QFileDialog::getSaveFileName(this,
                                                    tr("Save"), // TODO: this could say Lyrics or Patter
                                                    maybeFilename,
                                                    tr("HTML (*.html)"));
    if (!filename.isNull())
    {
        writeCuesheet(filename);
        loadCuesheets(currentMP3filenameWithPath, filename);
        saveCurrentSongSettings();
    }
}

void MainWindow::saveSequenceAs()
{
    // Ask me where to save it...
    RecursionGuard dialog_guard(inPreferencesDialog);

    QString sequenceFilename = QFileDialog::getSaveFileName(this,
                                                    tr("Save SD Sequence"),
                                                    musicRootPath + "/sd/sequence.txt",
                                                    tr("TXT (*.txt)"));
    if (!sequenceFilename.isNull())
    {
        QFile file(sequenceFilename);
        if ( file.open(QIODevice::WriteOnly) )
        {
            QTextStream stream( &file );
            for (int row = 0; row < ui->tableWidgetCurrentSequence->rowCount();
                 ++row)
            {
                QTableWidgetItem *item = ui->tableWidgetCurrentSequence->item(row,0);
                stream << item->text() + "\n";
            }
            stream.flush();
            file.close();
        }
    }
}

void MainWindow::on_actionSave_triggered()
{
    int i = ui->tabWidget->currentIndex();
    if (ui->tabWidget->tabText(i).endsWith("Music Player")) {
        // playlist
        on_actionSave_Playlist_triggered(); // really "Save As..."
    } else if (ui->tabWidget->tabText(i).endsWith("Lyrics") || ui->tabWidget->tabText(i).endsWith("Patter")) {
        // lyrics/patter
        saveLyrics();
    } else if (ui->tabWidget->tabText(i).endsWith("SD")) {
        // sequence
        saveSequenceAs();  // intentionally ..As()
    } else {
        // dance program
        // reference
        // timers
        // intentionally nothing...
    }
}


void MainWindow::on_actionSave_As_triggered()
{
    int i = ui->tabWidget->currentIndex();
    if (ui->tabWidget->tabText(i).endsWith("Music Player")) {
        // playlist
        on_actionSave_Playlist_triggered(); // really "Save As..."
    } else if (ui->tabWidget->tabText(i).endsWith("Lyrics") || ui->tabWidget->tabText(i).endsWith("Patter")) {
        // lyrics/patter
        saveLyricsAs();
    } else if (ui->tabWidget->tabText(i).endsWith("SD")) {
        // sequence
        saveSequenceAs();  // intentionally ..As()
    } else {
        // dance program
        // reference
        // timers
        // intentionally nothing...
    }
}

// ----------------------------------------------------------------------
void MainWindow::on_flashcallbasic_toggled(bool checked)
{
    ui->actionFlashCallBasic->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallbasic(ui->actionFlashCallBasic->isChecked());
}

void MainWindow::on_actionFlashCallBasic_triggered()
{
    on_flashcallbasic_toggled(ui->actionFlashCallBasic->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcallmainstream_toggled(bool checked)
{
    ui->actionFlashCallMainstream->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallmainstream(ui->actionFlashCallMainstream->isChecked());
}

void MainWindow::on_actionFlashCallMainstream_triggered()
{
    on_flashcallmainstream_toggled(ui->actionFlashCallMainstream->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcallplus_toggled(bool checked)
{
    ui->actionFlashCallPlus->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallplus(ui->actionFlashCallPlus->isChecked());
}

void MainWindow::on_actionFlashCallPlus_triggered()
{
    on_flashcallplus_toggled(ui->actionFlashCallPlus->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcalla1_toggled(bool checked)
{
    ui->actionFlashCallA1->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcalla1(ui->actionFlashCallA1->isChecked());
}

void MainWindow::on_actionFlashCallA1_triggered()
{
    on_flashcalla1_toggled(ui->actionFlashCallA1->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcalla2_toggled(bool checked)
{
    ui->actionFlashCallA2->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcalla2(ui->actionFlashCallA2->isChecked());
}

void MainWindow::on_actionFlashCallA2_triggered()
{
    on_flashcalla2_toggled(ui->actionFlashCallA2->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcallc1_toggled(bool checked)
{
    ui->actionFlashCallC1->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallc1(ui->actionFlashCallC1->isChecked());
}

void MainWindow::on_actionFlashCallC1_triggered()
{
    on_flashcallc1_toggled(ui->actionFlashCallC1->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcallc2_toggled(bool checked)
{
    ui->actionFlashCallC2->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallc2(ui->actionFlashCallC2->isChecked());
}

void MainWindow::on_actionFlashCallC2_triggered()
{
    on_flashcallc2_toggled(ui->actionFlashCallC2->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcallc3a_toggled(bool checked)
{
    ui->actionFlashCallC3a->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallc3a(ui->actionFlashCallC3a->isChecked());
}

void MainWindow::on_actionFlashCallC3a_triggered()
{
    on_flashcallc3a_toggled(ui->actionFlashCallC3a->isChecked());
    readFlashCallsList();
}

void MainWindow::on_flashcallc3b_toggled(bool checked)
{
    ui->actionFlashCallC3b->setChecked(checked);

    // the Flash Call settings are persistent across restarts of the application
    PreferencesManager prefsManager; // Will be using application information for correct location of your settings
    prefsManager.Setflashcallc3b(ui->actionFlashCallC3b->isChecked());
}

void MainWindow::on_actionFlashCallC3b_triggered()
{
    on_flashcallc3b_toggled(ui->actionFlashCallC3b->isChecked());
    readFlashCallsList();
}

// -----
void MainWindow::readFlashCallsList() {
#if defined(Q_OS_MAC)
    QString appPath = QApplication::applicationFilePath();
    QString allcallsPath = appPath + "/Contents/Resources/allcalls.csv";
    allcallsPath.replace("Contents/MacOS/SquareDeskPlayer/","");
#endif

#if defined(Q_OS_WIN32)
    // TODO: There has to be a better way to do this.
    QString appPath = QApplication::applicationFilePath();
    QString allcallsPath = appPath + "/allcalls.csv";
    allcallsPath.replace("SquareDeskPlayer.exe/","");
#endif

#if defined(Q_OS_MAC) | defined(Q_OS_WIN32)
    QFile file(allcallsPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open 'allcalls.csv' file.";
        qDebug() << "looked here:" << allcallsPath;
        return;
    }

    flashCalls.clear();  // remove all calls, let's read them in again

    while (!file.atEnd()) {
        QString line = file.readLine().simplified();
        QStringList lineparts = line.split(',');
        QString level = lineparts[0];
        QString call = lineparts[1].replace("\"","");

        if ((level == "basic" && ui->actionFlashCallBasic->isChecked()) ||
                (level == "ms" && ui->actionFlashCallMainstream->isChecked()) ||
                (level == "plus" && ui->actionFlashCallPlus->isChecked()) ||
                (level == "a1" && ui->actionFlashCallA1->isChecked()) ||
                (level == "a2" && ui->actionFlashCallA2->isChecked()) ||
                (level == "c1" && ui->actionFlashCallC1->isChecked()) ||
                (level == "c2" && ui->actionFlashCallC2->isChecked()) ||
                (level == "c3a" && ui->actionFlashCallC3a->isChecked()) ||
                (level == "c3b" && ui->actionFlashCallC3b->isChecked()) ) {
            flashCalls.append(call);
        }
    }

    qsrand(QTime::currentTime().msec());  // different random sequence of calls each time, please.
    if (flashCalls.length() == 0) {
        randCallIndex = 0;
    } else {
        randCallIndex = qrand() % flashCalls.length();   // start out with a number other than zero, please.
    }

//    qDebug() << "flashCalls: " << flashCalls;
//    qDebug() << "randCallIndex: " << randCallIndex;

#endif
}

// -------------------------------------
// LYRICS CUESHEET FETCHING

void MainWindow::fetchListOfCuesheetsFromCloud() {
//    qDebug() << "MainWindow::fetchListOfCuesheetsFromCloud() -- Download is STARTING...";

    // TODO: only fetch if the time is newer than the one we got last time....
    // TODO:    check Expires date.

    QList<QString> cuesheets;

    Downloader *d = new Downloader(this);

    // Apache Directory Listing, because it ends in "/" (~1.1MB uncompressed, ~250KB compressed)
    QUrl cuesheetListURL(QString(CURRENTSQVIEWCUESHEETSDIR));
    QString cuesheetListFilename = musicRootPath + "/.squaredesk/publishedCuesheets.html";

//    qDebug() << "cuesheet URL to download:" << cuesheetListURL.toDisplayString();
//    qDebug() << "             put it here:" << cuesheetListFilename;

    d->doDownload(cuesheetListURL, cuesheetListFilename);  // download URL and put it into cuesheetListFilename

    QObject::connect(d,SIGNAL(downloadFinished()), this, SLOT(cuesheetListDownloadEnd()));
    QObject::connect(d,SIGNAL(downloadFinished()), d, SLOT(deleteLater()));

    // PROGRESS BAR ---------------------
    progressDialog = new QProgressDialog("Downloading list of available cuesheets...\n ", "Cancel", 0, 100, this);
    progressDialog->setMinimumDuration(0);  // start it up right away
    progressCancelled = false;
    progressDialog->setWindowModality(Qt::WindowModal);  // stays until cancelled
    progressDialog->setMinimumWidth(450);  // avoid bug with Cancel button resizing itself

    connect(progressDialog, SIGNAL(canceled()), this, SLOT(cancelProgress()));
    connect(progressDialog, SIGNAL(canceled()), d, SLOT(abortTransfer()));      // if user hits CANCEL, abort transfer in progress.
    progressTimer = new QTimer(this);

    connect(progressTimer, SIGNAL(timeout()), this, SLOT(makeProgress()));
    progressOffset = 0.0;
    progressTotal = 0.0;
    progressTimer->start(1000);  // once per second to 33%
}

bool MainWindow::fuzzyMatchFilenameToCuesheetname(QString s1, QString s2) {
//    qDebug() << "trying to match: " << s1 << "," << s2;

// **** EXACT MATCH OF COMPLETE BASENAME (just for testing)
//    QFileInfo fi1(s1);
//    QFileInfo fi2(s2);

//    bool match = fi1.completeBaseName() == fi2.completeBaseName();
//    if (match) {
//        qDebug() << "fuzzy match: " << s1 << "," << s2;
//    }

//    return(match);

// **** OUR FUZZY MATCHING (same as findPossibleCuesheets)

    // SPLIT APART THE MUSIC FILENAME --------
    QFileInfo mp3FileInfo(s1);
    QString mp3CanonicalPath = mp3FileInfo.canonicalPath();
    QString mp3CompleteBaseName = mp3FileInfo.completeBaseName();
    QString mp3Label = "";
    QString mp3Labelnum = "";
    QString mp3Labelnum_short = "";
    QString mp3Labelnum_extra = "";
    QString mp3Title = "";
    QString mp3ShortTitle = "";
    breakFilenameIntoParts(mp3CompleteBaseName, mp3Label, mp3Labelnum, mp3Labelnum_extra, mp3Title, mp3ShortTitle);
    QList<CuesheetWithRanking *> possibleRankings;

    QStringList mp3Words = splitIntoWords(mp3CompleteBaseName);
    mp3Labelnum_short = mp3Labelnum;
    while (mp3Labelnum_short.length() > 0 && mp3Labelnum_short[0] == '0')
    {
        mp3Labelnum_short.remove(0,1);
    }

    // SPLIT APART THE CUESHEET FILENAME --------
    QFileInfo fi(s2);
    QString label = "";
    QString labelnum = "";
    QString title = "";
    QString labelnum_extra;
    QString shortTitle = "";

    QString completeBaseName = fi.completeBaseName(); // e.g. "/Users/mpogue/__squareDanceMusic/patter/RIV 307 - Going to Ceili (Patter).mp3" --> "RIV 307 - Going to Ceili (Patter)"
    breakFilenameIntoParts(completeBaseName, label, labelnum, labelnum_extra, title, shortTitle);
    QStringList words = splitIntoWords(completeBaseName);
    QString labelnum_short = labelnum;
    while (labelnum_short.length() > 0 && labelnum_short[0] == '0')
    {
        labelnum_short.remove(0,1);
    }

    // NOW SCORE IT ----------------------------
    int score = 0;
//    qDebug() << "label: " << label;
    if (completeBaseName.compare(mp3CompleteBaseName, Qt::CaseInsensitive) == 0         // exact match: entire filename
        || title.compare(mp3Title, Qt::CaseInsensitive) == 0                            // exact match: title (without label/labelNum)
        || (shortTitle.length() > 0                                                     // exact match: shortTitle
            && shortTitle.compare(mp3ShortTitle, Qt::CaseInsensitive) == 0)
        || (labelnum_short.length() > 0 && label.length() > 0                           // exact match: shortLabel + shortLabelNumber
            &&  labelnum_short.compare(mp3Labelnum_short, Qt::CaseInsensitive) == 0
            && label.compare(mp3Label, Qt::CaseInsensitive) == 0
            )
        || (labelnum.length() > 0 && label.length() > 0
            && mp3Title.length() > 0
            && mp3Title.compare(label + "-" + labelnum, Qt::CaseInsensitive) == 0)
        )
    {
        // Minimum criteria (we will accept as a match, without looking at sorted words):
//        qDebug() << "fuzzy match (meets minimum criteria): " << s1 << "," << s2;
        return(true);
    } else if ((score = compareSortedWordListsForRelevance(mp3Words, words)) > 0)
    {
        // fuzzy match, using the sorted words in the titles
//        qDebug() << "fuzzy match (meets sorted words criteria): " << s1 << "," << s2;
        return(true);
    }

    return(false);
}

void MainWindow::cuesheetListDownloadEnd() {

    if (progressDialog->wasCanceled()) {
        return;
    }

//    qDebug() << "MainWindow::cuesheetListDownloadEnd() -- Download is DONE";

    qApp->processEvents();  // allow the progress bar to move
//    qDebug() << "MainWindow::cuesheetListDownloadEnd() -- Making list of music files...";
    QList<QString> musicFiles = getListOfMusicFiles();

    qApp->processEvents();  // allow the progress bar to move
//    qDebug() << "MainWindow::cuesheetListDownloadEnd() -- Making list of cuesheet files...";
    QList<QString> cuesheetsInCloud = getListOfCuesheets();

    progressOffset = 33;
    progressTotal = 0;
    progressDialog->setValue(33);
//    progressDialog->setLabelText("Matching your music with cuesheets...");
    progressTimer->stop();

//    qDebug() << "MainWindow::cuesheetListDownloadEnd() -- Matching them up...";

//    qDebug() << "***** Here's the list of musicFiles:" << musicFiles;
//    qDebug() << "***** Here's the list of cuesheets:" << cuesheetsInCloud;

    float numCuesheets = cuesheetsInCloud.length();
    float numChecked = 0;

    // match up the music filenames against the cuesheets that the Cloud has
    QList<QString> maybeFilesToDownload;

    QList<QString>::iterator i;
    QList<QString>::iterator j;
    for (j = cuesheetsInCloud.begin(); j != cuesheetsInCloud.end(); ++j) {
        // should we download this Cloud cuesheet file?

        if ( (unsigned int)(numChecked) % 50 == 0 ) {
            progressDialog->setLabelText("Found matching cuesheets: " +
                                         QString::number((unsigned int)maybeFilesToDownload.length()) +
                                         " out of " + QString::number((unsigned int)numChecked) +
                                         "\n" + (maybeFilesToDownload.length() > 0 ? maybeFilesToDownload.last() : "")
                                         );
            progressDialog->setValue((unsigned int)(33 + 33.0*(numChecked/numCuesheets)));
            qApp->processEvents();  // allow the progress bar to move every 100 checks
            if (progressDialog->wasCanceled()) {
                return;
            }
        }

        numChecked++;

        for (i = musicFiles.begin(); i != musicFiles.end(); ++i) {
            if (fuzzyMatchFilenameToCuesheetname(*i, *j)) {
                // yes, let's download it, if we don't have it already.
                maybeFilesToDownload.append(*j);
//                qDebug() << "Will maybe download: " << *j;
                break;  // once we've decided to download this file, go on and look at the NEXT cuesheet
            }
        }
    }

    progressDialog->setValue(66);

//    qDebug() << "MainWindow::cuesheetListDownloadEnd() -- Maybe downloading " << maybeFilesToDownload.length() << " files";
//  qDebug() << "***** Maybe downloading " << maybeFilesToDownload.length() << " files.";

    float numDownloads = maybeFilesToDownload.length();
    float numDownloaded = 0;

    // download them (if we don't have them already)
    QList<QString>::iterator k;
    for (k = maybeFilesToDownload.begin(); k != maybeFilesToDownload.end(); ++k) {
        progressDialog->setLabelText("Downloading matching cuesheets (if needed): " +
                                     QString::number((unsigned int)numDownloaded++) +
                                     " out of " +
                                     QString::number((unsigned int)numDownloads) +
                                     "\n" +
                                     *k
                                     );
        progressDialog->setValue((unsigned int)(66 + 33.0*(numDownloaded/numDownloads)));
        qApp->processEvents();  // allow the progress bar to move constantly
        if (progressDialog->wasCanceled()) {
            break;
        }

        downloadCuesheetFileIfNeeded(*k);
    }

// qDebug() << "MainWindow::cuesheetListDownloadEnd() -- DONE.  All cuesheets we didn't have are downloaded.";

    progressDialog->setLabelText("Done.");
    progressDialog->setValue(100);  // kill the progress bar
    progressTimer->stop();
    progressOffset = 0;
    progressTotal = 0;

    // FINALLY, RESCAN THE ENTIRE MUSIC DIRECTORY FOR SONGS AND LYRICS ------------
    maybeLyricsChanged();

//    findMusic(musicRootPath,"","main", true);  // get the filenames from the user's directories
//    loadMusicList(); // and filter them into the songTable

//    reloadCurrentMP3File(); // in case the list of matching cuesheets changed by the recent addition of cuesheets
}

void MainWindow::downloadCuesheetFileIfNeeded(QString cuesheetFilename) {

//    qDebug() << "Maybe fetching: " << cuesheetFilename;
//    cout << ".";

    PreferencesManager prefsManager;
    QString musicDirPath = prefsManager.GetmusicPath();
    //    QString tempDirPath = "/Users/mpogue/clean4";
    QString destinationFolder = musicDirPath + "/lyrics/downloaded/";

    QDir dir(musicDirPath);
    dir.mkpath("lyrics/downloaded");    // make sure that the destination path exists (including intermediates)

    QFile file(destinationFolder + cuesheetFilename);
    QFileInfo fileinfo(file);

    // if we don't already have it...
    if (!fileinfo.exists()) {
        // ***** SYNCHRONOUS FETCH *****
        // "http://squaredesk.net/cuesheets/SqViewCueSheets_2017.03.14/"
        QNetworkAccessManager *networkMgr = new QNetworkAccessManager(this);
        QString URLtoFetch = CURRENTSQVIEWCUESHEETSDIR + cuesheetFilename; // individual files (~10KB)
        QNetworkReply *reply = networkMgr->get( QNetworkRequest( QUrl(URLtoFetch) ) );

        QEventLoop loop;
        QObject::connect(reply, SIGNAL(readyRead()), &loop, SLOT(quit()));

//        qDebug() << "Fetching file we don't have: " << URLtoFetch;
        // Execute the event loop here, now we will wait here until readyRead() signal is emitted
        // which in turn will trigger event loop quit.
        loop.exec();

        QString resultString(reply->readAll());  // only fetch this once!
        // qDebug() << "result:" << resultString;

        // OK, we have the file now...
        if (resultString.length() == 0) {
            qDebug() << "ERROR: file we got was zero length.";
            return;
        }

//        qDebug() << "***** WRITING TO: " << destinationFolder + cuesheetFilename;
        // let's try to write it
        if ( file.open(QIODevice::WriteOnly) )
        {
            QTextStream stream( &file );
            stream << resultString;
            stream.flush();
            file.close();
        } else {
            qDebug() << "ERROR: couldn't open the file for writing...";
        }
    } else {
//        qDebug() << "     Not fetching it, because we already have it.";
    }
}

QList<QString> MainWindow::getListOfMusicFiles()
{
    QList<QString> list;

    QListIterator<QString> iter(*pathStack);
    while (iter.hasNext()) {
        QString s = iter.next();
        QStringList sl1 = s.split("#!#");
        QString type = sl1[0];      // the type (of original pathname, before following aliases)
        QString filename = sl1[1];  // everything else

        // TODO: should we allow "patter" to match cuesheets?
        if ((type == "singing" || type == "vocals") && (filename.endsWith("mp3", Qt::CaseInsensitive) ||
                                                        filename.endsWith("m4a", Qt::CaseInsensitive) ||
                                                        filename.endsWith("wav", Qt::CaseInsensitive))) {
            QFileInfo fi(filename);
            QString justFilename = fi.fileName();
            list.append(justFilename);
//            qDebug() << "music that might have a cuesheet: " << type << ":" << justFilename;
        }
    }

    return(list);
}

QList<QString> MainWindow::getListOfCuesheets() {

    QList<QString> list;

    QString cuesheetListFilename = musicRootPath + "/.squaredesk/publishedCuesheets.html";
    QFile inputFile(cuesheetListFilename)
            ;
    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);
        int line_number = 0;
        while (!in.atEnd()) //  && line_number < 10)
        {
            line_number++;
            QString line = in.readLine();

            // <li><a href="RR%20147%20-%20Amarillo%20By%20Morning.html"> RR 147 - Amarillo By Morning.html</a></li>

            QRegularExpression regex_cuesheetName("^<li><a href=\"(.*?)\">(.*)</a></li>$"); // don't be greedy!
            QRegularExpressionMatch match = regex_cuesheetName.match(line);
//            qDebug() << "line: " << line;
            if (match.hasMatch())
            {
                QString cuesheetFilename(match.captured(2).trimmed());
//                qDebug() << "****** Cloud has cuesheet: " << cuesheetFilename << " *****";

                list.append(cuesheetFilename);
//                downloadCuesheetFileIfNeeded(cuesheetFilename, &musicFiles);
            }
        }
        inputFile.close();
        } else {
            qDebug() << "ERROR: could not open " << cuesheetListFilename;
        }

    return(list);
}

void MainWindow::maybeLyricsChanged() {
    // RESCAN THE ENTIRE MUSIC DIRECTORY FOR LYRICS FILES (and music files that might match) ------------
    findMusic(musicRootPath,"","main", true);  // get the filenames from the user's directories
    loadMusicList(); // and filter them into the songTable

    // AND, just in case the list of matching cuesheets for the current song has been
    //   changed by the recent addition of cuesheets...
    reloadCurrentMP3File();
}

void MainWindow::on_actionTest_Loop_triggered()
{
//    qDebug() << "Test Loop Intro: " << ui->seekBar->GetIntro() << ", outro: " << ui->seekBar->GetOutro();

    if (!songLoaded) {
        return;  // if there is no song loaded, no point in doing anything.
    }

    cBass.Stop();  // always pause playback

    double songLength = cBass.FileLength;
//    double intro = ui->seekBar->GetIntro(); // 0.0 - 1.0
    double outro = ui->seekBar->GetOutro(); // 0.0 - 1.0

    double startPosition_sec = fmax(0.0, songLength*outro - 5.0);

    cBass.StreamSetPosition(startPosition_sec);
    Info_Seekbar(false);  // update just the text

//    on_playButton_clicked();  // play, starting 5 seconds before the loop

    cBass.Play();  // currently paused, so start playing at new location
}

void MainWindow::on_dateTimeEditIntroTime_timeChanged(const QTime &time)
{
//    qDebug() << "newIntroTime: " << time;

    double position_ms = 60000*time.minute() + 1000*time.second() + time.msec();
    double length = cBass.FileLength;
    double t_ms = position_ms/length;

    ui->seekBarCuesheet->SetIntro((float)t_ms/1000.0);
    ui->seekBar->SetIntro((float)t_ms/1000.0);

    on_loopButton_toggled(ui->actionLoop->isChecked()); // call this, so that cBass is told what the loop points are (or they are cleared)
}

void MainWindow::on_dateTimeEditOutroTime_timeChanged(const QTime &time)
{
//    qDebug() << "newOutroTime: " << time;

    double position_ms = 60000*time.minute() + 1000*time.second() + time.msec();
    double length = cBass.FileLength;
    double t_ms = position_ms/length;

    ui->seekBarCuesheet->SetOutro((float)t_ms/1000.0);
    ui->seekBar->SetOutro((float)t_ms/1000.0);

    on_loopButton_toggled(ui->actionLoop->isChecked()); // call this, so that cBass is told what the loop points are (or they are cleared)
}

void MainWindow::on_pushButtonTestLoop_clicked()
{
    on_actionTest_Loop_triggered();
}

void MainWindow::on_actionClear_Recent_triggered()
{
    QString nowISO8601 = QDateTime::currentDateTime().toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate);  // add days is for later
//    qDebug() << "Setting fence to: " << nowISO8601;

    PreferencesManager prefsManager;
    prefsManager.SetrecentFenceDateTime(nowISO8601);  // e.g. "2018-01-01T01:23:45Z"
    recentFenceDateTime = QDateTime::fromString(prefsManager.GetrecentFenceDateTime(),
                                                          "yyyy-MM-dd'T'hh:mm:ss'Z'");
    recentFenceDateTime.setTimeSpec(Qt::UTC);  // set timezone (all times are UTC)

    firstTimeSongIsPlayed = true;   // this forces writing another record to the songplays DB when the song is next played
                                    //   so that the song will be marked Recent if it is played again after a Clear Recents,
                                    //   even though it was already loaded and played once before the Clear Recents.

    // update the song table
    reloadSongAges(ui->actionShow_All_Ages->isChecked());
}
