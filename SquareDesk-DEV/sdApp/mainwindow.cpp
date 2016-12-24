#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sdhighlighter.h"
#include "renderarea.h"

#include <QDebug>
#include <QProcess>
#include <QString>
#include <QDir>
#include <QStandardPaths>

// TODO: disallow "exit"
// TODO: integrate into a sequence window
// TODO: right at the beginning, if I type in heads square thru 4,
//   automatically add the heads start.  Same for sides.
// TODO: only allow focus in the prompt window
// TODO: allow audible beeps again...
// TODO: figure out where the sd output files really should go (right now: ~/Documents/sdApp)
// TODO: only display the LAST resolve (when there are multiple)
// TODO: where to display the resolve...in the sequence window, maybe?
//    then the only thing left in the input window is a single line of input.

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("sd in Qt");

    renderArea = new RenderArea;
    renderArea->setPen(QPen(Qt::blue));
    renderArea->setBrush(QBrush(Qt::green));

    console = new Console;
    console->setEnabled(true);
    console->setLocalEchoEnabled(true);

    currentSequenceWidget = new QTextEdit();
    currentSequenceWidget->setStyleSheet("QLabel { background-color : white; color : black; }");
    currentSequenceWidget->setAlignment(Qt::AlignTop);
    currentSequenceWidget->setReadOnly(true);

    ui->gridLayout_2->addWidget(currentSequenceWidget,0,0,1,1);
    ui->gridLayout_2->addWidget(console, 1,0,1,2);
    ui->gridLayout_2->addWidget(renderArea, 0,1);

    console->setFixedHeight(150);

//    qDebug() << "current dir:" << QDir::currentPath();

    QString pathToSD = QDir::currentPath() + "/sd";
    // NOTE: sd_calls.dat MUST be in the same directory.

    // start sd as a process -----
//    pathToSD = "/Users/mpogue/Documents/QtProjects/build-sd_qt-Desktop_Qt_5_7_0_clang_64bit-Debug/sd_qt";

    QStringList SDargs;
    SDargs << "-no_color" << "-no_cursor" << "-no_console" << "-lines" << "1000" << "a2";  // default level

    sd = new QProcess(Q_NULLPTR);

//    myProcess->setStandardOutputFile("/Users/mpogue/Documents/QtProjects/build-sdApp-Desktop_Qt_5_7_0_clang_64bit-Debug/foobar2.txt");
//    myProcess->setStandardInputFile("/Users/mpogue/Documents/QtProjects/build-sdApp-Desktop_Qt_5_7_0_clang_64bit-Debug/in.txt");

//    qDebug() << "standard doc locs:" << QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    QString sdWorkingDirectory = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/sdApp";
//    sdWorkingDirectory = "/Users/mpogue/Documents/QtProjects/build-sd_qt-Desktop_Qt_5_7_0_clang_64bit-Debug";

    qDebug() << "pathToSD:" << pathToSD;
    qDebug() << "sdWorkingDirectory:" << sdWorkingDirectory;

    // check whether required sd_calls.dat file exists in the Documents/sdApp directory
    QFileInfo check_file(sdWorkingDirectory + "/sd_calls.dat");
    if (!check_file.exists()) {
        qDebug() << "ERROR: sd_calls.dat file not found in ~/Documents/sdApp.  Please put a copy there!";
        exit(-1);
    }

    sd->setWorkingDirectory(sdWorkingDirectory);
    sd->start(pathToSD, SDargs);

    qDebug() << "waiting to start sd...";
    sd->waitForStarted();
    qDebug() << "started.";

    if (true) {
        sd->write("heads start\n");  // DEBUG
        sd->waitForBytesWritten();

        sd->write("square thru 4\n"); // DEBUG
        sd->waitForBytesWritten();
    } else {
        sd->write("just as they are\n");
        sd->waitForBytesWritten();
    }

    connect(sd, &QProcess::readyReadStandardOutput, this, &MainWindow::readData);  // output data from sd
    connect(console, &Console::getData, this, &MainWindow::writeData);      // input data to sd

    highlighter = new Highlighter(console->document());
}

MainWindow::~MainWindow()
{
    sd->terminate();
    delete ui;
}

void MainWindow::writeData(const QByteArray &data)
{
    // console has data, send to sd
//    qDebug() << "writeData() to sd:" << data;

    QString d = data;
    d.replace("\r","\n");
    sd->write(d.toUtf8());
    sd->waitForBytesWritten();
}

