#ifndef RUNDEVICE_H
#define RUNDEVICE_H
#include <QDialog>
#include <QString>

namespace Ui {
    class RunDevice;
}

class RunDevice : public QDialog
{
    Q_OBJECT

public:
    explicit RunDevice(QWidget *parent = 0);
    ~RunDevice();
    static RunDevice* instance(QWidget * parent = nullptr);

    QStringList getCurDeviceIdList();
    bool hasValidDefaultDeviceId();
    QString getValidDeviceId();

    int exec();

signals:
    void addDeviceList(QString dev);
protected slots:
    void accept();
    void reject();

    // adb command
    void onBuildAction();
    void onInstallAction();
    void onRunAction();
    void onStopAction();

    void onNewDevice(QString dev);
    void onRefreshDeviceList();
private:
    Ui::RunDevice *ui;

    QString mDeviceId;
    bool useDefault = false;
};

#endif // RUNDEVICE_H
