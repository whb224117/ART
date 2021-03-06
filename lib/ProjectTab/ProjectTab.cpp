//===- ProjectTab.cpp - ART-GUI Project Tab --------------------*- C++ -*-===//
//
//                     ANDROID REVERSE TOOLKIT
//
// This file is distributed under the GNU GENERAL PUBLIC LICENSE
// V3 License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
#include "ProjectTab/ProjectTab.h"
#include "ui_ProjectTab.h"

#include <utils/StringUtil.h>
#include <utils/CmdMsgUtil.h>
#include <utils/ProjectInfo.h>
#include <utils/ScriptEngine.h>
#include "SmaliAnalysis/SmaliAnalysis.h"


#include <QtCore/QDir>
#include <utils/Configuration.h>
#include <QMenu>
#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>
#include "QPushButton"

ProjectTab::ProjectTab(QWidget *parent) :
        QStackedWidget(parent),
        ui(new Ui::ProjectTab),
        mHasProject(false)
{
    ui->setupUi(this);

    setCurrentIndex(0);

    ui->mProjectList->setContextMenuPolicy(Qt::CustomContextMenu);
    mFileSystemModel = new QFileSystemModel(this);
    QDir dir(GetProjectsPath ());
    if (dir.exists()) {
        QString projectRootPath = dir.absolutePath();
        mFileSystemModel->setRootPath(projectRootPath);

        ui->mProjectList->setModel(mFileSystemModel);
        ui->mProjectList->setRootIndex(mFileSystemModel->index(projectRootPath));
        ui->mProjectList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->mProjectList->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->mProjectList->hideColumn(1); ui->mProjectList->hideColumn(2);

        QHeaderView * hHeader = ui->mProjectList->horizontalHeader();
        hHeader->setSectionResizeMode(QHeaderView::Stretch);
        hHeader->setStretchLastSection(false);
        hHeader->setSortIndicator(1, Qt::DescendingOrder);    // sort by date
        hHeader->hide();
        ui->mProjectList->verticalHeader()->hide();
    }

    // signals / slots
    // ProejctList
    connect(ui->mProjectList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(projectListShowMenu()));


    // button
    connect(ui->mButtonBox->button (QDialogButtonBox::Save), &QPushButton::clicked, [this]() {
        ProjectInfo::current()->config().m_compileCmd = ui->mCompileEdit->text ();
    });
    connect(ui->mButtonBox->button (QDialogButtonBox::Reset), &QPushButton::clicked, [this]{
        ui->mCompileEdit->setText (ProjectInfo::current()->config().m_compileCmd);
        readProjectInfo();
    });

    connect(ui->mEntryLabel, SIGNAL(linkActivated(QString)),
            this, SLOT(openActivityInEditor(QString)));

    // script
    auto* script = ScriptEngine::instance();
    connect(ui->mProjectList, &QTableView::doubleClicked, [this](const QModelIndex &index) {
        openProject (ui->mProjectList->model()->
                index(index.row(), 0, ui->mProjectList->rootIndex()).data().toString());
    });

    connect(script, SIGNAL(openProject (QStringList)),
            this, SLOT(openProject (QStringList)));
    connect(script, &ScriptEngine::closeProject, this, &ProjectTab::closeProject);
    connect(script, &ScriptEngine::projectOpened, this, &ProjectTab::onProjectOpened);
    connect(script, &ScriptEngine::projectClosed, this, &ProjectTab::onProjectClosed);


    connect(ui->mActivityInfoList, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(onActivityClick(QListWidgetItem*)));
}

ProjectTab::~ProjectTab()
{
    delete ui;
}

void ProjectTab::openProject (QStringList args)
{
    if(args.empty ())
        return;
    openProject (args.front ());
}

void ProjectTab::openProject (QString projectName)
{
    if(mHasProject)
        return;
    mHasProject = true;
    cmdmsg ()->addCmdMsg ("opening project " + projectName);

    auto projPath = GetProjectsProjectPath (projectName);

    QDir dir(projPath);
    if (!dir.exists()) {
        cmdmsg ()->addCmdMsg("project " + projectName + " not exist");
        return;
    }
    auto pinfo = ProjectInfo::openProject(projectName);

    mProjectName = projectName;

    readProjectInfo();
    pinfo->config().m_packageName = mPackageName;
    pinfo->config().m_applicationName = mApplicationName;
    pinfo->config().m_activityEntryName = mActivityEntryName;

    auto analysis = SmaliAnalysis::instance();
    for(auto& src: mSmaliDirectory) {
        analysis->addSourcePath(src);
    }

    cmdexec("ProjectOpened", projectName);
}

void ProjectTab::closeProject()
{
    if(!mHasProject)
        return;
    mHasProject = false;
    cmdmsg ()->addCmdMsg ("closing project " + mProjectName);



    auto analysis = SmaliAnalysis::instance ();
    analysis->clear();

    ProjectInfo::closeProject();
    cmdexec("ProjectClosed");
}

void ProjectTab::onProjectOpened (QStringList projName)
{
    cmdmsg ()->addCmdMsg ("project " + mProjectName + " opened.");
    setCurrentIndex(1);

}

void ProjectTab::onProjectClosed()
{
    cmdmsg ()->addCmdMsg ("project " + mProjectName + " closed");
    setCurrentIndex(0);
}


void ProjectTab::onActivityClick (QListWidgetItem *item)
{
    auto activityName = item->text ();
    openActivityInEditor (activityName);
}

