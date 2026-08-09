#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <QObject>
#include <QList>
#include <QVector>
#include <QTimer>
#include <QModbusDataUnit>
#include "types.h"

class IModbusClient;
class DataCache;
class AlarmEngine;

/// Orchestrates the acquisition pipeline for MULTIPLE devices:
///
///   for each device:
///     IModbusClient ──read──► DataCollector ──raw→real──► DataCache
///                                                       └────► AlarmEngine
///
/// Loads device/channel configuration from JSON, connects to every PLC or
/// simulator over Modbus (TCP or serial), polls all connected devices on a
/// shared timer, converts raw integers to real values and pushes them into
/// the cache and alarm engine. All devices are polled concurrently; the UI
/// switches which one is displayed. The transport (TCP vs serial) is decided
/// at connect-time by a factory, so the acquisition logic itself is
/// transport-agnostic.
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
    /// Whether the device is considered online (has responded recently).
    bool isDeviceOnline(int index) const;
    /// Consecutive poll failures of device \a index (diagnostics).
    int failureCount(int index) const;
    bool pollingActive() const { return m_pollTimer.isActive(); }

signals:
    /// Transport link state (TCP connected / serial port opened).
    void deviceConnectionChanged(int deviceIndex, bool connected);
    /// Data-alive state (received a response recently).
    void deviceOnlineChanged(int deviceIndex, bool online);
    void statusMessage(const QString &message);

private:
    struct DeviceContext {
        DeviceInfo   info;
        IModbusClient *client = nullptr;   // transport-specific instance
        int startAddr = 0;
        int regCount  = 0;
        int failCount = 0;   // consecutive poll failures (timeouts)
        qint64 lastSuccessMs = 0;  // last successful read timestamp
    };

    DataCache   *m_cache;
    AlarmEngine *m_alarms;

    QList<DeviceInfo>  m_devices;
    QVector<DeviceContext*> m_ctx;
    QTimer m_pollTimer;

    /// Consecutive failures after which a device is considered offline.
    static constexpr int kOfflineThreshold = 2;
    /// No successful response for this long → device considered offline.
    static constexpr int kOfflineTimeoutMs = 3000;

private slots:
    void onPollTick();
    void onRegistersRead(DeviceContext *ctx, const QModbusDataUnit &unit);
    void onConnectionChanged(DeviceContext *ctx, bool connected);
    void onCommError(DeviceContext *ctx, const QString &msg);
    void setDeviceOnline(DeviceContext *ctx, bool online);
};

#endif // DATA_COLLECTOR_H
