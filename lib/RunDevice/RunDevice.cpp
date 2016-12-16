//===- RunDevice.cpp - ART-GUI  --------------------------------*- C++ -*-===//
//
//                     ANDROID REVERSE TOOLKIT
//
// This file is distributed under the GNU GENERAL PUBLIC LICENSE
// V3 License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
#include "RunDevice/RunDevice.h"
#include "ui_RunDevice.h"

#include <utils/ScriptEngine.h>
#include <utils/ProjectInfo.h>
#include <utils/StringUtil.h>
#include <utils/CmdMsgUtil.h>
#include <utils/AdbUtil.h>

#include <QRegExp>
#include <QtConcurrent/QtConcurrent>
#include <QAtomicInteger>

void getDeviceMsgThread(RunDevice* e);

RunDevice::RunDevice(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RunDevice)
{
    ui->setupUi(this);

    setWindowTitle("Select Deployment Target");
    setWindowFlags(windowFlags()& ~Qt::WindowMaximizeButtonHint);
    setFixedSize(this->width(), this->height());

    ScriptEngine* script = ScriptEngine::instance();
    connect(script, &ScriptEngine::build, this, &RunDevice::onBuildAction);
    connect(script, &ScriptEngine::install, this, &RunDevice::onInstallAction);
    connect(script, &ScriptEngine::run, this, &RunDevice::onRunAction);
    connect(script, &ScriptEngine::debug, this, &RunDevice::onDebugAction);
    connect(script, &ScriptEngine::stop, this, &RunDevice::onStopAction);
    connect(script, &ScriptEngine::devices, this, &RunDevice::exec);


    connect(this, SIGNAL(addDeviceList(QString)), this, SLOT(onNewDevice(QString)));
    connect(ui->mRefreshBtn, SIGNAL(clicked(bool)), this, SLOT(onRefreshDeviceList()));
}

RunDevice::~RunDevice()
{
    delete ui;
}

RunDevice *RunDevice::instance(QWidget * parent)
{
    static RunDevice* mPtr = nullptr;
    if(mPtr == nullptr) {
        Q_ASSERT (parent != nullptr && "Rundevice should init in MainWindow");
        mPtr = new RunDevice(parent);
    }
    return mPtr;
}

void RunDevice::accept()
{
    if (ui->mDevicesList->currentRow() == -1)
        return QDialog::reject();
    QString devices = ui->mDevicesList->currentItem()->text();
    QStringList devMsg = devices.split('$', QString::SkipEmptyParts);
    if (devMsg.size() > 1) {
        mDeviceId = devMsg.last();
        useDefault = ui->mSameCheckBox->isChecked();
        projinfoset("DeviceId", mDeviceId);
    }
    QDialog::accept();
}

void RunDevice::reject()
{
    QDialog::reject();
}

void RunDevice::onBuildAction ()
{
    auto* project = ProjectInfo::instance ();
    if(!project->isProjectOpened ()) {
        return;
    }
    // build
    QString buildCmd = project->getInfo ("CompileCmd");
    cmdexec(buildCmd, 10, CmdMsg::cmd);

    // signed
    QStringList signArgs;
    signArgs << "-jar"
             << "./thirdparty/signapk/signapk.jar"
             << "./cfgs/keystore/FDA.x509.pem"
             << "./cfgs/keystore/FDA.pk8"
             << project->getProjectPath() + "/Bin/unsigned.apk"
             << project->getProjectPath() + "/Bin/signed.apk";
    cmdexec("java", signArgs, 10, CmdMsg::cmd);
}


void RunDevice::onInstallAction()
{
    auto* project = ProjectInfo::instance ();
    if(!project->isProjectOpened ()) {
        return;
    }
    QString devId = getValidDeviceId();
    if (devId.isEmpty())
        return;
    cmdmsg()->addCmdMsg ("install apk package " + project->getInfo ("PackageName"));

    QString projectRoot = project->getProjectPath();
    QStringList args;
    args << "-s"
         << mDeviceId
         << "install"
         << "-r"
         << projectRoot + "/Bin/signed.apk";
    ScriptEngine::instance()->adbShell(args);
}