void MainWindow::readData()
{
    // sd has data, send to console
    QByteArray s = sd->readAll();
//    qDebug() << "readData() from sd:" << s;

    QString qs(s);
    uneditedData.append(qs);

    // do deletes early
    bool done = false;
    while (!done) {
        int beforeLength = uneditedData.length();
        uneditedData.replace(QRegExp(".\b \b"),""); // if coming in as bulk text, delete one char
        int afterLength = uneditedData.length();
        done = (beforeLength == afterLength);
    }

    uneditedData.replace("\u0007","");  // delete BEL chars

    // echo is needed for entering the level, but NOT wanted after that
    if (s.contains("Enter startup command>")) {
        console->setLocalEchoEnabled(false);
    }

    QString lastLayout1;
    QString format2line;
    QList<QString> lastFormatList;

    QRegExp sequenceLine("^ *([0-9]+):(.*)");
    QRegExp sectionStart("^Sd 38.89");

    QStringList currentSequence;
    QString lastPrompt;

    // scan the unedited lines for sectionStart, layout1/2, and sequence lines
    QStringList lines = uneditedData.split("\n");
    foreach (const QString &line, lines) {
//        qDebug() << QString(" [%1] ").arg(line);
        if (line.contains("layout1:")) {
            lastLayout1 = line;
            lastLayout1 = lastLayout1.replace("layout1: ","").replace("\"","");
            lastFormatList.clear();
        } else if (line.contains("layout2:")) {
            format2line = line;
            format2line = format2line.replace("layout2: ","").replace("\"","");
            lastFormatList.append(format2line);
        } else if (sequenceLine.indexIn(line) > -1) {
            int itemNumber = sequenceLine.cap(1).toInt();
            QString call = sequenceLine.cap(2).trimmed();
//            qDebug() << itemNumber << ":" << call;
            if (itemNumber-1 < currentSequence.length()) {
                currentSequence.replace(itemNumber-1, call);
            } else {
                currentSequence.append(call);
            }
        } else if (line.contains(sectionStart)) {
            currentSequence.clear();
            editedData = "";  // sectionStart causes clear of editedData
        } else if (line == "") {
        } else if (line.contains("-->")) {
            // suppress output, but record it
            lastPrompt = line;
//            qDebug() << "lastPrompt:" << lastPrompt;
//            editedData += "DELETED PROMPT";  // no layout lines make it into here
//            editedData += "\n";  // no layout lines make it into here
        } else {
            editedData += line;  // no layout lines make it into here
            editedData += "\n";  // no layout lines make it into here
        }
    }

    editedData += lastPrompt.replace("\u0007","");  // keep only the last prompt (no NL)
//    qDebug() << "editedLastPrompt:" << lastPrompt.replace("\u0007","");
//    qDebug() << "currentSequence:" << currentSequence;
    // capitalize all the words in each call
    for (int i=0; i < currentSequence.length(); i++ ) {
        QString current = currentSequence.at(i);
        QStringList words = current.split(" ");
        for (int j=0; j < words.length(); j++ ) {
            QString current2 = words.at(j);
            QString replacement2 = current2.left(1).toUpper() + current2.mid(1).toLower();
            words.replace(j, replacement2);
        }
        QString replacement = words.join(" ");
//        qDebug() << current << replacement;
        currentSequence.replace(i, replacement);
    }

    currentSequenceWidget->setText(currentSequence.join("\n"));

    //editedData.chop(1);  // no last NL
    console->clear();
    console->putData(QByteArray(editedData.toLatin1()));

    console->setFocus();

    // look at unedited last line to see if there's a prompt
    if (lines[lines.length()-1].contains("-->")) {
//        qDebug() << "Found prompt:" << lines[lines.length()-1];
        QString formation;
        QRegExp r1( "[(](.*)[)]-->" );
        int pos = r1.indexIn( lines[lines.length()-1] );
        if ( pos > -1 ) {
            formation = r1.cap( 1 ); // "waves"
        }
        renderArea->setFormation(formation);
    }

//    qDebug() << "lastLayout1:" << lastLayout1;
//    qDebug() << "lastFormatList:" << lastFormatList;

    renderArea->setLayout1(lastLayout1);
    renderArea->setLayout2(lastFormatList);

}