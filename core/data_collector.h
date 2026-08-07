#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <QObject>
#include <QList>
#include <QTimer>
#include <QModbusDataUnit>
#include "types.h"

class ModbusTcpClient;
class DataCache;
class AlarmEngine;

/// Orchestrates the acquisition pipeline:
///
///   ModbusTcpClient ──read──► DataCollector ──raw→real──► DataCache
///                                                    └────► AlarmEngine
///
/// Loads device/channel configuration from JSON, connects to the PLC or
/// simulator over Modbus TCP, polls holding registers on a timer, converts
/// raw integers to real values (value = raw*scale + offset) and pushes them
/// into the cache and alarm engine.
class DataCollector : public QObject
{
    Q_OBJECT

public:
    explicit DataCollector(ModbusTcpClient *client,
                           DataCache *cache,
                           AlarmEngine *alarms,
                           QObject *parent = nullptr);

    /// Load devices.json. Returns false on parse failure.
    bool loadConfig(const QString &jsonPath);

    // ---- control ----
    void connectDevice();          // connect TCP to the configured host
    void disconnectDevice();
    void startPolling(int intervalMs = 100);
    void stopPolling();

    // ---- config access ----
    const DeviceInfo &device() const { return m_device; }
    const QList<Channel> &channels() const { return m_channels; }

signals:
    void connectionStateChanged(bool connected);
    void statusMessage(const QString &message);

private slots:
    void onPollTick();
    void onRegistersRead(const QModbusDataUnit &unit);
    void onConnectionChanged(bool connected);

private:
    ModbusTcpClient *m_client;
    DataCache       *m_cache;
    AlarmEngine     *m_alarms;

    QTimer m_pollTimer;
    DeviceInfo   m_device;
    QList<Channel> m_channels;
    int m_startAddr = 0;
    int m_regCount  = 0;
};

#endif // DATA_COLLECTOR_H