void RunDevice::onRunAction()
{
    auto* project = ProjectInfo::instance ();
    if(!project->isProjectOpened ()) {
        return;
    }
    QString devId = getValidDeviceId();
    if (devId.isEmpty())
        return;

    cmdmsg()->addCmdMsg ("Running apk file " + project->getInfo ("PackageName"));


    QString projectRoot = project->getProjectPath();
    QStringList args;
    args << "-s"
         << mDeviceId
         << "shell"
         << "am"
         << "start"
         << project->getInfo ("PackageName") +
                 "/" + project->getInfo ("ActivityEntryName");
    ScriptEngine::instance()->adbShell(args);
}

void RunDevice::onDebugAction()
{
    auto* project = ProjectInfo::instance ();
    if(!project->isProjectOpened ()) {
        return;
    }
    QString devId = getValidDeviceId();
    if (devId.isEmpty())
        return;

    cmdmsg()->addCmdMsg ("Running apk file " + project->getInfo ("PackageName"));

    QString projectRoot = project->getProjectPath();
    QStringList args;
    args << "-s"
         << mDeviceId
         << "shell"
         << "am"
         << "start"
         << "-D"
         << "-n"
         << project->getInfo ("PackageName") +
                 "/" + project->getInfo ("ActivityEntryName");
    ScriptEngine::instance()->adbShell(args);
    cmdexec("DebugStart", project->getInfo ("PackageName"));
}

void RunDevice::onStopAction()
{
    auto* project = ProjectInfo::instance ();
    if(!project->isProjectOpened ()) {
        return;
    }
    QString devId = getValidDeviceId();
    if (devId.isEmpty())
        return;

    cmdmsg()->addCmdMsg ("Stoping apk file " + project->getInfo ("PackageName"));

    QString projectRoot = project->getProjectPath();
    QStringList args;
    args << "-s"
         << mDeviceId
         << "shell"
         << "am"
         << "force-stop"
         << project->getInfo ("PackageName");
    ScriptEngine::instance()->adbShell(args);
}

void RunDevice::onNewDevice(QString dev)
{
    ui->mDevicesList->addItem(dev);
    ui->mDevicesList->setCurrentRow(0);
}

void RunDevice::onRefreshDeviceList()
{
    AdbUtil adbUtil;
    adbUtil.execute("kill-server");
    ui->mDevicesList->clear();
    QtConcurrent::run(getDeviceMsgThread, this);
}

int RunDevice::exec()
{
    ui->mDevicesList->clear();
    QtConcurrent::run(getDeviceMsgThread, this);
    return QDialog::exec();
}

QStringList RunDevice::getCurDeviceIdList()
{
    AdbUtil adbUtil;
    QStringList devs = adbUtil.execute("devices");
    // deviceId status
    QStringList devId;
    foreach(QString deviceMsg, devs) {
        QStringList device = deviceMsg.split(QRegExp("\\s+"), QString::SkipEmptyParts);
        if (device.size() != 2) {
            continue;
        }
        if(device[1].compare("device", Qt::CaseInsensitive) != 0) {
            // only online device is abled to use
            continue;
        }
        devId.push_back(device[0]);
    }
    return devId;
}

bool RunDevice::hasValidDefaultDeviceId()
{
    if (!useDefault)
        return false;
    if (!getCurDeviceIdList().contains(mDeviceId)) {
        return false;
    }
    return true;
}

QString RunDevice::getValidDeviceId()
{
    if (!hasValidDefaultDeviceId() && exec() != QDialog::Accepted )
        return QString();
    return mDeviceId;
}


void getDeviceMsgThread(RunDevice* runDevice) {
    static QAtomicInteger<bool> running;
    if(running) {
        return;
    }
    running = true;

    AdbUtil adbUtil;
    QStringList devIds = runDevice->getCurDeviceIdList();
    foreach(const QString& deviceId, devIds) {
            QStringList propList = adbUtil.execute("-s " + deviceId + " shell getprop");
            QMap<QString, QString> propMap;
            foreach(QString prop, propList) {
                    prop.remove(QRegExp("[ \\[\\]]"));
                    auto keyvalue = prop.split(':', QString::SkipEmptyParts);
                    if(keyvalue.size() == 2) {
                        propMap[keyvalue[0]] = keyvalue[1];
                    }
                }
            QString deviceMsg =
                propMap["ro.product.manufacturer"] + " " +
                propMap["ro.product.model"] + " (" +
                "Android " + propMap["ro.build.version.release"] +
                ", API " + propMap["ro.build.version.sdk"]   + ")" +
                "$" + deviceId;
        emit runDevice->addDeviceList(deviceMsg);
    }
    running = false;
}

