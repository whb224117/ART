//===- MainWindow.cpp - ART-GUI MainWindow ---------------------*- C++ -*-===//
//
//                     ANDROID REVERSE TOOLKIT
//
// This file is distributed under the GNU GENERAL PUBLIC LICENSE
// V3 License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
#include "MainWindow/MainWindow.h"
#include "ui_MainWindow.h"

#include "StatusLabel.h"
#include "CommandLineEdit.h"
#include "AboutArt/AboutArt.h"
#include "OpenApk/OpenApk.h"
#include "RunDevice/RunDevice.h"
#include "Config/Config.h"

#include "utils/CmdMsgUtil.h"
#include <utils/Configuration.h>
#include <utils/StringUtil.h>
#include <utils/ProjectInfo.h>
#include <utils/ScriptEngine.h>

#include <QMimeData>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Android Reverse Toolkit");
    loadFromConfig();

    mCenterWidget = new WorkSpace(this);
    setCentralWidget(mCenterWidget);

    setupCommandBar();
    setupStatusBar();
    mRunDevice = RunDevice::instance (this);
    
    // connect Menu signal/slots
    // File Menu
    connect(ui->actionExit, SIGNAL(triggered(bool)), this, SLOT(actionExit()));
    connect(ui->actionOpen, SIGNAL(triggered(bool)), this, SLOT(actionOpen()));
    connect(ui->actionClose_Project, SIGNAL(triggered(bool)),
            this, SLOT(actionCloseProject()));
    connect(ui->actionSave_All, SIGNAL(triggered(bool)), this, SLOT(actionSaveAll()));
    connect(ui->actionOption, SIGNAL(triggered(bool)), this, SLOT(actionOption()));

    // Edit
    connect(ui->actionUndo, SIGNAL(triggered(bool)), this, SLOT(actionUndo()));
    connect(ui->actionRedo, SIGNAL(triggered(bool)), this, SLOT(actionRedo()));
    connect(ui->actionCut, SIGNAL(triggered(bool)), this, SLOT(actionCut()));
    connect(ui->actionCopy, SIGNAL(triggered(bool)), this, SLOT(actionCopy()));
    connect(ui->actionPaste, SIGNAL(triggered(bool)), this, SLOT(actionPaste()));
    connect(ui->actionDelete, SIGNAL(triggered(bool)), this, SLOT(actionDelete()));
    connect(ui->actionSelect_All, SIGNAL(triggered(bool)), this, SLOT(actionSelectAll()));
    connect(ui->actionFind_Replace, SIGNAL(triggered(bool)), this, SLOT(actionFind()));
    connect(ui->actionGoto_Line, SIGNAL(triggered(bool)), this, SLOT(actionGotoLine()));
    connect(ui->actionSearch_Global, SIGNAL(triggered(bool)), this, SLOT(actionFindAdvance()));

    // run
    connect(ui->actionBuild, SIGNAL(triggered(bool)), this, SLOT(actionBuild()));
    connect(ui->actionInstall, SIGNAL(triggered(bool)), this, SLOT(actionInstall()));
    connect(ui->actionRun, SIGNAL(triggered(bool)), this, SLOT(actionRun()));
    connect(ui->actionDebug, SIGNAL(triggered(bool)), this, SLOT(actionDebug()));
    connect(ui->actionStop, SIGNAL(triggered(bool)), this, SLOT(actionStop()));
    connect(ui->actionDevices, SIGNAL(triggered(bool)), this, SLOT(actionDevices()));

    // About Menu
    connect(ui->actionAbout_ART, SIGNAL(triggered(bool)),
            this, SLOT(actionAboutArt()));

    // script
    auto* script = ScriptEngine::instance();
    connect(script, SIGNAL(projectOpened(QStringList)),
                this, SLOT(onProjectOpened(QStringList)));
    connect(script, SIGNAL(projectClosed(QStringList)),
                this, SLOT(onProjectClosed()));



    cmdmsg()->addCmdMsg("Android Reverse Toolkit v"
                        + GetCompileVersion () + " by f8left");
    cmdmsg()->setStatusMsg("ready");
    cmdmsg()->setLastLogMsg("init complete");
}

MainWindow::~MainWindow()
{
    saveToConfig ();
    delete ui;
}

void MainWindow::loadFromConfig()
{
    Configuration *config = Config();
    QPoint p;
    p.setX (config->getUint ("MainWindow","x"));
    p.setY (config->getUint ("MainWindow","y"));
    move(p);
    QSize s;
    s.setWidth (config->getUint ("MainWindow","width"));
    s.setHeight (config->getUint ("MainWindow","height"));
    resize (s);
}

void MainWindow::saveToConfig()
{
    Configuration *config = Config();

    QPoint p = pos();
    config->setUint ("MainWindow","x",p.x ());
    config->setUint ("MainWindow","y",p.y ());
    QSize s = size ();
    config->setUint ("MainWindow","width",s.width ());
    config->setUint ("MainWindow","height",s.height ());
}

void MainWindow::setupCommandBar()
{
    mCmdLineEdit = new CommandLineEdit(ui->cmdBar);
    ui->cmdBar->addWidget(mCmdLineEdit->selectorWidget());
    ui->cmdBar->addWidget(mCmdLineEdit);
}

void MainWindow::setupStatusBar()
{
    CmdMsg* cmdMsgUtil = CmdMsg::instance ();

    // Status label (Ready, Paused, ...)
    mStatusLabel = new StatusLabel(ui->statusBar);
    ui->statusBar->addWidget(mStatusLabel);

    // Log line
    mLastLogLabel = new StatusLabel();
    ui->statusBar->addPermanentWidget(mLastLogLabel, 1);

    connect(cmdMsgUtil, SIGNAL(setStatusMsg(QString)),
            mStatusLabel, SLOT(logUpdate(QString)));
    connect(cmdMsgUtil, SIGNAL(setLastLogMsg(QString)),
            mLastLogLabel, SLOT(logUpdate(QString)));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    QString fileName = urls.first().toLocalFile();
    openFile(fileName);
}

