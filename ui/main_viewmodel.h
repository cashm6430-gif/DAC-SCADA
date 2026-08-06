#ifndef MAIN_VIEWMODEL_H
#define MAIN_VIEWMODEL_H

#include <QObject>
#include <QVariantList>

class SerialPortComm;
class ModbusRtu;
class DeviceManager;
class DataCache;
class AlarmEngine;
class QAbstractTableModel;

/// ViewModel — owns all Model objects, exposes data & commands to the View.
class MainViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY portNameChanged)
    Q_PROPERTY(int activeAlarmCount READ activeAlarmCount NOTIFY activeAlarmCountChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit MainViewModel(QObject *parent = nullptr);
    ~MainViewModel() override;

    // ---- Properties ----
    bool isConnected() const;

    QString portName() const;
    void setPortName(const QString &name);

    int activeAlarmCount() const;

    QString statusText() const;

    // ---- Models exposed to View for direct binding ----
    QAbstractTableModel *dataModel() const;

    // ---- Commands (Q_INVOKABLE so View can call from QML or invokeMethod) ----
    Q_INVOKABLE void connectToDevice();
    Q_INVOKABLE void disconnectFromDevice();
    Q_INVOKABLE void scanDevices();
    Q_INVOKABLE void selectDevice(int index);
    Q_INVOKABLE void acknowledgeAlarm(int index);

signals:
    void connectedChanged();
    void portNameChanged();
    void activeAlarmCountChanged();
    void statusTextChanged(const QString &text);

    /// Non-property signals for one-shot UI feedback.
    void connectionError(const QString &message);
    void deviceListChanged();
    void newAlarmTriggered(const QString &message);

private:
    void setupModelConnections();

    // ---- Model objects (owned) ----
    SerialPortComm *m_serial   = nullptr;
    ModbusRtu      *m_modbus   = nullptr;
    DeviceManager  *m_deviceMgr = nullptr;
    DataCache      *m_cache    = nullptr;
    AlarmEngine    *m_alarms   = nullptr;
};

#endif // MAIN_VIEWMODEL_H
