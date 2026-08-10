#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <QObject>
#include <QList>
#include <QVector>
#include <QTimer>
#include <QModbusDataUnit>
#include "types.h"
#include "sample_queue.h"

class IModbusClient;
class AlarmEngine;
class HistoryStore;

/// Orchestrates the acquisition pipeline for MULTIPLE devices:
///
///   for each device:
///     IModbusClient ──read──► DataCollector ──real──► SampleQueue (GUI)
///                                              ├────► HistoryStore (SQLite)
///                                              └────► AlarmEngine
///
/// Owns the Modbus transports, the alarm engine, the history store and the
/// sample queue. The whole object is moved onto a dedicated QThread by the
/// ViewModel, so all network I/O, polling, alarm evaluation and SQLite writes
/// happen OFF the GUI thread ("producer-consumer" — this collector is the
/// producer; the GUI drains the queue on a timer).
///
/// Because it runs on the worker thread, its state must never be read
/// directly from the GUI thread — the ViewModel mirrors link/online/failure
/// state through signals instead.
class DataCollector : public QObject
{
    Q_OBJECT

public:
    explicit DataCollector(QObject *parent = nullptr);
    ~DataCollector() override;

    // ---- control (run on the worker thread; call via QMetaObject::invoke) ----
    Q_INVOKABLE bool loadConfig(const QString &jsonPath);
    Q_INVOKABLE void connectAll();
    Q_INVOKABLE void disconnectAll();
    /// Remote control: write one holding register on a device (遥控/参数下发).
    /// The write is async; the outcome is reported on writeFinished only after
    /// the transport gets a real reply (success or failure) from the peer —
    /// never merely on request acceptance.
    Q_INVOKABLE void writeRegister(int deviceIndex, int regAddr, quint16 value);
    void startPolling(int intervalMs = 100);
    void stopPolling();

    // ---- config access ----
    const QList<DeviceInfo> &devices() const { return m_devices; }

    /// The producer→consumer queue the GUI drains on its own thread.
    SampleQueue *queue() { return &m_queue; }

    AlarmEngine *alarmEngine() const { return m_alarms; }

    /// The SQLite history store — lives on the worker thread with this object,
    /// so the GUI must query it via QMetaObject::invokeMethod (queued).
    HistoryStore *historyStore() const { return m_history; }

    /// Total samples handed to the history store (thread-safe read).
    qint64 historySamples() const;

private:
    /// Per-link reconnection state machine (heartbeat + exponential backoff).
    enum class LinkState { Active, Reconnecting };

    struct DeviceContext {
        DeviceInfo   info;
        IModbusClient *client = nullptr;   // transport-specific instance
        int startAddr = 0;
        int regCount  = 0;
        int failCount = 0;   // consecutive poll failures (timeouts)
        qint64 lastSuccessMs = 0;  // last successful read timestamp

        LinkState linkState = LinkState::Active;
        int    retryCount = 0;        // completed failed attempts (backoff ^)
        qint64 nextRetryAtMs = 0;     // do not retry before this timestamp
        bool   manualDisconnect = false;  // user disconnect → no auto-reconnect
    };

    AlarmEngine  *m_alarms  = nullptr;   // owned (child)
    HistoryStore *m_history = nullptr;   // owned (child)
    SampleQueue   m_queue;               // owned (value member, thread-safe)

    QList<DeviceInfo>  m_devices;
    QVector<DeviceContext*> m_ctx;
    /// Poll timer — must be a QObject CHILD (not a value member) so that
    /// moveToThread() migrates it together with this collector onto the
    /// worker thread; a plain member timer would keep its GUI-thread affinity
    /// and its start() from the worker thread would be a no-op.
    QTimer *m_pollTimer = nullptr;

    /// Consecutive failures after which a device is considered offline.
    static constexpr int kOfflineThreshold = 2;
    /// Heartbeat timeout: no successful response for this long ⇒ offline.
    static constexpr int kOfflineTimeoutMs = 3000;
    /// Exponential-backoff reconnection parameters.
    static constexpr int kBackoffBaseMs = 500;    // first retry wait
    static constexpr int kBackoffMaxMs = 30000;   // retry wait cap

    // ---- reconnect state machine ----
    void enterReconnect(DeviceContext *ctx);
    void attemptReconnect(DeviceContext *ctx);
    qint64 backoffMs(const DeviceContext *ctx) const;

private slots:
    void onPollTick();
    void onRegistersRead(DeviceContext *ctx, const QModbusDataUnit &unit);
    void onConnectionChanged(DeviceContext *ctx, bool connected);
    void onCommError(DeviceContext *ctx, const QString &msg);
    void setDeviceOnline(DeviceContext *ctx, bool online);

signals:
    /// Transport link state (TCP connected / serial port opened).
    void deviceConnectionChanged(int deviceIndex, bool connected);
    /// Data-alive state (received a response recently).
    void deviceOnlineChanged(int deviceIndex, bool online);
    /// Consecutive-failure counter changed (GUI-side diagnostics mirror).
    void deviceFailCountChanged(int deviceIndex, int count);
    /// Poll timer running state (GUI-side diagnostics mirror).
    void pollingStateChanged(bool active);
    void statusMessage(const QString &message);
    /// loadConfig() finished and m_devices is ready (GUI updates the cache).
    void configLoaded();
    /// A writeRegister() completed (or failed) on the bus — emitted from the
    /// worker thread after the transport's writeSucceeded/writeFailed.
    void writeFinished(int deviceIndex, bool ok, const QString &message);
};

#endif // DATA_COLLECTOR_H