void MainWindow::closeEvent (QCloseEvent *event)
{
//    switch( QMessageBox::information( this, tr("Confirm Exit"),
//                                      tr("Are you sure you want to exit ART?"),
//                                      tr("No"), tr("Yes"),
//                                      0, 1 ))
//    {
//        case 1:
            cmdexec("CloseProject", CmdMsg::script, true, false);
            event->accept();
//            break;
//        default:
//            event->ignore();
//            break;
//    }
}


void MainWindow::actionExit()
{
    close ();
}

void MainWindow::actionOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                tr("Open File"), "", "apk File (*.apk *.dex)");
    openFile(fileName);
}

void MainWindow::actionCloseProject()
{
    cmdexec("CloseProject");
}

void MainWindow::actionSaveAll()
{
    cmdexec("SaveAll", CmdMsg::script, true, false);
}

void MainWindow::actionOption()
{
    Config config;
    config.exec();
}

void MainWindow::actionUndo()
{
    cmdexec("Undo", CmdMsg::script, true, false);
}

void MainWindow::actionRedo()
{
    cmdexec("Redo", CmdMsg::script, true, false);
}

void MainWindow::actionCut()
{
    cmdexec("Cut", CmdMsg::script, true, false);
}

void MainWindow::actionCopy()
{
    cmdexec("Copy", CmdMsg::script, true, false);
}

void MainWindow::actionPaste()
{
    cmdexec("Paste", CmdMsg::script, true, false);
}

void MainWindow::actionDelete()
{
    cmdexec("Delete", CmdMsg::script, true, false);
}

void MainWindow::actionSelectAll()
{
    cmdexec("SelectAll", CmdMsg::script, true, false);
}

void MainWindow::actionFind()
{
    cmdexec("Find", CmdMsg::script, true, false);
}

void MainWindow::actionGotoLine()
{
    cmdexec("GotoLine", CmdMsg::script, true, false);
}

void MainWindow::actionFindAdvance()
{
    cmdexec("FindAdvance", CmdMsg::script, true, false);
}

void MainWindow::actionAboutArt()
{
    AboutArt about(this);
    about.exec ();
}


void MainWindow::actionBuild()
{
    if(!ProjectInfo::instance ()->isProjectOpened ()) {
        return;
    }
    cmdexec("Build", CmdMsg::script, true, false);
}

void MainWindow::actionInstall()
{
    if(!ProjectInfo::instance ()->isProjectOpened ()) {
        return;
    }
    cmdexec("Install", CmdMsg::script, true, false);
}

void MainWindow::actionRun()
{
    if(!ProjectInfo::instance ()->isProjectOpened ()) {
        return;
    }
    if(needToRebuild ()) {
        qDebug() << "Current project need to rebuild.";
        cmdexec("Build");
        cmdexec("Install");
        cmdexec("Run");
    } else {
        cmdexec("Run", CmdMsg::script, true, false);
    }
}

void MainWindow::actionDebug()
{
    if(!ProjectInfo::instance ()->isProjectOpened ()) {
        return;
    }
    if(needToRebuild ()) {
        cmdexec("Build");
        cmdexec("Install");
        cmdexec("Debug");
    } else {
        cmdexec("Debug", CmdMsg::script, true, false);
    }
}

bool MainWindow::needToRebuild ()
{
    auto project = ProjectInfo::instance ();
    QFileInfo buildFile(project->getProjectPath() + "/Bin/signed.apk");
    if(!buildFile.exists()) {
        return true;
    }
    auto buildModified = buildFile.lastModified ();
    QDirIterator it(project->getProjectPath() + "/Project", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        auto filePath = it.next();
        auto info = QFileInfo(filePath);
        if(info.lastModified () > buildModified) {
            return true;
        }
    }
    return false;
}

void MainWindow::actionStop()
{
    cmdexec("Stop", CmdMsg::script, true, false);
}

void MainWindow::actionDevices()
{
    cmdexec("Devices", CmdMsg::script, true, false);
}

void MainWindow::onProjectOpened(QStringList projName)
{
    if(projName.empty())
        return;
    setWindowTitle("Android Reverse Toolkit - " + projName.front());
}

void MainWindow::onProjectClosed()
{
    setWindowTitle("Android Reverse Toolkit");
}

void MainWindow::openFile(QString fileName)
{
    if (fileName.isEmpty()) {
        return;
    }
    if (!fileName.endsWith(".apk") && !fileName.endsWith(".dex")) {
        return;
    }
    if(ProjectInfo::instance ()->isProjectOpened ()) {
        QMessageBox msg(QMessageBox::Warning,
                        tr("an opened project has been found"),
                        tr("do you want to close current project?"),
                        QMessageBox::Ok | QMessageBox::Cancel);
        msg.setWindowIcon(QIcon(":/icons/images/compile-warning.png"));
        msg.setWindowFlags(msg.windowFlags() & (~Qt::WindowContextHelpButtonHint));
        if(msg.exec() == QMessageBox::Cancel)
            return;
        else
            cmdexec("CloseProject");
    }

    OpenApk* openWidget = new OpenApk(fileName, this);
    if (openWidget->exec() == QDialog::Accepted) {
        // decompile
        QString cmd = openWidget->getDecompileCmd();
        cmdexec(cmd, CmdMsg::cmd);
//         openProject
        QStringList args;
        args << openWidget->getFileName ();
        cmdexec("OpenProject", args);
    }
    delete openWidget;
}


