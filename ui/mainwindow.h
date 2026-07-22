#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class DeviceManager;
class DataCache;
class AlarmEngine;
class ModbusRtu;
class SerialPortComm;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshDevices();
    void onDeviceSelected(int index);

private:
    void setupUi();
    void setupConnections();
    void createStatusBar();
    void updateConnectionState(bool connected);

    // Core subsystems (owned)
    SerialPortComm *m_serial = nullptr;
    ModbusRtu     *m_modbus = nullptr;
    DeviceManager *m_deviceMgr = nullptr;
    DataCache     *m_cache = nullptr;
    AlarmEngine   *m_alarms = nullptr;
};

#endif // MAINWINDOW_H
