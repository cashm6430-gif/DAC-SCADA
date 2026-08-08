#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <QObject>
#include <QList>
#include <QVector>
#include <QTimer>
#include <QModbusDataUnit>
#include "types.h"

class ModbusTcpClient;
class ModbusSerialClient;
class DataCache;
class AlarmEngine;

/// Orchestrates the acquisition pipeline for MULTIPLE devices:
///
///   for each device:
///     ModbusTcpClient ──read──► DataCollector ──raw→real──► DataCache
///                                                       └────► AlarmEngine
///
/// Loads device/channel configuration from JSON, connects to every PLC or
/// simulator over Modbus TCP, polls all connected devices on a shared timer,
/// converts raw integers to real values and pushes them into the cache and
/// alarm engine. All devices are polled concurrently; the UI switches which
/// one is displayed.
class DataCollector : public QObject
{
    Q_OBJECT

public:
    explicit DataCollector(DataCache *cache,
                           AlarmEngine *alarms,
                           QObject *parent = nullptr);
    ~DataCollector() override;

    /// Load devices.json (multi-device). Returns false on parse failure.
    bool loadConfig(const QString &jsonPath);

    // ---- control ----
    void connectAll();
    void disconnectAll();
    void startPolling(int intervalMs = 100);
    void stopPolling();

    // ---- config access ----
    const QList<DeviceInfo> &devices() const { return m_devices; }
    int deviceCount() const { return m_devices.size(); }

    /// Whether the TCP+Modbus connection for device \a index is established.
    bool isDeviceConnected(int index) const;
    bool pollingActive() const { return m_pollTimer.isActive(); }

signals:
    void deviceConnectionChanged(int deviceIndex, bool connected);
    void statusMessage(const QString &message);

private:
    struct DeviceContext {
        DeviceInfo info;
        ModbusTcpClient    *tcpClient    = nullptr;
        ModbusSerialClient *serialClient = nullptr;
        int startAddr = 0;
        int regCount  = 0;
    };

    DataCache   *m_cache;
    AlarmEngine *m_alarms;

    QList<DeviceInfo>  m_devices;
    QVector<DeviceContext*> m_ctx;
    QTimer m_pollTimer;
    QHash<QString, ModbusSerialClient*> m_serialClients;  // portName → client

private slots:
    void onPollTick();
    void onRegistersRead(DeviceContext *ctx, const QModbusDataUnit &unit);
    void onConnectionChanged(DeviceContext *ctx, bool connected);
};

#endif // DATA_COLLECTOR_H