void ProjectTab::readProjectInfo ()
{
    if(mHasProject) {
        QString cmd = ProjectInfo::current()->config().m_compileCmd;
        ui->mCompileEdit->setText(cmd);
        readProjectYmlInfo();
        readProjectManifestInfo();
    }
}

void ProjectTab::readProjectYmlInfo ()
{
    QString projectPath = GetProjectsProjectPath (mProjectName);

    // dexMaps defaul smali | smali_xxxx
    mSmaliDirectory.clear();
    QDir dir(projectPath);
    if (dir.exists()) {
        dir.setFilter(QDir::Dirs | QDir::NoSymLinks);
                foreach(QFileInfo mfi ,dir.entryInfoList())
            {
                if(mfi.isDir ())
                {
                    if(mfi.fileName ().startsWith ("smali")) {
                        mSmaliDirectory.append(mfi.absoluteFilePath ());
                    }
                }
            }
    }

    mVersion.clear(); mVersionCode.clear(); mVersionName.clear();
    QString ymlPath = projectPath + "/apktool.yml";
    QFile file(ymlPath);
    if(!file.open(QFile::ReadOnly)) {
        return;
    }
    for(bool version = false, versioncode = false, versionname = false;
            !file.atEnd(); ) {
        auto line = file.readLine();
        if(line.contains("version:")) {
            auto msglist = QString(line).split(QRegExp("\\s+"), QString::SkipEmptyParts);
            QString& msg = msglist.last();
            mVersion = msg; version = true;

        } else
        if(line.contains("versionCode:")) {
            auto msglist = QString(line).split(QRegExp("\\s+"), QString::SkipEmptyParts);
            QString& msg = msglist.last();
            mVersionCode = msg; versioncode = true;
        } else
        if(line.contains("versionName:")) {
            auto msglist = QString(line).split(QRegExp("\\s+"), QString::SkipEmptyParts);
            QString& msg = msglist.last();
            mVersionName = msg; versionname = true;
        }
        if(version && versioncode && versionname) {
            break;
        }
    }
    ui->mVersionLabel->setText(mVersion);
    ui->mVersionCodeLabel->setText(mVersionCode);
    ui->mVersionNameLabel->setText(mVersionName);
}

void ProjectTab::readProjectManifestInfo ()
{
    ui->mActivityInfoList->clear();

    QString projectPath = GetProjectsProjectPath (mProjectName);

    QString miniFest = projectPath + "/AndroidManifest.xml";
    QFile file(miniFest);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        return;
    }
    // Root Element
    mApplicationName.clear(); mActivityEntryName.clear();
    QDomElement docElem= doc.firstChildElement("manifest");
    if (!docElem.isNull()) {
        mPackageName = docElem.attribute("package");

        QDomElement appElem = docElem.firstChildElement("application");
        if (!appElem.isNull()) {
            mApplicationName = appElem.attribute("android:name");
            if(mApplicationName.isEmpty()) {
                mApplicationName = "Landroid/app/Application;";
            }

            for(QDomElement avityElem = appElem.firstChildElement("activity");
                !avityElem.isNull();
                avityElem = avityElem.nextSiblingElement("activity")) {
                ui->mActivityInfoList->addItem(avityElem.attribute("android:name"));

                QDomElement intentElem = avityElem.firstChildElement("intent-filter");
                if(intentElem.isNull()) {
                    continue;
                }
                QDomElement categoryElem = intentElem.firstChildElement("category");
                if (categoryElem.isNull()) {
                    continue;
                }
                if (categoryElem.attribute("android:name")
                    == "android.intent.category.LAUNCHER") {
                    mActivityEntryName = avityElem.attribute("android:name");
                    continue;
                }
            }
        }
    }
    ui->mPackageNameLabel->setText(mPackageName);
    ui->mApplicationNameLabel->setText(mApplicationName);
    ui->mEntryLabel->setText(
            "<a href=\"" + mActivityEntryName + "\">" +
            mActivityEntryName + "</a>");

    file.close();
}

void ProjectTab::openActivityInEditor (QString activityName)
{
    if(activityName.at (0) == '.') {
        activityName = mPackageName + activityName;
    }
    activityName.replace (".", "/");
    activityName.push_front ("/");
    activityName.push_back (".smali");

    QString filePath;
            foreach(QString smali, mSmaliDirectory) {
            auto tmp = smali + activityName;
            if(QFileInfo::exists (tmp)) {
                filePath = tmp;
                break;
            }
        }

    if(!filePath.isEmpty ()) {
        cmdexec("OpenFile", filePath, CmdMsg::script, true, false);
    }
}

void ProjectTab::projectListShowMenu()
{
    QModelIndex index= ui->mProjectList->currentIndex();
    if(!index.isValid ())
        return;

    QMenu* menu=new QMenu(this);
    menu->addAction(tr("Show in Files"), this, SLOT(projectFileShowInFile()));
    menu->addAction(tr("Remove Project"), this, SLOT(projectFileRemove()));
    menu->exec(QCursor::pos());
    menu->deleteLater ();
}

void ProjectTab::projectFileShowInFile()
{
    QModelIndex index= ui->mProjectList->currentIndex();
    auto fileinfo = mFileSystemModel->fileInfo (index);

    QDesktopServices::openUrl(QUrl::fromLocalFile(fileinfo.absolutePath ()));
}

void ProjectTab::projectFileRemove()
{
    QModelIndex index= ui->mProjectList->currentIndex();
    auto fileinfo = mFileSystemModel->fileInfo (index);

    QDir dir(fileinfo.absoluteFilePath ());
    dir.removeRecursively();
}




