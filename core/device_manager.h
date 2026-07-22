#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include <QModbusDataUnit>

class ModbusRtu;

/// Describes a single field device on the bus.
struct DeviceInfo {
    int     address = 0;
    QString name;
    QString type;       // e.g. "temperature_sensor", "pump_controller"
    bool    online = false;
};

/// Polls Modbus devices and emits parsed data / alarms.
class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(ModbusRtu *modbus, QObject *parent = nullptr);
    ~DeviceManager() override;

    /// Enumerate devices on the bus.
    void scan();

    /// Start / stop periodic polling of the active device.
    void setActiveDevice(int index);
    void startPolling(int intervalMs = 1000);
    void stopPolling();

    const QVector<DeviceInfo> &devices() const;

signals:
    void deviceDiscovered(const DeviceInfo &info);
    void deviceDataChanged(int deviceAddr, int registerAddr, double value);
    void alarmTriggered(int deviceAddr, const QString &message);

private slots:
    void onPollTimeout();
    void onRegistersRead(int deviceAddress, const QModbusDataUnit &unit);

private:
    ModbusRtu *m_modbus;
    QVector<DeviceInfo> m_devices;
    QTimer m_pollTimer;
    int m_activeIndex = -1;
};

#endif // DEVICE_MANAGER_H
